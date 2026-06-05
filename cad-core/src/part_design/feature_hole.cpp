#include "cad_core/part_design/feature_hole.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part_design {

namespace {

constexpr int baseProfileOnPoints = 1 << 0;
constexpr int baseProfileOnCircles = 1 << 1;
constexpr int baseProfileOnArcs = 1 << 2;
constexpr int baseProfileOnCirclesArcs = baseProfileOnCircles | baseProfileOnArcs;
constexpr double pi = 3.14159265358979323846;

struct HoleBuild {
    app::Link profile;
    std::string method;
    double diameter = 0.0;
    double threadDiameter = 0.0;
    double threadPitch = 0.0;
    std::string diameterSource;
    std::string threadType;
    std::string threadSize;
    std::string threadFit;
    std::string threadClass;
    std::string threadDirection;
    double threadClearance = 0.0;
    double threadRadiusClearance = 0.0;
    bool useCustomThreadClearance = false;
    double customThreadClearance = 0.0;
    std::string threadDepthType;
    double threadDepth = 0.0;
    double threadRunout = 0.0;
    double depth = 0.0;
    std::string holeCutType;
    std::string holeCutStandard;
    std::string holeCutDefinitionSource;
    std::string drillPoint;
    double holeCutDiameter = 0.0;
    double holeCutDepth = 0.0;
    double holeCutCountersinkAngle = 0.0;
    bool drillForDepth = false;
    bool tapered = false;
    double taperedAngle = 90.0;
    bool threaded = false;
    bool modelThread = false;
    TopoDS_Shape toolShape;
    part::NamedShape toolNamedShape;
    nlohmann::json historyFreeze;
};

struct HoleCenterSource {
    gp_Pnt location;
    std::string sourceSubname;
    std::string sourceKind;
};

struct PreviousSolidSource {
    std::string owner;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

struct HoleToolOptions {
    double diameter = 0.0;
    double depth = 0.0;
    bool cutIntoMaterial = false;
    std::string threadType;
    std::string threadSize;
    std::string holeCutType;
    std::string holeCutStandard;
    std::string holeCutDefinitionSource;
    double holeCutDiameter = 0.0;
    double holeCutDepth = 0.0;
    double holeCutCountersinkAngle = 0.0;
    std::string drillPoint;
    double drillPointAngle = 0.0;
    bool drillForDepth = false;
    bool tapered = false;
    double taperedAngle = 90.0;
};

struct ThreadDescription {
    std::string designation;
    double diameter = 0.0;
    double pitch = 0.0;
    double tapDrill = 0.0;
};

struct ThreadDiameterResult {
    double diameter = 0.0;
    double threadDiameter = 0.0;
    double threadPitch = 0.0;
    std::string source = "Diameter";
    std::string threadSize;
    std::string threadFit;
};

struct ThreadDepthResult {
    std::string type = "Hole Depth";
    double depth = 0.0;
    double runout = 0.0;
};

struct ThreadModelParameters {
    std::string threadClass = "None";
    std::string direction = "Right";
    double clearance = 0.0;
    double radiusClearance = 0.0;
    bool useCustomClearance = false;
    double customClearance = 0.0;
};

std::vector<std::string> threadClassValuesFor(const std::string& threadType);
double threadClassClearanceFor(const std::string& threadClass, double pitch);

struct CounterboreDimension {
    std::string thread;
    double diameter = 0.0;
    double depth = 0.0;
};

struct CountersinkDimension {
    std::string thread;
    double diameter = 0.0;
};

enum class HoleCutDefinitionKind {
    Counterbore,
    Countersink,
};

struct HoleCutDefinition {
    std::string name;
    std::string threadType;
    HoleCutDefinitionKind kind = HoleCutDefinitionKind::Counterbore;
    double angle = 0.0;
    std::vector<CounterboreDimension> boreData;
    std::vector<CountersinkDimension> sinkData;
    std::string source;
};

struct CounterboreLookup {
    CounterboreDimension dimension;
    std::string source;
};

struct CountersinkLookup {
    CountersinkDimension dimension;
    double angle = 90.0;
    std::string source;
};

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::optional<int> readIntegerProperty(const app::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr || !payload->is_number_integer()) {
        return std::nullopt;
    }
    return payload->get<int>();
}

std::string readEnumProperty(const app::DocumentObject& object,
                             const std::string& property,
                             const std::vector<std::string>& values,
                             const std::string& fallback)
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

double readNumberProperty(const app::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return app::readNumber(object, property).value_or(fallback);
}

bool readBoolProperty(const app::DocumentObject& object, const std::string& property, bool fallback = false)
{
    return app::readBool(object, property).value_or(fallback);
}

const std::vector<ThreadDescription>& isoMetricThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], ISO metric profile rows are "{name, thread diameter,
    // thread pitch, Tap-Drill diameter}" and drive Hole::determineDiameter().
    static const std::vector<ThreadDescription> threads = {
        {"M1x0.25", 1.0, 0.25, 0.75},
        {"M1.1x0.25", 1.1, 0.25, 0.85},
        {"M1.2x0.25", 1.2, 0.25, 0.95},
        {"M1.4x0.3", 1.4, 0.30, 1.10},
        {"M1.6x0.35", 1.6, 0.35, 1.25},
        {"M1.8x0.35", 1.8, 0.35, 1.45},
        {"M2x0.4", 2.0, 0.40, 1.60},
        {"M2.2x0.45", 2.2, 0.45, 1.75},
        {"M2.5x0.45", 2.5, 0.45, 2.05},
        {"M3x0.5", 3.0, 0.50, 2.50},
        {"M3.5x0.6", 3.5, 0.60, 2.90},
        {"M4x0.7", 4.0, 0.70, 3.30},
        {"M4.5x0.75", 4.5, 0.75, 3.70},
        {"M5x0.8", 5.0, 0.80, 4.20},
        {"M6x1.0", 6.0, 1.00, 5.00},
        {"M7x1.0", 7.0, 1.00, 6.00},
        {"M8x1.25", 8.0, 1.25, 6.80},
        {"M9x1.25", 9.0, 1.25, 7.80},
        {"M10x1.5", 10.0, 1.50, 8.50},
        {"M11x1.5", 11.0, 1.50, 9.50},
        {"M12x1.75", 12.0, 1.75, 10.20},
        {"M14x2.0", 14.0, 2.00, 12.00},
        {"M16x2.0", 16.0, 2.00, 14.00},
        {"M18x2.5", 18.0, 2.50, 15.50},
        {"M20x2.5", 20.0, 2.50, 17.50},
        {"M22x2.5", 22.0, 2.50, 19.50},
        {"M24x3.0", 24.0, 3.00, 21.00},
        {"M27x3.0", 27.0, 3.00, 24.00},
        {"M30x3.5", 30.0, 3.50, 26.50},
        {"M33x3.5", 33.0, 3.50, 29.50},
        {"M36x4.0", 36.0, 4.00, 32.00},
        {"M39x4.0", 39.0, 4.00, 35.00},
        {"M42x4.5", 42.0, 4.50, 37.50},
        {"M45x4.5", 45.0, 4.50, 40.50},
        {"M48x5.0", 48.0, 5.00, 43.00},
        {"M52x5.0", 52.0, 5.00, 47.00},
        {"M56x5.5", 56.0, 5.50, 50.50},
        {"M60x5.5", 60.0, 5.50, 54.50},
        {"M64x6.0", 64.0, 6.00, 58.00},
        {"M68x6.0", 68.0, 6.00, 62.00},
    };
    return threads;
}

const std::vector<ThreadDescription>& isoMetricFineThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], "ISO metric fine (drill = diameter - pitch)" rows are
    // "{name, thread diameter, thread pitch, Tap-Drill diameter}".
    static const std::vector<ThreadDescription> threads = {
        {"M1x0.2", 1.0, 0.20, 0.80},
        {"M1.1x0.2", 1.1, 0.20, 0.90},
        {"M1.2x0.2", 1.2, 0.20, 1.00},
        {"M1.4x0.2", 1.4, 0.20, 1.20},
        {"M1.6x0.2", 1.6, 0.20, 1.40},
        {"M1.8x0.2", 1.8, 0.20, 1.60},
        {"M2x0.25", 2.0, 0.25, 1.75},
        {"M2.2x0.25", 2.2, 0.25, 1.95},
        {"M2.5x0.35", 2.5, 0.35, 2.15},
        {"M3x0.35", 3.0, 0.35, 2.65},
        {"M3.5x0.35", 3.5, 0.35, 3.15},
        {"M4x0.5", 4.0, 0.50, 3.50},
        {"M4.5x0.5", 4.5, 0.50, 4.00},
        {"M5x0.5", 5.0, 0.50, 4.50},
        {"M5.5x0.5", 5.5, 0.50, 5.00},
        {"M6x0.75", 6.0, 0.75, 5.25},
        {"M7x0.75", 7.0, 0.75, 6.25},
        {"M8x0.75", 8.0, 0.75, 7.25},
        {"M8x1.0", 8.0, 1.00, 7.00},
        {"M9x0.75", 9.0, 0.75, 8.25},
        {"M9x1.0", 9.0, 1.00, 8.00},
        {"M10x0.75", 10.0, 0.75, 9.25},
        {"M10x1.0", 10.0, 1.00, 9.00},
        {"M10x1.25", 10.0, 1.25, 8.75},
        {"M11x0.75", 11.0, 0.75, 10.25},
        {"M11x1.0", 11.0, 1.00, 10.00},
        {"M12x1.0", 12.0, 1.00, 11.00},
        {"M12x1.25", 12.0, 1.25, 10.75},
        {"M12x1.5", 12.0, 1.50, 10.50},
        {"M14x1.0", 14.0, 1.00, 13.00},
        {"M14x1.25", 14.0, 1.25, 12.75},
        {"M14x1.5", 14.0, 1.50, 12.50},
        {"M15x1.0", 15.0, 1.00, 14.00},
        {"M15x1.5", 15.0, 1.50, 13.50},
        {"M16x1.0", 16.0, 1.00, 15.00},
        {"M16x1.5", 16.0, 1.50, 14.50},
        {"M17x1.0", 17.0, 1.00, 16.00},
        {"M17x1.5", 17.0, 1.50, 15.50},
        {"M18x1.0", 18.0, 1.00, 17.00},
        {"M18x1.5", 18.0, 1.50, 16.50},
        {"M18x2.0", 18.0, 2.00, 16.00},
        {"M20x1.0", 20.0, 1.00, 19.00},
        {"M20x1.5", 20.0, 1.50, 18.50},
        {"M20x2.0", 20.0, 2.00, 18.00},
        {"M22x1.0", 22.0, 1.00, 21.00},
        {"M22x1.5", 22.0, 1.50, 20.50},
        {"M22x2.0", 22.0, 2.00, 20.00},
        {"M24x1.0", 24.0, 1.00, 23.00},
        {"M24x1.5", 24.0, 1.50, 22.50},
        {"M24x2.0", 24.0, 2.00, 22.00},
        {"M25x1.0", 25.0, 1.00, 24.00},
        {"M25x1.5", 25.0, 1.50, 23.50},
        {"M25x2.0", 25.0, 2.00, 23.00},
        {"M27x1.0", 27.0, 1.00, 26.00},
        {"M27x1.5", 27.0, 1.50, 25.50},
        {"M27x2.0", 27.0, 2.00, 25.00},
        {"M28x1.0", 28.0, 1.00, 27.00},
        {"M28x1.5", 28.0, 1.50, 26.50},
        {"M28x2.0", 28.0, 2.00, 26.00},
        {"M30x1.0", 30.0, 1.00, 29.00},
        {"M30x1.5", 30.0, 1.50, 28.50},
        {"M30x2.0", 30.0, 2.00, 28.00},
        {"M30x3.0", 30.0, 3.00, 27.00},
        {"M32x1.5", 32.0, 1.50, 30.50},
        {"M32x2.0", 32.0, 2.00, 30.00},
        {"M33x1.5", 33.0, 1.50, 31.50},
        {"M33x2.0", 33.0, 2.00, 31.00},
        {"M33x3.0", 33.0, 3.00, 30.00},
        {"M35x1.5", 35.0, 1.50, 33.50},
        {"M35x2.0", 35.0, 2.00, 33.00},
        {"M36x1.5", 36.0, 1.50, 34.50},
        {"M36x2.0", 36.0, 2.00, 34.00},
        {"M36x3.0", 36.0, 3.00, 33.00},
        {"M39x1.5", 39.0, 1.50, 37.50},
        {"M39x2.0", 39.0, 2.00, 37.00},
        {"M39x3.0", 39.0, 3.00, 36.00},
        {"M40x1.5", 40.0, 1.50, 38.50},
        {"M40x2.0", 40.0, 2.00, 38.00},
        {"M40x3.0", 40.0, 3.00, 37.00},
        {"M42x1.5", 42.0, 1.50, 40.50},
        {"M42x2.0", 42.0, 2.00, 40.00},
        {"M42x3.0", 42.0, 3.00, 39.00},
        {"M42x4.0", 42.0, 4.00, 38.00},
        {"M45x1.5", 45.0, 1.50, 43.50},
        {"M45x2.0", 45.0, 2.00, 43.00},
        {"M45x3.0", 45.0, 3.00, 42.00},
        {"M45x4.0", 45.0, 4.00, 41.00},
        {"M48x1.5", 48.0, 1.50, 46.50},
        {"M48x2.0", 48.0, 2.00, 46.00},
        {"M48x3.0", 48.0, 3.00, 45.00},
        {"M48x4.0", 48.0, 4.00, 44.00},
        {"M50x1.5", 50.0, 1.50, 48.50},
        {"M50x2.0", 50.0, 2.00, 48.00},
        {"M50x3.0", 50.0, 3.00, 47.00},
        {"M52x1.5", 52.0, 1.50, 50.50},
        {"M52x2.0", 52.0, 2.00, 50.00},
        {"M52x3.0", 52.0, 3.00, 49.00},
        {"M52x4.0", 52.0, 4.00, 48.00},
        {"M55x1.5", 55.0, 1.50, 53.50},
        {"M55x2.0", 55.0, 2.00, 53.00},
        {"M55x3.0", 55.0, 3.00, 52.00},
        {"M55x4.0", 55.0, 4.00, 51.00},
        {"M56x1.5", 56.0, 1.50, 54.50},
        {"M56x2.0", 56.0, 2.00, 54.00},
        {"M56x3.0", 56.0, 3.00, 53.00},
        {"M56x4.0", 56.0, 4.00, 52.00},
        {"M58x1.5", 58.0, 1.50, 56.50},
        {"M58x2.0", 58.0, 2.00, 56.00},
        {"M58x3.0", 58.0, 3.00, 55.00},
        {"M58x4.0", 58.0, 4.00, 54.00},
        {"M60x1.5", 60.0, 1.50, 58.50},
        {"M60x2.0", 60.0, 2.00, 58.00},
        {"M60x3.0", 60.0, 3.00, 57.00},
        {"M60x4.0", 60.0, 4.00, 56.00},
        {"M62x1.5", 62.0, 1.50, 60.50},
        {"M62x2.0", 62.0, 2.00, 60.00},
        {"M62x3.0", 62.0, 3.00, 59.00},
        {"M62x4.0", 62.0, 4.00, 58.00},
        {"M64x1.5", 64.0, 1.50, 62.50},
        {"M64x2.0", 64.0, 2.00, 62.00},
        {"M64x3.0", 64.0, 3.00, 61.00},
        {"M64x4.0", 64.0, 4.00, 60.00},
        {"M65x1.5", 65.0, 1.50, 63.50},
        {"M65x2.0", 65.0, 2.00, 63.00},
        {"M65x3.0", 65.0, 3.00, 62.00},
        {"M65x4.0", 65.0, 4.00, 61.00},
        {"M68x1.5", 68.0, 1.50, 66.50},
        {"M68x2.0", 68.0, 2.00, 66.00},
        {"M68x3.0", 68.0, 3.00, 65.00},
        {"M68x4.0", 68.0, 4.00, 64.00},
        {"M70x1.5", 70.0, 1.50, 68.50},
        {"M70x2.0", 70.0, 2.00, 68.00},
        {"M70x3.0", 70.0, 3.00, 67.00},
        {"M70x4.0", 70.0, 4.00, 66.00},
        {"M70x6.0", 70.0, 6.00, 64.00},
        {"M72x1.5", 72.0, 1.50, 70.50},
        {"M72x2.0", 72.0, 2.00, 70.00},
        {"M72x3.0", 72.0, 3.00, 69.00},
        {"M72x4.0", 72.0, 4.00, 68.00},
        {"M72x6.0", 72.0, 6.00, 66.00},
        {"M75x1.5", 75.0, 1.50, 73.50},
        {"M75x2.0", 75.0, 2.00, 73.00},
        {"M75x3.0", 75.0, 3.00, 72.00},
        {"M75x4.0", 75.0, 4.00, 71.00},
        {"M75x6.0", 75.0, 6.00, 69.00},
        {"M76x1.5", 76.0, 1.50, 74.50},
        {"M76x2.0", 76.0, 2.00, 74.00},
        {"M76x3.0", 76.0, 3.00, 73.00},
        {"M76x4.0", 76.0, 4.00, 72.00},
        {"M76x6.0", 76.0, 6.00, 70.00},
        {"M80x1.5", 80.0, 1.50, 78.50},
        {"M80x2.0", 80.0, 2.00, 78.00},
        {"M80x3.0", 80.0, 3.00, 77.00},
        {"M80x4.0", 80.0, 4.00, 76.00},
        {"M80x6.0", 80.0, 6.00, 74.00},
        {"M85x2.0", 85.0, 2.00, 83.00},
        {"M85x3.0", 85.0, 3.00, 82.00},
        {"M85x4.0", 85.0, 4.00, 81.00},
        {"M85x6.0", 85.0, 6.00, 79.00},
        {"M90x2.0", 90.0, 2.00, 88.00},
        {"M90x3.0", 90.0, 3.00, 87.00},
        {"M90x4.0", 90.0, 4.00, 86.00},
        {"M90x6.0", 90.0, 6.00, 84.00},
        {"M95x2.0", 95.0, 2.00, 93.00},
        {"M95x3.0", 95.0, 3.00, 92.00},
        {"M95x4.0", 95.0, 4.00, 91.00},
        {"M95x6.0", 95.0, 6.00, 89.00},
        {"M100x2.0", 100.0, 2.00, 98.00},
        {"M100x3.0", 100.0, 3.00, 97.00},
        {"M100x4.0", 100.0, 4.00, 96.00},
        {"M100x6.0", 100.0, 6.00, 94.00},
    };
    return threads;
}

const std::vector<ThreadDescription>& uncThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], UNC rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() uses TapDrill for Threaded holes.
    static const std::vector<ThreadDescription> threads = {
        {"#1", 1.854, 0.397, 1.50},
        {"#2", 2.184, 0.454, 1.85},
        {"#3", 2.515, 0.529, 2.10},
        {"#4", 2.845, 0.635, 2.35},
        {"#5", 3.175, 0.635, 2.65},
        {"#6", 3.505, 0.794, 2.85},
        {"#8", 4.166, 0.794, 3.50},
        {"#10", 4.826, 1.058, 3.90},
        {"#12", 5.486, 1.058, 4.50},
        {"1/4", 6.350, 1.270, 5.10},
        {"5/16", 7.938, 1.411, 6.60},
        {"3/8", 9.525, 1.588, 8.00},
        {"7/16", 11.113, 1.814, 9.40},
        {"1/2", 12.700, 1.954, 10.80},
        {"9/16", 14.288, 2.117, 12.20},
        {"5/8", 15.875, 2.309, 13.50},
        {"3/4", 19.050, 2.540, 16.50},
        {"7/8", 22.225, 2.822, 19.50},
        {"1", 25.400, 3.175, 22.25},
        {"1 1/8", 28.575, 3.628, 25.00},
        {"1 1/4", 31.750, 3.628, 28.00},
        {"1 3/8", 34.925, 4.233, 30.75},
        {"1 1/2", 38.100, 4.233, 34.00},
        {"1 3/4", 44.450, 5.080, 39.50},
        {"2", 50.800, 5.644, 45.00},
        {"2 1/4", 57.150, 5.644, 51.50},
        {"2 1/2", 63.500, 6.350, 57.00},
        {"2 3/4", 69.850, 6.350, 63.50},
        {"3", 76.200, 6.350, 70.00},
        {"3 1/4", 82.550, 6.350, 76.50},
        {"3 1/2", 88.900, 6.350, 83.00},
        {"3 3/4", 95.250, 6.350, 89.00},
        {"4", 101.600, 6.350, 95.50},
    };
    return threads;
}

const std::vector<ThreadDescription>& unfThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], UNF rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() uses TapDrill for Threaded holes.
    static const std::vector<ThreadDescription> threads = {
        {"#0", 1.524, 0.317, 1.20},
        {"#1", 1.854, 0.353, 1.55},
        {"#2", 2.184, 0.397, 1.85},
        {"#3", 2.515, 0.454, 2.10},
        {"#4", 2.845, 0.529, 2.40},
        {"#5", 3.175, 0.577, 2.70},
        {"#6", 3.505, 0.635, 2.95},
        {"#8", 4.166, 0.706, 3.50},
        {"#10", 4.826, 0.794, 4.10},
        {"#12", 5.486, 0.907, 4.70},
        {"1/4", 6.350, 0.907, 5.50},
        {"5/16", 7.938, 1.058, 6.90},
        {"3/8", 9.525, 1.058, 8.50},
        {"7/16", 11.113, 1.270, 9.90},
        {"1/2", 12.700, 1.270, 11.50},
        {"9/16", 14.288, 1.411, 12.90},
        {"5/8", 15.875, 1.411, 14.50},
        {"3/4", 19.050, 1.588, 17.50},
        {"7/8", 22.225, 1.814, 20.40},
        {"1", 25.400, 2.117, 23.25},
        {"1 1/8", 28.575, 2.117, 26.50},
        {"1 3/16", 30.163, 1.588, 28.58},
        {"1 1/4", 31.750, 2.117, 29.50},
        {"1 3/8", 34.925, 2.117, 32.75},
        {"1 1/2", 38.100, 2.117, 36.00},
    };
    return threads;
}

const std::vector<ThreadDescription>& unefThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], UNEF rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() uses TapDrill for Threaded holes.
    static const std::vector<ThreadDescription> threads = {
        {"#12", 5.486, 0.794, 4.80},
        {"1/4", 6.350, 0.794, 5.70},
        {"5/16", 7.938, 0.794, 7.25},
        {"3/8", 9.525, 0.794, 8.85},
        {"7/16", 11.113, 0.907, 10.35},
        {"1/2", 12.700, 0.907, 11.80},
        {"9/16", 14.288, 1.058, 13.40},
        {"5/8", 15.875, 1.058, 15.00},
        {"11/16", 17.462, 1.058, 16.60},
        {"3/4", 19.050, 1.270, 18.00},
        {"13/16", 20.638, 1.270, 19.60},
        {"7/8", 22.225, 1.270, 21.15},
        {"15/16", 23.812, 1.270, 22.70},
        {"1", 25.400, 1.270, 24.30},
        {"1 1/16", 26.988, 1.411, 25.80},
        {"1 1/8", 28.575, 1.411, 27.35},
        {"1 1/4", 31.750, 1.411, 30.55},
        {"1 5/16", 33.338, 1.411, 32.10},
        {"1 3/8", 34.925, 1.411, 33.70},
        {"1 7/16", 36.512, 1.411, 35.30},
        {"1 1/2", 38.100, 1.411, 36.90},
        {"1 9/16", 39.688, 1.411, 38.55},
        {"1 5/8", 41.275, 1.411, 40.10},
        {"1 11/16", 42.862, 1.411, 41.60},
    };
    return threads;
}

const std::vector<ThreadDescription>& nptThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], NPT rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() calculates NPT depth when TapDrill is 0.
    static const std::vector<ThreadDescription> threads = {
        {"1/16", 7.938, 0.941, 0.0},
        {"1/8", 10.287, 0.941, 0.0},
        {"1/4", 13.716, 1.411, 0.0},
        {"3/8", 17.145, 1.411, 0.0},
        {"1/2", 21.336, 1.814, 0.0},
        {"3/4", 26.670, 1.814, 0.0},
        {"1", 33.401, 2.209, 0.0},
        {"1 1/4", 42.164, 2.209, 0.0},
        {"1 1/2", 48.260, 2.209, 0.0},
        {"2", 60.325, 2.209, 0.0},
        {"2 1/2", 73.025, 3.175, 0.0},
        {"3", 88.900, 3.175, 0.0},
        {"3 1/2", 101.600, 3.175, 0.0},
        {"4", 114.300, 3.175, 0.0},
        {"5", 141.300, 3.175, 0.0},
        {"6", 168.275, 3.175, 0.0},
        {"8", 219.075, 3.175, 0.0},
        {"10", 273.050, 3.175, 0.0},
        {"12", 323.850, 3.175, 0.0},
    };
    return threads;
}

const std::vector<ThreadDescription>& bspThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], BSP rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() applies the BSP/BSW/BSF formula.
    static const std::vector<ThreadDescription> threads = {
        {"1/16", 7.723, 0.907, 6.6},
        {"1/8", 9.728, 0.907, 8.8},
        {"1/4", 13.157, 1.337, 11.8},
        {"3/8", 16.662, 1.337, 15.25},
        {"1/2", 20.955, 1.814, 19.00},
        {"5/8", 22.911, 1.814, 21.00},
        {"3/4", 26.441, 1.814, 24.50},
        {"7/8", 30.201, 1.814, 28.25},
        {"1", 33.249, 2.309, 30.75},
        {"1 1/8", 37.897, 2.309, 0.0},
        {"1 1/4", 41.910, 2.309, 39.50},
        {"1 1/2", 47.803, 2.309, 45.50},
        {"1 3/4", 53.743, 2.309, 51.00},
        {"2", 59.614, 2.309, 57.00},
        {"2 1/4", 65.710, 2.309, 0.0},
        {"2 1/2", 75.184, 2.309, 0.0},
        {"2 3/4", 81.534, 2.309, 0.0},
        {"3", 87.884, 2.309, 0.0},
        {"3 1/2", 100.330, 2.309, 0.0},
        {"4", 113.030, 2.309, 0.0},
        {"4 1/2", 125.730, 2.309, 0.0},
        {"5", 138.430, 2.309, 0.0},
        {"5 1/2", 151.130, 2.309, 0.0},
        {"6", 163.830, 2.309, 0.0},
    };
    return threads;
}

const std::vector<ThreadDescription>& bswThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], BSW rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() applies the BSP/BSW/BSF formula.
    static const std::vector<ThreadDescription> threads = {
        {"1/8", 3.175, 0.635, 2.55},
        {"3/16", 4.762, 1.058, 3.70},
        {"1/4", 6.350, 1.270, 5.10},
        {"5/16", 7.938, 1.411, 6.50},
        {"3/8", 9.525, 1.588, 7.90},
        {"7/16", 11.113, 1.814, 9.30},
        {"1/2", 12.700, 2.117, 10.50},
        {"9/16", 14.290, 2.117, 12.10},
        {"5/8", 15.876, 2.309, 13.50},
        {"11/16", 17.463, 2.309, 15.00},
        {"3/4", 19.051, 2.540, 16.25},
        {"7/8", 22.226, 2.822, 19.25},
        {"1", 25.400, 3.175, 22.00},
        {"1 1/8", 28.576, 3.629, 24.75},
        {"1 1/4", 31.751, 3.629, 28.00},
        {"1 1/2", 38.100, 4.233, 33.50},
        {"1 3/4", 44.452, 5.080, 39.00},
        {"2", 50.802, 5.644, 44.50},
        {"2 1/4", 57.152, 6.350, 0.0},
        {"2 1/2", 63.502, 6.350, 0.0},
        {"2 3/4", 69.853, 7.257, 0.0},
        {"3", 76.203, 7.257, 0.0},
        {"3 1/4", 82.553, 7.815, 0.0},
        {"3 1/2", 88.903, 7.815, 0.0},
        {"3 3/4", 95.254, 8.467, 0.0},
        {"4", 101.604, 8.467, 0.0},
        {"4 1/2", 114.304, 8.835, 0.0},
        {"5", 127.005, 9.236, 0.0},
        {"5 1/2", 139.705, 9.676, 0.0},
        {"6", 152.406, 10.16, 0.0},
    };
    return threads;
}

const std::vector<ThreadDescription>& bsfThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], BSF rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() applies the BSP/BSW/BSF formula.
    static const std::vector<ThreadDescription> threads = {
        {"3/16", 4.763, 0.794, 4.00},
        {"7/32", 5.558, 0.907, 4.60},
        {"1/4", 6.350, 0.977, 5.30},
        {"9/32", 7.142, 0.977, 6.10},
        {"5/16", 7.938, 1.154, 6.80},
        {"3/8", 9.525, 1.270, 8.30},
        {"7/16", 11.113, 1.411, 9.70},
        {"1/2", 12.700, 1.588, 11.10},
        {"9/16", 14.288, 1.588, 12.70},
        {"5/8", 15.875, 1.814, 14.00},
        {"11/16", 17.463, 1.814, 15.50},
        {"3/4", 19.050, 2.116, 16.75},
        {"7/8", 22.225, 2.309, 19.75},
        {"1", 25.400, 2.540, 22.75},
        {"1 1/8", 28.575, 2.822, 25.50},
        {"1 1/4", 31.750, 2.822, 28.50},
        {"1 3/8", 34.925, 3.175, 31.50},
        {"1 1/2", 38.100, 3.175, 34.50},
        {"1 5/8", 41.275, 3.175, 0.0},
        {"1 3/4", 44.450, 3.629, 0.0},
        {"2", 50.800, 3.629, 0.0},
        {"2 1/4", 57.150, 4.233, 0.0},
        {"2 1/2", 63.500, 4.233, 0.0},
        {"2 3/4", 69.850, 4.233, 0.0},
        {"3", 76.200, 5.080, 0.0},
        {"3 1/4", 82.550, 5.080, 0.0},
        {"3 1/2", 88.900, 5.644, 0.0},
        {"3 3/4", 95.250, 5.644, 0.0},
        {"4", 101.600, 5.644, 0.0},
        {"4 1/4", 107.950, 6.350, 0.0},
    };
    return threads;
}

const std::vector<ThreadDescription>& isoTyreThreads()
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::threadDescription[], ISOTyre rows are "{name, thread diameter, thread pitch,
    // Tap-Drill diameter}" and Hole::determineDiameter() falls back to "diameter - pitch".
    static const std::vector<ThreadDescription> threads = {
        {"5v1", 5.334, 0.705, 0.0},
        {"5v2", 5.370, 1.058, 0.0},
        {"6v1", 6.160, 0.800, 0.0},
        {"8v1", 7.798, 0.794, 0.0},
        {"9v1", 9.525, 0.794, 0.0},
        {"10v2", 10.414, 0.907, 0.0},
        {"12v1", 12.319, 0.977, 0.0},
        {"13v1", 12.700, 1.270, 0.0},
        {"8v2", 7.938, 1.058, 0.0},
        {"10v1", 9.800, 1.000, 0.0},
        {"11v1", 11.113, 1.270, 0.0},
        {"13v2", 12.700, 0.794, 0.0},
        {"15v1", 15.137, 1.000, 0.0},
        {"16v1", 15.875, 0.941, 0.0},
        {"17v1", 17.137, 1.000, 0.0},
        {"17v2", 17.463, 1.058, 0.0},
        {"17v3", 17.463, 1.588, 0.0},
        {"19v1", 19.050, 1.588, 0.0},
        {"20v1", 20.642, 1.000, 0.0},
    };
    return threads;
}

bool isIsoMetricThreadType(const std::string& threadType)
{
    return threadType == "ISOMetricProfile" || threadType == "ISOMetricFineProfile";
}

bool isWhitworthThreadType(const std::string& threadType)
{
    return threadType == "BSP" || threadType == "BSW" || threadType == "BSF";
}

bool isUtsThreadType(const std::string& threadType)
{
    return threadType == "UNC" || threadType == "UNF" || threadType == "UNEF";
}

const std::vector<ThreadDescription>* threadTableFor(const std::string& threadType)
{
    if (threadType == "ISOMetricProfile") {
        return &isoMetricThreads();
    }
    if (threadType == "ISOMetricFineProfile") {
        return &isoMetricFineThreads();
    }
    if (threadType == "UNC") {
        return &uncThreads();
    }
    if (threadType == "UNF") {
        return &unfThreads();
    }
    if (threadType == "UNEF") {
        return &unefThreads();
    }
    if (threadType == "NPT") {
        return &nptThreads();
    }
    if (threadType == "BSP") {
        return &bspThreads();
    }
    if (threadType == "BSW") {
        return &bswThreads();
    }
    if (threadType == "BSF") {
        return &bsfThreads();
    }
    if (threadType == "ISOTyre") {
        return &isoTyreThreads();
    }
    return nullptr;
}

std::optional<ThreadDescription> threadDescriptionFor(const app::DocumentObject& object,
                                                      const std::string& threadType,
                                                      runtime::ComputeContext& context)
{
    if (threadType == "None") {
        return std::nullopt;
    }
    const std::vector<ThreadDescription>* threads = threadTableFor(threadType);
    if (threads == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Hole thread table currently supports ISOMetricProfile, ISOMetricFineProfile, UNC, UNF, UNEF, NPT, BSP, BSW, BSF and ISOTyre",
                               object.name,
                               "ThreadType");
        return std::nullopt;
    }

    const nlohmann::json* payload = propertyPayload(object, "ThreadSize");
    if (payload != nullptr && payload->is_string()) {
        const std::string requested = payload->get<std::string>();
        for (const ThreadDescription& thread : *threads) {
            if (thread.designation == requested) {
                return thread;
            }
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported Hole ThreadSize " + requested,
                               object.name,
                               "ThreadSize");
        return std::nullopt;
    }

    const int index = readIntegerProperty(object, "ThreadSize").value_or(0);
    if (index < 0 || static_cast<std::size_t>(index) >= threads->size()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Hole ThreadSize index is outside the " + threadType + " table",
                               object.name,
                               "ThreadSize");
        return std::nullopt;
    }
    return threads->at(static_cast<std::size_t>(index));
}

int readThreadFitIndex(const app::DocumentObject& object)
{
    if (const auto index = readIntegerProperty(object, "ThreadFit")) {
        return *index;
    }
    const auto fit = app::readString(object, "ThreadFit").value_or("Medium");
    if (fit == "Fine" || fit == "Close") {
        return 1;
    }
    if (fit == "Coarse" || fit == "Loose" || fit == "Wide") {
        return 2;
    }
    return 0;
}

std::string threadFitName(const std::string& threadType, int fit)
{
    if (threadType == "ISOMetricProfile" || threadType == "ISOMetricFineProfile" || threadType == "BSP") {
        if (fit == 1) {
            return "Fine";
        }
        if (fit == 2) {
            return "Coarse";
        }
        return "Medium";
    }
    if (threadType == "BSW" || threadType == "BSF") {
        if (fit == 1) {
            return "Close";
        }
        if (fit == 2) {
            return "Wide";
        }
        return "Normal";
    }
    if (fit == 1) {
        return "Close";
    }
    if (fit == 2) {
        return "Loose";
    }
    return "Normal";
}

double isoMetricClearanceDiameter(double threadDiameter, int fit)
{
    struct ClearanceRow {
        double screw = 0.0;
        double fine = 0.0;
        double medium = 0.0;
        double coarse = 0.0;
    };
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::metricHoleDiameters stores ISO 273 "{screw diameter, fine, medium, coarse}".
    static const std::vector<ClearanceRow> rows = {
        {1.0, 1.1, 1.2, 1.3},   {1.2, 1.3, 1.4, 1.5},   {1.4, 1.5, 1.6, 1.8},
        {1.6, 1.7, 1.8, 2.0},   {1.8, 2.0, 2.1, 2.2},   {2.0, 2.2, 2.4, 2.6},
        {2.5, 2.7, 2.9, 3.1},   {3.0, 3.2, 3.4, 3.6},   {3.5, 3.7, 3.9, 4.2},
        {4.0, 4.3, 4.5, 4.8},   {4.5, 4.8, 5.0, 5.3},   {5.0, 5.3, 5.5, 5.8},
        {6.0, 6.4, 6.6, 7.0},   {7.0, 7.4, 7.6, 8.0},   {8.0, 8.4, 9.0, 10.0},
        {10.0, 10.5, 11.0, 12.0},
        {12.0, 13.0, 13.5, 14.5},
        {14.0, 15.0, 15.5, 16.5},
        {16.0, 17.0, 17.5, 18.5},
        {18.0, 19.0, 20.0, 21.0},
        {20.0, 21.0, 22.0, 24.0},
    };
    for (const ClearanceRow& row : rows) {
        if (std::abs(row.screw - threadDiameter) <= Precision::Confusion()) {
            if (fit == 1) {
                return row.fine;
            }
            if (fit == 2) {
                return row.coarse;
            }
            return row.medium;
        }
    }
    if (fit == 1) {
        return threadDiameter * 1.06;
    }
    if (fit == 2) {
        return threadDiameter * 1.16;
    }
    return threadDiameter * 1.10;
}

double utsClearanceDiameter(const std::string& designation, double threadDiameter, int fit)
{
    struct ClearanceRow {
        std::string designation;
        double close = 0.0;
        double normal = 0.0;
        double loose = 0.0;
    };
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::UTSHoleDiameters stores ASME B18.2.8 "{screw class, close, normal, loose}".
    static const std::vector<ClearanceRow> rows = {
        {"#0", 1.7, 1.9, 2.4},       {"#1", 2.1, 2.3, 2.6},
        {"#2", 2.4, 2.6, 2.9},       {"#3", 2.7, 2.9, 3.3},
        {"#4", 3.0, 3.3, 3.7},       {"#5", 3.6, 4.0, 4.4},
        {"#6", 3.9, 4.3, 4.7},       {"#8", 4.6, 5.0, 5.4},
        {"#10", 5.2, 5.6, 6.0},      {"1/4", 6.8, 7.1, 7.5},
        {"5/16", 8.3, 8.7, 9.1},     {"3/8", 9.9, 10.3, 10.7},
        {"7/16", 11.5, 11.9, 12.3},  {"1/2", 13.5, 14.3, 15.5},
        {"5/8", 16.7, 17.5, 18.6},   {"3/4", 19.8, 20.6, 23.0},
        {"7/8", 23.0, 23.8, 26.2},   {"1", 26.2, 27.8, 29.4},
        {"1 1/8", 29.4, 31.0, 33.3}, {"1 3/16", 31.0, 32.5, 34.9},
        {"1 1/4", 32.5, 34.1, 36.5}, {"1 3/8", 36.5, 38.1, 40.9},
        {"1 1/2", 39.7, 41.3, 44.0},
    };
    for (const ClearanceRow& row : rows) {
        if (row.designation == designation) {
            if (fit == 1) {
                return row.close;
            }
            if (fit == 2) {
                return row.loose;
            }
            return row.normal;
        }
    }
    if (fit == 1) {
        return threadDiameter * 1.04;
    }
    if (fit == 2) {
        return threadDiameter * 1.12;
    }
    return threadDiameter * 1.08;
}

double nonMetricClearanceFallback(double threadDiameter, int fit)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::determineDiameter(), for non-ISO/non-UTS clearance holes calculates
    // "diameter * 1.1", "diameter * 1.05" or "diameter * 1.15".
    if (fit == 1) {
        return threadDiameter * 1.05;
    }
    if (fit == 2) {
        return threadDiameter * 1.15;
    }
    return threadDiameter * 1.10;
}

std::optional<ThreadDiameterResult> resolveThreadDiameter(const app::DocumentObject& object,
                                                          const std::string& threadType,
                                                          double requestedDiameter,
                                                          bool threaded,
                                                          bool modelThread,
                                                          runtime::ComputeContext& context)
{
    ThreadDiameterResult result;
    result.diameter = requestedDiameter;
    result.threadFit = threadFitName(threadType, readThreadFitIndex(object));
    if (threadType == "None") {
        return result;
    }

    const auto thread = threadDescriptionFor(object, threadType, context);
    if (!thread) {
        return std::nullopt;
    }
    result.threadDiameter = thread->diameter;
    result.threadPitch = thread->pitch;
    result.threadSize = thread->designation;

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::determineDiameter(), "if (ModelThread.getValue())" reads either
    // "CustomThreadClearance.getValue()" or "getThreadClassClearance()", then threaded
    // holes use "TapDrill + clearance" or the fallback diameter plus the same clearance.
    // for non-threaded thread profiles it reads clearance diameters from the ISO 273 table.
    if (threaded) {
        double clearance = 0.0;
        if (modelThread) {
            if (readBoolProperty(object, "UseCustomThreadClearance")) {
                clearance = readNumberProperty(object, "CustomThreadClearance", 0.0);
            }
            else {
                const std::vector<std::string> threadClasses = threadClassValuesFor(threadType);
                const std::string threadClass = readEnumProperty(
                    object,
                    "ThreadClass",
                    threadClasses,
                    threadClasses.empty() ? "None" : threadClasses.front()
                );
                clearance = threadClassClearanceFor(threadClass, thread->pitch);
            }
        }
        if (thread->tapDrill > Precision::Confusion()) {
            result.diameter = thread->tapDrill + clearance;
            result.source = "thread_tap_drill";
            return result;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
        // ::Hole::determineDiameter(), for "BSP"/"BSW"/"BSF" uses
        // "double thread = 2 * (0.640327 * pitch)" and for "NPT" uses
        // "double thread = 2 * (0.8 * pitch)" before subtracting "thread * 0.75".
        if (isWhitworthThreadType(threadType)) {
            const double threadDepth = 2.0 * (0.640327 * thread->pitch);
            result.diameter = thread->diameter - threadDepth * 0.75 + clearance;
            result.source = "thread_whitworth_fallback";
            return result;
        }
        if (threadType == "NPT") {
            const double threadDepth = 2.0 * (0.8 * thread->pitch);
            result.diameter = thread->diameter - threadDepth * 0.75 + clearance;
            result.source = "thread_npt_fallback";
            return result;
        }
        result.diameter = thread->diameter - thread->pitch + clearance;
        result.source = "thread_pitch_fallback";
        return result;
    }

    const int fit = readThreadFitIndex(object);
    result.threadFit = threadFitName(threadType, fit);
    if (isIsoMetricThreadType(threadType)) {
        result.diameter = isoMetricClearanceDiameter(thread->diameter, fit);
        result.source = "thread_clearance";
        return result;
    }
    if (isUtsThreadType(threadType)) {
        result.diameter = utsClearanceDiameter(thread->designation, thread->diameter, fit);
        result.source = "thread_uts_clearance";
        return result;
    }
    result.diameter = nonMetricClearanceFallback(thread->diameter, fit);
    result.source = "thread_clearance_fallback";
    return result;
}

std::optional<PreviousSolidSource> previousSolidSource(const runtime::ComputeContext& context)
{
    for (auto it = context.executionOrder.rbegin(); it != context.executionOrder.rend(); ++it) {
        const auto shapeIt = context.shapes.find(*it);
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
            const auto namedShapeIt = context.namedShapes.find(*it);
            return PreviousSolidSource{*it,
                                       shapeIt->second.shape,
                                       namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second};
        }
    }
    return std::nullopt;
}

double throughAllLength(const TopoDS_Shape& base, const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getThroughAllLength(), returns "2.02 * sqrt(box.SquareExtent())".
    Bnd_Box box;
    BRepBndLib::Add(base, box);
    if (!profile.IsNull()) {
        BRepBndLib::Add(profile, box);
    }
    box.SetGap(0.0);
    return 2.02 * std::sqrt(box.SquareExtent());
}

std::optional<gp_Dir> planarProfileNormal(const TopoDS_Shape& profileShape)
{
    for (TopExp_Explorer explorer(profileShape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Plane) {
            return surface.Plane().Axis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> circularProfileNormal(const TopoDS_Shape& profileShape)
{
    for (TopExp_Explorer explorer(profileShape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Circle) {
            return curve.Circle().Axis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> guessHoleDirection(const TopoDS_Shape& rawProfile,
                                         const std::optional<TopoDS_Shape>& profileFace,
                                         const std::string& profileObject,
                                         const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::guessNormalDirection(), uses a cylindrical profile axis first, otherwise
    // "getProfileNormal()". FeatureSketchBased.cpp::ProfileBased::getProfileNormal()
    // starts with "Base::Vector3d SketchVector(0, 0, 1)" and rotates it by the
    // Part::Part2DObject Placement for Sketch profiles.
    if (const auto circleNormal = circularProfileNormal(rawProfile)) {
        return circleNormal;
    }
    if (profileFace) {
        return planarProfileNormal(*profileFace);
    }
    gp_Dir direction(0, 0, 1);
    const auto placementIt = context.globalPlacements.find(profileObject);
    if (placementIt != context.globalPlacements.end()) {
        direction.Transform(placementIt->second);
    }
    return direction;
}

int readBaseProfileType(const app::DocumentObject& object)
{
    if (const auto value = readIntegerProperty(object, "BaseProfileType")) {
        return *value;
    }
    const auto text = app::readString(object, "BaseProfileType");
    if (!text) {
        return baseProfileOnCirclesArcs;
    }
    if (*text == "Points") {
        return baseProfileOnPoints;
    }
    if (*text == "Circles") {
        return baseProfileOnCircles;
    }
    if (*text == "Arcs") {
        return baseProfileOnArcs;
    }
    if (*text == "PointsCirclesArcs" || *text == "OnPointsCirclesArcs") {
        return baseProfileOnPoints | baseProfileOnCircles | baseProfileOnArcs;
    }
    return baseProfileOnCirclesArcs;
}

bool profileUsesFlatFaceSupport(const std::string& profileObject, const runtime::ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(profileObject);
    if (objectIt == context.documentObjects.end() || objectIt->second == nullptr) {
        return false;
    }
    const app::DocumentObject& object = *objectIt->second;
    const std::string mapMode = app::readString(object, "MapMode").value_or("FlatFace");
    if (mapMode != "FlatFace") {
        return false;
    }
    auto support = app::readLink(object, "AttachmentSupport");
    if (!support) {
        support = app::readLink(object, "Support");
    }
    return support && !support->subnames.empty() && support->subnames.front().rfind("Face", 0U) == 0U;
}

double defaultCountersinkAngle(const std::string& threadType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::getCountersinkAngle(), returns 100 for "BSW"/"BSF", 82 for "UNC"/"UNF"/"UNEF",
    // otherwise 90.
    if (threadType == "BSW" || threadType == "BSF") {
        return 100.0;
    }
    if (threadType == "UNC" || threadType == "UNF" || threadType == "UNEF") {
        return 82.0;
    }
    return 90.0;
}

double threadRunoutForPitch(double pitch)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::getThreadRunout(), uses DIN 76-1 ThreadRunout "{Pitch, e1}" rows and for
    // non-standard pitch falls back to "4 * pitch".
    static const std::vector<std::pair<double, double>> runouts = {
        {0.2, 1.3},  {0.25, 1.5}, {0.3, 1.8},  {0.35, 2.1}, {0.4, 2.3},  {0.45, 2.6},
        {0.5, 2.8},  {0.6, 3.4},  {0.7, 3.8},  {0.75, 4.0}, {0.8, 4.2},  {1.0, 5.1},
        {1.25, 6.2}, {1.5, 7.3},  {1.75, 8.3}, {2.0, 9.3},  {2.5, 11.2}, {3.0, 13.1},
        {3.5, 15.2}, {4.0, 16.8}, {4.5, 18.4}, {5.0, 20.8}, {5.5, 22.4}, {6.0, 24.0},
    };
    for (const auto& [limit, runout] : runouts) {
        if (pitch <= limit) {
            return runout;
        }
    }
    return 4.0 * pitch;
}

std::vector<std::string> threadClassValuesFor(const std::string& threadType)
{
    if (threadType == "ISOMetricProfile" || threadType == "ISOMetricFineProfile") {
        return {"4G", "4H", "5G", "5H", "6G", "6H", "7G", "7H", "8G", "8H"};
    }
    if (threadType == "UNC" || threadType == "UNF" || threadType == "UNEF") {
        return {"1B", "2B", "3B"};
    }
    if (threadType == "BSW" || threadType == "BSF") {
        return {"Medium", "Normal"};
    }
    return {"None"};
}

double threadClassClearanceFor(const std::string& threadClass, double pitch)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::getThreadClassClearance(), only classes whose second character is 'G'
    // add ISO metric clearance from ThreadClass_ISOmetric_data; other classes return 0.
    if (threadClass.size() < 2 || threadClass[1] != 'G') {
        return 0.0;
    }
    static const std::vector<std::pair<double, double>> clearances = {
        {0.2, 0.017},  {0.25, 0.018}, {0.3, 0.018},  {0.35, 0.019}, {0.4, 0.019},
        {0.45, 0.020}, {0.5, 0.020},  {0.6, 0.021},  {0.7, 0.022},  {0.75, 0.022},
        {0.8, 0.024},  {1.0, 0.026},  {1.25, 0.028}, {1.5, 0.032},  {1.75, 0.034},
        {2.0, 0.038},  {2.5, 0.042},  {3.0, 0.048},  {3.5, 0.053},  {4.0, 0.060},
        {4.5, 0.063},  {5.0, 0.071},  {5.5, 0.075},  {6.0, 0.080},  {8.0, 0.100},
    };
    for (const auto& [limit, clearance] : clearances) {
        if (pitch <= limit) {
            return clearance;
        }
    }
    return 0.0;
}

ThreadModelParameters resolveThreadModelParameters(const app::DocumentObject& object,
                                                   const std::string& threadType,
                                                   double threadPitch,
                                                   bool threaded)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread() reads "bool leftHanded = (bool)ThreadDirection.getValue()";
    // then applies either "CustomThreadClearance / 2" or "getThreadClassClearance() / 2"
    // to the thread major radius before Hole::makeThread() builds the modeled thread tool.
    ThreadModelParameters result;
    const std::vector<std::string> threadClasses = threadClassValuesFor(threadType);
    result.threadClass =
        readEnumProperty(object, "ThreadClass", threadClasses, threadClasses.empty() ? "None" : threadClasses.front());
    result.direction = readEnumProperty(object, "ThreadDirection", {"Right", "Left"}, "Right");
    result.useCustomClearance = readBoolProperty(object, "UseCustomThreadClearance");
    result.customClearance = readNumberProperty(object, "CustomThreadClearance", 0.0);
    if (!threaded) {
        return result;
    }
    result.clearance = result.useCustomClearance
        ? result.customClearance
        : threadClassClearanceFor(result.threadClass, threadPitch);
    result.radiusClearance = result.clearance / 2.0;
    return result;
}

ThreadDepthResult resolveThreadDepth(const app::DocumentObject& object,
                                     const std::string& holeDepthType,
                                     double holeDepth,
                                     bool threaded,
                                     double threadPitch)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::updateThreadDepthParam(), "Hole Depth" copies Depth, "Dimension" clamps
    // ThreadDepth to hole depth, and "Tapped (DIN76)" uses "Depth - getThreadRunout()".
    ThreadDepthResult result;
    result.type = readEnumProperty(
        object,
        "ThreadDepthType",
        {"Hole Depth", "Dimension", "Tapped (DIN76)"},
        "Hole Depth"
    );
    result.depth = readNumberProperty(object, "ThreadDepth", holeDepth);
    result.runout = threadPitch > Precision::Confusion() ? threadRunoutForPitch(threadPitch) : 0.0;
    if (!threaded) {
        return result;
    }

    if (holeDepthType == "ThroughAll") {
        if (result.type != "Dimension") {
            result.depth = holeDepth;
        }
        else if (result.depth > holeDepth) {
            result.depth = holeDepth;
        }
        return result;
    }

    if (result.type == "Hole Depth") {
        result.depth = holeDepth;
    }
    else if (result.type == "Dimension") {
        if (result.depth > holeDepth) {
            result.depth = holeDepth;
        }
    }
    else if (result.type == "Tapped (DIN76)") {
        result.depth = holeDepth - result.runout;
    }
    return result;
}

std::optional<std::string> threadTypeFromHoleResource(const std::string& threadType)
{
    if (threadType == "metric") {
        return "ISOMetricProfile";
    }
    if (threadType == "metricfine") {
        return "ISOMetricFineProfile";
    }
    return std::nullopt;
}

std::optional<HoleCutDefinitionKind> cutKindFromHoleResource(const std::string& cutType)
{
    if (cutType == "counterbore") {
        return HoleCutDefinitionKind::Counterbore;
    }
    if (cutType == "countersink") {
        return HoleCutDefinitionKind::Countersink;
    }
    return std::nullopt;
}

std::optional<HoleCutDefinition> readHoleCutDefinitionFile(const std::filesystem::path& path)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::readCutDefinitions(), reads each Resources/Hole "*.json", converts it through
    // from_json(CutDimensionSet), then addCutType() registers the JSON "name" for metric or
    // metricfine HoleCutType enum values. cad-core keeps the same resource schema and exposes
    // the matched filename in the recompute result.
    try {
        std::ifstream input(path);
        if (!input) {
            return std::nullopt;
        }
        nlohmann::json raw;
        input >> raw;

        const auto threadType = threadTypeFromHoleResource(raw.at("thread_type").get<std::string>());
        const auto kind = cutKindFromHoleResource(raw.at("cut_type").get<std::string>());
        if (!threadType || !kind) {
            return std::nullopt;
        }

        HoleCutDefinition definition;
        definition.name = raw.at("name").get<std::string>();
        definition.threadType = *threadType;
        definition.kind = *kind;
        definition.source = path.filename().string();

        if (*kind == HoleCutDefinitionKind::Counterbore) {
            for (const nlohmann::json& item : raw.at("data")) {
                definition.boreData.push_back(CounterboreDimension{
                    item.at("thread").get<std::string>(),
                    item.at("diameter").get<double>(),
                    item.at("depth").get<double>(),
                });
            }
        }
        else {
            definition.angle = raw.at("angle").get<double>();
            for (const nlohmann::json& item : raw.at("data")) {
                definition.sinkData.push_back(CountersinkDimension{
                    item.at("thread").get<std::string>(),
                    item.at("diameter").get<double>(),
                });
            }
        }
        return definition;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::filesystem::path> holeCutDefinitionSearchDirs()
{
    std::vector<std::filesystem::path> dirs;
#ifdef CAD_CORE_SOURCE_DIR
    dirs.emplace_back(std::filesystem::path(CAD_CORE_SOURCE_DIR) / "resources" / "hole");
#endif
    std::error_code cwdError;
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError) {
        dirs.emplace_back(cwd / "resources" / "hole");
        dirs.emplace_back(cwd / "cad-core" / "resources" / "hole");
    }
    if (const char* customDir = std::getenv("CAD_CORE_HOLE_RESOURCE_DIR")) {
        dirs.emplace_back(customDir);
    }
    return dirs;
}

const std::vector<HoleCutDefinition>& resourceHoleCutDefinitions()
{
    static const std::vector<HoleCutDefinition> definitions = [] {
        std::vector<HoleCutDefinition> loaded;
        std::set<std::pair<std::string, std::string>> seen;
        for (const std::filesystem::path& dir : holeCutDefinitionSearchDirs()) {
            std::error_code dirError;
            if (!std::filesystem::exists(dir, dirError) || !std::filesystem::is_directory(dir, dirError)) {
                continue;
            }

            std::vector<std::filesystem::path> files;
            std::filesystem::directory_iterator it(dir, dirError);
            if (dirError) {
                continue;
            }
            for (const std::filesystem::directory_entry& entry : it) {
                std::error_code entryError;
                if (entry.is_regular_file(entryError) && entry.path().extension() == ".json") {
                    files.push_back(entry.path());
                }
            }
            std::sort(files.begin(), files.end());

            for (const std::filesystem::path& file : files) {
                const auto definition = readHoleCutDefinitionFile(file);
                if (!definition) {
                    continue;
                }
                const auto key = std::make_pair(definition->threadType, definition->name);
                if (seen.insert(key).second) {
                    loaded.push_back(*definition);
                }
            }
        }
        return loaded;
    }();
    return definitions;
}

const HoleCutDefinition* resourceHoleCutDefinitionFor(const std::string& threadType,
                                                      const std::string& name,
                                                      HoleCutDefinitionKind kind)
{
    for (const HoleCutDefinition& definition : resourceHoleCutDefinitions()) {
        if (definition.threadType == threadType && definition.name == name && definition.kind == kind) {
            return &definition;
        }
    }
    return nullptr;
}

const std::vector<CounterboreDimension>* iso4762CounterboreTable(const std::string& threadType)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::updateHoleCutParams(), for metric "Counterbore" reads "ISO 4762" with
    // "const CounterBoreDimension& dimen = counter.get_bore(threadSizeStr)".
    // Data source:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso4762.json
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso4762-fine.json
    static const std::vector<CounterboreDimension> metric = {
        {"M1.6x0.35", 3.5, 1.7}, {"M2x0.4", 4.3, 2.1},
        {"M2.5x0.45", 5.0, 3.0}, {"M3x0.5", 6.0, 3.4},
        {"M3.5x0.6", 6.5, 3.9}, {"M4x0.7", 8.0, 4.4},
        {"M5x0.8", 10.0, 5.4}, {"M6x1.0", 11.0, 6.4},
        {"M8x1.25", 15.0, 8.6}, {"M10x1.5", 18.0, 10.6},
        {"M12x1.75", 20.0, 12.6}, {"M14x2.0", 24.0, 14.6},
        {"M16x2.0", 26.0, 16.6}, {"M18x2.5", 30.0, 18.6},
        {"M20x2.5", 33.0, 20.6}, {"M22x2.5", 36.0, 22.8},
        {"M24x3.0", 40.0, 24.8}, {"M30x3.5", 50.0, 31.0},
        {"M33x3.5", 54.0, 34.0}, {"M36x4.0", 58.0, 37.0},
        {"M42x4.5", 69.0, 43.0}, {"M48x5.0", 78.0, 49.0},
        {"M56x5.5", 90.0, 57.0}, {"M64x6.0", 103.0, 65.0},
    };
    static const std::vector<CounterboreDimension> metricFine = {
        {"M1.6x0.2", 3.5, 1.7}, {"M2x0.25", 4.3, 2.1},
        {"M2.5x0.35", 5.0, 3.0}, {"M3x0.35", 6.0, 3.4},
        {"M3.5x0.35", 6.5, 3.9}, {"M4x0.5", 8.0, 4.4},
        {"M5x0.5", 10.0, 5.4}, {"M6x0.75", 11.0, 6.4},
        {"M8x0.75", 15.0, 8.6}, {"M8x1.0", 15.0, 8.6},
        {"M10x1.0", 18.0, 10.6}, {"M10x1.25", 18.0, 10.6},
        {"M12x1.25", 20.0, 12.6}, {"M12x1.5", 20.0, 12.6},
        {"M14x1.5", 24.0, 14.6}, {"M16x1.5", 26.0, 16.6},
        {"M18x1.5", 30.0, 18.6}, {"M18x2.0", 30.0, 18.6},
        {"M20x1.5", 33.0, 20.6}, {"M20x2.0", 33.0, 20.6},
        {"M22x1.5", 36.0, 22.8}, {"M22x2.0", 36.0, 22.8},
        {"M24x2.0", 40.0, 24.8}, {"M30x2.0", 50.0, 31.0},
        {"M33x2.0", 54.0, 34.0}, {"M36x3.0", 58.0, 37.0},
        {"M42x3.0", 69.0, 43.0}, {"M48x3.0", 78.0, 49.0},
    };

    if (threadType == "ISOMetricProfile") {
        return &metric;
    }
    if (threadType == "ISOMetricFineProfile") {
        return &metricFine;
    }
    return nullptr;
}

const std::vector<CountersinkDimension>* iso10642CountersinkTable(const std::string& threadType)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::updateHoleCutParams(), for metric "Countersink" and "Counterdrill" reads
    // "ISO 10642" and applies "HoleCutCountersinkAngle.setValue(counter.angle)".
    // Data source:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso10642.json
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso10642-fine.json
    static const std::vector<CountersinkDimension> metric = {
        {"M2x0.4", 4.7}, {"M2.5x0.45", 5.9}, {"M3x0.5", 6.7},
        {"M4x0.7", 9.0}, {"M5x0.8", 11.2}, {"M6x1.0", 13.4},
        {"M8x1.25", 17.9}, {"M10x1.5", 22.4}, {"M12x1.75", 26.9},
        {"M14x2.0", 30.8}, {"M16x2.0", 33.6}, {"M20x2.5", 40.3},
    };
    static const std::vector<CountersinkDimension> metricFine = {
        {"M1.6x0.2", 3.6}, {"M2x0.25", 4.5}, {"M2.5x0.35", 5.6},
        {"M3x0.35", 6.7}, {"M4x0.5", 9.0}, {"M5x0.5", 11.2},
        {"M6x0.75", 13.5}, {"M8x0.75", 18.0}, {"M8x1.0", 18.0},
        {"M10x1.0", 22.4}, {"M10x1.25", 22.4}, {"M12x1.25", 26.8},
        {"M12x1.5", 26.8}, {"M14x1.5", 30.9}, {"M16x1.5", 33.6},
        {"M20x1.5", 40.3}, {"M20x2.0", 40.3},
    };

    if (threadType == "ISOMetricProfile") {
        return &metric;
    }
    if (threadType == "ISOMetricFineProfile") {
        return &metricFine;
    }
    return nullptr;
}

const std::vector<CounterboreDimension>* namedCounterboreTable(const std::string& threadType,
                                                              const std::string& tableName)
{
    if (tableName == "ISO 4762") {
        return iso4762CounterboreTable(threadType);
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::readCutDefinitions() loads Resources/Hole/*.json and addCutType() registers
    // their JSON "name" values as dynamic HoleCutType enums for metric / metricfine threads.
    static const std::vector<CounterboreDimension> din7984 = {
        {"M2x0.4", 4.3, 1.6}, {"M2.5x0.45", 5.0, 2.0}, {"M3x0.5", 6.0, 2.4},
        {"M3.5x0.6", 6.5, 2.9}, {"M4x0.7", 8.0, 3.2}, {"M5x0.8", 10.0, 4.0},
        {"M6x1.0", 11.0, 4.7}, {"M8x1.25", 15.0, 6.0}, {"M10x1.5", 18.0, 7.0},
        {"M12x1.75", 20.0, 8.0}, {"M14x2.0", 24.0, 9.0}, {"M16x2.0", 26.0, 10.5},
        {"M18x2.5", 30.0, 11.5}, {"M20x2.5", 33.0, 12.5}, {"M22x2.5", 36.0, 13.5},
        {"M24x3.0", 40.0, 14.5},
    };
    static const std::vector<CounterboreDimension> iso4762WithWasher = {
        {"M2x0.4", 6.0, 2.4}, {"M2.5x0.45", 7.0, 3.5}, {"M3x0.5", 9.0, 3.9},
        {"M3.5x0.6", 9.0, 4.4}, {"M4x0.7", 10.0, 5.2}, {"M5x0.8", 13.0, 6.4},
        {"M6x1.0", 15.0, 8.0}, {"M8x1.25", 18.0, 10.2}, {"M10x1.5", 24.0, 12.6},
        {"M12x1.75", 26.0, 15.1}, {"M14x2.0", 30.0, 17.1}, {"M16x2.0", 33.0, 16.6},
        {"M18x2.5", 36.0, 21.6}, {"M20x2.5", 40.0, 23.6}, {"M22x2.5", 43.0, 25.8},
        {"M24x3.0", 48.0, 29.8}, {"M27x3.0", 54.0, 35.0}, {"M30x3.5", 61.0, 38.0},
        {"M33x3.5", 63.0, 41.0}, {"M36x4.0", 69.0, 42.0},
    };
    static const std::vector<CounterboreDimension> iso14583 = {
        {"M2x0.4", 4.4, 2.0}, {"M2.5x0.45", 5.4, 2.5}, {"M3x0.5", 6.0, 2.8},
        {"M3.5x0.6", 7.6, 3.0}, {"M4x0.7", 8.6, 3.5}, {"M5x0.8", 10.1, 4.1},
        {"M6x1.0", 12.6, 5.0}, {"M8x1.25", 16.6, 6.6}, {"M10x1.5", 20.8, 8.1},
    };
    static const std::vector<CounterboreDimension> iso14583Partial = {
        {"M2x0.4", 4.4, 1.0}, {"M2.5x0.45", 5.4, 1.4}, {"M3x0.5", 6.0, 1.7},
        {"M3.5x0.6", 7.6, 1.7}, {"M4x0.7", 8.6, 2.0}, {"M5x0.8", 10.1, 2.5},
        {"M6x1.0", 12.6, 3.0}, {"M8x1.25", 16.6, 3.9}, {"M10x1.5", 20.8, 4.6},
    };
    static const std::vector<CounterboreDimension> iso12474 = {
        {"M8x1.0", 15.0, 8.6}, {"M10x1.0", 18.0, 10.6}, {"M10x1.25", 18.0, 10.6},
        {"M12x1.25", 20.0, 12.6}, {"M12x1.5", 20.0, 12.6}, {"M14x1.5", 24.0, 14.6},
        {"M16x1.5", 26.0, 16.6}, {"M18x1.5", 30.0, 18.6}, {"M20x1.5", 33.0, 20.6},
        {"M20x2.0", 33.0, 20.6}, {"M22x1.5", 36.0, 22.8}, {"M24x2.0", 40.0, 24.8},
        {"M30x2.0", 50.0, 31.0}, {"M33x1.5", 54.0, 34.0}, {"M36x3.0", 58.0, 37.0},
        {"M42x3.0", 69.0, 41.0},
    };

    if (threadType == "ISOMetricProfile") {
        if (tableName == "DIN 7984") {
            return &din7984;
        }
        if (tableName == "ISO 4762 + 7089") {
            return &iso4762WithWasher;
        }
        if (tableName == "ISO 14583") {
            return &iso14583;
        }
        if (tableName == "ISO 14583 (partial)") {
            return &iso14583Partial;
        }
    }
    if (threadType == "ISOMetricFineProfile" && tableName == "ISO 12474") {
        return &iso12474;
    }
    return nullptr;
}

const std::vector<CountersinkDimension>* namedCountersinkTable(const std::string& threadType,
                                                              const std::string& tableName)
{
    if (tableName == "ISO 10642") {
        return iso10642CountersinkTable(threadType);
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso2009.json
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/iso7046.json
    // are registered through Hole::readCutDefinitions() as metric countersink HoleCutType values.
    static const std::vector<CountersinkDimension> iso2009 = {
        {"M2x0.4", 4.3}, {"M2.5x0.45", 5.3}, {"M3x0.5", 6.3},
        {"M4x0.7", 9.5}, {"M5x0.8", 10.5}, {"M6x1.0", 12.7},
        {"M8x1.25", 17.7}, {"M10x1.5", 20.2}, {"M12x1.75", 24.7},
    };
    static const std::vector<CountersinkDimension> iso7046 = {
        {"M1.6x0.35", 3.6}, {"M2x0.4", 4.4}, {"M2.5x0.45", 5.5},
        {"M3x0.5", 6.3}, {"M3.5x0.6", 8.2}, {"M4x0.7", 9.4},
        {"M5x0.8", 10.4}, {"M6x1.0", 12.6}, {"M8x1.25", 17.3},
        {"M10x1.5", 20.0},
    };

    if (threadType == "ISOMetricProfile") {
        if (tableName == "ISO 2009") {
            return &iso2009;
        }
        if (tableName == "ISO 7046") {
            return &iso7046;
        }
    }
    return nullptr;
}

std::optional<CounterboreLookup> standardCounterboreFor(const HoleToolOptions& options)
{
    const std::string tableName =
        options.holeCutStandard.empty() ? "ISO 4762" : options.holeCutStandard;
    if (const HoleCutDefinition* definition =
            resourceHoleCutDefinitionFor(options.threadType, tableName, HoleCutDefinitionKind::Counterbore)) {
        for (const CounterboreDimension& dimension : definition->boreData) {
            if (options.threadSize == dimension.thread) {
                return CounterboreLookup{dimension, definition->source};
            }
        }
        return std::nullopt;
    }

    const std::vector<CounterboreDimension>* dimensions =
        namedCounterboreTable(options.threadType, tableName);
    if (dimensions == nullptr) {
        return std::nullopt;
    }
    for (const CounterboreDimension& dimension : *dimensions) {
        if (options.threadSize == dimension.thread) {
            return CounterboreLookup{dimension, ""};
        }
    }
    return std::nullopt;
}

std::optional<CountersinkLookup> standardCountersinkFor(const HoleToolOptions& options)
{
    const std::string tableName =
        options.holeCutStandard.empty() ? "ISO 10642" : options.holeCutStandard;
    if (const HoleCutDefinition* definition =
            resourceHoleCutDefinitionFor(options.threadType, tableName, HoleCutDefinitionKind::Countersink)) {
        for (const CountersinkDimension& dimension : definition->sinkData) {
            if (options.threadSize == dimension.thread) {
                return CountersinkLookup{dimension, definition->angle, definition->source};
            }
        }
        return std::nullopt;
    }

    const std::vector<CountersinkDimension>* dimensions =
        namedCountersinkTable(options.threadType, tableName);
    if (dimensions == nullptr) {
        return std::nullopt;
    }
    for (const CountersinkDimension& dimension : *dimensions) {
        if (options.threadSize == dimension.thread) {
            return CountersinkLookup{dimension, 90.0, ""};
        }
    }
    return std::nullopt;
}

bool usesMetricStandardHoleCut(const HoleToolOptions& options)
{
    return options.threadType == "ISOMetricProfile" || options.threadType == "ISOMetricFineProfile";
}

bool isDynamicCounterboreHoleCut(const HoleToolOptions& options)
{
    if (resourceHoleCutDefinitionFor(options.threadType, options.holeCutType, HoleCutDefinitionKind::Counterbore)
        != nullptr) {
        return true;
    }
    return namedCounterboreTable(options.threadType, options.holeCutType) != nullptr;
}

bool isDynamicCountersinkHoleCut(const HoleToolOptions& options)
{
    if (resourceHoleCutDefinitionFor(options.threadType, options.holeCutType, HoleCutDefinitionKind::Countersink)
        != nullptr) {
        return true;
    }
    return namedCountersinkTable(options.threadType, options.holeCutType) != nullptr;
}

gp_Vec computePerpendicular(const gp_Dir& direction)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::computePerpendicular(), computes an xDir normal to the hole zDir and normalizes it.
    gp_Vec zDir(direction);
    gp_Vec xDir;
    if (std::abs(zDir.Z() - zDir.X()) > Precision::Confusion()) {
        xDir = gp_Vec(zDir.Z(), 0, -zDir.X());
    }
    else if (std::abs(zDir.Z() - zDir.Y()) > Precision::Confusion()) {
        xDir = gp_Vec(zDir.Y(), -zDir.X(), 0);
    }
    else {
        xDir = gp_Vec(0, -zDir.Z(), zDir.Y());
    }
    xDir.Normalize();
    return xDir;
}

void rotateShapeToNormal(TopoDS_Shape& shape, const gp_Dir& sourceAxis, const gp_Dir& targetAxis)
{
    if (sourceAxis.IsEqual(targetAxis, Precision::Angular())) {
        return;
    }

    const double angle = std::acos(sourceAxis * targetAxis);
    gp_Dir rotationAxis(1.0, 0.0, 0.0);
    if (sourceAxis.IsOpposite(targetAxis, Precision::Angular())) {
        gp_XYZ xyz(sourceAxis.XYZ());
        if (std::fabs(xyz.X()) <= std::fabs(xyz.Y()) && std::fabs(xyz.X()) <= std::fabs(xyz.Z())) {
            xyz.SetX(1.0);
        }
        else if (std::fabs(xyz.Y()) <= std::fabs(xyz.X()) && std::fabs(xyz.Y()) <= std::fabs(xyz.Z())) {
            xyz.SetY(1.0);
        }
        else {
            xyz.SetZ(1.0);
        }
        rotationAxis = sourceAxis.Crossed(gp_Dir(xyz));
    }
    else {
        rotationAxis = sourceAxis.Crossed(targetAxis);
    }

    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), rotationAxis), angle);
    shape.Move(TopLoc_Location(rotation));
}

bool computeIntersection2d(const gp_Pnt& pa1,
                           const gp_Pnt& pa2,
                           const gp_Pnt& pb1,
                           const gp_Pnt& pb2,
                           double& x,
                           double& y)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // static computeIntersection(), solves the 2D line-line intersection used by countersinks
    // and angled drill points.
    const double vx1 = pa1.X() - pa2.X();
    const double vy1 = pa1.Y() - pa2.Y();
    const double vx2 = pb1.X() - pb2.X();
    const double vy2 = pb1.Y() - pb2.Y();
    const double det = (vx1 * -vy2) - (-vx2 * vy1);
    if (std::abs(det) <= Precision::Confusion()) {
        return false;
    }

    const double x1 = pa1.X();
    const double y1 = pa1.Y();
    const double x2 = pb1.X();
    const double y2 = pb1.Y();
    const double f = 1.0 / det;
    const double t1 = -vy2 * f * (x2 - x1) + vx2 * f * (y2 - y1);
    x = x1 + t1 * vx1;
    y = y1 + t1 * vy1;
    return true;
}

gp_Pnt offsetPoint(const gp_Pnt& center, const gp_Vec& radialDir, const gp_Vec& axisDir, double radius, double depth)
{
    gp_Vec offset = radialDir.Multiplied(radius);
    offset.Add(axisDir.Multiplied(depth));
    return center.Translated(offset);
}

bool addProfilePoint(std::vector<gp_Pnt>& points, const gp_Pnt& point)
{
    if (!points.empty() && points.back().Distance(point) <= Precision::Confusion()) {
        return false;
    }
    points.push_back(point);
    return true;
}

bool addWireEdge(BRepBuilderAPI_MakeWire& wireBuilder,
                 const gp_Pnt& start,
                 const gp_Pnt& end,
                 const app::DocumentObject& object,
                 runtime::ComputeContext& context)
{
    if (start.Distance(end) <= Precision::Confusion()) {
        return true;
    }
    BRepBuilderAPI_MakeEdge edgeBuilder(start, end);
    if (!edgeBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole profile edge",
                               object.name,
                               "Profile");
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return true;
}

std::vector<HoleCenterSource> holeCentersFromCircularProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), iterates profile edges, keeps GeomAbs_Circle, then filters
    // "adaptor.IsClosed()" by BaseProfileTypeOptions::OnCircles / OnArcs and uses
    // "circle.Axis().Location()" as the hole center. cad-core keeps the EdgeN source here so
    // Hole tool history can mirror findHoles() instead of inferring ownership from output faces.
    std::vector<HoleCenterSource> centers;
    if ((baseProfileType & baseProfileOnCircles) == 0 && (baseProfileType & baseProfileOnArcs) == 0) {
        return centers;
    }

    int edgeIndex = 0;
    for (TopExp_Explorer explorer(rawProfile, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        ++edgeIndex;
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        if ((baseProfileType & baseProfileOnCircles) == 0 && curve.IsClosed()) {
            continue;
        }
        if ((baseProfileType & baseProfileOnArcs) == 0 && !curve.IsClosed()) {
            continue;
        }
        centers.push_back(HoleCenterSource{curve.Circle().Axis().Location(),
                                           "Edge" + std::to_string(edgeIndex),
                                           "edge"});
    }

    return centers;
}

bool vertexBelongsToEdge(const TopoDS_Vertex& vertex, const TopoDS_Shape& rawProfile)
{
    for (TopExp_Explorer edgeExplorer(rawProfile, TopAbs_EDGE); edgeExplorer.More(); edgeExplorer.Next()) {
        for (TopExp_Explorer vertexExplorer(edgeExplorer.Current(), TopAbs_VERTEX); vertexExplorer.More();
             vertexExplorer.Next()) {
            if (vertex.IsSame(TopoDS::Vertex(vertexExplorer.Current()))) {
                return true;
            }
        }
    }
    return false;
}

std::vector<HoleCenterSource> holeCentersFromPointProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), when BaseProfileTypeOptions::OnPoints is active, iterates
    // "getSubTopoShapes(TopAbs_VERTEX, TopAbs_EDGE)" so curve endpoint vertices are ignored.
    std::vector<HoleCenterSource> centers;
    if ((baseProfileType & baseProfileOnPoints) == 0) {
        return centers;
    }

    int vertexIndex = 0;
    for (TopExp_Explorer explorer(rawProfile, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        ++vertexIndex;
        const TopoDS_Vertex vertex = TopoDS::Vertex(explorer.Current());
        if (vertexBelongsToEdge(vertex, rawProfile)) {
            continue;
        }
        centers.push_back(HoleCenterSource{BRep_Tool::Pnt(vertex),
                                           "Vertex" + std::to_string(vertexIndex),
                                           "vertex"});
    }
    return centers;
}

std::vector<HoleCenterSource> holeCentersFromProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    std::vector<HoleCenterSource> centers = holeCentersFromCircularProfile(rawProfile, baseProfileType);
    std::vector<HoleCenterSource> points = holeCentersFromPointProfile(rawProfile, baseProfileType);
    centers.insert(centers.end(), points.begin(), points.end());
    return centers;
}

std::vector<gp_Pnt> holeCenterPoints(const std::vector<HoleCenterSource>& centers)
{
    std::vector<gp_Pnt> points;
    points.reserve(centers.size());
    for (const HoleCenterSource& center : centers) {
        points.push_back(center.location);
    }
    return points;
}

std::optional<TopoDS_Shape> buildCylinderTool(const std::vector<gp_Pnt>& centers,
                                              const gp_Dir& direction,
                                              double radius,
                                              double depth,
                                              bool cutIntoMaterial,
                                              const app::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    if (centers.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole profile did not provide circle or arc centers",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    std::vector<TopoDS_Shape> holes;
    for (const gp_Pnt& center : centers) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
        // ::Hole::execute(), flat hole sections add the bottom point as "-length * zDir";
        // for Sketch profiles attached with MapMode "FlatFace", zDir is the support face
        // normal and the tool cuts into material along the opposite sketch normal.
        gp_Dir cutDirection = direction;
        if (cutIntoMaterial) {
            cutDirection.Reverse();
        }
        BRepPrimAPI_MakeCylinder builder(gp_Ax2(center, cutDirection), radius, depth);
        builder.Build();
        if (!builder.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build Hole cylinder",
                                   object.name,
                                   "Profile");
            return std::nullopt;
        }
        holes.push_back(builder.Shape());
    }

    if (holes.size() == 1U) {
        return holes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& hole : holes) {
        builder.Add(compound, hole);
    }
    return compound;
}

TopoDS_Compound compoundShapes(const std::vector<TopoDS_Shape>& shapes)
{
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        builder.Add(compound, shape);
    }
    return compound;
}

double modelThreadHelixLength(const ThreadDepthResult& threadDepth,
                              const std::string& depthMethod,
                              double holeDepth,
                              double pitch)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), initializes "helixLength = threadDepth + Pitch / 2"; non-Dimension
    // ThroughAll uses "threadDepth + 2 * Pitch", Hole Depth uses "threadDepth + Pitch / 8",
    // and Dimension clamps to "holeDepth + Pitch / 8" near the hole bottom.
    const double depth = threadDepth.depth;
    double helixLength = depth + pitch / 2.0;
    if (threadDepth.type != "Dimension") {
        if (depthMethod == "ThroughAll") {
            helixLength = holeDepth + 2.0 * pitch;
        }
        else if (threadDepth.type == "Tapped (DIN76)") {
            helixLength = depth + pitch / 2.0;
        }
        else {
            helixLength = depth + pitch / 8.0;
        }
    }
    else if (depthMethod == "Dimension" && depth > holeDepth - pitch / 2.0) {
        helixLength = holeDepth + pitch / 8.0;
    }
    return helixLength;
}

bool buildThreadWireEdge(BRepBuilderAPI_MakeWire& wireBuilder,
                         const gp_Pnt& start,
                         const gp_Pnt& end,
                         const app::DocumentObject& object,
                         runtime::ComputeContext& context)
{
    if (start.Distance(end) <= Precision::Confusion()) {
        return true;
    }
    BRepBuilderAPI_MakeEdge edgeBuilder(start, end);
    if (!edgeBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole thread profile edge",
                               object.name,
                               "ModelThread");
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return true;
}

bool buildThreadWireArc(BRepBuilderAPI_MakeWire& wireBuilder,
                        const gp_Pnt& start,
                        const gp_Pnt& mid,
                        const gp_Pnt& end,
                        const app::DocumentObject& object,
                        runtime::ComputeContext& context)
{
    if (start.Distance(end) <= Precision::Confusion()) {
        return true;
    }
    Handle(Geom_TrimmedCurve) arc = GC_MakeArcOfCircle(start, mid, end).Value();
    BRepBuilderAPI_MakeEdge edgeBuilder(arc);
    if (!edgeBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole thread profile arc",
                               object.name,
                               "ModelThread");
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return true;
}

std::optional<TopoDS_Wire> buildThreadProfileWire(const gp_Pnt& center,
                                                  const gp_Vec& radialDir,
                                                  const gp_Vec& axisDir,
                                                  const std::string& threadType,
                                                  double majorRadius,
                                                  double pitch,
                                                  double radiusClearance,
                                                  const app::DocumentObject& object,
                                                  runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), builds "mkThreadWire" from ISO/UTS sharp-V formulas or
    // BSP/BSW/BSF Whitworth formulas, with "RmajC = Rmaj + clearance".
    const double rmajC = majorRadius + radiusClearance;
    constexpr double marginZ = 0.001;

    BRepBuilderAPI_MakeWire wireBuilder;
    if (threadType == "BSP" || threadType == "BSW" || threadType == "BSF") {
        const double h = 0.960491 * pitch;
        const double crestRadius = 0.137329 * pitch;
        const double marginX = std::tan(62.5 * pi / 180.0) * marginZ;
        const double p23x = rmajC - crestRadius * 0.58284013094;
        const gp_Pnt p1 = offsetPoint(center, radialDir, axisDir, rmajC - 5.0 * h / 6.0 + marginX, marginZ);
        const gp_Pnt p2 = offsetPoint(center, radialDir, axisDir, p23x, 3.0 * pitch / 8.0);
        const gp_Pnt crest = offsetPoint(center, radialDir, axisDir, rmajC, pitch / 2.0);
        const gp_Pnt p3 = offsetPoint(center, radialDir, axisDir, p23x, 5.0 * pitch / 8.0);
        const gp_Pnt p4 =
            offsetPoint(center, radialDir, axisDir, rmajC - 5.0 * h / 6.0 + marginX, pitch - marginZ);
        if (!buildThreadWireEdge(wireBuilder, p1, p2, object, context)
            || !buildThreadWireArc(wireBuilder, p2, crest, p3, object, context)
            || !buildThreadWireEdge(wireBuilder, p3, p4, object, context)
            || !buildThreadWireEdge(wireBuilder, p4, p1, object, context)) {
            return std::nullopt;
        }
    }
    else {
        const double h = 7.0 * (std::sqrt(3.0) / 2.0 * pitch) / 8.0;
        const double marginX = std::tan(pi / 3.0) * marginZ;
        const gp_Pnt p1 = offsetPoint(center, radialDir, axisDir, rmajC - h + marginX, marginZ);
        const gp_Pnt p2 = offsetPoint(center, radialDir, axisDir, rmajC, 7.0 * pitch / 16.0);
        const gp_Pnt p3 = offsetPoint(center, radialDir, axisDir, rmajC, 9.0 * pitch / 16.0);
        const gp_Pnt p4 = offsetPoint(center, radialDir, axisDir, rmajC - h + marginX, pitch - marginZ);
        if (!buildThreadWireEdge(wireBuilder, p1, p2, object, context)) {
            return std::nullopt;
        }
        if (threadType == "ISOTyre") {
            const gp_Pnt crest = offsetPoint(center, radialDir, axisDir, rmajC + pitch / 32.0, pitch / 2.0);
            if (!buildThreadWireArc(wireBuilder, p2, crest, p3, object, context)) {
                return std::nullopt;
            }
        }
        else if (!buildThreadWireEdge(wireBuilder, p2, p3, object, context)) {
            return std::nullopt;
        }
        if (!buildThreadWireEdge(wireBuilder, p3, p4, object, context)
            || !buildThreadWireEdge(wireBuilder, p4, p1, object, context)) {
            return std::nullopt;
        }
    }

    wireBuilder.Build();
    if (!wireBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole thread profile wire",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<TopoDS_Wire> buildThreadHelix(const gp_Pnt& center,
                                            const gp_Dir& direction,
                                            const gp_Vec& radialDir,
                                            double pitch,
                                            double helixLength,
                                            double majorRadius,
                                            double taperedAngle,
                                            bool tapered,
                                            bool leftHanded,
                                            const app::DocumentObject& object,
                                            runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), calls TopoShape::makeLongHelix(Pitch, helixLength, Rmaj,
    // "TaperedAngle - 90", leftHanded) before feeding the wire to BRepOffsetAPI_MakePipeShell.
    if (pitch <= Precision::Confusion() || helixLength <= Precision::Confusion()
        || majorRadius <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole model thread pitch, length, and radius must be positive",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    const double helixAngle = tapered ? (taperedAngle - 90.0) * pi / 180.0 : 0.0;
    const double topRadius = majorRadius + helixLength * std::tan(helixAngle);
    if (topRadius <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole model thread taper collapses the helix radius",
                               object.name,
                               "TaperedAngle");
        return std::nullopt;
    }

    (void)radialDir;

    gp_Ax2 cylinderAxis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    Handle(Geom_Surface) surface;
    const bool isCylinder = std::fabs(helixAngle) < Precision::Confusion();
    if (isCylinder) {
        surface = new Geom_CylindricalSurface(cylinderAxis, majorRadius);
    }
    else {
        surface = new Geom_ConicalSurface(gp_Ax3(cylinderAxis), helixAngle, majorRadius);
    }

    const double turns = helixLength / pitch;
    const unsigned long wholeTurns = static_cast<unsigned long>(std::floor(turns));
    const double partTurn = turns - static_cast<double>(wholeTurns);
    const double coneDirection = leftHanded ? -1.0 : 1.0;
    gp_Pnt2d begin(0.0, 0.0);
    gp_Dir2d lineDirection(2.0 * pi, pitch);
    if (leftHanded) {
        lineDirection.SetCoord(-2.0 * pi, pitch);
    }
    gp_Ax2d lineAxis(begin, lineDirection);
    Handle(Geom2d_Line) line = new Geom2d_Line(lineAxis);
    begin = line->Value(0.0);

    BRepBuilderAPI_MakeWire wireBuilder;
    for (unsigned long index = 0; index < wholeTurns; ++index) {
        gp_Pnt2d end;
        if (isCylinder) {
            end = line->Value(std::sqrt(4.0 * pi * pi + pitch * pitch) * static_cast<double>(index + 1));
        }
        else {
            end = gp_Pnt2d(coneDirection * static_cast<double>(index + 1) * 2.0 * pi,
                           (static_cast<double>(index + 1) * pitch) / std::cos(helixAngle));
        }
        Handle(Geom2d_TrimmedCurve) segment = GCE2d_MakeSegment(begin, end);
        BRepBuilderAPI_MakeEdge edgeBuilder(segment, surface);
        if (!edgeBuilder.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build Hole thread helix edge",
                                   object.name,
                                   "ModelThread");
            return std::nullopt;
        }
        wireBuilder.Add(edgeBuilder.Edge());
        begin = end;
    }
    if (partTurn > Precision::Confusion()) {
        gp_Pnt2d end;
        if (isCylinder) {
            end = line->Value(std::sqrt(4.0 * pi * pi + pitch * pitch) * turns);
        }
        else {
            end = gp_Pnt2d(coneDirection * turns * 2.0 * pi, helixLength / std::cos(helixAngle));
        }
        Handle(Geom2d_TrimmedCurve) segment = GCE2d_MakeSegment(begin, end);
        BRepBuilderAPI_MakeEdge edgeBuilder(segment, surface);
        if (!edgeBuilder.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build Hole thread helix edge",
                                   object.name,
                                   "ModelThread");
            return std::nullopt;
        }
        wireBuilder.Add(edgeBuilder.Edge());
    }

    wireBuilder.Build();
    if (!wireBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole thread helix wire",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }
    TopoDS_Wire wire = wireBuilder.Wire();
    BRepLib::BuildCurves3d(wire);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), after TopoShape::makeLongHelix(), runs "mov.SetRotation(gp_Ax1(origo,
    // dir_axis2), std::numbers::pi)" with the comment "Reverse the direction of the helix. So
    // that it goes into the material", then "rotateToNormal(dir_axis1, zDir, helix)".
    gp_Trsf reverseIntoMaterial;
    reverseIntoMaterial.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), pi);
    wire.Move(TopLoc_Location(reverseIntoMaterial));
    rotateShapeToNormal(wire, gp_Dir(0.0, 0.0, 1.0), direction);
    gp_Trsf translateToCenter;
    translateToCenter.SetTranslation(gp_Vec(center.X(), center.Y(), center.Z()));
    wire.Move(TopLoc_Location(translateToCenter));
    return wire;
}

std::optional<TopoDS_Shape> buildModelThreadAtCenter(const gp_Pnt& center,
                                                     const gp_Dir& direction,
                                                     const HoleToolOptions& options,
                                                     const ThreadModelParameters& threadModel,
                                                     const ThreadDepthResult& threadDepth,
                                                     const std::string& depthMethod,
                                                     double threadDiameter,
                                                     double threadPitch,
                                                     const app::DocumentObject& object,
                                                     runtime::ComputeContext& context)
{
    const double majorRadius = threadDiameter / 2.0;
    const double helixLength = modelThreadHelixLength(threadDepth, depthMethod, options.depth, threadPitch);
    if (threadDepth.depth <= Precision::Confusion() || helixLength <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole model thread depth must be positive",
                               object.name,
                               "ThreadDepth");
        return std::nullopt;
    }

    const gp_Vec axisDir(direction);
    const gp_Vec radialDir = computePerpendicular(direction);
    const auto threadWire = buildThreadProfileWire(center,
                                                   radialDir,
                                                   axisDir,
                                                   options.threadType,
                                                   majorRadius,
                                                   threadPitch,
                                                   threadModel.radiusClearance,
                                                   object,
                                                   context);
    if (!threadWire) {
        return std::nullopt;
    }
    const auto helix = buildThreadHelix(center,
                                        direction,
                                        radialDir,
                                        threadPitch,
                                        helixLength,
                                        majorRadius,
                                        options.taperedAngle,
                                        options.tapered,
                                        threadModel.direction == "Left",
                                        object,
                                        context);
    if (!helix) {
        return std::nullopt;
    }

    BRepOffsetAPI_MakePipeShell pipeBuilder(*helix);
    pipeBuilder.SetTolerance(Precision::Confusion());
    pipeBuilder.SetTransitionMode(BRepBuilderAPI_Transformed);
    pipeBuilder.SetMode(true);
    pipeBuilder.Add(*threadWire);
    if (!pipeBuilder.IsReady()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not prepare Hole model thread pipe shell",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), calls "TopoDS_Shape shell = mkPS.Shape()" directly after
    // IsReady(), then calls "mkPS.Simulate(2, sim)" for the end caps.
    const TopoDS_Shape shell = pipeBuilder.Shape();
    if (shell.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole model thread pipe shell is empty",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    TopTools_ListOfShape simulated;
    pipeBuilder.Simulate(2, simulated);
    if (simulated.Extent() < 2) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not simulate Hole model thread end sections",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::makeThread(), caps mkPS.Simulate() end wires with
    // "Part::FaceMakerCheese::makeFace(frontwires/backwires)" before sewing the thread solid.
    const auto frontFace = part::makeCheeseFaceFromClosedWires({TopoDS::Wire(simulated.First())});
    const auto backFace = part::makeCheeseFaceFromClosedWires({TopoDS::Wire(simulated.Last())});
    if (!frontFace || frontFace->IsNull() || !backFace || backFace->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not cap Hole model thread pipe shell",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    BRepBuilderAPI_Sewing sewing;
    sewing.SetTolerance(Precision::Confusion());
    sewing.Add(*frontFace);
    sewing.Add(*backFace);
    sewing.Add(shell);
    sewing.Perform();
    if (sewing.SewedShape().IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not sew Hole model thread shell",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    BRepBuilderAPI_MakeSolid solidBuilder;
    solidBuilder.Add(TopoDS::Shell(sewing.SewedShape()));
    if (!solidBuilder.IsDone() || solidBuilder.Shape().IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not solidify Hole model thread shell",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }
    TopoDS_Shape result = solidBuilder.Shape();
    BRepClass3d_SolidClassifier classifier(result);
    classifier.PerformInfinitePoint(Precision::Confusion());
    if (classifier.State() == TopAbs_IN) {
        result.Reverse();
    }
    return result;
}

std::optional<TopoDS_Shape> buildModelThreadTool(const std::vector<gp_Pnt>& centers,
                                                 const gp_Dir& direction,
                                                 const HoleToolOptions& options,
                                                 const ThreadModelParameters& threadModel,
                                                 const ThreadDepthResult& threadDepth,
                                                 const std::string& depthMethod,
                                                 double threadDiameter,
                                                 double threadPitch,
                                                 const app::DocumentObject& object,
                                                 runtime::ComputeContext& context)
{
    if (centers.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole profile did not provide circle or arc centers",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    if (threadDiameter <= Precision::Confusion() || threadPitch <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole model thread requires positive thread diameter and pitch",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }

    std::vector<TopoDS_Shape> threads;
    for (const gp_Pnt& center : centers) {
        const auto thread = buildModelThreadAtCenter(center,
                                                     direction,
                                                     options,
                                                     threadModel,
                                                     threadDepth,
                                                     depthMethod,
                                                     threadDiameter,
                                                     threadPitch,
                                                     object,
                                                     context);
        if (!thread) {
            return std::nullopt;
        }
        threads.push_back(*thread);
    }
    if (threads.size() == 1U) {
        return threads.front();
    }
    return compoundShapes(threads);
}

std::optional<TopoDS_Shape> combineHoleAndThreadTools(const TopoDS_Shape& holeTool,
                                                      const TopoDS_Shape& threadTool,
                                                      const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), when "Threaded && ModelThread", adds "protoHole" and "protoThread"
    // into one "holeWithThread" compound with "builder.MakeCompound()" and two "builder.Add()"
    // calls. cad-core returns the same compound; Body marks this Hole ModelThread source so
    // part::makeElementBooleanFromSources() can follow FreeCAD's RecursiveCutFusedTools() path
    // only for this producer instead of globally rewriting every compound Cut tool.
    TopoDS_Compound holeWithThread;
    BRep_Builder builder;
    builder.MakeCompound(holeWithThread);
    builder.Add(holeWithThread, holeTool);
    builder.Add(holeWithThread, threadTool);
    if (holeWithThread.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not combine Hole tap drill and model thread compound",
                               object.name,
                               "ModelThread");
        return std::nullopt;
    }
    return holeWithThread;
}

bool normalizeHoleToolOptions(HoleToolOptions& options,
                              const app::DocumentObject& object,
                              runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::updateHoleCutParams(), for metric threaded head cuts first uses ISO 4762 /
    // ISO 10642 standard dimensions, then falls back to calculateAndSetCounterbore() /
    // calculateAndSetCountersink(); non-metric head cuts keep the existing rule-of-thumb path.
    if (options.drillPoint != "Flat" && options.drillPoint != "Angled") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported DrillPoint " + options.drillPoint,
                               object.name,
                               "DrillPoint");
        return false;
    }

    if (options.holeCutType != "None" && options.holeCutType != "Counterbore"
        && options.holeCutType != "Countersink" && options.holeCutType != "Counterdrill") {
        if (isDynamicCounterboreHoleCut(options)) {
            options.holeCutStandard = options.holeCutType;
            options.holeCutType = "Counterbore";
        }
        else if (isDynamicCountersinkHoleCut(options)) {
            options.holeCutStandard = options.holeCutType;
            options.holeCutType = "Countersink";
        }
        else {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Unsupported HoleCutType " + options.holeCutType,
                                   object.name,
                                   "HoleCutType");
            return false;
        }
    }

    if (options.holeCutType == "Counterbore") {
        if (options.holeCutDiameter <= options.diameter) {
            if (const auto standard = standardCounterboreFor(options)) {
                options.holeCutDiameter = standard->dimension.diameter;
                options.holeCutDepth = standard->dimension.depth;
                options.holeCutDefinitionSource = standard->source;
            }
            else if (usesMetricStandardHoleCut(options)) {
                options.holeCutDiameter = options.diameter * 1.5 + 1.0;
                options.holeCutDepth = options.diameter;
            }
            else {
                options.holeCutDiameter = options.diameter * 1.6;
                options.holeCutDepth = options.diameter * 0.9;
            }
        }
        if (options.holeCutDepth <= Precision::Confusion()) {
            if (const auto standard = standardCounterboreFor(options)) {
                options.holeCutDepth = standard->dimension.depth;
                options.holeCutDefinitionSource = standard->source;
            }
            else if (usesMetricStandardHoleCut(options)) {
                options.holeCutDepth = options.diameter;
            }
            else {
                options.holeCutDepth = options.diameter * 0.9;
            }
        }
        options.holeCutCountersinkAngle = 90.0;
    }
    else if (options.holeCutType == "Countersink" || options.holeCutType == "Counterdrill") {
        bool appliedMetricStandard = false;
        double standardAngle = 90.0;
        if (options.holeCutDiameter <= options.diameter) {
            if (const auto standard = standardCountersinkFor(options)) {
                options.holeCutDiameter = standard->dimension.diameter;
                options.holeCutDefinitionSource = standard->source;
                standardAngle = standard->angle;
                appliedMetricStandard = true;
            }
            else if (usesMetricStandardHoleCut(options)) {
                options.holeCutDiameter = options.diameter * 2.24;
                appliedMetricStandard = true;
            }
            else {
                options.holeCutDiameter = options.diameter * 1.7;
            }
        }
        if (appliedMetricStandard || options.holeCutCountersinkAngle <= Precision::Confusion()) {
            options.holeCutCountersinkAngle = usesMetricStandardHoleCut(options)
                ? standardAngle
                : defaultCountersinkAngle(options.threadType);
        }
        if (options.holeCutType == "Countersink") {
            options.holeCutDepth = 0.0;
        }
        if (options.holeCutType == "Counterdrill" && options.holeCutDepth <= Precision::Confusion()) {
            options.holeCutDepth = 1.0;
        }
    }

    if (options.holeCutType != "None") {
        if (options.holeCutDiameter / 2.0 < options.diameter / 2.0) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Hole cut diameter too small",
                                   object.name,
                                   "HoleCutDiameter");
            return false;
        }
        if (options.holeCutDepth < 0.0) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Hole cut depth must be greater or equal to zero",
                                   object.name,
                                   "HoleCutDepth");
            return false;
        }
        if (options.holeCutDepth > options.depth) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Hole cut depth must be less than hole depth",
                                   object.name,
                                   "HoleCutDepth");
            return false;
        }
        if (options.holeCutType != "Counterbore"
            && (options.holeCutCountersinkAngle <= Precision::Angular()
                || options.holeCutCountersinkAngle >= 180.0 - Precision::Angular())) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_angle",
                                   "Hole countersink angle is invalid",
                                   object.name,
                                   "HoleCutCountersinkAngle");
            return false;
        }
    }

    if (options.drillPoint == "Angled"
        && (options.drillPointAngle <= Precision::Angular()
            || options.drillPointAngle >= 180.0 - Precision::Angular())) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Hole drill point angle is invalid",
                               object.name,
                               "DrillPointAngle");
        return false;
    }
    if (options.tapered
        && (options.taperedAngle <= Precision::Angular()
            || options.taperedAngle >= 180.0 - Precision::Angular())) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Hole taper angle is invalid",
                               object.name,
                               "TaperedAngle");
        return false;
    }

    return true;
}

std::optional<TopoDS_Shape> buildProfiledToolAtCenter(const gp_Pnt& center,
                                                      const gp_Dir& direction,
                                                      const HoleToolOptions& options,
                                                      const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), builds a "BRepBuilderAPI_MakeWire" section for HoleCutType and
    // DrillPoint, then calls "BRepPrimAPI_MakeRevol(face, gp_Ax1(firstPoint, zDir), angle)".
    gp_Vec axisDir(direction);
    if (options.cutIntoMaterial) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
        // ::Hole::execute(), all section-depth points are expressed as "-holeCutDepth * zDir" /
        // "-length * zDir" while the revolve axis remains "gp_Ax1(firstPoint, zDir)".
        axisDir.Reverse();
    }
    const gp_Vec radialDir = computePerpendicular(direction);
    const double radius = options.diameter / 2.0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), sets "TaperedAngleVal = Tapered ? radians(TaperedAngle) : radians(90)"
    // and "radiusBottom = Diameter / 2.0 - length / tan(TaperedAngleVal)".
    const double taperAngle = options.tapered ? options.taperedAngle * pi / 180.0 : pi / 2.0;
    const double radiusBottom = radius - options.depth / std::tan(taperAngle);
    double lengthCounter = 0.0;
    std::vector<gp_Pnt> points;
    addProfilePoint(points, center);

    const bool hasHeadCut = options.holeCutType == "Counterbore" || options.holeCutType == "Countersink"
        || options.holeCutType == "Counterdrill";
    if (hasHeadCut) {
        const double holeCutRadius = options.holeCutDiameter / 2.0;
        double holeCutDepth = options.holeCutDepth;
        double countersinkAngle = options.holeCutType == "Counterbore"
            ? pi / 2.0
            : options.holeCutCountersinkAngle * pi / 360.0;

        if (options.holeCutType == "Countersink") {
            holeCutDepth = 0.0;
        }

        addProfilePoint(points, offsetPoint(center, radialDir, axisDir, holeCutRadius, 0.0));
        if (holeCutDepth > Precision::Confusion()) {
            addProfilePoint(points, offsetPoint(center, radialDir, axisDir, holeCutRadius, holeCutDepth));
        }

        double xPosCounter = 0.0;
        double zPosCounter = 0.0;
        if (!computeIntersection2d(gp_Pnt(holeCutRadius, -holeCutDepth, 0),
                                   gp_Pnt(holeCutRadius - std::sin(countersinkAngle),
                                          -std::cos(countersinkAngle) - holeCutDepth,
                                          0),
                                   gp_Pnt(radius, 0, 0),
                                   gp_Pnt(radiusBottom, -options.depth, 0),
                                   xPosCounter,
                                   zPosCounter)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Hole countersink intersection failed",
                                   object.name,
                                   "HoleCutType");
            return std::nullopt;
        }
        if (-options.depth > zPosCounter) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Hole countersink is invalid",
                                   object.name,
                                   "HoleCutType");
            return std::nullopt;
        }

        lengthCounter = zPosCounter;
        addProfilePoint(points, offsetPoint(center, radialDir, axisDir, xPosCounter, -zPosCounter));
    }
    else {
        addProfilePoint(points, offsetPoint(center, radialDir, axisDir, radius, 0.0));
    }

    if (options.drillPoint == "Flat") {
        addProfilePoint(points, offsetPoint(center, radialDir, axisDir, radiusBottom, options.depth));
        addProfilePoint(points, offsetPoint(center, radialDir, axisDir, 0.0, options.depth));
    }
    else {
        const double drillPointAngle = (180.0 - options.drillPointAngle) * pi / 360.0;
        double xPosDrill = 0.0;
        double zPosDrill = 0.0;

        if (options.drillForDepth) {
            if (!computeIntersection2d(gp_Pnt(0, -options.depth, 0),
                                       gp_Pnt(radius, radius * std::tan(drillPointAngle) - options.depth, 0),
                                       gp_Pnt(radius, 0, 0),
                                       gp_Pnt(radiusBottom, -options.depth, 0),
                                       xPosDrill,
                                       zPosDrill)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Hole drill point intersection failed",
                                       object.name,
                                       "DrillPoint");
                return std::nullopt;
            }
            if (zPosDrill > 0.0 || zPosDrill >= lengthCounter) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_length",
                                       "Hole drill point is invalid",
                                       object.name,
                                       "DrillPoint");
                return std::nullopt;
            }
            addProfilePoint(points, offsetPoint(center, radialDir, axisDir, xPosDrill, -zPosDrill));
            addProfilePoint(points, offsetPoint(center, radialDir, axisDir, 0.0, options.depth));
        }
        else {
            xPosDrill = radiusBottom;
            zPosDrill = -options.depth;
            addProfilePoint(points, offsetPoint(center, radialDir, axisDir, xPosDrill, -zPosDrill));
            addProfilePoint(points,
                            offsetPoint(center,
                                        radialDir,
                                        axisDir,
                                        0.0,
                                        options.depth + radius * std::tan(drillPointAngle)));
        }
    }
    addProfilePoint(points, center);

    BRepBuilderAPI_MakeWire wireBuilder;
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (!addWireEdge(wireBuilder, points[index - 1], points[index], object, context)) {
            return std::nullopt;
        }
    }
    if (!wireBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole profile wire",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    BRepBuilderAPI_MakeFace faceBuilder(wireBuilder.Wire());
    if (!faceBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build Hole profile face",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const double angle = 2.0 * pi;
    BRepPrimAPI_MakeRevol revolBuilder(faceBuilder.Face(), gp_Ax1(center, direction), angle);
    revolBuilder.Build();
    if (!revolBuilder.IsDone() || revolBuilder.Shape().IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not revolve Hole profile",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    return revolBuilder.Shape();
}

std::optional<TopoDS_Shape> buildProfiledTool(const std::vector<gp_Pnt>& centers,
                                              const gp_Dir& direction,
                                              const HoleToolOptions& options,
                                              const app::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    if (centers.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole profile did not provide circle or arc centers",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    std::vector<TopoDS_Shape> holes;
    for (const gp_Pnt& center : centers) {
        const auto hole = buildProfiledToolAtCenter(center, direction, options, object, context);
        if (!hole) {
            return std::nullopt;
        }
        holes.push_back(*hole);
    }

    if (holes.size() == 1U) {
        return holes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& hole : holes) {
        builder.Add(compound, hole);
    }
    return compound;
}

std::size_t subshapeCount(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(shape, kind, shapes);
    return static_cast<std::size_t>(std::max(0, shapes.Extent()));
}

void addUniqueString(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void addModifiedProfileSource(part::NamedShape& namedShape,
                              const std::string& targetFace,
                              const std::string& profileObject,
                              const HoleCenterSource& centerSource,
                              std::size_t centerIndex,
                              std::size_t targetFaceIndex,
                              std::size_t toolFaceCount,
                              bool threaded,
                              bool modelThread)
{
    const std::string sourceStableName = profileObject + "." + centerSource.sourceSubname;
    auto elementIt = namedShape.elements.find(targetFace);
    if (elementIt == namedShape.elements.end()) {
        return;
    }

    elementIt->second.status = part::ElementHistoryKind::Modified;
    addUniqueString(elementIt->second.sources, sourceStableName);
    namedShape.elementMap[sourceStableName] = targetFace;

    const auto duplicateHistory = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const part::ElementHistory& entry) {
            return entry.kind == part::ElementHistoryKind::Modified && entry.element == targetFace
                && entry.sources == std::vector<std::string> {sourceStableName};
        }
    );
    if (duplicateHistory == namedShape.history.end()) {
        namedShape.history.push_back(
            part::ElementHistory {part::ElementHistoryKind::Modified, targetFace, {sourceStableName}}
        );
    }

    part::MapperHistoryEvent event;
    event.source = part::MapperHistoryEndpoint {profileObject, centerSource.sourceSubname};
    event.target = part::MapperHistoryEndpoint {namedShape.owner, targetFace};
    event.shapeKind = "face";
    event.relation = part::MapperHistoryRelation::Modified;
    event.makerStage = "hole_find_holes";
    event.recoverability = part::MapperHistoryRecoverability::Resolved;
    event.evidence = {
        {"producer", "PartDesign::Hole::findHoles"},
        {"make_shape_with_element_map", true},
        {"source_profile", profileObject},
        {"source_subname", centerSource.sourceSubname},
        {"source_kind", centerSource.sourceKind},
        {"center_index", centerIndex},
        {"target_face_index", targetFaceIndex},
        {"tool_face_count", toolFaceCount},
        {"threaded", threaded},
        {"model_thread", modelThread},
    };
    part::addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
}

part::NamedShape namedShapeForHoleToolHistory(const std::string& owner,
                                             const TopoDS_Shape& toolShape,
                                             const app::Link& profile,
                                             const std::vector<HoleCenterSource>& centerSources,
                                             bool threaded,
                                             bool modelThread)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), calls "mapper.populate(Part::MappingStatus::Modified, baseshape,
    // TopoShape(protoHole).getSubTopoShapes(TopAbs_FACE))" and then
    // "hole.makeShapeWithElementMap(protoHole, mapper, {baseshape})". cad-core records the
    // request-local source profile EdgeN/VertexN -> tool FaceN ledger here, so Body's subtractive
    // cut consumes producer history through AddSubShape.subNamedShape instead of adapter fixes.
    part::NamedShape namedShape = part::indexedNamedShapeForObject(owner, toolShape);
    if (centerSources.empty()) {
        return namedShape;
    }

    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(toolShape, TopAbs_FACE, faces);
    if (faces.Extent() <= 0) {
        return namedShape;
    }

    const std::size_t toolFaceCount = static_cast<std::size_t>(faces.Extent());
    const std::size_t facesPerCenter = std::max<std::size_t>(1U, toolFaceCount / centerSources.size());
    for (std::size_t faceIndex = 1; faceIndex <= toolFaceCount; ++faceIndex) {
        const std::size_t centerIndex = std::min((faceIndex - 1U) / facesPerCenter, centerSources.size() - 1U);
        addModifiedProfileSource(namedShape,
                                 "Face" + std::to_string(faceIndex),
                                 profile.object,
                                 centerSources[centerIndex],
                                 centerIndex + 1U,
                                 faceIndex,
                                 toolFaceCount,
                                 threaded,
                                 modelThread);
    }

    addUniqueString(namedShape.elementHistoryStatus, "hole_find_holes:profile_source");
    addUniqueString(namedShape.elementHistoryStatus, "hole_cut_history:element_map_freeze");
    if (modelThread) {
        addUniqueString(namedShape.elementHistoryStatus, "hole_model_thread:pipe_shell_tool_history");
    }
    return namedShape;
}

nlohmann::json centerSourcesToJson(const std::vector<HoleCenterSource>& centerSources)
{
    nlohmann::json result = nlohmann::json::array();
    for (const HoleCenterSource& centerSource : centerSources) {
        result.push_back({
            {"subname", centerSource.sourceSubname},
            {"kind", centerSource.sourceKind},
        });
    }
    return result;
}

nlohmann::json holeHistoryFreezeJson(const app::Link& profile,
                                     const PreviousSolidSource& base,
                                     const TopoDS_Shape& toolShape,
                                     const std::vector<HoleCenterSource>& centerSources,
                                     const std::string& holeCutType,
                                     bool threaded,
                                     bool modelThread)
{
    nlohmann::json covered = {
        "find_holes_make_shape_with_element_map",
        "profile_source_tool_face_mapper_history",
        "subtractive_body_cut_history",
    };
    if (modelThread) {
        covered.push_back("model_thread_tool_face_history");
        covered.push_back("model_thread_compound_tool_shape");
    }
    const bool hasHeadCut = holeCutType != "None";
    if (threaded && modelThread && hasHeadCut) {
        covered.push_back("threaded_model_thread_head_cut_native_oracle");
    }
    nlohmann::json remaining = nlohmann::json::array();

    nlohmann::json history = {
        {"status", "element_map_freeze_first_slice"},
        {"producer", "PartDesign::Hole::findHoles"},
        {"source_profile", profile.object},
        {"base_solid", base.owner},
        {"center_count", centerSources.size()},
        {"center_sources", centerSourcesToJson(centerSources)},
        {"tool_faces", subshapeCount(toolShape, TopAbs_FACE)},
        {"tool_edges", subshapeCount(toolShape, TopAbs_EDGE)},
        {"tool_vertices", subshapeCount(toolShape, TopAbs_VERTEX)},
        {"mapper_stage", "hole_find_holes"},
        {"covered", covered},
        {"remaining", remaining},
        {"head_cut", hasHeadCut},
        {"threaded", threaded},
        {"model_thread", modelThread},
    };
    return history;
}

std::optional<HoleBuild> buildHole(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), reads "Profile", "Diameter", "DepthType", "Depth", "DrillPoint",
    // computes "SketchVector = guessNormalDirection(profileshape)", then calls findHoles().
    if (app::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Hole Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto profileLink = app::readLink(object, "Profile");
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Hole Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end()
        || (shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Profile)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a sketch profile",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const TopoDS_Shape rawProfile = shapeIt->second.shape;
    if (rawProfile.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole Profile target " + profileLink->object + " did not produce profile geometry",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const auto base = previousSolidSource(context);
    if (!base) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole requires a previous base solid",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const double requestedDiameter = readNumberProperty(object, "Diameter", 6.0);
    const bool threaded = readBoolProperty(object, "Threaded");
    const bool modelThread = readBoolProperty(object, "ModelThread");
    const std::string threadType = readEnumProperty(object,
                                                    "ThreadType",
                                                    {"None",
                                                     "ISOMetricProfile",
                                                     "ISOMetricFineProfile",
                                                     "UNC",
                                                     "UNF",
                                                     "UNEF",
                                                     "NPT",
                                                     "BSP",
                                                     "BSW",
                                                     "BSF",
                                                     "ISOTyre"},
                                                    "None");
    const auto threadDiameter =
        resolveThreadDiameter(object, threadType, requestedDiameter, threaded, modelThread, context);
    if (!threadDiameter) {
        return std::nullopt;
    }
    const ThreadModelParameters threadModel =
        resolveThreadModelParameters(object, threadType, threadDiameter->threadPitch, threaded);

    const double diameter = threadDiameter->diameter;
    if (diameter <= 10.0 * Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole Diameter must be positive",
                               object.name,
                               "Diameter");
        return std::nullopt;
    }

    const std::string method = readEnumProperty(object, "DepthType", {"Dimension", "ThroughAll"}, "Dimension");
    double depth = 0.0;
    if (method == "Dimension") {
        depth = readNumberProperty(object, "Depth", 25.0);
    }
    else if (method == "ThroughAll") {
        depth = throughAllLength(base->shape, rawProfile);
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported Hole DepthType " + method,
                               object.name,
                               "DepthType");
        return std::nullopt;
    }

    if (depth <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole Depth must be positive",
                               object.name,
                               method == "Dimension" ? "Depth" : "DepthType");
        return std::nullopt;
    }
    const ThreadDepthResult threadDepth =
        resolveThreadDepth(object, method, depth, threaded, threadDiameter->threadPitch);

    auto direction = guessHoleDirection(rawProfile, shapeIt->second.profileShape, profileLink->object, context);
    if (!direction) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_direction",
                               "Hole could not infer the profile normal direction",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    if (readBoolProperty(object, "Reversed")) {
        direction->Reverse();
    }

    const int baseProfileType = readBaseProfileType(object);
    const std::vector<HoleCenterSource> centerSources = holeCentersFromProfile(rawProfile, baseProfileType);
    const std::vector<gp_Pnt> centers = holeCenterPoints(centerSources);

    HoleToolOptions options;
    options.diameter = diameter;
    options.depth = depth;
    options.cutIntoMaterial = profileUsesFlatFaceSupport(profileLink->object, context);
    options.threadType = threadType;
    options.threadSize = threadDiameter->threadSize;
    options.holeCutType = readEnumProperty(object,
                                           "HoleCutType",
                                           {"None", "Counterbore", "Countersink", "Counterdrill"},
                                           "None");
    options.holeCutDiameter = readNumberProperty(object, "HoleCutDiameter", 0.0);
    options.holeCutDepth = readNumberProperty(object, "HoleCutDepth", 0.0);
    options.holeCutCountersinkAngle =
        readNumberProperty(object, "HoleCutCountersinkAngle", defaultCountersinkAngle(options.threadType));
    options.drillPoint = readEnumProperty(object, "DrillPoint", {"Flat", "Angled"}, "Angled");
    options.drillPointAngle = readNumberProperty(object, "DrillPointAngle", 118.0);
    options.drillForDepth = readBoolProperty(object, "DrillForDepth");
    options.tapered = readBoolProperty(object, "Tapered");
    options.taperedAngle = readNumberProperty(object, "TaperedAngle", 90.0);
    if (!normalizeHoleToolOptions(options, object, context)) {
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> toolShape;
    if (options.holeCutType == "None" && options.drillPoint == "Flat" && !options.tapered) {
        toolShape =
            buildCylinderTool(centers, *direction, diameter / 2.0, depth, options.cutIntoMaterial, object, context);
    }
    else {
        toolShape = buildProfiledTool(centers, *direction, options, object, context);
    }
    if (!toolShape) {
        return std::nullopt;
    }

    if (threaded && modelThread) {
        const auto threadTool = buildModelThreadTool(centers,
                                                     *direction,
                                                     options,
                                                     threadModel,
                                                     threadDepth,
                                                     method,
                                                     threadDiameter->threadDiameter,
                                                     threadDiameter->threadPitch,
                                                     object,
                                                     context);
        if (!threadTool) {
            return std::nullopt;
        }
        toolShape = combineHoleAndThreadTools(*toolShape, *threadTool, object, context);
        if (!toolShape) {
            return std::nullopt;
        }
    }

    part::NamedShape toolNamedShape =
        namedShapeForHoleToolHistory(object.name, *toolShape, *profileLink, centerSources, threaded, modelThread);
    nlohmann::json historyFreeze =
        holeHistoryFreezeJson(*profileLink, *base, *toolShape, centerSources, options.holeCutType, threaded, modelThread);

    return HoleBuild{*profileLink,
                     method,
                     diameter,
                     threadDiameter->threadDiameter,
                     threadDiameter->threadPitch,
                     threadDiameter->source,
                     threadType,
                     threadDiameter->threadSize,
                     threadDiameter->threadFit,
                     threadModel.threadClass,
                     threadModel.direction,
                     threadModel.clearance,
                     threadModel.radiusClearance,
                     threadModel.useCustomClearance,
                     threadModel.customClearance,
                     threadDepth.type,
                     threadDepth.depth,
                     threadDepth.runout,
                     depth,
                     options.holeCutType,
                     options.holeCutStandard,
                     options.holeCutDefinitionSource,
                     options.drillPoint,
                     options.holeCutDiameter,
                     options.holeCutDepth,
                     options.holeCutCountersinkAngle,
                     options.drillForDepth,
                     options.tapered,
                     options.taperedAngle,
                     threaded,
                     modelThread,
                     *toolShape,
                     std::move(toolNamedShape),
                     std::move(historyFreeze)};
}

}  // namespace

void executeHole(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::findHoles()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Profile",
                                      "Threaded",
                                      "ModelThread",
                                      "CosmeticThread",
                                      "ThreadType",
                                      "ThreadSize",
                                      "ThreadClass",
                                      "ThreadFit",
                                      "ThreadDirection",
                                      "ThreadPitch",
                                      "ThreadDiameter",
                                      "ThreadDepthType",
                                      "ThreadDepth",
                                      "Diameter",
                                      "HoleCutType",
                                      "HoleCutCustomValues",
                                      "HoleCutDiameter",
                                      "HoleCutDepth",
                                      "HoleCutCountersinkAngle",
                                      "DepthType",
                                      "Depth",
                                      "DrillPoint",
                                      "DrillPointAngle",
                                      "DrillForDepth",
                                      "Tapered",
                                      "TaperedAngle",
                                      "UseCustomThreadClearance",
                                      "CustomThreadClearance",
                                      "BaseProfileType",
                                      "Reversed",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto hole = buildHole(object, context);
    if (!hole) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto holeNamedShape = hole->toolNamedShape;
    context.namedShapes[object.name] = holeNamedShape;
    context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, hole->toolShape, std::nullopt, holeNamedShape};
    context.mesh[object.name] = cad_core::part::meshForShape(hole->toolShape);
    context.subshapes[object.name] = part::subshapeMapForShape(hole->toolShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "sub"},
        {"method", hole->method},
        {"source_profile", hole->profile.object},
        {"diameter", hole->diameter},
        {"diameter_source", hole->diameterSource},
        {"thread_type", hole->threadType},
        {"thread_size", hole->threadSize},
        {"thread_fit", hole->threadFit},
        {"thread_class", hole->threadClass},
        {"thread_direction", hole->threadDirection},
        {"thread_clearance", hole->threadClearance},
        {"thread_radius_clearance", hole->threadRadiusClearance},
        {"use_custom_thread_clearance", hole->useCustomThreadClearance},
        {"custom_thread_clearance", hole->customThreadClearance},
        {"thread_diameter", hole->threadDiameter},
        {"thread_pitch", hole->threadPitch},
        {"thread_depth_type", hole->threadDepthType},
        {"thread_depth", hole->threadDepth},
        {"thread_runout", hole->threadRunout},
        {"depth", hole->depth},
        {"hole_cut_type", hole->holeCutType},
        {"hole_cut_standard", hole->holeCutStandard},
        {"hole_cut_definition_source", hole->holeCutDefinitionSource},
        {"hole_cut_diameter", hole->holeCutDiameter},
        {"hole_cut_depth", hole->holeCutDepth},
        {"hole_cut_countersink_angle", hole->holeCutCountersinkAngle},
        {"drill_point", hole->drillPoint},
        {"drill_for_depth", hole->drillForDepth},
        {"tapered", hole->tapered},
        {"tapered_angle", hole->taperedAngle},
        {"threaded", hole->threaded},
        {"model_thread", hole->modelThread},
        {"model_thread_geometry", hole->threaded && hole->modelThread ? "pipe_shell" : "none"},
        {"history", hole->historyFreeze},
        {"bbox", cad_core::part::bboxForShape(hole->toolShape)},
        {"volume", cad_core::part::volumeForShape(hole->toolShape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
}

}  // namespace cad_core::part_design
