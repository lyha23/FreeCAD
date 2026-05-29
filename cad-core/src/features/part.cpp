#include "cad_core/features/part.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrim_Cylinder.hxx>
#include <BRepPrim_Wedge.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GeomAbs_Shape.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Plane.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Precision.hxx>
#include <STEPControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Vec2d.hxx>

#include <cmath>
#include <filesystem>
#include <string>

namespace cad_core::features
{

namespace
{

struct ImportFile
{
    std::string name;
    std::filesystem::path path;
};

double readNumberProperty(
    const document::DocumentObject& object,
    const std::string& property,
    double fallback
)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool readEnumAsBool(
    const document::DocumentObject& object,
    const std::string& property,
    const std::string& truthyLabel,
    bool fallback
)
{
    const auto stringValue = document::readString(object, property);
    if (stringValue) {
        return *stringValue == truthyLabel;
    }

    const auto* value = document::propertyValue(object, property);
    if (value == nullptr) {
        return fallback;
    }
    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (payload->is_number_integer()) {
        return payload->get<int>() != 0;
    }
    return fallback;
}

bool rejectTooSmall(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& message,
    double value
)
{
    if (value >= Precision::Confusion()) {
        return false;
    }
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "invalid_length",
        message,
        object.name,
        property,
        "runtime"
    );
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

bool rejectNegative(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& message,
    double value
)
{
    if (value >= 0.0) {
        return false;
    }
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "invalid_length",
        message,
        object.name,
        property,
        "runtime"
    );
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

bool rejectTooFewSides(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& message,
    long value
)
{
    if (value >= 3) {
        return false;
    }
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "invalid_length",
        message,
        object.name,
        property,
        "runtime"
    );
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

bool rejectAngleOutOfRange(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& message,
    double value,
    double min,
    double max
)
{
    if (value >= min && value <= max) {
        return false;
    }
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "invalid_angle",
        message,
        object.name,
        property,
        "runtime"
    );
    context.objects[object.name] = {{"status", "error"}};
    return true;
}

double radians(double degrees)
{
    return degrees * std::acos(-1.0) / 180.0;
}

BRepBuilderAPI_MakePolygon makeRegularPolygonWire(long nodes, double circumradius)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Prism::execute() and ::RegularPolygon::execute(), rotate a
    // "Base::Vector3d v(Circumradius.getValue(), 0, 0)" by "360.0 / nodes",
    // add every point to BRepBuilderAPI_MakePolygon, then add the closing point.
    BRepBuilderAPI_MakePolygon polygon;
    for (long i = 0; i <= nodes; ++i) {
        const double angle = 2.0 * std::acos(-1.0) * static_cast<double>(i)
            / static_cast<double>(nodes);
        polygon.Add(gp_Pnt(circumradius * std::cos(angle), circumradius * std::sin(angle), 0.0));
    }
    return polygon;
}

TopoDS_Shape makeTorusShape(double radius1, double radius2, double angle1, double angle2, double angle3)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::makeTorus(), builds a profile circle at "(radius1, 0, 0)" and
    // returns "BRepPrimAPI_MakeRevol(..., Base::toRadians<double>(angle3), Standard_True)".
    gp_Circ circle;
    circle.SetRadius(radius2);
    const gp_Pnt center(radius1, 0.0, 0.0);
    circle.SetAxis(gp_Ax1(center, gp_Dir(0.0, 1.0, 0.0)));

    BRepBuilderAPI_MakeEdge edgeBuilder(circle, radians(angle1), radians(angle2));
    BRepBuilderAPI_MakeWire wireBuilder;
    wireBuilder.Add(edgeBuilder.Edge());

    if (angle1 > -180.0 || angle2 < 180.0) {
        BRepBuilderAPI_MakeVertex vertexBuilder(center);
        BRepBuilderAPI_MakeEdge startEdge(vertexBuilder.Vertex(), edgeBuilder.Vertex1());
        BRepBuilderAPI_MakeEdge endEdge(vertexBuilder.Vertex(), edgeBuilder.Vertex2());
        wireBuilder.Add(startEdge.Edge());
        wireBuilder.Add(endEdge.Edge());
    }

    BRepBuilderAPI_MakeFace faceBuilder(wireBuilder.Wire());
    BRepPrimAPI_MakeRevol revolBuilder(
        faceBuilder.Face(),
        gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
        radians(angle3),
        Standard_True
    );
    return revolBuilder.Shape();
}

TopoDS_Shape makeSpiralHelixShape(
    double radiusBottom,
    double radiusTop,
    double height,
    double turns,
    double breakPeriod,
    bool leftHanded
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::makeSpiralHelix(), checks "Break period must be in [0, 1000]" and
    // "Number of turns must be greater than 0"; it builds a Geom_SurfaceOfRevolution from a
    // two-pole Geom_BezierCurve meridian, then creates edge segments from GCE2d_MakeSegment
    // and calls BRepLib::BuildCurves3d(...) on the resulting wire.
    if (breakPeriod < 0.0 || breakPeriod > 1000.0) {
        throw Standard_Failure("Break period must be in [0, 1000]");
    }
    if (breakPeriod == 0.0) {
        breakPeriod = 1000.0;
    }
    if (turns <= 0.0) {
        throw Standard_Failure("Number of turns must be greater than 0");
    }

    const double pi = std::acos(-1.0);
    const double periods = turns / breakPeriod;
    const auto fullPeriods = static_cast<unsigned long>(std::floor(periods));
    const double partPeriod = periods - static_cast<double>(fullPeriods);

    TColgp_Array1OfPnt poles(1, 2);
    poles(1) = gp_Pnt(radiusBottom, 0.0, 0.0);
    poles(2) = gp_Pnt(radiusTop, 0.0, height);
    Handle(Geom_BezierCurve) meridian = new Geom_BezierCurve(poles);

    Handle(Geom_Surface) surface = new Geom_SurfaceOfRevolution(
        meridian,
        gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0))
    );

    gp_Pnt2d begin(0.0, 0.0);
    gp_Vec2d direction(breakPeriod * 2.0 * pi, 1.0 / periods);
    if (leftHanded) {
        direction = gp_Vec2d(-breakPeriod * 2.0 * pi, 1.0 / periods);
    }

    BRepBuilderAPI_MakeWire wireBuilder;
    for (unsigned long i = 0; i < fullPeriods; ++i) {
        const gp_Pnt2d end = begin.Translated(direction);
        Handle(Geom2d_TrimmedCurve) segment = GCE2d_MakeSegment(begin, end);
        TopoDS_Edge edgeOnSurface = BRepBuilderAPI_MakeEdge(segment, surface);
        wireBuilder.Add(edgeOnSurface);
        begin = end;
    }
    if (partPeriod > Precision::Confusion()) {
        direction.Scale(partPeriod);
        const gp_Pnt2d end = begin.Translated(direction);
        Handle(Geom2d_TrimmedCurve) segment = GCE2d_MakeSegment(begin, end);
        TopoDS_Edge edgeOnSurface = BRepBuilderAPI_MakeEdge(segment, surface);
        wireBuilder.Add(edgeOnSurface);
    }

    TopoDS_Wire wire = wireBuilder.Wire();
    BRepLib::BuildCurves3d(
        wire,
        Precision::Confusion() * 1e-6 * (radiusBottom + radiusTop),
        GeomAbs_C1,
        14,
        10000
    );
    return TopoDS_Shape(std::move(wire));
}

double linearLengthForShape(const TopoDS_Shape& shape)
{
    GProp_GProps properties;
    BRepGProp::LinearProperties(shape, properties);
    return properties.Mass();
}

TopoDS_Shape applyGlobalPlacement(
    const document::DocumentObject& object,
    const runtime::ComputeContext& context,
    const TopoDS_Shape& shape
)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt == context.globalPlacements.end()) {
        return shape;
    }
    return geometry::transformShape(shape, placementIt->second);
}

std::string shapeLabelForPartShape(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        default:
            return "occt_shape";
    }
}

runtime::ShapeValue::Kind shapeKindForPartShape(const TopoDS_Shape& shape)
{
    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    return solidExplorer.More() ? runtime::ShapeValue::Kind::Solid
                                : runtime::ShapeValue::Kind::PartPrimitive;
}

void publishPrimitive(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& localShape,
    const nlohmann::json& metadata,
    runtime::ShapeValue::Kind kind = runtime::ShapeValue::Kind::Solid,
    const std::string& label = "occt_solid"
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::execute(),
    // executes Part feature Shape and stores it as PropertyPartShape "Shape"; cad-core exports the
    // request-local OCCT shape plus indexed subelements until primitive maker history is migrated.
    const TopoDS_Shape shape = applyGlobalPlacement(object, context, localShape);
    context.shapes[object.name] = runtime::ShapeValue {kind, shape};
    context.mesh[object.name] = geometry::meshForShape(shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(shape);
    context.namedShapes[object.name] = topo::indexedNamedShapeForObject(object.name, shape);

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = label;
    result["bbox"] = geometry::bboxForShape(shape);
    result["volume"] = geometry::volumeForShape(shape);
    result["kernel"] = geometry::kernelVersion();
    context.objects[object.name] = result;
}

void publishPartShape(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& localShape,
    const nlohmann::json& metadata
)
{
    const TopoDS_Shape shape = applyGlobalPlacement(object, context, localShape);
    context.shapes[object.name] = runtime::ShapeValue {shapeKindForPartShape(shape), shape};
    context.mesh[object.name] = geometry::meshForShape(shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(shape);
    context.namedShapes[object.name] = topo::indexedNamedShapeForObject(object.name, shape);

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabelForPartShape(shape);
    result["bbox"] = geometry::bboxForShape(shape);
    result["volume"] = geometry::volumeForShape(shape);
    result["kernel"] = geometry::kernelVersion();
    context.objects[object.name] = result;
}

std::optional<ImportFile> readImportFile(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName
)
{
    const auto fileName = document::readString(object, "FileName");
    if (!fileName || fileName->empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            featureName + " FileName is not set",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    std::error_code existsError;
    const std::filesystem::path filePath(*fileName);
    if (!std::filesystem::exists(filePath, existsError)
        || !std::filesystem::is_regular_file(filePath, existsError)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Cannot open file " + *fileName,
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    return ImportFile {*fileName, filePath};
}

}  // namespace

void executePart(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // initializes GroupExtension, while GeoFeatureGroupExtension provides placement/group
    // semantics. cad-core keeps App::Part as a container and exposes a single child solid
    // only as the frontend display result for the current CAD Core adapter.
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Group", "Type", "Id", "Uid", "Material", "Meta", "License", "LicenseURL", "Color"}
        )) {
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
        if (shapeIt != context.shapes.end()
            && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
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

void executePartVertex(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Vertex::Vertex() adds X/Y/Z defaults 0.0 and Vertex::execute() creates
    // "BRepBuilderAPI_MakeVertex(point)" before writing Shape.
    if (!rejectUnsupportedProperties(object, context, {"X", "Y", "Z"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double x = readNumberProperty(object, "X", 0.0);
    const double y = readNumberProperty(object, "Y", 0.0);
    const double z = readNumberProperty(object, "Z", 0.0);
    try {
        BRepBuilderAPI_MakeVertex maker(gp_Pnt(x, y, z));
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "vertex"}, {"x", x}, {"y", y}, {"z", z}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_vertex"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartLine(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Line::Line() adds X1/Y1/Z1 defaults 0.0 and X2/Y2/Z2 defaults 0/0/1;
    // Line::execute() creates "BRepBuilderAPI_MakeEdge(point1, point2)".
    if (!rejectUnsupportedProperties(object, context, {"X1", "Y1", "Z1", "X2", "Y2", "Z2"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double x1 = readNumberProperty(object, "X1", 0.0);
    const double y1 = readNumberProperty(object, "Y1", 0.0);
    const double z1 = readNumberProperty(object, "Z1", 0.0);
    const double x2 = readNumberProperty(object, "X2", 0.0);
    const double y2 = readNumberProperty(object, "Y2", 0.0);
    const double z2 = readNumberProperty(object, "Z2", 1.0);
    try {
        BRepBuilderAPI_MakeEdge maker(gp_Pnt(x1, y1, z1), gp_Pnt(x2, y2, z2));
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to create edge",
                object.name,
                {},
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "line"}, {"start", {x1, y1, z1}}, {"end", {x2, y2, z2}}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_edge"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartPlane(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Plane::Plane() adds Length and Width defaults 100.0; Plane::execute() builds a
    // "Geom_Plane" and "BRepBuilderAPI_MakeFace(aPlane, 0.0, L, 0.0, W, Precision::Confusion())".
    if (!rejectUnsupportedProperties(object, context, {"Length", "Width"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double length = readNumberProperty(object, "Length", 100.0);
    const double width = readNumberProperty(object, "Width", 100.0);
    if (rejectTooSmall(object, context, "Length", "Length of plane too small", length)
        || rejectTooSmall(object, context, "Width", "Width of plane too small", width)) {
        return;
    }

    try {
        Handle(Geom_Plane) plane = new Geom_Plane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
        BRepBuilderAPI_MakeFace maker(plane, 0.0, length, 0.0, width, Precision::Confusion());
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to create face",
                object.name,
                {},
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "plane"}, {"length", length}, {"width", width}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_face"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartBox(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartBox.cpp::Box::Box(),
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
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "box"}, {"length", length}, {"width", width}, {"height", height}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartCylinder(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp::Cylinder::Cylinder(),
    // adds "Radius" default 2.0, "Height" default 10.0 and "Angle" default 360.0;
    // Cylinder::execute() builds BRepPrimAPI_MakeCylinder(...), then
    // PrismExtension::makePrism(height, prim.BottomFace()).
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Radius", "Height", "Angle", "FirstAngle", "SecondAngle"}
        )) {
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
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle",
            "Rotation angle of cylinder out of supported range",
            angle,
            Precision::Confusion(),
            360.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "FirstAngle",
            "First prism angle out of supported range",
            firstAngle,
            -89.99999,
            89.99999
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "SecondAngle",
            "Second prism angle out of supported range",
            secondAngle,
            -89.99999,
            89.99999
        )) {
        return;
    }

    try {
        BRepPrimAPI_MakeCylinder cylinder(radius, height, radians(angle));
        BRepPrim_Cylinder primitive = cylinder.Cylinder();
        BRepPrimAPI_MakePrism prism(
            primitive.BottomFace(),
            gp_Vec(height * std::tan(radians(firstAngle)), height * std::tan(radians(secondAngle)), height)
        );
        publishPrimitive(
            object,
            context,
            prism.Shape(),
            {{"primitive", "cylinder"},
             {"radius", radius},
             {"height", height},
             {"angle", angle},
             {"first_angle", firstAngle},
             {"second_angle", secondAngle}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartPrism(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Prism::Prism() adds Polygon default 6, Circumradius default 2.0 and Height default 10.0;
    // Prism::execute() makes a regular polygon face and calls
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrismExtension.cpp
    // ::PrismExtension::makePrism(), whose vector is
    // "gp_Vec(height * tan(FirstAngle), height * tan(SecondAngle), height)".
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Polygon", "Circumradius", "Height", "FirstAngle", "SecondAngle"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const long polygon = static_cast<long>(std::llround(readNumberProperty(object, "Polygon", 6.0)));
    const double circumradius = readNumberProperty(object, "Circumradius", 2.0);
    const double height = readNumberProperty(object, "Height", 10.0);
    const double firstAngle = readNumberProperty(object, "FirstAngle", 0.0);
    const double secondAngle = readNumberProperty(object, "SecondAngle", 0.0);
    if (rejectTooFewSides(
            object,
            context,
            "Polygon",
            "Polygon of prism is invalid, must have 3 or more sides",
            polygon
        )
        || rejectTooSmall(
            object,
            context,
            "Circumradius",
            "Circumradius of the polygon, of the prism, is too small",
            circumradius
        )
        || rejectTooSmall(object, context, "Height", "Height of prism is too small", height)
        || rejectAngleOutOfRange(
            object,
            context,
            "FirstAngle",
            "First prism angle out of supported range",
            firstAngle,
            -89.99999,
            89.99999
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "SecondAngle",
            "Second prism angle out of supported range",
            secondAngle,
            -89.99999,
            89.99999
        )) {
        return;
    }

    try {
        BRepBuilderAPI_MakePolygon polygonBuilder = makeRegularPolygonWire(polygon, circumradius);
        BRepBuilderAPI_MakeFace faceBuilder(polygonBuilder.Wire());
        BRepPrimAPI_MakePrism prism(
            faceBuilder.Face(),
            gp_Vec(height * std::tan(radians(firstAngle)), height * std::tan(radians(secondAngle)), height)
        );
        publishPrimitive(
            object,
            context,
            prism.Shape(),
            {{"primitive", "prism"},
             {"polygon", polygon},
             {"circumradius", circumradius},
             {"height", height},
             {"first_angle", firstAngle},
             {"second_angle", secondAngle}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartRegularPolygon(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::RegularPolygon::RegularPolygon() adds Polygon default 6 and Circumradius default 2.0;
    // RegularPolygon::execute() stores "mkPoly.Shape()" directly as the Part Shape.
    if (!rejectUnsupportedProperties(object, context, {"Polygon", "Circumradius"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const long polygon = static_cast<long>(std::llround(readNumberProperty(object, "Polygon", 6.0)));
    const double circumradius = readNumberProperty(object, "Circumradius", 2.0);
    if (rejectTooFewSides(object, context, "Polygon", "the polygon is invalid, must have 3 or more sides", polygon)
        || rejectTooSmall(
            object,
            context,
            "Circumradius",
            "Circumradius of the polygon is too small",
            circumradius
        )) {
        return;
    }

    try {
        BRepBuilderAPI_MakePolygon polygonBuilder = makeRegularPolygonWire(polygon, circumradius);
        publishPrimitive(
            object,
            context,
            polygonBuilder.Shape(),
            {{"primitive", "regular_polygon"}, {"polygon", polygon}, {"circumradius", circumradius}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_wire"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartSphere(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Sphere::Sphere() adds Radius default 5.0, Angle1 -90.0, Angle2 90.0 and Angle3 360.0;
    // Sphere::execute() builds "BRepPrimAPI_MakeSphere(Radius, Angle1, Angle2, Angle3)".
    if (!rejectUnsupportedProperties(object, context, {"Radius", "Angle1", "Angle2", "Angle3"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double radius = readNumberProperty(object, "Radius", 5.0);
    const double angle1 = readNumberProperty(object, "Angle1", -90.0);
    const double angle2 = readNumberProperty(object, "Angle2", 90.0);
    const double angle3 = readNumberProperty(object, "Angle3", 360.0);
    if (rejectTooSmall(object, context, "Radius", "Radius of sphere too small", radius)
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle1",
            "First angle of sphere out of supported range",
            angle1,
            -90.0,
            90.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle2",
            "Second angle of sphere out of supported range",
            angle2,
            -90.0,
            90.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle3",
            "Rotation angle of sphere out of supported range",
            angle3,
            Precision::Confusion(),
            360.0
        )) {
        return;
    }

    try {
        BRepPrimAPI_MakeSphere maker(radius, radians(angle1), radians(angle2), radians(angle3));
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "sphere"},
             {"radius", radius},
             {"angle1", angle1},
             {"angle2", angle2},
             {"angle3", angle3}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartEllipsoid(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Ellipsoid::Ellipsoid() adds Radius1 default 2.0, Radius2 default 4.0, Radius3 default 0.0,
    // Angle1 -90.0, Angle2 90.0 and Angle3 360.0; Ellipsoid::execute() builds a sphere with
    // Radius2, then applies "BRepBuilderAPI_GTransform" with scaleZ Radius1/Radius2 and
    // scaleY Radius3/Radius2 only when Radius3 is not the compatibility default 0.0.
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Radius1", "Radius2", "Radius3", "Angle1", "Angle2", "Angle3"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double radius1 = readNumberProperty(object, "Radius1", 2.0);
    const double radius2 = readNumberProperty(object, "Radius2", 4.0);
    const double radius3 = readNumberProperty(object, "Radius3", 0.0);
    const double angle1 = readNumberProperty(object, "Angle1", -90.0);
    const double angle2 = readNumberProperty(object, "Angle2", 90.0);
    const double angle3 = readNumberProperty(object, "Angle3", 360.0);
    if (rejectTooSmall(object, context, "Radius1", "Radius of ellipsoid too small", radius1)
        || rejectTooSmall(object, context, "Radius2", "Radius of ellipsoid too small", radius2)
        || (radius3 < 0.0
            && rejectNegative(object, context, "Radius3", "Radius of ellipsoid too small", radius3))
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle1",
            "First angle of ellipsoid out of supported range",
            angle1,
            -90.0,
            90.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle2",
            "Second angle of ellipsoid out of supported range",
            angle2,
            -90.0,
            90.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle3",
            "Rotation angle of ellipsoid out of supported range",
            angle3,
            Precision::Confusion(),
            360.0
        )) {
        return;
    }

    try {
        BRepPrimAPI_MakeSphere sphere(
            gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
            radius2,
            radians(angle1),
            radians(angle2),
            radians(angle3)
        );
        const double scaleY = radius3 >= Precision::Confusion() ? radius3 / radius2 : 1.0;
        const double scaleZ = radius1 / radius2;
        gp_GTrsf transform;
        transform.SetValue(1, 1, 1.0);
        transform.SetValue(2, 1, 0.0);
        transform.SetValue(3, 1, 0.0);
        transform.SetValue(1, 2, 0.0);
        transform.SetValue(2, 2, scaleY);
        transform.SetValue(3, 2, 0.0);
        transform.SetValue(1, 3, 0.0);
        transform.SetValue(2, 3, 0.0);
        transform.SetValue(3, 3, scaleZ);
        BRepBuilderAPI_GTransform transformed(sphere.Shape(), transform);
        publishPrimitive(
            object,
            context,
            transformed.Shape(),
            {{"primitive", "ellipsoid"},
             {"radius1", radius1},
             {"radius2", radius2},
             {"radius3", radius3},
             {"angle1", angle1},
             {"angle2", angle2},
             {"angle3", angle3}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartCone(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Cone::Cone() adds Radius1 default 2.0, Radius2 4.0, Height 10.0 and Angle 360.0;
    // Cone::execute() uses MakeCylinder when radii match and otherwise "BRepPrimAPI_MakeCone".
    if (!rejectUnsupportedProperties(object, context, {"Radius1", "Radius2", "Height", "Angle"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double radius1 = readNumberProperty(object, "Radius1", 2.0);
    const double radius2 = readNumberProperty(object, "Radius2", 4.0);
    const double height = readNumberProperty(object, "Height", 10.0);
    const double angle = readNumberProperty(object, "Angle", 360.0);
    if (rejectNegative(object, context, "Radius1", "Radius of cone too small", radius1)
        || rejectNegative(object, context, "Radius2", "Radius of cone too small", radius2)
        || rejectTooSmall(object, context, "Height", "Height of cone too small", height)
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle",
            "Rotation angle of cone out of supported range",
            angle,
            Precision::Confusion(),
            360.0
        )) {
        return;
    }

    try {
        TopoDS_Shape shape;
        if (std::abs(radius1 - radius2) < Precision::Confusion()) {
            BRepPrimAPI_MakeCylinder maker(radius1, height, radians(angle));
            shape = maker.Shape();
        }
        else {
            BRepPrimAPI_MakeCone maker(radius1, radius2, height, radians(angle));
            shape = maker.Shape();
        }
        publishPrimitive(
            object,
            context,
            shape,
            {{"primitive", "cone"},
             {"radius1", radius1},
             {"radius2", radius2},
             {"height", height},
             {"angle", angle}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartTorus(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Torus::Torus() adds Radius1 default 10.0, Radius2 2.0, Angle1 -180.0, Angle2 180.0
    // and Angle3 360.0; Torus::execute() stores TopoShape::makeTorus(...) as Shape.
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Radius1", "Radius2", "Angle1", "Angle2", "Angle3"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double radius1 = readNumberProperty(object, "Radius1", 10.0);
    const double radius2 = readNumberProperty(object, "Radius2", 2.0);
    const double angle1 = readNumberProperty(object, "Angle1", -180.0);
    const double angle2 = readNumberProperty(object, "Angle2", 180.0);
    const double angle3 = readNumberProperty(object, "Angle3", 360.0);
    if (rejectTooSmall(object, context, "Radius1", "Radius of torus too small", radius1)
        || rejectTooSmall(object, context, "Radius2", "Radius of torus too small", radius2)
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle1",
            "First angle of torus out of supported range",
            angle1,
            -180.0,
            180.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle2",
            "Second angle of torus out of supported range",
            angle2,
            -180.0,
            180.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle3",
            "Rotation angle of torus out of supported range",
            angle3,
            Precision::Confusion(),
            360.0
        )) {
        return;
    }

    try {
        publishPrimitive(
            object,
            context,
            makeTorusShape(radius1, radius2, angle1, angle2, angle3),
            {{"primitive", "torus"},
             {"radius1", radius1},
             {"radius2", radius2},
             {"angle1", angle1},
             {"angle2", angle2},
             {"angle3", angle3}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartWedge(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Wedge::Wedge() adds Xmin/Ymin/Zmin/X2min/Z2min/Xmax/Ymax/Zmax/X2max/Z2max; Wedge::execute()
    // checks positive outer deltas, non-negative inner deltas, creates BRepPrim_Wedge and adds
    // "mkWedge.Shell()" to BRepBuilderAPI_MakeSolid before writing Shape.
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Xmin", "Ymin", "Zmin", "X2min", "Z2min", "Xmax", "Ymax", "Zmax", "X2max", "Z2max"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double xmin = readNumberProperty(object, "Xmin", 0.0);
    const double ymin = readNumberProperty(object, "Ymin", 0.0);
    const double zmin = readNumberProperty(object, "Zmin", 0.0);
    const double x2min = readNumberProperty(object, "X2min", 2.0);
    const double z2min = readNumberProperty(object, "Z2min", 2.0);
    const double xmax = readNumberProperty(object, "Xmax", 10.0);
    const double ymax = readNumberProperty(object, "Ymax", 10.0);
    const double zmax = readNumberProperty(object, "Zmax", 10.0);
    const double x2max = readNumberProperty(object, "X2max", 8.0);
    const double z2max = readNumberProperty(object, "Z2max", 8.0);
    const double dx = xmax - xmin;
    const double dy = ymax - ymin;
    const double dz = zmax - zmin;
    const double dx2 = x2max - x2min;
    const double dz2 = z2max - z2min;
    if (rejectTooSmall(object, context, "Xmax", "delta x of wedge too small", dx)
        || rejectTooSmall(object, context, "Ymax", "delta y of wedge too small", dy)
        || rejectTooSmall(object, context, "Zmax", "delta z of wedge too small", dz)
        || rejectNegative(object, context, "Z2max", "delta z2 of wedge is negative", dz2)
        || rejectNegative(object, context, "X2max", "delta x2 of wedge is negative", dx2)) {
        return;
    }

    try {
        BRepPrim_Wedge wedge(
            gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)),
            xmin,
            ymin,
            zmin,
            z2min,
            x2min,
            xmax,
            ymax,
            zmax,
            z2max,
            x2max
        );
        BRepBuilderAPI_MakeSolid solid;
        solid.Add(wedge.Shell());
        publishPrimitive(
            object,
            context,
            solid.Solid(),
            {{"primitive", "wedge"},
             {"xmin", xmin},
             {"ymin", ymin},
             {"zmin", zmin},
             {"x2min", x2min},
             {"z2min", z2min},
             {"xmax", xmax},
             {"ymax", ymax},
             {"zmax", zmax},
             {"x2max", x2max},
             {"z2max", z2max}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartEllipse(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Ellipse::Ellipse() adds MajorRadius and MinorRadius defaults 4.0 and Angle1/Angle2 0/360;
    // Ellipse::execute() checks "Minor radius greater than major radius", creates gp_Elips and
    // stores BRepBuilderAPI_MakeEdge(ellipse, Angle1, Angle2) as Shape.
    if (
        !rejectUnsupportedProperties(object, context, {"MajorRadius", "MinorRadius", "Angle1", "Angle2"})
    ) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double majorRadius = readNumberProperty(object, "MajorRadius", 4.0);
    const double minorRadius = readNumberProperty(object, "MinorRadius", 4.0);
    const double angle1 = readNumberProperty(object, "Angle1", 0.0);
    const double angle2 = readNumberProperty(object, "Angle2", 360.0);
    if (minorRadius > majorRadius) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Minor radius greater than major radius",
            object.name,
            "MinorRadius",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (rejectTooSmall(object, context, "MinorRadius", "Minor radius of ellipse too small", minorRadius)
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle1",
            "First angle of ellipse out of supported range",
            angle1,
            0.0,
            360.0
        )
        || rejectAngleOutOfRange(
            object,
            context,
            "Angle2",
            "Second angle of ellipse out of supported range",
            angle2,
            0.0,
            360.0
        )) {
        return;
    }

    try {
        gp_Elips ellipse;
        ellipse.SetMajorRadius(majorRadius);
        ellipse.SetMinorRadius(minorRadius);
        BRepBuilderAPI_MakeEdge maker(ellipse, radians(angle1), radians(angle2));
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to create ellipse edge",
                object.name,
                {},
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        publishPrimitive(
            object,
            context,
            maker.Shape(),
            {{"primitive", "ellipse"},
             {"major_radius", majorRadius},
             {"minor_radius", minorRadius},
             {"angle1", angle1},
             {"angle2", angle2}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_edge"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartHelix(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Helix::Helix() adds Pitch default 1.0, Height 2.0, Radius 1.0, Angle 0.0,
    // SegmentLength 0.0 and LocalCoord "Right-handed"/"Left-handed". Helix::execute()
    // checks "Pitch too small" and "Number of turns too high (> 1e4)", then calls
    // TopoShape().makeSpiralHelix(myRadius, myRadiusTop, myHeight, nbTurns, mySegLen, myLocalCS).
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Pitch", "Height", "Radius", "Angle", "SegmentLength", "LocalCoord", "Style", "Length"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double pitch = readNumberProperty(object, "Pitch", 1.0);
    const double height = readNumberProperty(object, "Height", 2.0);
    const double radius = readNumberProperty(object, "Radius", 1.0);
    const double angle = readNumberProperty(object, "Angle", 0.0);
    const double segmentLength = readNumberProperty(object, "SegmentLength", 0.0);
    const bool leftHanded = readEnumAsBool(object, "LocalCoord", "Left-handed", false);

    if (rejectTooSmall(object, context, "Pitch", "Pitch too small", pitch)) {
        return;
    }
    const double turns = height / pitch;
    if (turns > 1e4) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Number of turns too high (> 1e4)",
            object.name,
            "Height",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const double radiusTop = radius + height * std::tan(radians(angle));
        const TopoDS_Shape shape
            = makeSpiralHelixShape(radius, radiusTop, height, turns, segmentLength, leftHanded);
        publishPrimitive(
            object,
            context,
            shape,
            {{"primitive", "helix"},
             {"pitch", pitch},
             {"height", height},
             {"radius", radius},
             {"radius_top", radiusTop},
             {"angle", angle},
             {"turns", turns},
             {"segment_length", segmentLength},
             {"left_handed", leftHanded},
             {"length", linearLengthForShape(shape)}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_wire"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartSpiral(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
    // ::Spiral::Spiral() adds Growth default 1.0, Radius 1.0, Rotations 2.0 and SegmentLength 1.0;
    // Spiral::execute() computes "myRadiusTop = myRadius + myGrowth * myNumRot", checks
    // "Number of rotations too small", then calls TopoShape().makeSpiralHelix(..., 0, myNumRot, ...).
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Growth", "Radius", "Rotations", "SegmentLength", "Length"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double growth = readNumberProperty(object, "Growth", 1.0);
    const double radius = readNumberProperty(object, "Radius", 1.0);
    const double rotations = readNumberProperty(object, "Rotations", 2.0);
    const double segmentLength = readNumberProperty(object, "SegmentLength", 1.0);
    if (rejectTooSmall(object, context, "Rotations", "Number of rotations too small", rotations)) {
        return;
    }

    try {
        const double radiusTop = radius + growth * rotations;
        const TopoDS_Shape shape
            = makeSpiralHelixShape(radius, radiusTop, 0.0, rotations, segmentLength, false);
        publishPrimitive(
            object,
            context,
            shape,
            {{"primitive", "spiral"},
             {"growth", growth},
             {"radius", radius},
             {"radius_top", radiusTop},
             {"rotations", rotations},
             {"segment_length", segmentLength},
             {"length", linearLengthForShape(shape)}},
            runtime::ShapeValue::Kind::PartPrimitive,
            "occt_wire"
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartImportBrep(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportBrep.cpp
    // ::ImportBrep::execute(), reads PropertyString "FileName", checks "fi.isReadable()",
    // then calls "TopoShape aShape; aShape.importBrep(FileName.getValue())" before writing Shape.
    // cad-core keeps the file path as request input and only returns derived mesh/subshape data.
    if (!rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportBrep");
    if (!importFile) {
        return;
    }

    try {
        TopoDS_Shape shape;
        BRep_Builder builder;
        if (!BRepTools::Read(shape, importFile->path.string().c_str(), builder) || shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to import BREP file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_brep"}, {"file_name", importFile->name}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import BREP file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartImportStep(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportStep.cpp
    // ::ImportStep::execute(), reads PropertyString "FileName" and calls
    // "TopoShape aShape; aShape.importStep(FileName.getValue())". TopoShape::importStep()
    // in /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // uses "STEPControl_Reader", "ReadFile(...)", "TransferRoots()" and "OneShape()".
    if (!rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportStep");
    if (!importFile) {
        return;
    }

    try {
        STEPControl_Reader reader;
        if (reader.ReadFile(importFile->path.string().c_str()) != IFSelect_RetDone) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Error in reading STEP file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        reader.TransferRoots();
        const TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Imported STEP shape is null",
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_step"}, {"file_name", importFile->name}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import STEP file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartImportIges(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportIges.cpp
    // ::ImportIges::execute(), reads PropertyString "FileName" and calls
    // "TopoShape aShape; aShape.importIges(FileName.getValue())". TopoShape::importIges()
    // in /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // calls "IGESControl_Controller::Init()", sets "SetReadVisible(Standard_True)",
    // then uses "ReadFile(...)", "TransferRoots()" and "OneShape()".
    if (!rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportIges");
    if (!importFile) {
        return;
    }

    try {
        IGESControl_Controller::Init();
        IGESControl_Reader reader;
        reader.SetReadVisible(Standard_True);
        if (reader.ReadFile(importFile->path.string().c_str()) != IFSelect_RetDone) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Error in reading IGES file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        reader.ClearShapes();
        reader.TransferRoots();
        const TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Imported IGES shape is null",
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_iges"}, {"file_name", importFile->name}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import IGES file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace cad_core::features
