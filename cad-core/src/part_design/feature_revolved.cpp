#include "cad_core/part_design/feature_revolved.h"

#include "cad_core/app/property.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part_design/profile_resolver.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Geom_Circle.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gce_MakeCirc.hxx>
#include <gce_MakeDir.hxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part_design {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct AxisSelection {
    gp_Pnt base;
    gp_Dir direction;
};

struct UpToFaceSelection {
    TopoDS_Face face;
    std::string target;
    std::string subname;
};

struct PreviousSolidSource {
    std::string owner;
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};

nlohmann::json pointToJson(const gp_Pnt& point)
{
    return nlohmann::json::array({point.X(), point.Y(), point.Z()});
}

nlohmann::json directionToJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

std::string enumNameFromIndex(double value, const std::vector<std::string>& names)
{
    const auto index = static_cast<std::size_t>(value);
    if (std::abs(value - static_cast<double>(index)) > Precision::Confusion() || index >= names.size()) {
        return {};
    }
    return names[index];
}

std::string readEnumeration(const app::DocumentObject& object,
                            const std::string& property,
                            const std::vector<std::string>& names,
                            const std::string& fallback)
{
    if (const auto value = app::readString(object, property)) {
        return *value;
    }
    if (const auto value = app::readNumber(object, property)) {
        const std::string name = enumNameFromIndex(*value, names);
        return name.empty() ? fallback : name;
    }
    return fallback;
}

std::optional<double> readAngleDegrees(const app::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& property,
                                       const std::string& featureName,
                                       double fallback)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return fallback;
    }
    const auto number = app::readNumber(object, property);
    if (!number) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_property_type",
                               featureName + " " + property + " must be numeric degrees",
                               object.name,
                               property);
        return std::nullopt;
    }
    return *number;
}

bool readBool(const app::DocumentObject& object, const std::string& property, bool fallback)
{
    const auto value = app::readBool(object, property);
    return value.value_or(fallback);
}

bool containsMethod(const std::vector<std::string>& methods, const std::string& method)
{
    return std::find(methods.begin(), methods.end(), method) != methods.end();
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

std::optional<PreviousSolidSource> previousSolidSource(const runtime::ComputeContext& context)
{
    for (auto it = context.executionOrder.rbegin(); it != context.executionOrder.rend(); ++it) {
        const auto shapeIt = context.shapes.find(*it);
        if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            continue;
        }
        std::optional<part::NamedShape> namedShape;
        const auto namedShapeIt = context.namedShapes.find(*it);
        if (namedShapeIt != context.namedShapes.end()) {
            namedShape = namedShapeIt->second;
        }
        return PreviousSolidSource {*it, shapeIt->second.shape, std::move(namedShape)};
    }
    return std::nullopt;
}

std::optional<TopoDS_Face> firstFaceOf(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return TopoDS::Face(explorer.Current());
    }
    return std::nullopt;
}

bool rejectUpToFaceBoundary(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            const std::string& featureName)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
    // ::Revolved::tryExecuteRevolved(), for "method == RevolMethod::ToFace" calls
    // "getUpToFaceFromLinkSub(upToFace, UpToFace)" and then BRepFeat-backed
    // "tryToRevolveToFace(...)" through TopoShape::makeElementRevolution().
    const auto upToFace = app::readLink(object, "UpToFace");
    if (!upToFace || upToFace->object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " Type=UpToFace requires UpToFace LinkSub",
                               object.name,
                               "UpToFace");
        return false;
    }
    const std::string subname = upToFace->subnames.empty() ? std::string {} : upToFace->subnames.front();
    if (subname.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " UpToFace must select an explicit FaceN subshape",
                               object.name,
                               "UpToFace",
                               "runtime",
                               upToFace->object);
        return false;
    }

    const auto shapeIt = context.shapes.find(upToFace->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "UpToFace target " + upToFace->object + " did not produce a shape",
                               object.name,
                               "UpToFace",
                               "runtime",
                               upToFace->object,
                               subname);
        return false;
    }

    const auto subshape = part::subshapeByName(shapeIt->second.shape, subname);
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "UpToFace target " + upToFace->object + " has no subshape " + subname,
                               object.name,
                               "UpToFace",
                               "runtime",
                               upToFace->object,
                               subname);
        return false;
    }
    if (subshape->ShapeType() != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "UpToFace must resolve to a FaceN subshape",
                               object.name,
                               "UpToFace",
                               "runtime",
                               upToFace->object,
                               subname);
        return false;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           featureName + " Type=UpToFace requires FreeCAD BRepFeat_MakeRevol support",
                           object.name,
                           "UpToFace",
                           "runtime",
                           upToFace->object,
                           subname);
    return false;
}

bool validateRevolvedMethodBoundary(const app::DocumentObject& object,
                                    runtime::ComputeContext& context,
                                    const std::string& featureName,
                                    const std::string& method,
                                    const std::vector<std::string>& validMethods)
{
    if (!containsMethod(validMethods, method)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_property_value",
                               featureName + " Type=" + method + " is not in the FreeCAD Type enum",
                               object.name,
                               "Type");
        return false;
    }

    if (method == "Angle" || method == "TwoAngles" || method == "ThroughAll") {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
    // ::Revolved::tryExecuteRevolved(), "ToFirst/ToFace/ToLast" delegate to BRepFeat_MakeRevol
    // through TopoShape::makeElementRevolution(); later resolve/build stages validate the base
    // solid, target face and BRepFeat maker result without bbox/output-order target guessing.
    return method == "UpToFirst" || method == "UpToLast" || method == "UpToFace";
}

bool axisIsParallelToProfileNormal(const AxisSelection& axis, const std::optional<gp_Dir>& normal)
{
    return normal && std::abs(axis.direction.Dot(*normal)) > (1.0 - Precision::Angular());
}

std::optional<int> parseSketchConstructionAxisIndex(const std::string& subname)
{
    if (subname.rfind("Axis", 0) != 0U || subname.size() == 4U) {
        return std::nullopt;
    }
    char* end = nullptr;
    const long value = std::strtol(subname.c_str() + 4, &end, 10);
    if (end == nullptr || *end != '\0' || value < 0) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::optional<gp_Pnt> readSketchPoint2d(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() != 2U || !value.at(0).is_number() || !value.at(1).is_number()) {
        return std::nullopt;
    }
    return gp_Pnt(value.at(0).get<double>(), value.at(1).get<double>(), 0.0);
}

std::optional<AxisSelection> sketchAxis(const std::string& subname,
                                        const std::string& objectName,
                                        const app::DocumentObject& object,
                                        runtime::ComputeContext& context)
{
    AxisSelection axis;
    axis.base = gp_Pnt(0.0, 0.0, 0.0);
    if (subname == "H_Axis") {
        axis.direction = gp_Dir(1.0, 0.0, 0.0);
    }
    else if (subname == "V_Axis") {
        axis.direction = gp_Dir(0.0, 1.0, 0.0);
    }
    else if (subname == "N_Axis") {
        axis.direction = gp_Dir(0.0, 0.0, 1.0);
    }
    else if (const auto axisIndex = parseSketchConstructionAxisIndex(subname)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::ProfileBased::getAxis(), for "AxisN" calls "sketch->getAxis(AxId)"; SketchObject.cpp
        // returns a construction line axis by index.
        const auto documentIt = context.documentObjects.find(objectName);
        if (documentIt == context.documentObjects.end()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "ReferenceAxis target " + objectName + " is missing from the document graph",
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   objectName,
                                   subname);
            return std::nullopt;
        }
        const auto geometryIt = documentIt->second->properties.find("Geometry");
        if (geometryIt == documentIt->second->properties.end() || !geometryIt->is_array()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   "ReferenceAxis target " + objectName + " has no sketch axis " + subname,
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   objectName,
                                   subname);
            return std::nullopt;
        }
        int constructionLineIndex = 0;
        bool resolved = false;
        for (const auto& item : *geometryIt) {
            if (!item.is_object() || item.value("kind", "") != "LineSegment" || !item.value("construction", false)) {
                continue;
            }
            if (constructionLineIndex++ != *axisIndex) {
                continue;
            }
            const auto startIt = item.find("start");
            const auto endIt = item.find("end");
            const auto start = startIt != item.end() ? readSketchPoint2d(*startIt) : std::nullopt;
            const auto end = endIt != item.end() ? readSketchPoint2d(*endIt) : std::nullopt;
            if (start && end && start->Distance(*end) > Precision::Confusion()) {
                axis.base = *start;
                axis.direction = gp_Dir(gp_Vec(*start, *end));
                resolved = true;
            }
            break;
        }
        if (!resolved) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   "ReferenceAxis target " + objectName + " has no sketch axis " + subname,
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   objectName,
                                   subname);
            return std::nullopt;
        }
    }
    else {
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(objectName);
    if (placementIt != context.globalPlacements.end()) {
        axis.base.Transform(placementIt->second);
        axis.direction.Transform(placementIt->second);
    }
    return axis;
}

std::optional<AxisSelection> axisFromEdge(const TopoDS_Edge& edge,
                                          const app::DocumentObject& object,
                                          runtime::ComputeContext& context,
                                          const std::string& target,
                                          const std::string& subname)
{
    try {
        BRepAdaptor_Curve curve(edge);
        AxisSelection axis;
        if (curve.GetType() == GeomAbs_Line) {
            axis.base = curve.Line().Location();
            axis.direction = curve.Line().Direction();
        }
        else if (curve.GetType() == GeomAbs_Circle) {
            axis.base = curve.Circle().Location();
            axis.direction = curve.Circle().Axis().Direction();
        }
        else {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_axis",
                                   "ReferenceAxis edge must be a straight line, circle or arc of circle",
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   target,
                                   subname);
            return std::nullopt;
        }
        return axis;
    }
    catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               std::string("ReferenceAxis edge could not be read: ")
                                   + (message != nullptr ? message : "unknown OCCT error"),
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               target,
                               subname);
        return std::nullopt;
    }
}

std::optional<TopoDS_Shape> resolveSameSketchInternalEdgeAxis(const runtime::ShapeValue& shapeValue,
                                                              const app::DocumentObject& object,
                                                              runtime::ComputeContext& context,
                                                              const std::string& target,
                                                              const std::string& subname)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getAxis(), after "AxisN" sketch-axis handling, reads selected Part::Feature
    // subelements through "refShape.getSubShape(subReferenceAxis[0].c_str())"; SketchObject.cpp
    // publishes request-local "InternalShape" ids such as "InternalEdgeN".
    const auto parsed = part::parseInternalSubshapeName(subname);
    if (!parsed || parsed->kind != TopAbs_EDGE) {
        return std::nullopt;
    }
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "ReferenceAxis subshape " + subname + " is not available on " + target,
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               target,
                               subname);
        return std::nullopt;
    }
    const auto subshape = part::subshapeByName(*shapeValue.internalShape, *parsed);
    if (!subshape || subshape->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "ReferenceAxis subshape " + subname + " is not available on " + target,
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               target,
                               subname);
        return std::nullopt;
    }
    return *subshape;
}

std::optional<AxisSelection> resolveReferenceAxis(const app::DocumentObject& object,
                                                  runtime::ComputeContext& context,
                                                  const ProfileBasedProfileSelection& profile,
                                                  const std::string& featureName)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getAxis(), accepts sketch axes "H_Axis"/"V_Axis"/"N_Axis"/"AxisN",
    // PartDesign::Line/App::Line, and support edges where curve type is "GeomAbs_Line" or
    // "GeomAbs_Circle".
    const auto referenceAxis = app::readLink(object, "ReferenceAxis");
    if (!referenceAxis || referenceAxis->object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               featureName + " ReferenceAxis must link to a sketch axis or edge",
                               object.name,
                               "ReferenceAxis");
        return std::nullopt;
    }
    const std::string subname = referenceAxis->subnames.empty() ? std::string {} : referenceAxis->subnames.front();

    if (referenceAxis->object == profile.link.object && !subname.empty()) {
        auto axis = sketchAxis(subname, referenceAxis->object, object, context);
        if (axis) {
            if (axisIsParallelToProfileNormal(*axis, profile.normal)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_axis",
                                       "Axis must not be perpendicular to the sketch plane",
                                       object.name,
                                       "ReferenceAxis",
                                       "runtime",
                                       referenceAxis->object,
                                       subname);
                return std::nullopt;
            }
            return axis;
        }
        if (subname.rfind("Axis", 0) == 0U) {
            return std::nullopt;
        }
    }

    const auto shapeIt = context.shapes.find(referenceAxis->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "ReferenceAxis target " + referenceAxis->object + " did not produce a shape",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object);
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && subname.empty()) {
        auto axis = axisFromEdge(TopoDS::Edge(shapeIt->second.shape), object, context, referenceAxis->object, subname);
        if (axis && axisIsParallelToProfileNormal(*axis, profile.normal)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_axis",
                                   "Axis must not be perpendicular to the sketch plane",
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   referenceAxis->object);
            return std::nullopt;
        }
        return axis;
    }

    TopoDS_Shape axisShape;
    if (subname.empty()) {
        axisShape = shapeIt->second.shape;
    }
    else if (const auto parsed = part::parseInternalSubshapeName(subname);
             referenceAxis->object == profile.link.object
             && shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch
             && parsed
             && parsed->kind == TopAbs_EDGE) {
        const auto subshape = resolveSameSketchInternalEdgeAxis(shapeIt->second, object, context, referenceAxis->object, subname);
        if (!subshape) {
            return std::nullopt;
        }
        axisShape = *subshape;
    }
    else if (const auto subshape = part::subshapeByName(shapeIt->second.shape, subname)) {
        axisShape = *subshape;
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "ReferenceAxis subshape " + subname + " is not available on " + referenceAxis->object,
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }

    if (axisShape.IsNull() || axisShape.ShapeType() != TopAbs_EDGE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ReferenceAxis must resolve to an edge",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }

    auto axis = axisFromEdge(TopoDS::Edge(axisShape), object, context, referenceAxis->object, subname);
    if (axis && axisIsParallelToProfileNormal(*axis, profile.normal)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "Axis must not be perpendicular to the sketch plane",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }
    return axis;
}

std::optional<gp_Pnt> profileSurfaceCenter(const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeature.cpp
    // ::findAllFacesCutBy(), "Find the centre of gravity of the face" before intersecting the
    // support with a line/circle through that center.
    GProp_GProps props;
    BRepGProp::SurfaceProperties(profile, props);
    if (props.Mass() > Precision::Confusion()) {
        return props.CentreOfMass();
    }
    return std::nullopt;
}

struct CutFaceCandidate {
    TopoDS_Face face;
    double distance = 0.0;
};

std::vector<CutFaceCandidate> findFacesCutByRevolutionAxis(const TopoDS_Shape& target,
                                                           const TopoDS_Shape& profile,
                                                           const gp_Ax1& axis)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeature.cpp
    // ::findAllFacesCutBy(shape, face, gp_Ax1), projects the profile COG to the rotation axis,
    // builds a "Geom_Circle(..., radius)", intersects support faces, then stores "angle * radius".
    std::vector<CutFaceCandidate> result;
    const auto centerOfGravity = profileSurfaceCenter(profile);
    if (!centerOfGravity) {
        return result;
    }

    gp_XYZ relative = centerOfGravity->XYZ();
    relative.Subtract(axis.Location().XYZ());
    const double parameter = relative.Dot(axis.Direction().XYZ());
    gp_XYZ projected = axis.Location().XYZ();
    projected.Add(axis.Direction().XYZ() * parameter);
    gp_Pnt center(projected);

    const gp_Lin line(axis);
    const double radius = line.Distance(*centerOfGravity);
    if (radius < Precision::Confusion()) {
        return result;
    }

    Handle(Geom_Circle) circle = new Geom_Circle(gce_MakeCirc(center, axis.Direction(), radius));
    GeomAdaptor_Curve adaptor(circle);

    gp_Dir vx(gp_Vec(center, *centerOfGravity));
    gp_Ax2 rhs(center, axis.Direction(), vx);
    gp_Ax3 localCoordinateSystem(rhs);
    gp_Trsf toLocal;
    toLocal.SetTransformation(localCoordinateSystem);

    BRepIntCurveSurface_Inter intersection;
    for (intersection.Init(target, adaptor, Precision::Confusion()); intersection.More(); intersection.Next()) {
        const gp_Pnt point = intersection.Pnt();
        if (centerOfGravity->SquareDistance(point) < Precision::Confusion()) {
            continue;
        }

        gce_MakeDir pointDirection(*centerOfGravity, point);
        if (!pointDirection.IsDone() || pointDirection.Value().IsParallel(axis.Direction(), Precision::Confusion())) {
            continue;
        }

        const gp_Pnt localPoint = point.Transformed(toLocal);
        const double x = std::clamp(localPoint.X(), -radius, radius);
        double angle = std::acos(x / radius);
        if (localPoint.Y() < 0.0) {
            angle = 2.0 * kPi - angle;
        }
        result.push_back(CutFaceCandidate {intersection.Face(), angle * radius});
    }
    return result;
}

bool validateRevolvedUpToFace(const app::DocumentObject& object,
                              runtime::ComputeContext& context,
                              const TopoDS_Face& face,
                              const gp_Ax1& axis,
                              const std::string& property,
                              const std::string& target = {},
                              const std::string& subname = {})
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() == GeomAbs_Plane
        && axis.Direction().IsParallel(surface.Plane().Axis().Direction(), Precision::Confusion())) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Up-to face must not be normal to rotation axis",
                               object.name,
                               property,
                               "runtime",
                               target,
                               subname);
        return false;
    }
    return true;
}

std::optional<UpToFaceSelection> resolveUpToFaceLink(const app::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const std::string& property)
{
    if (!app::hasPropertyType(object, property, "App::PropertyLinkSub")) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " Type requires an App::PropertyLinkSub " + property + " property",
                               object.name,
                               property);
        return std::nullopt;
    }

    const auto link = app::readLink(object, property);
    if (!link || link->object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " must link to a target face",
                               object.name,
                               property);
        return std::nullopt;
    }
    if (link->subnames.size() != 1U || link->subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference exactly one FaceN subshape",
                               object.name,
                               property,
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    const std::string& subname = link->subnames.front();
    const auto parsed = part::parseSubshapeName(subname);
    if (!parsed || parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " requires a face subshape",
                               object.name,
                               property,
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link->object + " did not produce a shape",
                               object.name,
                               property,
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    const auto namedShapeIt = context.namedShapes.find(link->object);
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, subname);
    }
    if (!subshape) {
        subshape = part::subshapeByName(shapeIt->second.shape, subname);
    }
    if (!subshape || subshape->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + link->object + " has no subshape " + subname,
                               object.name,
                               property,
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }
    return UpToFaceSelection {TopoDS::Face(*subshape), link->object, subname};
}

std::optional<UpToFaceSelection> resolveRevolvedUpToFace(const app::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const ProfileBasedProfileSelection& profile,
                                                         const AxisSelection& axisSelection,
                                                         const std::string& method,
                                                         const std::string& featureName)
{
    gp_Ax1 axis(axisSelection.base, axisSelection.direction);
    const bool reversed = readBool(object, "Reversed", false);
    if (reversed) {
        axis.Reverse();
    }

    if (method == "UpToFace") {
        auto selection = resolveUpToFaceLink(object, context, "UpToFace");
        if (!selection) {
            return std::nullopt;
        }
        if (!validateRevolvedUpToFace(object, context, selection->face, axis, "UpToFace", selection->target, selection->subname)) {
            return std::nullopt;
        }
        return selection;
    }

    const auto base = previousSolidShape(context);
    if (!base) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               featureName + " " + method + " requires a previous base solid",
                               object.name,
                               "Type");
        return std::nullopt;
    }

    auto candidates = findFacesCutByRevolutionAxis(*base, profile.shape, axis);
    if (candidates.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "No faces found in this direction",
                               object.name,
                               "Type");
        return std::nullopt;
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return left.distance < right.distance;
    });
    const auto& selected = method == "UpToLast" ? candidates.back() : candidates.front();
    if (!validateRevolvedUpToFace(object, context, selected.face, axis, "Type")) {
        return std::nullopt;
    }
    return UpToFaceSelection {selected.face, {}, method};
}

TopoDS_Shape rotatedShape(const TopoDS_Shape& shape, const gp_Ax1& axis, double angle)
{
    gp_Trsf transform;
    transform.SetRotation(axis, angle);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

std::optional<part::NamedShapeBuild> buildRevolvedTool(const app::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const ProfileBasedProfileSelection& profile,
                                                       const AxisSelection& axisSelection,
                                                       double angleRadians,
                                                       double angleOffsetRadians,
                                                       bool reversed,
                                                       bool profileWasMoved)
{
    gp_Ax1 axis(axisSelection.base, axisSelection.direction);
    if (reversed) {
        axis.Reverse();
    }

    TopoDS_Shape sourceShape = profile.shape;
    if (std::abs(angleOffsetRadians) > Precision::Angular()) {
        sourceShape = rotatedShape(sourceShape, axis, angleOffsetRadians);
    }

    part::NamedShapeSource source{profile.link.object, sourceShape};
    const auto namedShapeIt = context.namedShapes.find(profile.link.object);
    if (namedShapeIt != context.namedShapes.end() && !profileWasMoved) {
        source.namedShape = &namedShapeIt->second;
    }
    return part::makeElementRevolveFromSource(object.name, source, axis, angleRadians);
}

std::optional<part::NamedShapeBuild> buildRevolvedUntil(const app::DocumentObject& object,
                                                        runtime::ComputeContext& context,
                                                        const ProfileBasedProfileSelection& profile,
                                                        const AxisSelection& axisSelection,
                                                        const UpToFaceSelection& upToFace,
                                                        RevolvedAddSubMode mode,
                                                        const std::string& featureName)
{
    const auto base = previousSolidSource(context);
    if (!base) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               featureName + " UpTo requires a previous base solid",
                               object.name,
                               "Type");
        return std::nullopt;
    }

    gp_Ax1 axis(axisSelection.base, axisSelection.direction);
    if (readBool(object, "Reversed", false)) {
        axis.Reverse();
    }

    const auto profileFace = firstFaceOf(profile.shape);
    if (!profileFace) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               featureName + " Profile must provide a face for UpTo revolution",
                               object.name,
                               "Profile",
                               "runtime",
                               profile.link.object);
        return std::nullopt;
    }

    part::NamedShapeSource baseSource {base->owner, base->shape, base->namedShape ? &*base->namedShape : nullptr};
    part::NamedShapeSource profileSource {profile.link.object, profile.shape};
    const auto profileNamedShapeIt = context.namedShapes.find(profile.link.object);
    if (profileNamedShapeIt != context.namedShapes.end()) {
        profileSource.namedShape = &profileNamedShapeIt->second;
    }
    const int revolMode = mode == RevolvedAddSubMode::Additive ? 1 : 0;
    auto build = part::makeElementRevolutionUntilFromSources(
        object.name,
        baseSource,
        profileSource,
        axis,
        TopoDS_Face {},
        upToFace.face,
        revolMode,
        true
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build.error.empty() ? featureName + " UpTo revolution failed" : build.error,
                               object.name,
                               upToFace.target.empty() ? "Type" : "UpToFace",
                               "runtime",
                               upToFace.target,
                               upToFace.subname);
        return std::nullopt;
    }
    return build;
}

}  // namespace

void executeRevolvedFeature(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            RevolvedAddSubMode mode,
                            const std::string& featureName)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp
    // ::Revolution::execute() calls executeRevolved(Part::RevolMode::FuseWithBase);
    // FeatureGroove.cpp::Groove::execute() calls executeRevolved(Part::RevolMode::CutFromBase).
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"Profile",
                                               "Type",
                                               "Angle",
                                               "Angle2",
                                               "ReferenceAxis",
                                               "Base",
                                               "Axis",
                                               "Reversed",
                                               "Midplane",
                                               "UpToFace",
                                               "UpToFace2",
                                               "BaseFeature",
                                               "Refine",
                                               "FuzzyTolerance",
                                               "FuseOrder"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<std::string> revolutionTypes {"Angle", "UpToLast", "UpToFirst", "UpToFace", "TwoAngles"};
    const std::vector<std::string> grooveTypes {"Angle", "ThroughAll", "UpToFirst", "UpToFace", "TwoAngles"};
    const auto& validTypes = featureName == "Groove" ? grooveTypes : revolutionTypes;
    const std::string method = readEnumeration(
        object,
        "Type",
        validTypes,
        "Angle"
    );
    if (!validateRevolvedMethodBoundary(object, context, featureName, method, validTypes)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    std::string fuseOrder = "BaseFirst";
    runtime::AddSubShape::AdditiveFuseOrder additiveFuseOrder =
        runtime::AddSubShape::AdditiveFuseOrder::BaseFirst;
    if (featureName == "Revolution") {
        fuseOrder = readEnumeration(object, "FuseOrder", {"BaseFirst", "FeatureFirst"}, "BaseFirst");
        if (fuseOrder == "FeatureFirst") {
            additiveFuseOrder = runtime::AddSubShape::AdditiveFuseOrder::FeatureFirst;
        }
    }

    const auto angleDegrees = readAngleDegrees(object, context, "Angle", featureName, 360.0);
    if (!angleDegrees) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (*angleDegrees > 360.0) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Angle of revolution too large",
                               object.name,
                               "Angle");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (method == "Angle" && std::abs(*angleDegrees) <= Precision::Angular()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Angle of revolution too small",
                               object.name,
                               "Angle");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    double angle2Degrees = 0.0;
    if (method == "TwoAngles") {
        const auto angle2 = readAngleDegrees(object, context, "Angle2", featureName, 0.0);
        if (!angle2) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        angle2Degrees = *angle2;
        const double angleTotalRadians = (*angleDegrees + angle2Degrees) * kPi / 180.0;
        if (std::abs(angleTotalRadians) < Precision::Angular()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_angle",
                                   "Angles of revolution nullify each other",
                                   object.name,
                                   "Angle2");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
    }

    const auto profile = resolveProfileBasedProfile(object, context, featureName);
    if (!profile) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto axis = resolveReferenceAxis(object, context, *profile, featureName);
    if (!axis) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const bool reversed = readBool(object, "Reversed", false);
    const bool midplane = readBool(object, "Midplane", false);
    double angleRadians = *angleDegrees * kPi / 180.0;
    double angleOffsetRadians = midplane ? -angleRadians / 2.0 : 0.0;
    if (method == "TwoAngles") {
        const double angle2Radians = angle2Degrees * kPi / 180.0;
        angleRadians += angle2Radians;
        angleOffsetRadians = -angle2Radians;
    }
    else if (method == "ThroughAll") {
        angleRadians = 2.0 * kPi;
    }
    const bool profileWasMoved = std::abs(angleOffsetRadians) > Precision::Angular();
    const bool upToMethod = method == "UpToFirst" || method == "UpToLast" || method == "UpToFace";
    std::optional<UpToFaceSelection> upToFace;
    std::optional<part::NamedShapeBuild> build;
    if (upToMethod) {
        upToFace = resolveRevolvedUpToFace(object, context, *profile, *axis, method, featureName);
        if (!upToFace) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        build = buildRevolvedUntil(object, context, *profile, *axis, *upToFace, mode, featureName);
    }
    else {
        build = buildRevolvedTool(
            object,
            context,
            *profile,
            *axis,
            angleRadians,
            angleOffsetRadians,
            reversed,
            profileWasMoved
        );
    }
    if (!build || !build->error.empty() || build->shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build && !build->error.empty() ? build->error : "Could not revolve the sketch",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<part::NamedShape> namedShape = build->namedShape;
    runtime::RefineShapeResult shapeResult{build->shape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, build->shape, namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        shapeResult = *refined;
    }

    const TopoDS_Shape tool = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.mesh[object.name] = cad_core::part::meshForShape(tool);
    context.subshapes[object.name] = part::subshapeMapForShape(tool);

    if (upToMethod) {
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, tool};
    }
    else if (mode == RevolvedAddSubMode::Additive) {
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, tool};
        runtime::AddSubShape addSubShape{tool, std::nullopt, namedShape, std::nullopt};
        addSubShape.additiveFuseOrder = additiveFuseOrder;
        context.addSubShapes[object.name] = std::move(addSubShape);
    }
    else {
        context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, tool, std::nullopt, namedShape};
    }

    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", mode == RevolvedAddSubMode::Additive ? "add" : "sub"},
        {"method", method},
        {"source_profile", profile->link.object},
        {"angle", *angleDegrees},
        {"angle_total", angleRadians * 180.0 / kPi},
        {"axis_base", pointToJson(axis->base)},
        {"axis_direction", directionToJson(axis->direction)},
        {"bbox", cad_core::part::bboxForShape(tool)},
        {"volume", cad_core::part::volumeForShape(tool)},
        {"topo_naming_history", upToMethod ? "maker_history:brepfeat_revolution" : "maker_history:revolve"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (upToMethod) {
        result["body_mode"] = "replace";
        if (upToFace && !upToFace->target.empty()) {
            result["up_to_target"] = upToFace->target;
            result["up_to_subname"] = upToFace->subname;
        }
    }
    if (featureName == "Revolution") {
        result["fuse_order"] = fuseOrder;
    }
    if (reversed) {
        result["reversed"] = true;
    }
    if (midplane) {
        result["midplane"] = true;
    }
    if (method == "TwoAngles") {
        result["angle2"] = angle2Degrees;
        result["angle_offset"] = angleOffsetRadians * 180.0 / kPi;
    }
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::part_design
