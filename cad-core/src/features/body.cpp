#include "cad_core/features/body.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <TopoDS_Shape.hxx>
#include <gp_TrsfForm.hxx>

#include <algorithm>
#include <optional>

namespace cad_core::features {

namespace {

std::optional<std::vector<std::string>> readGroupNames(const document::DocumentObject& object)
{
    const std::vector<document::Link> links = document::readLinks(object, "Group");
    if (links.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> names;
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

struct BooleanBuild {
    TopoDS_Shape shape;
    topo::NamedShape namedShape;
};

topo::NamedShape namedShapeForFeatureOrIndexed(const std::string& feature,
                                               const TopoDS_Shape& shape,
                                               const runtime::ComputeContext& context)
{
    const auto namedShapeIt = context.namedShapes.find(feature);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    return topo::indexedNamedShapeForObject(feature, shape);
}

topo::NamedShapeSource sourceForCurrentBody(const std::string& bodyName,
                                            const TopoDS_Shape& shape,
                                            const std::optional<topo::NamedShape>& namedShape)
{
    return topo::NamedShapeSource{namedShape ? namedShape->owner : bodyName, shape, namedShape ? &*namedShape : nullptr};
}

topo::NamedShapeSource sourceForFeature(const std::string& feature,
                                        const TopoDS_Shape& shape,
                                        const runtime::ComputeContext& context)
{
    const auto namedShapeIt = context.namedShapes.find(feature);
    return topo::NamedShapeSource{namedShapeIt != context.namedShapes.end() ? namedShapeIt->second.owner : feature,
                                  shape,
                                  namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr};
}

std::optional<BooleanBuild> fuseShapes(const TopoDS_Shape& base,
                                       const TopoDS_Shape& tool,
                                       const document::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& feature,
                                       const std::optional<topo::NamedShape>& baseNamedShape)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
    // selects BRepAlgoAPI_Fuse and calls makeElementShape(*mk, inputs, ...), where MapperMaker
    // consumes "BRepBuilderAPI_MakeShape::Modified/Generated()" for every boolean input.
    const auto build = topo::makeElementBooleanFromSources(object.name,
                                                           {sourceForCurrentBody(object.name, base, baseNamedShape),
                                                            sourceForFeature(feature, tool, context)},
                                                           topo::BooleanOperation::Fuse);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Body could not fuse additive feature " + feature + ": " + build.error,
                               object.name);
        return std::nullopt;
    }
    return BooleanBuild{build.shape,
                        build.namedShape ? *build.namedShape : topo::indexedNamedShapeForObject(object.name, build.shape)};
}

std::optional<BooleanBuild> cutShapes(const TopoDS_Shape& base,
                                      const TopoDS_Shape& tool,
                                      const document::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      const std::string& feature,
                                      const std::optional<topo::NamedShape>& baseNamedShape)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
    // selects BRepAlgoAPI_Cut and calls makeElementShape(*mk, inputs, ...), where MapperMaker
    // consumes "BRepBuilderAPI_MakeShape::Modified/Generated()" for every boolean input.
    const auto build = topo::makeElementBooleanFromSources(object.name,
                                                           {sourceForCurrentBody(object.name, base, baseNamedShape),
                                                            sourceForFeature(feature, tool, context)},
                                                           topo::BooleanOperation::Cut);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Body could not cut subtractive feature " + feature + ": " + build.error,
                               object.name);
        return std::nullopt;
    }
    return BooleanBuild{build.shape,
                        build.namedShape ? *build.namedShape : topo::indexedNamedShapeForObject(object.name, build.shape)};
}

bool isIdentityPlacement(const gp_Trsf& placement)
{
    return placement.Form() == gp_Identity;
}

}  // namespace

void executeBody(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/Body.cpp Body::execute()
    // src/Mod/PartDesign/App/FeatureAddSub.cpp FeatureAddSub::getAddSubShape()
    if (!rejectUnsupportedProperties(object, context, {"Group", "Tip", "BaseFeature"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Group")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Group must be a list of object links", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Tip")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto tip = document::readLink(object, "Tip");
    if (!tip) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto group = readGroupNames(object);
    if (!group) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Group item must be an object link", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const std::vector<std::string>& groupNames = *group;

    if (std::find(groupNames.begin(), groupNames.end(), tip->object) == groupNames.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Body Tip is not present in Group",
                               object.name,
                               "Tip",
                               "runtime",
                               tip->object);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<TopoDS_Shape> bodyShape;
    std::optional<topo::NamedShape> bodyNamedShape;
    if (object.properties.contains("BaseFeature")) {
        const auto baseLink = document::readLink(object, "BaseFeature");
        if (!baseLink) {
            runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body BaseFeature must link to a solid feature", object.name, "BaseFeature");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const auto baseIt = context.shapes.find(baseLink->object);
        if (baseIt == context.shapes.end() || baseIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "Body BaseFeature target " + baseLink->object + " did not produce a solid",
                                   object.name,
                                   "BaseFeature",
                                   "runtime",
                                   baseLink->object);
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        bodyShape = baseIt->second.shape;
        bodyNamedShape = namedShapeForFeatureOrIndexed(baseLink->object, *bodyShape, context);
    }

    for (const auto& feature : groupNames) {
        const auto shapeIt = context.shapes.find(feature);
        const auto objectIt = context.objects.find(feature);
        const bool replacesBodyShape = shapeIt != context.shapes.end()
            && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid
            && objectIt != context.objects.end() && objectIt->second.value("body_mode", "") == "replace";
        if (replacesBodyShape) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
            // ::Body::execute(), reads only the Tip feature's "Shape". DressUp and Transformed
            // features publish full replacement solids even when they also expose AddSubShape
            // caches for later pattern features.
            bodyShape = shapeIt->second.shape;
            bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
            if (feature == tip->object) {
                break;
            }
            continue;
        }

        const auto addSubIt = context.addSubShapes.find(feature);
        if (addSubIt == context.addSubShapes.end()) {
            if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp,
                // derives from FeatureAddSub but execute() writes a full dressed "Shape"; Body Tip
                // must be able to become that replacement solid instead of reusing the previous Pad/Pocket.
                bodyShape = shapeIt->second.shape;
                bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
                if (feature == tip->object) {
                    break;
                }
            }
            continue;
        }

        const runtime::AddSubShape& addSubShape = addSubIt->second;
        if (addSubShape.addShape) {
            if (!bodyShape) {
                bodyShape = *addSubShape.addShape;
                bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
            }
            else {
                const auto build = fuseShapes(*bodyShape, *addSubShape.addShape, object, context, feature, bodyNamedShape);
                if (build) {
                    bodyShape = build->shape;
                    bodyNamedShape = build->namedShape;
                }
                else {
                    bodyShape = std::nullopt;
                }
            }
        }
        else if (addSubShape.subShape) {
            if (!bodyShape) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Body cannot apply subtractive feature " + feature + " without a base solid",
                                       object.name,
                                       "Group");
                context.objects[object.name] = {{"status", "error"}};
                return;
            }
            const auto build = cutShapes(*bodyShape, *addSubShape.subShape, object, context, feature, bodyNamedShape);
            if (build) {
                bodyShape = build->shape;
                bodyNamedShape = build->namedShape;
            }
            else {
                bodyShape = std::nullopt;
            }
        }

        if (!bodyShape) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        if (feature == tip->object) {
            break;
        }
    }

    if (!bodyShape) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body Tip did not produce a shape", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape resultShape = *bodyShape;
    const auto placementIt = context.globalPlacements.find(object.name);
    const bool hasNonIdentityPlacement = placementIt != context.globalPlacements.end() && !isIdentityPlacement(placementIt->second);
    if (hasNonIdentityPlacement) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
        // ::GeoFeature::getGlobalPlacement(), "return ext->globalGroupPlacement() * placementProperty->getValue()".
        resultShape = geometry::transformShape(resultShape, placementIt->second);
    }

    if (bodyNamedShape && !hasNonIdentityPlacement) {
        bodyNamedShape->owner = object.name;
        bodyNamedShape->shape = resultShape;
        context.namedShapes[object.name] = *bodyNamedShape;
    }
    else {
        context.namedShapes[object.name] = topo::indexedNamedShapeForObject(object.name, resultShape);
    }
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, resultShape};
    context.mesh[object.name] = geometry::meshForShape(resultShape);
    context.subshapes[object.name] = topo::subshapeMapForShape(resultShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"tip", tip->object},
        {"group", groupNames},
        {"shape", "occt_solid"},
        {"bbox", geometry::bboxForShape(resultShape)},
        {"volume", geometry::volumeForShape(resultShape)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
