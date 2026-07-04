#include "cad_core/part_design/feature_shape_binder.h"

#include "../part/part_feature_support.h"

#include "cad_core/base/placement.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part_design
{
namespace
{

using part::part_feature_detail::shapeLabelForPartShape;

struct SelectedShape
{
    std::string objectName;
    std::string subname;
    TopoDS_Shape shape;
    const part::NamedShape* sourceNamedShape = nullptr;
};

struct BinderBuild
{
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    runtime::ShapeValue::Kind kind = runtime::ShapeValue::Kind::PartPrimitive;
    std::optional<TopoDS_Shape> profileShape;
};

const nlohmann::json* rawPayload(const app::DocumentObject& object, const std::string& property)
{
    const app::PropertyValue* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string enumLabel(const app::DocumentObject& object,
                      const std::string& property,
                      const std::vector<std::string>& labels,
                      const std::string& fallback)
{
    const nlohmann::json* payload = rawPayload(object, property);
    if (payload == nullptr) {
        return fallback;
    }
    if (payload->is_string()) {
        return payload->get<std::string>();
    }
    if (payload->is_number_integer()) {
        const auto index = payload->get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < labels.size()) {
            return labels.at(static_cast<std::size_t>(index));
        }
    }
    return fallback;
}

gp_Trsf localPlacement(const app::DocumentObject& object)
{
    const auto placement = app::readPlacement(object, "Placement");
    if (!placement) {
        return gp_Trsf {};
    }
    return base::placementFromComponents(placement->base, placement->rotation);
}

gp_Trsf parentCoordinateSystem(const app::DocumentObject& object, const runtime::ComputeContext& context)
{
    const auto globalIt = context.globalPlacements.find(object.name);
    if (globalIt == context.globalPlacements.end()) {
        return gp_Trsf {};
    }
    return globalIt->second * localPlacement(object).Inverted();
}

gp_Trsf parentCoordinateSystem(const std::string& objectName, const runtime::ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(objectName);
    if (objectIt == context.documentObjects.end() || objectIt->second == nullptr) {
        return gp_Trsf {};
    }
    return parentCoordinateSystem(*objectIt->second, context);
}

TopoDS_Shape transformShapeIfNeeded(const TopoDS_Shape& shape, const gp_Trsf& transform)
{
    if (transform.Form() == gp_Identity) {
        return shape;
    }
    return base::transformShape(shape, transform);
}

bool containsShapeKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    if (shape.IsNull()) {
        return false;
    }
    if (shape.ShapeType() == kind) {
        return true;
    }
    TopExp_Explorer explorer(shape, kind);
    return explorer.More();
}

runtime::ShapeValue::Kind shapeValueKindForBinder(const TopoDS_Shape& shape)
{
    if (containsShapeKind(shape, TopAbs_SOLID)) {
        return runtime::ShapeValue::Kind::Solid;
    }
    if (containsShapeKind(shape, TopAbs_FACE)) {
        return runtime::ShapeValue::Kind::Profile;
    }
    return runtime::ShapeValue::Kind::PartPrimitive;
}

double linearLengthForShape(const TopoDS_Shape& shape)
{
    GProp_GProps properties;
    BRepGProp::LinearProperties(shape, properties);
    return properties.Mass();
}

double surfaceAreaForShape(const TopoDS_Shape& shape)
{
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape, properties);
    return properties.Mass();
}

nlohmann::json selectedLinkSubnamesJson(const std::vector<SelectedShape>& selections)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto& selection : selections) {
        result.push_back({
            {"object", selection.objectName},
            {"subname", selection.subname},
        });
    }
    return result;
}

void publishBinderShape(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const BinderBuild& build,
                        nlohmann::json metadata)
{
    runtime::ShapeValue shapeValue {build.kind, build.shape};
    if (build.profileShape && !build.profileShape->IsNull()) {
        shapeValue.profileShape = build.profileShape;
    }
    context.shapes[object.name] = shapeValue;
    context.mesh[object.name] = part::meshForShape(build.shape);
    context.subshapes[object.name] = part::subshapeMapForShape(build.shape);
    if (build.namedShape) {
        context.namedShapes[object.name] = *build.namedShape;
        context.namedShapes[object.name].owner = object.name;
        context.namedShapes[object.name].shape = build.shape;
    }
    else {
        context.namedShapes[object.name] = part::indexedNamedShapeForObject(object.name, build.shape);
    }

    metadata["status"] = "ok";
    metadata["shape"] = shapeLabelForPartShape(build.shape);
    metadata["bbox"] = part::objectBBoxForShape(build.shape);
    metadata["volume"] = part::volumeForShape(build.shape);
    metadata["area"] = surfaceAreaForShape(build.shape);
    metadata["length"] = linearLengthForShape(build.shape);
    metadata["kernel"] = part::kernelVersion();
    metadata["topo_naming_history"] = "shape_binder_element_map_request_local";
    context.objects[object.name] = std::move(metadata);
}

void setObjectError(runtime::ComputeContext& context,
                    const app::DocumentObject& object,
                    const std::string& code,
                    const std::string& message,
                    const std::string& property = "Support",
                    const std::string& target = {},
                    const std::string& subname = {})
{
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        code,
        message,
        object.name,
        property,
        "runtime",
        target,
        subname
    );
    context.objects[object.name] = {{"status", "error"}};
}

part::NamedShapeSource sourceForSelection(const SelectedShape& selection)
{
    return part::NamedShapeSource {
        selection.objectName,
        selection.shape,
        selection.sourceNamedShape,
    };
}

std::optional<SelectedShape> wholeShapeSelection(const std::string& objectName,
                                                 const runtime::ComputeContext& context)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(objectName);
    return SelectedShape {
        objectName,
        {},
        shapeIt->second.shape,
        namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
    };
}

std::optional<TopoDS_Shape> resolveSubshape(const TopoDS_Shape& shape,
                                            const part::NamedShape* namedShape,
                                            const std::string& subname)
{
    if (namedShape != nullptr) {
        if (const auto resolved = part::subshapeByName(*namedShape, subname)) {
            return resolved;
        }
    }
    part::NamedShape indexed = part::indexedNamedShapeForObject("source", shape);
    return part::subshapeByName(indexed, subname);
}

std::string selectedOutputElementName(const TopoDS_Shape& shape, const std::string& sourceSubname)
{
    const std::string prefix = sourceSubname.substr(0, sourceSubname.find_first_of("0123456789"));
    if (prefix == "Face" && containsShapeKind(shape, TopAbs_FACE)) {
        return "Face1";
    }
    if (prefix == "Edge" && containsShapeKind(shape, TopAbs_EDGE)) {
        return "Edge1";
    }
    if (prefix == "Vertex" && containsShapeKind(shape, TopAbs_VERTEX)) {
        return "Vertex1";
    }
    return sourceSubname;
}

std::optional<std::string> childObjectFromNestedToken(const std::string& parentObject,
                                                      const std::string& token,
                                                      const runtime::ComputeContext& context)
{
    const std::string labelToken = token.rfind('$', 0) == 0 ? token.substr(1) : token;
    for (const auto& [childName, parentName] : context.parentGroupByObject) {
        if (parentName != parentObject) {
            continue;
        }
        if (childName == labelToken) {
            return childName;
        }
        const auto childIt = context.documentObjects.find(childName);
        if (childIt == context.documentObjects.end() || childIt->second == nullptr) {
            continue;
        }
        if (app::readString(*childIt->second, "Label").value_or(childName) == labelToken) {
            return childName;
        }
    }
    return std::nullopt;
}

std::optional<SelectedShape> selectLinkedSubshape(const app::DocumentObject& object,
                                                  runtime::ComputeContext& context,
                                                  const std::string& linkObject,
                                                  const std::string& rawSubname,
                                                  bool allowNestedRoute,
                                                  bool shapeBinderLocalSource)
{
    std::string sourceObject = linkObject;
    std::string subname = rawSubname;
    if (allowNestedRoute && !subname.empty()) {
        const auto dot = subname.find('.');
        if (dot != std::string::npos) {
            const auto nested = childObjectFromNestedToken(linkObject, subname.substr(0, dot), context);
            if (nested) {
                sourceObject = *nested;
                subname = subname.substr(dot + 1U);
            }
        }
    }

    auto selection = wholeShapeSelection(sourceObject, context);
    if (!selection) {
        setObjectError(context,
                       object,
                       "missing_link_target",
                       object.typeId + " Support target " + sourceObject + " did not produce a shape",
                       "Support",
                       sourceObject,
                       rawSubname);
        return std::nullopt;
    }

    if (!subname.empty()) {
        const auto subshape = resolveSubshape(selection->shape, selection->sourceNamedShape, subname);
        if (!subshape || subshape->IsNull()) {
            setObjectError(context,
                           object,
                           "invalid_subshape",
                           object.typeId + " Support target " + sourceObject + " has no subshape " + subname,
                           "Support",
                           sourceObject,
                           rawSubname);
            return std::nullopt;
        }
        selection->shape = *subshape;
        selection->subname = subname;
    }
    else {
        selection->subname.clear();
    }

    if (shapeBinderLocalSource) {
        selection->shape = transformShapeIfNeeded(
            selection->shape,
            parentCoordinateSystem(sourceObject, context).Inverted()
        );
    }
    return selection;
}

bool typeHasPartShape(const std::string& typeId)
{
    return typeId.rfind("Part::", 0) == 0 || typeId.rfind("PartDesign::", 0) == 0
        || typeId == "Sketcher::SketchObject";
}

std::vector<SelectedShape> shapeBinderFilteredPartReferences(const app::DocumentObject& object,
                                                             runtime::ComputeContext& context)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::ShapeBinder::getFilteredReferences(), "Choose first part feature found in support"
    // and either binds the whole shape when "SubList is empty" or only the collected subnames.
    const auto supports = app::readLinks(object, "Support");
    std::optional<std::string> selectedObject;
    bool selectedWhole = false;
    std::vector<std::string> selectedSubnames;
    for (const auto& link : supports) {
        const auto docIt = context.documentObjects.find(link.object);
        if (docIt == context.documentObjects.end() || docIt->second == nullptr
            || !typeHasPartShape(docIt->second->typeId) || context.shapes.count(link.object) == 0U) {
            continue;
        }
        if (!selectedObject) {
            selectedObject = link.object;
        }
        if (*selectedObject != link.object) {
            continue;
        }
        if (link.subnames.empty()) {
            if (selectedSubnames.empty()) {
                selectedWhole = true;
            }
            continue;
        }
        selectedWhole = false;
        selectedSubnames.insert(selectedSubnames.end(), link.subnames.begin(), link.subnames.end());
    }

    std::vector<SelectedShape> selections;
    if (!selectedObject) {
        return selections;
    }
    if (selectedWhole || selectedSubnames.empty()) {
        if (auto selection = selectLinkedSubshape(object, context, *selectedObject, {}, false, true)) {
            selections.push_back(*selection);
        }
        return selections;
    }
    for (const std::string& subname : selectedSubnames) {
        if (auto selection = selectLinkedSubshape(object, context, *selectedObject, subname, false, true)) {
            selections.push_back(*selection);
        }
    }
    return selections;
}

std::optional<SelectedShape> shapeBinderDatumFallback(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::ShapeBinder::buildShapeFromReferences(), for App::Line/App::Plane/App::Point builds
    // "BRepBuilderAPI_MakeEdge(line)", "BRepBuilderAPI_MakeFace(plane)" and
    // "BRepBuilderAPI_MakeVertex(point)" then applies each datum object's Placement.
    for (const auto& link : app::readLinks(object, "Support")) {
        const auto docIt = context.documentObjects.find(link.object);
        if (docIt == context.documentObjects.end() || docIt->second == nullptr) {
            continue;
        }
        const app::DocumentObject& target = *docIt->second;
        TopoDS_Shape shape;
        if (target.typeId == "App::Line") {
            BRepBuilderAPI_MakeEdge builder(gp_Lin(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
            if (!builder.IsDone()) {
                continue;
            }
            shape = builder.Shape();
        }
        else if (target.typeId == "App::Plane") {
            BRepBuilderAPI_MakeFace builder(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
            if (!builder.IsDone()) {
                continue;
            }
            shape = builder.Shape();
        }
        else if (target.typeId == "App::Point") {
            BRepBuilderAPI_MakeVertex builder(gp_Pnt(0, 0, 0));
            if (!builder.IsDone()) {
                continue;
            }
            shape = builder.Shape();
        }
        else {
            continue;
        }
        shape = base::transformShape(shape, localPlacement(target));
        return SelectedShape {target.name, {}, shape, nullptr};
    }
    setObjectError(context,
                   object,
                   "missing_link_target",
                   "PartDesign::ShapeBinder Support did not resolve to a Part shape or App datum",
                   "Support");
    return std::nullopt;
}

BinderBuild buildShapeBinderResult(const app::DocumentObject& object,
                                   runtime::ComputeContext& context,
                                   std::vector<SelectedShape>& selections)
{
    if (selections.size() == 1U) {
        const SelectedShape& selection = selections.front();
        const part::NamedShapeSource source = sourceForSelection(selection);
        part::NamedShape namedShape = selection.subname.empty()
            ? part::namedShapeForLinkedShape(object.name, selection.shape, source)
            : part::namedShapeForLinkedSubshape(object.name,
                                                selection.shape,
                                                source,
                                                selection.subname,
                                                selectedOutputElementName(selection.shape, selection.subname));
        return BinderBuild {
            selection.shape,
            std::move(namedShape),
            shapeValueKindForBinder(selection.shape),
            containsShapeKind(selection.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {selection.shape}
                                                           : std::nullopt,
        };
    }

    std::vector<part::NamedShapeSource> sources;
    sources.reserve(selections.size());
    for (const auto& selection : selections) {
        sources.push_back(sourceForSelection(selection));
    }
    part::NamedShapeBuild compound = part::makeElementCompoundFromSources(object.name, sources, false);
    if (!compound.error.empty() || compound.shape.IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       compound.error.empty() ? "PartDesign::ShapeBinder could not build support compound"
                                              : compound.error);
        return {};
    }
    return BinderBuild {
        compound.shape,
        compound.namedShape,
        shapeValueKindForBinder(compound.shape),
        containsShapeKind(compound.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {compound.shape}
                                                       : std::nullopt,
    };
}

std::vector<SelectedShape> resolveSubShapeBinderSupport(const app::DocumentObject& object,
                                                        runtime::ComputeContext& context)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::update(), iterates "Support.getValues()" and for every subvalue calls
    // "Part::Feature::getTopoShape(obj, ..., Transform, sub.c_str())"; empty subvalue means
    // binding the whole target shape.
    std::vector<SelectedShape> selections;
    const auto supports = app::readLinks(object, "Support");
    if (supports.empty()) {
        setObjectError(context,
                       object,
                       "missing_property",
                       "PartDesign::SubShapeBinder Support must link to at least one object",
                       "Support");
        return {};
    }

    const gp_Trsf targetParentInverse = parentCoordinateSystem(object, context).Inverted();
    for (const auto& link : supports) {
        std::vector<std::string> subnames = link.subnames;
        if (subnames.empty()) {
            subnames.push_back({});
        }
        for (const std::string& subname : subnames) {
            auto selection = selectLinkedSubshape(object, context, link.object, subname, true, false);
            if (!selection) {
                return {};
            }
            if (app::readBool(object, "Relative").value_or(true)) {
                selection->shape = transformShapeIfNeeded(selection->shape, targetParentInverse);
            }
            selections.push_back(*selection);
        }
    }
    return selections;
}

std::vector<TopoDS_Wire> wiresForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Wire> wires;
    for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        wires.push_back(TopoDS::Wire(explorer.Current()));
    }
    return wires;
}

BinderBuild compoundFromSelections(const app::DocumentObject& object,
                                   runtime::ComputeContext& context,
                                   const std::vector<SelectedShape>& selections)
{
    std::vector<part::NamedShapeSource> sources;
    sources.reserve(selections.size());
    for (const auto& selection : selections) {
        sources.push_back(sourceForSelection(selection));
    }
    part::NamedShapeBuild compound = part::makeElementCompoundFromSources(object.name, sources, false);
    if (!compound.error.empty() || compound.shape.IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       compound.error.empty() ? "PartDesign::SubShapeBinder could not build support compound"
                                              : compound.error);
        return {};
    }
    return BinderBuild {
        compound.shape,
        compound.namedShape,
        shapeValueKindForBinder(compound.shape),
        containsShapeKind(compound.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {compound.shape}
                                                       : std::nullopt,
    };
}

std::optional<BinderBuild> applyMakeFaceIfNeeded(const app::DocumentObject& object,
                                                 runtime::ComputeContext& context,
                                                 BinderBuild build,
                                                 const std::vector<SelectedShape>& selections)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::update(), when result "hasSubShape(TopAbs_EDGE)" and no face, calls
    // "makeElementWires(...)" and then "makeElementFace(..., \"Part::FaceMakerBuildFace\")".
    const bool makeFace = app::readBool(object, "MakeFace").value_or(true);
    const double offset = app::readNumber(object, "Offset").value_or(0.0);
    if ((!makeFace && std::abs(offset) <= Precision::Confusion()) || containsShapeKind(build.shape, TopAbs_FACE)
        || !containsShapeKind(build.shape, TopAbs_EDGE)) {
        return build;
    }

    std::vector<part::NamedShapeSource> edgeSources;
    edgeSources.reserve(selections.size());
    for (const auto& selection : selections) {
        edgeSources.push_back(sourceForSelection(selection));
    }
    part::NamedShapeBuild wireBuild = part::makeElementWiresWithPropagatedSources(
        object.name,
        edgeSources,
        "Part::FaceMakerBuildFace"
    );
    if (!wireBuild.error.empty() || wireBuild.shape.IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       wireBuild.error.empty() ? "PartDesign::SubShapeBinder could not build support wire"
                                               : wireBuild.error);
        return std::nullopt;
    }
    build.shape = wireBuild.shape;
    build.namedShape = wireBuild.namedShape;
    build.kind = runtime::ShapeValue::Kind::PartPrimitive;
    build.profileShape = std::nullopt;

    if (!makeFace) {
        return build;
    }

    const std::vector<TopoDS_Wire> wires = wiresForShape(build.shape);
    const auto face = part::makeFaceWithHolesFromClosedWires(wires);
    if (!face || face->IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       "PartDesign::SubShapeBinder could not make a face from support wires");
        return std::nullopt;
    }
    const part::NamedShapeSource wireSource {
        object.name,
        build.shape,
        build.namedShape ? &*build.namedShape : nullptr,
    };
    part::NamedShape namedShape = part::namedShapeForPreservedSources(object.name, *face, {wireSource});
    namedShape.elementHistoryStatus.push_back("element_map_policy_propagate:sub_shape_binder_make_face");
    build.shape = *face;
    build.namedShape = std::move(namedShape);
    build.kind = runtime::ShapeValue::Kind::Profile;
    build.profileShape = build.shape;
    return build;
}

std::optional<BinderBuild> applyOffsetIfNeeded(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               BinderBuild build)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::update(), after optional MakeFace, if "result.hasSubShape(TopAbs_WIRE)"
    // and Offset != 0 then calls "result.makeElementOffset2D(...)".
    const double offset = app::readNumber(object, "Offset").value_or(0.0);
    if (std::abs(offset) <= Precision::Confusion() || !containsShapeKind(build.shape, TopAbs_WIRE)) {
        return build;
    }
    const short join = static_cast<short>(app::readNumber(object, "OffsetJoinType").value_or(0.0));
    const bool fill = app::readBool(object, "OffsetFill").value_or(false);
    const bool openResult = app::readBool(object, "OffsetOpenResult").value_or(true);
    const bool intersection = app::readBool(object, "OffsetIntersection").value_or(false);
    const part::NamedShapeSource source {
        object.name,
        build.shape,
        build.namedShape ? &*build.namedShape : nullptr,
    };
    part::NamedShapeBuild offsetBuild = part::makeElementOffset2DFromSource(
        object.name,
        source,
        offset,
        join,
        fill,
        openResult,
        intersection
    );
    if (!offsetBuild.error.empty() || offsetBuild.shape.IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       offsetBuild.error.empty() ? "PartDesign::SubShapeBinder could not offset support wire"
                                                : offsetBuild.error,
                       "Offset");
        return std::nullopt;
    }
    build.shape = offsetBuild.shape;
    build.namedShape = offsetBuild.namedShape;
    build.kind = shapeValueKindForBinder(build.shape);
    build.profileShape = containsShapeKind(build.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {build.shape}
                                                                     : std::nullopt;
    return build;
}

std::optional<BinderBuild> applyFuseIfNeeded(const app::DocumentObject& object,
                                             runtime::ComputeContext& context,
                                             const std::vector<SelectedShape>& selections,
                                             BinderBuild build)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::update(), when Fuse is true, collects only source solids and fuses them;
    // non-solid sources are left out of the fused result.
    if (!app::readBool(object, "Fuse").value_or(false)) {
        return build;
    }
    std::vector<part::NamedShapeSource> solidSources;
    for (const auto& selection : selections) {
        if (containsShapeKind(selection.shape, TopAbs_SOLID)) {
            solidSources.push_back(sourceForSelection(selection));
        }
    }
    if (solidSources.empty()) {
        return build;
    }
    part::NamedShapeBuild fused = part::makeElementBooleanFromSources(
        object.name,
        solidSources,
        part::BooleanOperation::Fuse
    );
    if (!fused.error.empty() || fused.shape.IsNull()) {
        setObjectError(context,
                       object,
                       "execution_failed",
                       fused.error.empty() ? "PartDesign::SubShapeBinder Fuse failed" : fused.error,
                       "Fuse");
        return std::nullopt;
    }
    build.shape = fused.shape;
    build.namedShape = fused.namedShape;
    build.kind = shapeValueKindForBinder(build.shape);
    build.profileShape = containsShapeKind(build.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {build.shape}
                                                                     : std::nullopt;
    return build;
}

std::optional<BinderBuild> applyRefineIfNeeded(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               BinderBuild build)
{
    const bool refine = app::readBool(object, "Refine").value_or(true);
    if (!refine) {
        return build;
    }
    const part::NamedShapeSource source {
        object.name,
        build.shape,
        build.namedShape ? &*build.namedShape : nullptr,
    };
    part::NamedShapeBuild refined = part::makeElementRefineFromSource(object.name, source);
    if (!refined.error.empty() || refined.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "warning",
                               "refine_failed",
                               refined.error.empty() ? "PartDesign::SubShapeBinder Refine failed; keeping source shape"
                                                     : refined.error,
                               object.name,
                               "Refine");
        return build;
    }
    build.shape = refined.shape;
    build.namedShape = refined.namedShape;
    build.kind = shapeValueKindForBinder(build.shape);
    build.profileShape = containsShapeKind(build.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {build.shape}
                                                                     : std::nullopt;
    return build;
}

void addLifecycleMetadata(const app::DocumentObject& object,
                          runtime::ComputeContext& context,
                          nlohmann::json& metadata,
                          std::size_t supportCount)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::onChanged(), "if(mode == BindModeEnums::Detached) { Support.setValues({}) }";
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::setupCopyOnChange(), "BindCopyOnChange.getValue() == 0 || support.size() != 1"
    // is the gate for the native temporary-document cache. cad-core keeps the supported subset
    // request-local: the support graph in this request is recomputed directly and no backend
    // _tmp_binder, _CopiedObjs, TopoDS, NamedShape or ElementMap cache survives the response.
    const std::string bindMode = enumLabel(object, "BindMode", {"Synchronized", "Frozen", "Detached"}, "Synchronized");
    const std::string copyOnChange = enumLabel(
        object,
        "BindCopyOnChange",
        {"Disabled", "Enabled", "Mutated"},
        "Disabled"
    );
    const bool partialLoad = app::readBool(object, "PartialLoad").value_or(false);
    metadata["bind_mode"] = bindMode;
    metadata["bind_copy_on_change"] = copyOnChange;
    metadata["partial_load"] = partialLoad;
    if (bindMode == "Detached") {
        context.documentObjectUpdates.push_back({
            {"object", object.name},
            {"property", "Support"},
            {"action", "clear"},
            {"reason", "PartDesign::SubShapeBinder BindMode=Detached request-local writeback"},
        });
        metadata["bind_mode_writeback"] = "clear_support";
    }
    if (bindMode == "Frozen") {
        metadata["bind_mode_boundary"] = "request_local_frozen_without_persistent_previous_shape";
    }
    if (copyOnChange == "Enabled" || copyOnChange == "Mutated") {
        metadata["copy_on_change_boundary"] = "request_local_support_recompute_no_persistent_temp_doc";
        metadata["copy_on_change_support_gate"] =
            supportCount == 1U ? "single_support_native_gate_satisfied" : "native_temp_doc_gate_not_entered";
        metadata["copy_on_change_lifecycle"] =
            copyOnChange == "Mutated" ? "mutated_from_request_graph" : "enabled_waiting_for_frontend_mutation";
    }
    if (partialLoad) {
        metadata["partial_load_boundary"] = "request_local_input_no_lazy_backend_session";
    }
}

}  // namespace

void executeShapeBinder(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::ShapeBinder::execute() calls "this->Shape.setValue(updatedShape())"; updatedShape()
    // invokes getFilteredReferences(), buildShapeFromReferences(), then applies TraceSupport.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Support", "TraceSupport", "ClaimChildren"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<SelectedShape> selections = shapeBinderFilteredPartReferences(object, context);
    if (selections.empty()) {
        if (auto datum = shapeBinderDatumFallback(object, context)) {
            selections.push_back(*datum);
        }
        else {
            return;
        }
    }

    BinderBuild build = buildShapeBinderResult(object, context, selections);
    const auto objectIt = context.objects.find(object.name);
    if (build.shape.IsNull()
        || (objectIt != context.objects.end() && objectIt->second.is_object()
            && objectIt->second.value("status", std::string {}) == "error")) {
        return;
    }

    if (app::readBool(object, "TraceSupport").value_or(false) && !selections.empty()) {
        const gp_Trsf sourceCS = parentCoordinateSystem(selections.front().objectName, context);
        const gp_Trsf targetCS = parentCoordinateSystem(object, context);
        const gp_Trsf transform = targetCS.Inverted() * sourceCS;
        build.shape = transformShapeIfNeeded(build.shape, transform);
        const part::NamedShapeSource source {
            object.name,
            build.namedShape ? build.namedShape->shape : build.shape,
            build.namedShape ? &*build.namedShape : nullptr,
        };
        build.namedShape = part::namedShapeForTransformedCopy(object.name, build.shape, source);
        build.kind = shapeValueKindForBinder(build.shape);
        build.profileShape = containsShapeKind(build.shape, TopAbs_FACE) ? std::optional<TopoDS_Shape> {build.shape}
                                                                         : std::nullopt;
    }

    publishBinderShape(
        object,
        context,
        build,
        {
            {"feature", "shape_binder"},
            {"selected_supports", selectedLinkSubnamesJson(selections)},
            {"trace_support", app::readBool(object, "TraceSupport").value_or(false)},
        }
    );
}

void executeSubShapeBinder(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::execute(), for BindMode Synchronized, calls "update(true)" where update()
    // builds a compound, optional Fuse, MakeFace, Offset2D and Refine before assigning Shape.
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"Support",
                                               "ClaimChildren",
                                               "Relative",
                                               "Fuse",
                                               "MakeFace",
                                               "BindMode",
                                               "PartialLoad",
                                               "Context",
                                               "_Version",
                                               "BindCopyOnChange",
                                               "Refine",
                                               "Offset",
                                               "OffsetJoinType",
                                               "OffsetFill",
                                               "OffsetOpenResult",
                                               "OffsetIntersection"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<SelectedShape> selections = resolveSubShapeBinderSupport(object, context);
    if (selections.empty()) {
        return;
    }

    BinderBuild build = compoundFromSelections(object, context, selections);
    if (build.shape.IsNull()) {
        return;
    }
    if (auto fused = applyFuseIfNeeded(object, context, selections, build)) {
        build = *fused;
    }
    else {
        return;
    }
    if (auto faced = applyMakeFaceIfNeeded(object, context, build, selections)) {
        build = *faced;
    }
    else {
        return;
    }
    if (auto offset = applyOffsetIfNeeded(object, context, build)) {
        build = *offset;
    }
    else {
        return;
    }
    if (auto refined = applyRefineIfNeeded(object, context, build)) {
        build = *refined;
    }
    else {
        return;
    }

    nlohmann::json metadata = {
        {"feature", "sub_shape_binder"},
        {"selected_supports", selectedLinkSubnamesJson(selections)},
        {"relative", app::readBool(object, "Relative").value_or(true)},
        {"make_face", app::readBool(object, "MakeFace").value_or(true)},
        {"fuse", app::readBool(object, "Fuse").value_or(false)},
        {"refine", app::readBool(object, "Refine").value_or(true)},
        {"offset", app::readNumber(object, "Offset").value_or(0.0)},
    };
    addLifecycleMetadata(object, context, metadata, selections.size());
    publishBinderShape(object, context, build, std::move(metadata));
}

}  // namespace cad_core::part_design
