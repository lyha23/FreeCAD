#include "cad_core/features/part.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepPrim_Cylinder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <string>

namespace cad_core::features {

namespace {

double readNumberProperty(const document::DocumentObject& object, const std::string& property, double fallback)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool rejectTooSmall(const document::DocumentObject& object,
                    runtime::ComputeContext& context,
                    const std::string& property,
                    const std::string& message,
                    double value)
{
    if (value >= Precision::Confusion()) {
        return false;
    }
    runtime::addDiagnostic(context.diagnostics, "error", "invalid_length", message, object.name, property, "runtime");
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

bool rejectAngleOutOfRange(const document::DocumentObject& object,
                           runtime::ComputeContext& context,
                           const std::string& property,
                           const std::string& message,
                           double value,
                           double min,
                           double max)
{
    if (value >= min && value <= max) {
        return false;
    }
    runtime::addDiagnostic(context.diagnostics, "error", "invalid_angle", message, object.name, property, "runtime");
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

double radians(double degrees)
{
    return degrees * std::acos(-1.0) / 180.0;
}

TopoDS_Shape applyGlobalPlacement(const document::DocumentObject& object,
                                  const runtime::ComputeContext& context,
                                  const TopoDS_Shape& shape)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt == context.globalPlacements.end()) {
        return shape;
    }
    return geometry::transformShape(shape, placementIt->second);
}

void publishPrimitive(const document::DocumentObject& object,
                      runtime::ComputeContext& context,
                      const TopoDS_Shape& localShape,
                      const nlohmann::json& metadata)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::execute(),
    // executes Part feature Shape and stores it as PropertyPartShape "Shape"; cad-core exports the
    // request-local OCCT shape plus indexed subelements until primitive maker history is migrated.
    const TopoDS_Shape shape = applyGlobalPlacement(object, context, localShape);
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, shape};
    context.mesh[object.name] = geometry::meshForShape(shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(shape);
    context.namedShapes[object.name] = topo::indexedNamedShapeForObject(object.name, shape);

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = "occt_solid";
    result["bbox"] = geometry::bboxForShape(shape);
    result["volume"] = geometry::volumeForShape(shape);
    result["kernel"] = geometry::kernelVersion();
    context.objects[object.name] = result;
}

}  // namespace

void executePart(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // initializes GroupExtension, while GeoFeatureGroupExtension provides placement/group
    // semantics. cad-core keeps App::Part as a container and exposes a single child solid
    // only as the frontend display result for the current CAD Core adapter.
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Group", "Type", "Id", "Uid", "Material", "Meta", "License", "LicenseURL", "Color"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<document::Link> links = document::readLinks(object, "Group");
    if (links.empty()) {
        context.objects[object.name] = {
            {"status", "ok"},
            {"container", "geo_feature_group"},
            {"group", nlohmann::json::array()},
        };
        return;
    }

    nlohmann::json group = nlohmann::json::array();
    const runtime::ShapeValue* displayShape = nullptr;
    std::string displayObject;
    for (const auto& link : links) {
        group.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
            displayShape = &shapeIt->second;
            displayObject = link.object;
        }
    }

    if (displayShape == nullptr) {
        context.objects[object.name] = {
            {"status", "ok"},
            {"container", "geo_feature_group"},
            {"group", group},
        };
        return;
    }

    context.shapes[object.name] = *displayShape;
    context.mesh[object.name] = geometry::meshForShape(displayShape->shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(displayShape->shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"container", "geo_feature_group"},
        {"display_object", displayObject},
        {"group", group},
        {"shape", "occt_solid"},
        {"bbox", geometry::bboxForShape(displayShape->shape)},
        {"volume", geometry::volumeForShape(displayShape->shape)},
        {"kernel", geometry::kernelVersion()},
    };
}

void executePartBox(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartBox.cpp::Box::Box(),
    // adds "Length", "Width", "Height" with defaults 10.0; Box::execute() checks each dimension
    // against Precision::Confusion() and builds "BRepPrimAPI_MakeBox mkBox(L, W, H)".
    if (!rejectUnsupportedProperties(object, context, {"Length", "Width", "Height"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double length = readNumberProperty(object, "Length", 10.0);
    const double width = readNumberProperty(object, "Width", 10.0);
    const double height = readNumberProperty(object, "Height", 10.0);
    if (rejectTooSmall(object, context, "Length", "Length of box too small", length)
        || rejectTooSmall(object, context, "Width", "Width of box too small", width)
        || rejectTooSmall(object, context, "Height", "Height of box too small", height)) {
        return;
    }

    try {
        BRepPrimAPI_MakeBox maker(length, width, height);
        publishPrimitive(object,
                         context,
                         maker.Shape(),
                         {{"primitive", "box"}, {"length", length}, {"width", width}, {"height", height}});
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics, "error", "execution_failed", failure.GetMessageString(), object.name, {}, "runtime");
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartCylinder(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp::Cylinder::Cylinder(),
    // adds "Radius" default 2.0, "Height" default 10.0 and "Angle" default 360.0; Cylinder::execute()
    // builds BRepPrimAPI_MakeCylinder(...), then PrismExtension::makePrism(height, prim.BottomFace()).
    if (!rejectUnsupportedProperties(object, context, {"Radius", "Height", "Angle", "FirstAngle", "SecondAngle"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double radius = readNumberProperty(object, "Radius", 2.0);
    const double height = readNumberProperty(object, "Height", 10.0);
    const double angle = readNumberProperty(object, "Angle", 360.0);
    const double firstAngle = readNumberProperty(object, "FirstAngle", 0.0);
    const double secondAngle = readNumberProperty(object, "SecondAngle", 0.0);
    if (rejectTooSmall(object, context, "Radius", "Radius of cylinder too small", radius)
        || rejectTooSmall(object, context, "Height", "Height of cylinder too small", height)
        || rejectAngleOutOfRange(object,
                                 context,
                                 "Angle",
                                 "Rotation angle of cylinder out of supported range",
                                 angle,
                                 Precision::Confusion(),
                                 360.0)
        || rejectAngleOutOfRange(
            object, context, "FirstAngle", "First prism angle out of supported range", firstAngle, -89.99999, 89.99999)
        || rejectAngleOutOfRange(object,
                                 context,
                                 "SecondAngle",
                                 "Second prism angle out of supported range",
                                 secondAngle,
                                 -89.99999,
                                 89.99999)) {
        return;
    }

    try {
        BRepPrimAPI_MakeCylinder cylinder(radius, height, radians(angle));
        BRepPrim_Cylinder primitive = cylinder.Cylinder();
        BRepPrimAPI_MakePrism prism(primitive.BottomFace(),
                                    gp_Vec(height * std::tan(radians(firstAngle)),
                                           height * std::tan(radians(secondAngle)),
                                           height));
        publishPrimitive(object,
                         context,
                         prism.Shape(),
                         {{"primitive", "cylinder"},
                          {"radius", radius},
                          {"height", height},
                          {"angle", angle},
                          {"first_angle", firstAngle},
                          {"second_angle", secondAngle}});
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics, "error", "execution_failed", failure.GetMessageString(), object.name, {}, "runtime");
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace cad_core::features
