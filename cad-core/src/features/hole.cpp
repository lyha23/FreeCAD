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

std::optional<ThreadDescription> threadDescriptionFor(const document::DocumentObject& object,
                                                      const std::string& threadType,
                                                      runtime::ComputeContext& context)
{
    if (threadType == "None") {
        return std::nullopt;
    }
    if (threadType != "ISOMetricProfile") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Hole thread table currently supports ISOMetricProfile",
                               object.name,
                               "ThreadType");
        return std::nullopt;
    }

    const std::vector<ThreadDescription>& threads = isoMetricThreads();
    const nlohmann::json* payload = propertyPayload(object, "ThreadSize");
    if (payload != nullptr && payload->is_string()) {
        const std::string requested = payload->get<std::string>();
        for (const ThreadDescription& thread : threads) {
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
    if (index < 0 || static_cast<std::size_t>(index) >= threads.size()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Hole ThreadSize index is outside the ISOMetricProfile table",
                               object.name,
                               "ThreadSize");
        return std::nullopt;
    }
    return threads[static_cast<std::size_t>(index)];
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
    if (threadType == "ISOMetricProfile" || threadType == "ISOMetricFineProfile") {
        if (fit == 1) {
            return "Fine";
        }
        if (fit == 2) {
            return "Coarse";
        }
        return "Medium";
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
        result.diameter = thread->diameter - thread->pitch;
        result.source = "thread_pitch_fallback";
        return result;
    }

    const int fit = readThreadFitIndex(object);
    result.diameter = isoMetricClearanceDiameter(thread->diameter, fit);
    result.threadFit = threadFitName(threadType, fit);
    result.source = "thread_clearance";
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
    if (!rejectActiveRefineProperty(object, context)) {
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
