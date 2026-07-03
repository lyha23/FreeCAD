#include "cad_core/part_design/feature_chamfer.h"
#include "cad_core/part_design/feature_draft.h"
#include "cad_core/part_design/feature_fillet.h"
#include "cad_core/part_design/feature_thickness.h"

#include "feature_dress_up_support.h"

#include "cad_core/part_design/body_topo_shape.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRep_Tool.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_Shape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAPI_IntSS.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Geom_Plane.hxx>
#include <Geom_Surface.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::part_design
{

namespace detail
{

struct BodyLocalFeatureContext
{
    const app::DocumentObject* body = nullptr;
    std::vector<std::string> groupNames;
    std::size_t featureIndex = 0U;
};

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType")
        && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string readEnumProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::vector<std::string>& values,
    const std::string& fallback
)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr) {
        return fallback;
    }
    if (payload->is_string()) {
        return payload->get<std::string>();
    }
    if (payload->is_number_integer()) {
        const int index = payload->get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < values.size()) {
            return values[static_cast<std::size_t>(index)];
        }
    }
    return fallback;
}

bool readBoolProperty(const app::DocumentObject& object, const std::string& property, bool fallback)
{
    return app::readBool(object, property).value_or(fallback);
}

double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback)
{
    return app::readNumber(object, property).value_or(fallback);
}

bool isDressUpType(const std::string& typeId)
{
    return typeId == "PartDesign::Fillet" || typeId == "PartDesign::Chamfer"
        || typeId == "PartDesign::Draft" || typeId == "PartDesign::Thickness";
}

bool hasSolid(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    TopExp_Explorer explorer(shape, TopAbs_SOLID);
    return explorer.More();
}

int countSolids(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

std::vector<TopoDS_Shape> solidSubshapes(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> solids;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        solids.push_back(explorer.Current());
    }
    return solids;
}

std::vector<std::string> readBodyGroupNames(const app::DocumentObject& body)
{
    std::vector<std::string> result;
    for (const auto& link : app::readLinks(body, "Group")) {
        result.push_back(link.object);
    }
    return result;
}

std::optional<std::size_t> indexOf(const std::vector<std::string>& values, const std::string& value)
{
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(values.begin(), it));
}

std::optional<BodyLocalFeatureContext> bodyLocalFeatureContext(
    const std::string& featureName,
    const runtime::ComputeContext& context
)
{
    const auto parentIt = context.parentGroupByObject.find(featureName);
    if (parentIt == context.parentGroupByObject.end()) {
        return std::nullopt;
    }
    const auto bodyIt = context.documentObjects.find(parentIt->second);
    if (bodyIt == context.documentObjects.end() || bodyIt->second == nullptr
        || bodyIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }

    std::vector<std::string> groupNames = readBodyGroupNames(*bodyIt->second);
    const auto featureIndex = indexOf(groupNames, featureName);
    if (!featureIndex) {
        return std::nullopt;
    }
    return BodyLocalFeatureContext {bodyIt->second, std::move(groupNames), *featureIndex};
}

std::optional<BodyLocalFeatureContext> sameBodyEarlierFeatureContext(
    const app::DocumentObject& object,
    const std::string& targetFeature,
    const runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.h
    // ::DressUp::Base, "for consistency if BaseFeature is nonzero this links to the same body
    // as it"; same-Body dress-up links therefore need the Body.Group feature order.
    const auto current = bodyLocalFeatureContext(object.name, context);
    const auto target = bodyLocalFeatureContext(targetFeature, context);
    if (!current || !target || current->body != target->body) {
        return std::nullopt;
    }
    if (target->featureIndex >= current->featureIndex) {
        return std::nullopt;
    }
    return target;
}

std::string stripObjectPrefix(const std::string& value, const std::string& objectName)
{
    const std::string prefix = objectName + ".";
    if (value.rfind(prefix, 0U) == 0U) {
        return value.substr(prefix.size());
    }
    return value;
}

app::Link bodyTopoShapeLink(const app::Link& link)
{
    app::Link result = link;
    for (std::size_t index = 0; index < result.subnames.size(); ++index) {
        result.subnames[index] = stripObjectPrefix(result.subnames[index], link.object);
    }
    for (std::size_t index = 0; index < result.stableSubnames.size(); ++index) {
        std::string& stableSubname = result.stableSubnames[index];
        if (stableSubname.empty() || stableSubname.find('.') != std::string::npos) {
            continue;
        }
        const std::string currentSubname =
            index < result.subnames.size() ? result.subnames[index] : std::string {};
        if (stableSubname == currentSubname) {
            // Omitted StableSubList decodes to SubList as a current-name fallback, not target-owner evidence.
            continue;
        }
        stableSubname = link.object + "." + stableSubname;
    }
    return result;
}

std::optional<BodyTopoShapeResult> bodyTopoShapeAtFeature(
    const BodyLocalFeatureContext& target,
    runtime::ComputeContext& context,
    bool includeDisplayOnlyGeometry = true
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
    // ::Body::execute(), "Shape.setValue(tipShape)" publishes the cumulative Body shape at the
    // current Tip; cad-core reconstructs that same Body-at-feature state for earlier features.
    const BodyTopoShapeOptions options {
        false,
        false,
        includeDisplayOnlyGeometry,
    };
    const std::size_t diagnosticCount = context.diagnostics.size();
    const auto result =
        getBodyTopoShapeAtFeature(*target.body, context, target.groupNames.at(target.featureIndex), options);
    if (context.diagnostics.size() > diagnosticCount) {
        context.diagnostics.erase(context.diagnostics.begin() + static_cast<std::ptrdiff_t>(diagnosticCount),
                                  context.diagnostics.end());
    }
    return result;
}

std::optional<BodyTopoShapeResult> previousBodyTopoShape(
    const std::string& featureName,
    runtime::ComputeContext& context,
    bool includeDisplayOnlyGeometry = true
)
{
    const auto current = bodyLocalFeatureContext(featureName, context);
    if (!current || current->featureIndex == 0U) {
        return std::nullopt;
    }

    for (std::size_t index = current->featureIndex; index > 0U; --index) {
        BodyLocalFeatureContext target = *current;
        target.featureIndex = index - 1U;
        const auto bodyTopoShape = bodyTopoShapeAtFeature(target, context, includeDisplayOnlyGeometry);
        if (bodyTopoShape && hasSolid(bodyTopoShape->shape)) {
            return bodyTopoShape;
        }
    }
    return std::nullopt;
}

std::optional<part::NamedShape> namedShapeForObject(
    const std::string& objectName,
    const TopoDS_Shape& shape,
    const runtime::ComputeContext& context
)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    if (!shape.IsNull()) {
        return part::indexedNamedShapeForObject(objectName, shape);
    }
    return std::nullopt;
}

part::NamedShapeSource namedShapeSource(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
)
{
    return part::NamedShapeSource {
        namedShape ? namedShape->owner : owner,
        shape,
        namedShape ? &*namedShape : nullptr
    };
}

std::optional<std::string> linkedObjectName(const app::DocumentObject& object, const std::string& property)
{
    if (app::propertyValue(object, property) == nullptr) {
        return std::nullopt;
    }
    const auto link = app::readLink(object, property);
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<TopoDS_Shape> solidShapeForObject(
    const std::string& objectName,
    runtime::ComputeContext& context,
    const app::DocumentObject& object,
    const std::string& property
)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + objectName + " did not produce a solid",
            object.name,
            property,
            "runtime",
            objectName
        );
        return std::nullopt;
    }
    return shapeIt->second.shape;
}

std::optional<TopoDS_Shape> baseTopoShapeForFeature(
    const std::string& featureName,
    runtime::ComputeContext& context,
    const app::DocumentObject& object,
    bool diagnoseMissingTarget
)
{
    const auto documentIt = context.documentObjects.find(featureName);
    if (documentIt == context.documentObjects.end()) {
        return std::nullopt;
    }

    const auto baseFeatureName = linkedObjectName(*documentIt->second, "BaseFeature");
    if (!baseFeatureName) {
        return std::nullopt;
    }

    if (!diagnoseMissingTarget) {
        const auto shapeIt = context.shapes.find(*baseFeatureName);
        if (shapeIt == context.shapes.end()
            || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            return std::nullopt;
        }
        return shapeIt->second.shape;
    }
    return solidShapeForObject(*baseFeatureName, context, object, "BaseFeature");
}

std::optional<TopoDS_Shape> priorBodyOrBaseTopoShapeForFeature(
    const std::string& featureName,
    runtime::ComputeContext& context,
    const app::DocumentObject& object,
    bool diagnoseMissingTarget,
    bool includeDisplayOnlyGeometry = true
)
{
    const auto baseFeatureShape =
        baseTopoShapeForFeature(featureName, context, object, diagnoseMissingTarget);
    if (baseFeatureShape) {
        return baseFeatureShape;
    }
    const auto priorBodyShape = previousBodyTopoShape(featureName, context, includeDisplayOnlyGeometry);
    if (priorBodyShape) {
        return priorBodyShape->shape;
    }
    return std::nullopt;
}

std::optional<TopoDS_Shape> priorBodySolidOrBaseTopoShapeForFeature(
    const std::string& featureName,
    runtime::ComputeContext& context,
    const app::DocumentObject& object,
    bool diagnoseMissingTarget
)
{
    return priorBodyOrBaseTopoShapeForFeature(
        featureName,
        context,
        object,
        diagnoseMissingTarget,
        false
    );
}

std::optional<std::string> baseLinkObjectName(const app::DocumentObject& object)
{
    const auto link = app::readLink(object, "Base");
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<std::string> resolveSupportTransformFeature(
    const DressUpBase& base,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getAddSubShape(), SupportTransform finds "the previous support feature
    // (which must be of type FeatureAddSub), and skipping any consecutive DressUp in-between".
    std::string currentName = base.link.object;
    std::set<std::string> visited;
    while (visited.insert(currentName).second) {
        const auto documentIt = context.documentObjects.find(currentName);
        if (documentIt == context.documentObjects.end()
            || !isDressUpType(documentIt->second->typeId)) {
            return currentName;
        }

        if (const auto baseFeatureName = linkedObjectName(*documentIt->second, "BaseFeature")) {
            currentName = *baseFeatureName;
            continue;
        }
        if (const auto sourceBaseIt = context.objects.find(currentName);
            sourceBaseIt != context.objects.end() && sourceBaseIt->second.contains("source_base")
            && sourceBaseIt->second.at("source_base").is_string()) {
            currentName = sourceBaseIt->second.at("source_base").get<std::string>();
            continue;
        }
        if (const auto linkedBase = baseLinkObjectName(*documentIt->second)) {
            currentName = *linkedBase;
            continue;
        }
        break;
    }
    return std::nullopt;
}

AddSubKind addSubKindForFeature(const std::string& featureName, runtime::ComputeContext& context)
{
    const auto addSubIt = context.addSubShapes.find(featureName);
    if (addSubIt != context.addSubShapes.end()) {
        if (addSubIt->second.addShape) {
            return AddSubKind::Additive;
        }
        if (addSubIt->second.subShape) {
            return AddSubKind::Subtractive;
        }
    }

    const auto documentIt = context.documentObjects.find(featureName);
    if (documentIt == context.documentObjects.end()) {
        return AddSubKind::Unknown;
    }
    if (documentIt->second->typeId == "PartDesign::Pad"
        || documentIt->second->typeId == "PartDesign::FeatureBase") {
        return AddSubKind::Additive;
    }
    if (documentIt->second->typeId == "PartDesign::Pocket"
        || documentIt->second->typeId == "PartDesign::Hole") {
        return AddSubKind::Subtractive;
    }
    return AddSubKind::Unknown;
}

ShapeSlotBuild cutSlot(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& owner,
    const std::string& leftOwner,
    const TopoDS_Shape& left,
    const std::optional<part::NamedShape>& leftNamedShape,
    const std::string& rightOwner,
    const TopoDS_Shape& right,
    const std::optional<part::NamedShape>& rightNamedShape,
    const std::string& property,
    const std::string& failureSeverity = "error",
    const std::string& failureCode = "execution_failed",
    const std::string& failureMessagePrefix =
        "Dress-up AddSubShape cache could not cut boolean sources: "
)
{
    const auto build = part::makeElementBooleanFromSources(
        owner,
        {namedShapeSource(leftOwner, left, leftNamedShape),
         namedShapeSource(rightOwner, right, rightNamedShape)},
        part::BooleanOperation::Cut
    );
    if (!build.error.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            failureSeverity,
            failureCode,
            failureMessagePrefix + build.error,
            object.name,
            property
        );
        return ShapeSlotBuild {false, std::nullopt, std::nullopt};
    }
    if (!hasSolid(build.shape)) {
        return ShapeSlotBuild {true, std::nullopt, std::nullopt};
    }
    return ShapeSlotBuild {
        true,
        build.shape,
        build.namedShape
            ? std::optional<part::NamedShape> {*build.namedShape}
            : std::optional<part::NamedShape> {part::indexedNamedShapeForObject(owner, build.shape)}
    };
}

std::string stableSubnameDiagnosticCode(part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case part::ElementResolveStatus::Split:
            return "split_stable_subname";
        case part::ElementResolveStatus::Resolved:
        case part::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(
    const std::string& target,
    const std::string& stableSubname,
    part::ElementResolveStatus status
)
{
    if (status == part::ElementResolveStatus::Deleted) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == part::ElementResolveStatus::Split) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return "Base target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

bool validateTargetStableSubnames(
    const app::Link& link,
    const std::optional<part::NamedShape>& namedShape,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    if (!namedShape) {
        return true;
    }

    for (std::size_t index = 0; index < link.subnames.size(); ++index) {
        const std::string stableSubname =
            index < link.stableSubnames.size() ? link.stableSubnames.at(index) : std::string {};
        if (stableSubname.empty() || stableSubname == link.subnames.at(index)) {
            continue;
        }

        const auto resolved =
            part::resolveElementReference(*namedShape, link.subnames.at(index), stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            continue;
        }

        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            stableSubnameDiagnosticCode(resolved.status),
            stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
            object.name,
            "Base",
            "runtime",
            link.object,
            stableSubname
        );
        return false;
    }
    return true;
}

std::optional<DressUpBase> resolveDressUpBase(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.h::DressUp,
    // declares "App::PropertyLinkSub Base" for the feature and its selected subelements.
    if (app::propertyValue(object, "Base") == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            object.typeId + " Base must link to a solid and selected subelements",
            object.name,
            "Base"
        );
        return std::nullopt;
    }

    const auto link = app::readLink(object, "Base");
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            object.typeId + " Base must be an App::PropertyLinkSub",
            object.name,
            "Base"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "Base target " + link->object + " did not produce a solid",
            object.name,
            "Base",
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    std::optional<part::NamedShape> namedShape;
    const auto namedShapeIt = context.namedShapes.find(link->object);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = namedShapeIt->second;
    }

    if (const auto bodyContext = sameBodyEarlierFeatureContext(object, link->object, context)) {
        app::Link targetLocalLink = *link;
        for (std::string& stableSubname : targetLocalLink.stableSubnames) {
            stableSubname = stripObjectPrefix(stableSubname, link->object);
        }
        if (!validateTargetStableSubnames(targetLocalLink, namedShape, object, context)) {
            return std::nullopt;
        }
        if (const auto bodyTopoShape = bodyTopoShapeAtFeature(*bodyContext, context)) {
            if (!bodyTopoShape->shape.IsSame(shapeIt->second.shape)) {
                return DressUpBase {
                    bodyTopoShapeLink(*link),
                    bodyTopoShape->shape,
                    bodyTopoShape->namedShape
                        ? bodyTopoShape->namedShape
                        : std::optional<part::NamedShape> {
                              part::indexedNamedShapeForObject(bodyContext->body->name, bodyTopoShape->shape)
                          }
                };
            }
        }
    }

    return DressUpBase {*link, shapeIt->second.shape, namedShape};
}

std::optional<TopoDS_Shape> resolveDressUpSubshape(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    std::size_t subnameIndex
)
{
    const std::string& requestedSubname = base.link.subnames[subnameIndex];
    const std::string stableSubname = subnameIndex < base.link.stableSubnames.size()
        ? base.link.stableSubnames[subnameIndex]
        : std::string {};

    std::string currentSubname = requestedSubname;
    if (base.namedShape) {
        const auto resolved
            = part::resolveElementReference(*base.namedShape, requestedSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != requestedSubname) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage(base.link.object, stableSubname, resolved.status),
                object.name,
                "Base",
                "runtime",
                base.link.object,
                stableSubname
            );
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Invalid dress-up Base subshape " + currentSubname,
            object.name,
            "Base",
            "runtime",
            base.link.object,
            currentSubname
        );
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_FACE && parsed->kind != TopAbs_WIRE) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            "Dress-up Base requires EdgeN or FaceN, not " + part::subshapeKindName(parsed->kind),
            object.name,
            "Base",
            "runtime",
            base.link.object,
            currentSubname
        );
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (base.namedShape) {
        subshape = part::subshapeByName(*base.namedShape, currentSubname);
    }
    else {
        subshape = part::subshapeByName(base.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Base target " + base.link.object + " has no subshape " + currentSubname,
            object.name,
            "Base",
            "runtime",
            base.link.object,
            currentSubname
        );
        return std::nullopt;
    }

    return subshape;
}

bool isC0ContinuousEdge(const TopoDS_Shape& baseShape, const TopoDS_Edge& edge)
{
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    if (!edgeFaces.Contains(edge)) {
        return false;
    }
    const TopTools_ListOfShape& faces = edgeFaces.FindFromKey(edge);
    if (faces.Extent() != 2) {
        return false;
    }

    TopTools_ListIteratorOfListOfShape it(faces);
    const TopoDS_Face face1 = TopoDS::Face(it.Value());
    it.Next();
    const TopoDS_Face face2 = TopoDS::Face(it.Value());
    return BRep_Tool::Continuity(edge, face1, face2) == GeomAbs_C0;
}

std::optional<std::string> edgeSubnameInBase(const TopoDS_Shape& baseShape, const TopoDS_Edge& edge)
{
    int index = 1;
    for (TopExp_Explorer explorer(baseShape, TopAbs_EDGE); explorer.More(); explorer.Next(), ++index) {
        if (TopoDS::Edge(explorer.Current()).IsSame(edge)) {
            return "Edge" + std::to_string(index);
        }
    }
    return std::nullopt;
}

void addSelectedEdge(
    const TopoDS_Shape& baseShape,
    const TopoDS_Edge& edge,
    const std::string& sourceSubname,
    DressUpEdgeSelection& result
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getContinuousEdges(), keeps selected edges only when they have exactly two
    // adjacent faces and "BRep_Tool::Continuity(...) == GeomAbs_C0".
    if (!isC0ContinuousEdge(baseShape, edge)) {
        return;
    }
    for (const auto& existing : result.edges) {
        if (existing.edge.IsSame(edge)) {
            return;
        }
    }

    const std::string edgeSubname = edgeSubnameInBase(baseShape, edge).value_or(sourceSubname);
    result.edges.push_back(EdgeSelection {edge, sourceSubname, edgeSubname});
    result.evidence.selectedEdgeSubnames.push_back(edgeSubname);
    result.evidence.selectedEdgeSources.push_back(sourceSubname);
}

DressUpEdgeSelection allEdges(const TopoDS_Shape& baseShape)
{
    DressUpEdgeSelection result;
    result.evidence.useAllEdges = true;
    int index = 1;
    for (TopExp_Explorer explorer(baseShape, TopAbs_EDGE); explorer.More(); explorer.Next(), ++index) {
        const std::string subname = "Edge" + std::to_string(index);
        result.edges.push_back(EdgeSelection {TopoDS::Edge(explorer.Current()), subname, subname});
        result.evidence.selectedEdgeSubnames.push_back(subname);
        result.evidence.selectedEdgeSources.push_back(subname);
    }
    return result;
}

std::optional<DressUpEdgeSelection> selectedDressUpEdges(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    if (readBoolProperty(object, "UseAllEdges")) {
        return allEdges(base.shape);
    }
    if (base.link.subnames.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Dress-up Base must select at least one EdgeN or FaceN unless UseAllEdges=true",
            object.name,
            "Base",
            "runtime",
            base.link.object
        );
        return std::nullopt;
    }

    DressUpEdgeSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        const std::string& subname = base.link.subnames[index];
        if (subshape->ShapeType() == TopAbs_EDGE) {
            ++selection.evidence.requestedEdgeCount;
            addSelectedEdge(base.shape, TopoDS::Edge(*subshape), subname, selection);
            continue;
        }
        if (subshape->ShapeType() == TopAbs_FACE || subshape->ShapeType() == TopAbs_WIRE) {
            if (subshape->ShapeType() == TopAbs_FACE) {
                ++selection.evidence.requestedFaceCount;
            }
            else {
                ++selection.evidence.requestedWireCount;
            }
            for (TopExp_Explorer explorer(*subshape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
                addSelectedEdge(base.shape, TopoDS::Edge(explorer.Current()), subname, selection);
            }
        }
    }

    if (selection.edges.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Dress-up operation is not possible on selected shapes",
            object.name,
            "Base"
        );
        return std::nullopt;
    }
    return selection;
}

std::optional<DraftFaceSelection> selectedDraftFaces(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
    // calls Base.getSubValuesStartsWith("Face") and copies the previous feature when no FaceN
    // subelement is selected.
    DraftFaceSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const std::string& subname = base.link.subnames[index];
        const auto parsed = part::parseSubshapeName(subname);
        if (!parsed || parsed->kind != TopAbs_FACE) {
            continue;
        }

        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        if (subshape->ShapeType() != TopAbs_FACE) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_subshape_kind",
                "Draft Base requires FaceN, not " + part::subshapeKindName(subshape->ShapeType()),
                object.name,
                "Base",
                "runtime",
                base.link.object,
                subname
            );
            return std::nullopt;
        }

        ++selection.evidence.requestedFaceCount;
        selection.faces.push_back(TopoDS::Face(*subshape));
        selection.selectedFaceSubnames.push_back(subname);
    }
    return selection;
}

std::optional<ThicknessFaceSelection> selectedThicknessFaces(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), reads "Base.getSubValues(true)" and returns the unchanged TopShape
    // when no sub elements are selected; selected entries are then consumed as close faces.
    ThicknessFaceSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const std::string& subname = base.link.subnames[index];
        const auto parsed = part::parseSubshapeName(subname);
        if (!parsed) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_subshape",
                "Invalid Thickness Base subshape " + subname,
                object.name,
                "Base",
                "runtime",
                base.link.object,
                subname
            );
            return std::nullopt;
        }
        if (parsed->kind != TopAbs_FACE) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_subshape_kind",
                "Thickness Base currently requires FaceN, not " + part::subshapeKindName(parsed->kind),
                object.name,
                "Base",
                "runtime",
                base.link.object,
                subname
            );
            return std::nullopt;
        }

        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        if (subshape->ShapeType() != TopAbs_FACE) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_subshape_kind",
                "Thickness Base requires FaceN, not " + part::subshapeKindName(subshape->ShapeType()),
                object.name,
                "Base",
                "runtime",
                base.link.object,
                subname
            );
            return std::nullopt;
        }

        ++selection.evidence.requestedFaceCount;
        selection.faces.push_back(TopoDS::Face(*subshape));
        selection.selectedFaceSubnames.push_back(subname);
    }
    return selection;
}

std::optional<TopoDS_Shape> resolveReferenceSubshape(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    TopAbs_ShapeEnum expectedKind
)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " must reference exactly one " + part::subshapeKindName(expectedKind)
                + " subshape",
            object.name,
            property,
            "runtime",
            link.object
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link.object + " did not produce a shape",
            object.name,
            property,
            "runtime",
            link.object,
            link.subnames.front()
        );
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front()
                                                                       : std::string {};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved
            = part::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                object.name,
                property,
                "runtime",
                link.object,
                stableSubname
            );
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Invalid " + property + " subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    if (parsed->kind != expectedKind) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " requires " + part::subshapeKindName(expectedKind) + " subshape, not "
                + part::subshapeKindName(parsed->kind),
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " target " + link.object + " has no subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    return subshape;
}

bool cacheDressUpAddSubShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    DressUpResult& result
)
{
    runtime::AddSubShape cache;
    const auto resultNamedShape = std::optional<part::NamedShape> {result.namedShape};
    result.addSubCacheStatus = "empty";
    result.addSubCacheWarning.clear();

    if (result.supportTransform) {
        const auto supportFeature = resolveSupportTransformFeature(result.base, context);
        if (!supportFeature) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_link_target",
                "Cannot find additive or subtractive support for " + object.name,
                object.name,
                "SupportTransform"
            );
            return false;
        }
        result.supportTransformSource = *supportFeature;

        const auto supportKind = addSubKindForFeature(*supportFeature, context);
        const auto priorBaseShape =
            priorBodySolidOrBaseTopoShapeForFeature(*supportFeature, context, object, true);
        const auto degradeSupportTransformCache = [&result]() {
            result.addSubCacheStatus = "degraded";
            result.addSubCacheWarning = "support_transform_cache_degraded";
            return true;
        };
        if (supportKind == AddSubKind::Additive) {
            if (priorBaseShape && hasSolid(*priorBaseShape)) {
                const auto priorBaseNamedShape
                    = namedShapeForObject(*supportFeature + ".Base", *priorBaseShape, context);
                const auto add = cutSlot(
                    object,
                    context,
                    object.name,
                    object.name,
                    result.shape,
                    resultNamedShape,
                    *supportFeature + ".Base",
                    *priorBaseShape,
                    priorBaseNamedShape,
                    "SupportTransform",
                    "warning",
                    "support_transform_cache_degraded",
                    "Dress-up SupportTransform AddSubShape cache degraded: "
                );
                if (!add.ok) {
                    return degradeSupportTransformCache();
                }
                cache.addShape = add.shape;
                cache.addNamedShape = add.namedShape;
            }
            else {
                cache.addShape = result.shape;
                cache.addNamedShape = result.namedShape;
            }
        }
        else if (supportKind == AddSubKind::Subtractive) {
            if (priorBaseShape && hasSolid(*priorBaseShape)) {
                const auto priorBaseNamedShape
                    = namedShapeForObject(*supportFeature + ".Base", *priorBaseShape, context);
                const auto sub = cutSlot(
                    object,
                    context,
                    object.name,
                    *supportFeature + ".Base",
                    *priorBaseShape,
                    priorBaseNamedShape,
                    object.name,
                    result.shape,
                    resultNamedShape,
                    "SupportTransform",
                    "warning",
                    "support_transform_cache_degraded",
                    "Dress-up SupportTransform AddSubShape cache degraded: "
                );
                if (!sub.ok) {
                    return degradeSupportTransformCache();
                }
                cache.subShape = sub.shape;
                cache.subNamedShape = sub.namedShape;
            }
            else {
                cache.subShape = result.shape;
                cache.subNamedShape = result.namedShape;
            }
        }
        else {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "SupportTransform support " + *supportFeature
                    + " is not an additive or subtractive FeatureAddSub",
                object.name,
                "SupportTransform",
                "runtime",
                *supportFeature
            );
            return false;
        }

        if (cache.addShape || cache.subShape) {
            context.addSubShapes[object.name] = cache;
            result.addSubCacheStatus = "support_transform";
        }
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getAddSubShape(), without SupportTransform builds two slots:
    // "shape.makeElementCut(baseShape)" and "baseShape.makeElementCut(shape)" so transformed
    // features can fuse added dressing material and cut removed dressing material independently.
    const auto baseFeatureShape = priorBodyOrBaseTopoShapeForFeature(object.name, context, object, false);
    const TopoDS_Shape baseShape = baseFeatureShape && hasSolid(*baseFeatureShape)
        ? *baseFeatureShape
        : result.base.shape;
    const auto baseNamedShape = namedShapeForObject(result.base.link.object, baseShape, context);

    const auto add = cutSlot(
        object,
        context,
        object.name,
        object.name,
        result.shape,
        resultNamedShape,
        result.base.link.object,
        baseShape,
        baseNamedShape,
        "Base"
    );
    if (!add.ok) {
        return false;
    }
    const auto sub = cutSlot(
        object,
        context,
        object.name,
        result.base.link.object,
        baseShape,
        baseNamedShape,
        object.name,
        result.shape,
        resultNamedShape,
        "Base"
    );
    if (!sub.ok) {
        return false;
    }
    cache.addShape = add.shape;
    cache.addNamedShape = add.namedShape;
    cache.subShape = sub.shape;
    cache.subNamedShape = sub.namedShape;
    if (cache.addShape || cache.subShape) {
        context.addSubShapes[object.name] = cache;
        result.addSubCacheStatus = "delta";
    }
    return true;
}

bool applyDressUpRefine(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    DressUpResult& result
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp
    // ::Fillet::execute() and FeatureChamfer.cpp::Chamfer::execute(), both store rawShape and
    // then call "shape = refineShapeIfActive(shape)" before publishing Shape.
    const auto refined = runtime::applyRefineProperty(object, context, result.shape, result.namedShape);
    if (!refined) {
        return false;
    }
    result.shape = refined->shape;
    if (refined->namedShape) {
        result.namedShape = *refined->namedShape;
    }
    result.refineApplied = refined->applied;
    return true;
}

nlohmann::json selectionEvidenceJson(const DressUpSelectionEvidence& evidence)
{
    return {
        {"use_all_edges", evidence.useAllEdges},
        {"requested_subnames", evidence.requestedSubnames},
        {"requested_edge_count", evidence.requestedEdgeCount},
        {"requested_face_count", evidence.requestedFaceCount},
        {"requested_wire_count", evidence.requestedWireCount},
        {"selected_edge_count", static_cast<int>(evidence.selectedEdgeSubnames.size())},
        {"selected_edge_subnames", evidence.selectedEdgeSubnames},
        {"selected_edge_sources", evidence.selectedEdgeSources},
    };
}

void publishDressUpResult(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const DressUpResult& result
)
{
    context.shapes[object.name] = runtime::ShapeValue {runtime::ShapeValue::Kind::Solid, result.shape};
    context.namedShapes[object.name] = result.namedShape;
    context.mesh[object.name] = cad_core::part::meshForShape(result.shape);
    context.subshapes[object.name] = part::subshapeMapForShape(result.shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"body_mode", "replace"},
        {"dress_up", result.mode},
        {"support_transform", result.supportTransform},
        {"support_transform_source", result.supportTransformSource},
        {"add_sub_cache", result.addSubCacheStatus},
        {"source_base", result.sourceBase},
        {"base_selection", selectionEvidenceJson(result.selection)},
        {"bbox", cad_core::part::objectBBoxForShape(result.shape)},
        {"volume", cad_core::part::volumeForShape(result.shape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (result.refineApplied) {
        context.objects[object.name]["refine"] = "applied";
    }
    if (!result.addSubCacheWarning.empty()) {
        context.objects[object.name]["add_sub_cache_warning"] = result.addSubCacheWarning;
    }
    if (!result.parameters.empty()) {
        context.objects[object.name]["parameters"] = result.parameters;
    }
}

}  // namespace detail

}  // namespace cad_core::part_design
