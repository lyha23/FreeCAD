#include "cad_core/features/hole.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::features {

namespace {

constexpr int baseProfileOnPoints = 1 << 0;
constexpr int baseProfileOnCircles = 1 << 1;
constexpr int baseProfileOnArcs = 1 << 2;
constexpr int baseProfileOnCirclesArcs = baseProfileOnCircles | baseProfileOnArcs;
constexpr double pi = 3.14159265358979323846;

struct HoleBuild {
    document::Link profile;
    std::string method;
    double diameter = 0.0;
    double threadDiameter = 0.0;
    double threadPitch = 0.0;
    std::string diameterSource;
    std::string threadType;
    std::string threadSize;
    std::string threadFit;
    double depth = 0.0;
    std::string holeCutType;
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
};

struct HoleToolOptions {
    double diameter = 0.0;
    double depth = 0.0;
    std::string threadType;
    std::string holeCutType;
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

const nlohmann::json* propertyPayload(const document::DocumentObject& object, const std::string& property)
{
    const auto* value = document::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::optional<int> readIntegerProperty(const document::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr || !payload->is_number_integer()) {
        return std::nullopt;
    }
    return payload->get<int>();
}

std::string readEnumProperty(const document::DocumentObject& object,
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

double readNumberProperty(const document::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool readBoolProperty(const document::DocumentObject& object, const std::string& property, bool fallback = false)
{
    return document::readBool(object, property).value_or(fallback);
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

std::optional<ThreadDescription> threadDescriptionFor(const document::DocumentObject& object,
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

int readThreadFitIndex(const document::DocumentObject& object)
{
    if (const auto index = readIntegerProperty(object, "ThreadFit")) {
        return *index;
    }
    const auto fit = document::readString(object, "ThreadFit").value_or("Medium");
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

std::optional<ThreadDiameterResult> resolveThreadDiameter(const document::DocumentObject& object,
                                                          const std::string& threadType,
                                                          double requestedDiameter,
                                                          bool threaded,
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
    // ::Hole::determineDiameter(), for Threaded holes uses "TapDrill + clearance";
    // for non-threaded thread profiles it reads clearance diameters from the ISO 273 table.
    if (threaded) {
        if (thread->tapDrill > Precision::Confusion()) {
            result.diameter = thread->tapDrill;
            result.source = "thread_tap_drill";
            return result;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
        // ::Hole::determineDiameter(), for "BSP"/"BSW"/"BSF" uses
        // "double thread = 2 * (0.640327 * pitch)" and for "NPT" uses
        // "double thread = 2 * (0.8 * pitch)" before subtracting "thread * 0.75".
        if (isWhitworthThreadType(threadType)) {
            const double threadDepth = 2.0 * (0.640327 * thread->pitch);
            result.diameter = thread->diameter - threadDepth * 0.75;
            result.source = "thread_whitworth_fallback";
            return result;
        }
        if (threadType == "NPT") {
            const double threadDepth = 2.0 * (0.8 * thread->pitch);
            result.diameter = thread->diameter - threadDepth * 0.75;
            result.source = "thread_npt_fallback";
            return result;
        }
        result.diameter = thread->diameter - thread->pitch;
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

std::optional<TopoDS_Shape> previousSolidShape(const runtime::ComputeContext& context)
{
    for (auto it = context.executionOrder.rbegin(); it != context.executionOrder.rend(); ++it) {
        const auto shapeIt = context.shapes.find(*it);
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
            return shapeIt->second.shape;
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

int readBaseProfileType(const document::DocumentObject& object)
{
    if (const auto value = readIntegerProperty(object, "BaseProfileType")) {
        return *value;
    }
    const auto text = document::readString(object, "BaseProfileType");
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
                 const document::DocumentObject& object,
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

std::vector<gp_Pnt> holeCentersFromCircularProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), iterates profile edges, keeps GeomAbs_Circle, then filters
    // "adaptor.IsClosed()" by BaseProfileTypeOptions::OnCircles / OnArcs and uses
    // "circle.Axis().Location()" as the hole center.
    std::vector<gp_Pnt> centers;
    if ((baseProfileType & baseProfileOnCircles) == 0 && (baseProfileType & baseProfileOnArcs) == 0) {
        return centers;
    }

    for (TopExp_Explorer explorer(rawProfile, TopAbs_EDGE); explorer.More(); explorer.Next()) {
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
        centers.push_back(curve.Circle().Axis().Location());
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

std::vector<gp_Pnt> holeCentersFromPointProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), when BaseProfileTypeOptions::OnPoints is active, iterates
    // "getSubTopoShapes(TopAbs_VERTEX, TopAbs_EDGE)" so curve endpoint vertices are ignored.
    std::vector<gp_Pnt> centers;
    if ((baseProfileType & baseProfileOnPoints) == 0) {
        return centers;
    }

    for (TopExp_Explorer explorer(rawProfile, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        const TopoDS_Vertex vertex = TopoDS::Vertex(explorer.Current());
        if (vertexBelongsToEdge(vertex, rawProfile)) {
            continue;
        }
        centers.push_back(BRep_Tool::Pnt(vertex));
    }
    return centers;
}

std::vector<gp_Pnt> holeCentersFromProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    std::vector<gp_Pnt> centers = holeCentersFromCircularProfile(rawProfile, baseProfileType);
    std::vector<gp_Pnt> points = holeCentersFromPointProfile(rawProfile, baseProfileType);
    centers.insert(centers.end(), points.begin(), points.end());
    return centers;
}

std::optional<TopoDS_Shape> buildCylinderTool(const std::vector<gp_Pnt>& centers,
                                              const gp_Dir& direction,
                                              double radius,
                                              double depth,
                                              const document::DocumentObject& object,
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
        BRepPrimAPI_MakeCylinder builder(gp_Ax2(center, direction), radius, depth);
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

bool normalizeHoleToolOptions(HoleToolOptions& options,
                              const document::DocumentObject& object,
                              runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::updateHoleCutParams(), for non-threaded holes fills Counterbore with
    // "diameter * 1.6" / "diameter * 0.9" and Countersink/Counterdrill with
    // "diameter * 1.7" plus getCountersinkAngle().
    if (options.holeCutType != "None" && options.holeCutType != "Counterbore"
        && options.holeCutType != "Countersink" && options.holeCutType != "Counterdrill") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported HoleCutType " + options.holeCutType,
                               object.name,
                               "HoleCutType");
        return false;
    }
    if (options.drillPoint != "Flat" && options.drillPoint != "Angled") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported DrillPoint " + options.drillPoint,
                               object.name,
                               "DrillPoint");
        return false;
    }

    if (options.holeCutType == "Counterbore") {
        if (options.holeCutDiameter <= options.diameter) {
            options.holeCutDiameter = options.diameter * 1.6;
            options.holeCutDepth = options.diameter * 0.9;
        }
        if (options.holeCutDepth <= Precision::Confusion()) {
            options.holeCutDepth = options.diameter * 0.9;
        }
        options.holeCutCountersinkAngle = 90.0;
    }
    else if (options.holeCutType == "Countersink" || options.holeCutType == "Counterdrill") {
        if (options.holeCutDiameter <= options.diameter) {
            options.holeCutDiameter = options.diameter * 1.7;
        }
        if (options.holeCutCountersinkAngle <= Precision::Confusion()) {
            options.holeCutCountersinkAngle = defaultCountersinkAngle(options.threadType);
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
                                                      const document::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), builds a "BRepBuilderAPI_MakeWire" section for HoleCutType and
    // DrillPoint, then calls "BRepPrimAPI_MakeRevol(face, gp_Ax1(firstPoint, zDir), angle)".
    const gp_Vec axisDir(direction);
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
                                              const document::DocumentObject& object,
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

bool rejectActiveHoleGaps(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), after the revolved/cylindrical tool is built, only calls makeThread()
    // under "if (Threaded.getValue() && ModelThread.getValue())". Non-modeled Threaded holes
    // are geometric plain holes whose Diameter was already stored on the document object.
    struct Gap {
        std::string property;
        std::string message;
    };
    std::vector<Gap> gaps;
    if (readBoolProperty(object, "ModelThread")) {
        gaps.push_back({"ModelThread", "Hole model thread geometry requires FreeCAD Hole::makeThread migration"});
    }
    for (const auto& gap : gaps) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               gap.message,
                               object.name,
                               gap.property);
    }
    return gaps.empty();
}

std::optional<HoleBuild> buildHole(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), reads "Profile", "Diameter", "DepthType", "Depth", "DrillPoint",
    // computes "SketchVector = guessNormalDirection(profileshape)", then calls findHoles().
    if (document::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Hole Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto profileLink = document::readLink(object, "Profile");
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

    const auto base = previousSolidShape(context);
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
        resolveThreadDiameter(object, threadType, requestedDiameter, threaded, context);
    if (!threadDiameter) {
        return std::nullopt;
    }

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
        depth = throughAllLength(*base, rawProfile);
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
    const std::vector<gp_Pnt> centers = holeCentersFromProfile(rawProfile, baseProfileType);

    HoleToolOptions options;
    options.diameter = diameter;
    options.depth = depth;
    options.threadType = threadType;
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
        toolShape = buildCylinderTool(centers, *direction, diameter / 2.0, depth, object, context);
    }
    else {
        toolShape = buildProfiledTool(centers, *direction, options, object, context);
    }
    if (!toolShape) {
        return std::nullopt;
    }

    return HoleBuild{*profileLink,
                     method,
                     diameter,
                     threadDiameter->threadDiameter,
                     threadDiameter->threadPitch,
                     threadDiameter->source,
                     threadType,
                     threadDiameter->threadSize,
                     threadDiameter->threadFit,
                     depth,
                     options.holeCutType,
                     options.drillPoint,
                     options.holeCutDiameter,
                     options.holeCutDepth,
                     options.holeCutCountersinkAngle,
                     options.drillForDepth,
                     options.tapered,
                     options.taperedAngle,
                     threaded,
                     modelThread,
                     *toolShape};
}

}  // namespace

void executeHole(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::findHoles()
    if (!rejectUnsupportedProperties(object,
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
    if (!rejectActiveHoleGaps(object, context)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto hole = buildHole(object, context);
    if (!hole) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, hole->toolShape};
    context.mesh[object.name] = geometry::meshForShape(hole->toolShape);
    context.subshapes[object.name] = topo::subshapeMapForShape(hole->toolShape);
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
        {"thread_diameter", hole->threadDiameter},
        {"thread_pitch", hole->threadPitch},
        {"depth", hole->depth},
        {"hole_cut_type", hole->holeCutType},
        {"hole_cut_diameter", hole->holeCutDiameter},
        {"hole_cut_depth", hole->holeCutDepth},
        {"hole_cut_countersink_angle", hole->holeCutCountersinkAngle},
        {"drill_point", hole->drillPoint},
        {"drill_for_depth", hole->drillForDepth},
        {"tapered", hole->tapered},
        {"tapered_angle", hole->taperedAngle},
        {"threaded", hole->threaded},
        {"model_thread", hole->modelThread},
        {"bbox", geometry::bboxForShape(hole->toolShape)},
        {"volume", geometry::volumeForShape(hole->toolShape)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
