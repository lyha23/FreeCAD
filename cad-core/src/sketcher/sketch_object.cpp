#include "cad_core/sketcher/sketch_object.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"
#include "cad_core/sketcher/sketch_internal_result.h"
#include "cad_core/part/property_topo_shape.h"

#include "sketch_object_constraints.h"
#include "sketch_object_external.h"
#include "sketch_object_geometry.h"
#include "sketch_object_operations.h"

#include <BRepAlgoAPI_Section.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::sketcher
{

namespace
{

double readNumber3(const nlohmann::json& value, std::size_t index, bool& ok)
{
    if (!value.is_array() || value.size() != 3 || !value.at(index).is_number()) {
        ok = false;
        return 0.0;
    }
    const double number = value.at(index).get<double>();
    if (!std::isfinite(number)) {
        ok = false;
        return 0.0;
    }
    return number;
}

std::optional<gp_Pnt> readPoint3Field(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return std::nullopt;
    }
    bool ok = true;
    const double x = readNumber3(*it, 0, ok);
    const double y = readNumber3(*it, 1, ok);
    const double z = readNumber3(*it, 2, ok);
    if (!ok) {
        return std::nullopt;
    }
    return gp_Pnt(x, y, z);
}

std::optional<gp_Vec> readVector3Field(const nlohmann::json& value, const std::string& field)
{
    const auto point = readPoint3Field(value, field);
    if (!point) {
        return std::nullopt;
    }
    return gp_Vec(point->X(), point->Y(), point->Z());
}

std::optional<app::Link> readSupportLink(const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Part2DObject.h
    // says a 2D object "has a link to a supporting Face"; the property is provided by
    // Part::AttachExtension as AttachmentSupport. cad-core also accepts "Support" as
    // the fixture-facing alias used by ShapeBinder and older exported graph payloads.
    auto support = app::readLink(object, "AttachmentSupport");
    if (support) {
        return support;
    }
    return app::readLink(object, "Support");
}

std::optional<TopoDS_Face> supportFace(const app::Link& support, runtime::ComputeContext& context)
{
    if (support.subnames.empty() || support.subnames.front().rfind("Face", 0U) != 0U) {
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(support.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        return std::nullopt;
    }

    const auto face = part::subshapeByName(shapeIt->second.shape, support.subnames.front());
    if (!face || face->IsNull() || face->ShapeType() != TopAbs_FACE) {
        return std::nullopt;
    }
    return TopoDS::Face(*face);
}

std::optional<gp_Trsf> flatFaceSupportPlacement(
    const app::DocumentObject& object,
    const app::Link& support,
    runtime::ComputeContext& context
)
{
    const std::string mapMode = app::readString(object, "MapMode").value_or("FlatFace");
    if (mapMode != "FlatFace") {
        return std::nullopt;
    }

    const auto face = supportFace(support, context);
    if (!face) {
        return std::nullopt;
    }

    BRepAdaptor_Surface surface(*face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            "Sketch FlatFace support currently requires a planar Face subshape",
            object.name,
            support.property.empty() ? "Support" : support.property,
            "runtime",
            support.object + "." + support.subnames.front()
        );
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::positionBySupport(), MapMode "FlatFace" places the sketch on the
    // linked face frame; PartDesign::Hole::execute() then consumes the transformed profile.
    Bnd_Box bounds;
    BRepBndLib::Add(*face, bounds);
    if (bounds.IsVoid()) {
        return std::nullopt;
    }
    double xMin = 0.0;
    double yMin = 0.0;
    double zMin = 0.0;
    double xMax = 0.0;
    double yMax = 0.0;
    double zMax = 0.0;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    const gp_Pln plane = surface.Plane();
    gp_Dir normal = plane.Axis().Direction();
    if (face->Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }

    gp_Pnt origin(xMin, yMin, zMin);
    gp_Vec xVector(1.0, 0.0, 0.0);
    const double absX = std::abs(normal.X());
    const double absY = std::abs(normal.Y());
    const double absZ = std::abs(normal.Z());
    if (absZ >= absX && absZ >= absY) {
        origin.SetZ(normal.Z() >= 0.0 ? zMax : zMin);
        xVector = gp_Vec(1.0, 0.0, 0.0);
    }
    else if (absX >= absY) {
        origin.SetX(normal.X() >= 0.0 ? xMax : xMin);
        xVector = gp_Vec(0.0, 1.0, 0.0);
    }
    else {
        origin.SetY(normal.Y() >= 0.0 ? yMax : yMin);
        xVector = gp_Vec(1.0, 0.0, 0.0);
    }

    const gp_Vec zVector(normal);
    gp_Vec yVector = zVector.Crossed(xVector);
    if (yVector.SquareMagnitude() <= Precision::SquareConfusion()) {
        return std::nullopt;
    }
    yVector.Normalize();

    gp_Trsf placement;
    placement.SetValues(
        xVector.X(),
        yVector.X(),
        zVector.X(),
        origin.X(),
        xVector.Y(),
        yVector.Y(),
        zVector.Y(),
        origin.Y(),
        xVector.Z(),
        yVector.Z(),
        zVector.Z(),
        origin.Z()
    );
    return placement;
}

std::optional<gp_Trsf> supportPlacement(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto support = readSupportLink(object);
    if (!support) {
        return std::nullopt;
    }

    if (const auto facePlacement = flatFaceSupportPlacement(object, *support, context)) {
        return facePlacement;
    }

    const auto placementIt = context.globalPlacements.find(support->object);
    if (placementIt == context.globalPlacements.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "Sketch support " + support->object + " did not produce a placement",
            object.name,
            support->property.empty() ? "Support" : support->property,
            "runtime",
            support->object
        );
        return std::nullopt;
    }
    return placementIt->second;
}

std::optional<gp_Trsf> readSketchPlaneFramePlacement(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto* value = app::propertyValue(object, "SketchPlaneFrame");
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& frame = value->raw;
    if (!frame.is_object()
        || frame.value("PropertyType", std::string {}) != "Chili::SketchPlaneFrame") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_property_type",
            "SketchPlaneFrame must be a Chili::SketchPlaneFrame property",
            object.name,
            "SketchPlaneFrame",
            "runtime"
        );
        return std::nullopt;
    }

    const auto origin = readPoint3Field(frame, "Origin");
    const auto normalVector = readVector3Field(frame, "Normal");
    const auto xVector = readVector3Field(frame, "XDirection");
    if (!origin || !normalVector || !xVector) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_property_type",
            "SketchPlaneFrame requires numeric Origin, Normal and XDirection vectors",
            object.name,
            "SketchPlaneFrame",
            "runtime"
        );
        return std::nullopt;
    }
    if (normalVector->SquareMagnitude() <= Precision::SquareConfusion()
        || xVector->SquareMagnitude() <= Precision::SquareConfusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_property_type",
            "SketchPlaneFrame Normal and XDirection must be non-zero",
            object.name,
            "SketchPlaneFrame",
            "runtime"
        );
        return std::nullopt;
    }

    const gp_Dir normal(*normalVector);
    const gp_Vec normalUnit(normal.X(), normal.Y(), normal.Z());
    const double normalProjection = xVector->Dot(normalUnit);
    const gp_Vec projectedX(
        xVector->X() - normalProjection * normalUnit.X(),
        xVector->Y() - normalProjection * normalUnit.Y(),
        xVector->Z() - normalProjection * normalUnit.Z()
    );
    if (projectedX.SquareMagnitude() <= Precision::SquareConfusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_property_type",
            "SketchPlaneFrame Normal and XDirection must not be parallel",
            object.name,
            "SketchPlaneFrame",
            "runtime"
        );
        return std::nullopt;
    }

    const gp_Dir xDirection(projectedX);
    const gp_Vec xUnit(xDirection.X(), xDirection.Y(), xDirection.Z());
    const gp_Vec yVector = normalUnit.Crossed(xUnit);
    const gp_Dir yDirection(yVector);

    gp_Trsf placement;
    placement.SetValues(
        xDirection.X(),
        yDirection.X(),
        normal.X(),
        origin->X(),
        xDirection.Y(),
        yDirection.Y(),
        normal.Y(),
        origin->Y(),
        xDirection.Z(),
        yDirection.Z(),
        normal.Z(),
        origin->Z()
    );
    return placement;
}

gp_Dir sketchProfileNormalFromPlacement(const gp_Trsf& placement)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getProfileNormal(), for "Part::Part2DObject", multiplies the sketch
    // Placement rotation into "Base::Vector3d(0, 0, 1)".
    gp_Dir normal(0.0, 0.0, 1.0);
    normal.Transform(placement);
    return normal;
}

}  // namespace

void executeSketchObject(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/Sketcher/App/SketchObject.cpp
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Geometry",
             "Constraints",
             "Support",
             "AttachmentSupport",
             "MapMode",
             "ExternalGeometry",
             "ExternalGeo",
             "ExternalTypes",
             "SketchPlaneFrame"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Geometry") || !object.properties.at("Geometry").is_array()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Sketch Geometry must be a list",
            object.name,
            "Geometry"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto& geometry = object.properties.at("Geometry");
    SketchGeometrySet parsed;
    if (!parseSketchGeometry(geometry, object, context, parsed)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto constraintsIt = object.properties.find("Constraints");
    const nlohmann::json emptyConstraints;
    const nlohmann::json& constraints = constraintsIt == object.properties.end()
        ? emptyConstraints
        : constraintsIt.value();
    const auto appliedConstraints = applySketchConstraints(
        constraints,
        object,
        context,
        parsed.segments,
        parsed.points,
        parsed.circles,
        parsed.ellipses,
        parsed.arcs,
        parsed.ellipseArcs,
        parsed.bsplines
    );
    if (!appliedConstraints) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (solverStateBlocksProfile(appliedConstraints->solver.state)) {
        context.objects[object.name] = sketchSolverFailureObject(*appliedConstraints);
        return;
    }

    gp_Trsf placement;
    bool hasPlacement = false;
    const bool hasSketchPlaneFrame = app::propertyValue(object, "SketchPlaneFrame") != nullptr;
    const bool hasSupportProperty = app::propertyValue(object, "Support") != nullptr
        || app::propertyValue(object, "AttachmentSupport") != nullptr;
    // SketchPlaneFrame is an explicit world-space sketch plane from cad-web. It replaces
    // Support/AttachmentSupport, while App::PropertyPlacement remains a local transform
    // composed after that frame, matching the existing support * local placement order.
    if (hasSketchPlaneFrame && hasSupportProperty) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "conflicting_property",
            "SketchPlaneFrame is an explicit sketch plane and cannot be combined with Support or "
            "AttachmentSupport",
            object.name,
            "SketchPlaneFrame",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (hasSketchPlaneFrame) {
        const auto framePlacement = readSketchPlaneFramePlacement(object, context);
        if (!framePlacement) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        placement = *framePlacement;
        hasPlacement = true;
    }
    else if (const auto support = supportPlacement(object, context)) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::ProfileBased::positionByPrevious() falls back to sketch->AttachmentSupport Placement
        // when there is no previous base feature.
        placement = *support;
        hasPlacement = true;
    }
    if (app::propertyValue(object, "Placement") != nullptr) {
        const auto localPlacement = app::readPlacement(object, "Placement");
        if (!localPlacement) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_placement",
                "Sketch Placement must be an App::PropertyPlacement",
                object.name,
                "Placement",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const gp_Trsf localTransform
            = base::placementFromComponents(localPlacement->base, localPlacement->rotation);
        placement = hasPlacement ? placement * localTransform : localTransform;
        hasPlacement = true;
    }

    const auto externalGeometry
        = rebuildExternalGeometry(object, context, hasPlacement ? placement : gp_Trsf {});
    if (!externalGeometry) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<SketchSegment> resolvedSegments = parsed.segments;
    resolvedSegments.insert(
        resolvedSegments.end(),
        externalGeometry->segments.begin(),
        externalGeometry->segments.end()
    );
    std::vector<SketchPoint> resolvedPoints = parsed.points;
    resolvedPoints.insert(
        resolvedPoints.end(),
        externalGeometry->definingPoints.begin(),
        externalGeometry->definingPoints.end()
    );
    std::vector<SketchCircle> resolvedCircles = parsed.circles;
    resolvedCircles.insert(
        resolvedCircles.end(),
        externalGeometry->circles.begin(),
        externalGeometry->circles.end()
    );
    std::vector<SketchArc> resolvedArcs = parsed.arcs;
    resolvedArcs
        .insert(resolvedArcs.end(), externalGeometry->arcs.begin(), externalGeometry->arcs.end());
    std::vector<SketchEllipse> resolvedEllipses = parsed.ellipses;
    resolvedEllipses.insert(
        resolvedEllipses.end(),
        externalGeometry->ellipses.begin(),
        externalGeometry->ellipses.end()
    );
    std::vector<SketchEllipseArc> resolvedEllipseArcs = parsed.ellipseArcs;
    resolvedEllipseArcs.insert(
        resolvedEllipseArcs.end(),
        externalGeometry->ellipseArcs.begin(),
        externalGeometry->ellipseArcs.end()
    );
    std::vector<SketchHyperbolaArc> resolvedHyperbolaArcs = parsed.hyperbolaArcs;
    resolvedHyperbolaArcs.insert(
        resolvedHyperbolaArcs.end(),
        externalGeometry->hyperbolaArcs.begin(),
        externalGeometry->hyperbolaArcs.end()
    );
    std::vector<SketchParabolaArc> resolvedParabolaArcs = parsed.parabolaArcs;
    resolvedParabolaArcs.insert(
        resolvedParabolaArcs.end(),
        externalGeometry->parabolaArcs.begin(),
        externalGeometry->parabolaArcs.end()
    );
    std::vector<SketchBSpline> resolvedBSplines = parsed.bsplines;
    resolvedBSplines.insert(
        resolvedBSplines.end(),
        externalGeometry->bsplines.begin(),
        externalGeometry->bsplines.end()
    );
    std::vector<SketchInterpolatedSpline> resolvedInterpolatedSplines = parsed.interpolatedSplines;
    std::vector<SketchBezier> resolvedBeziers = parsed.beziers;
    resolvedBeziers.insert(
        resolvedBeziers.end(),
        externalGeometry->beziers.begin(),
        externalGeometry->beziers.end()
    );

    const std::vector<SketchSegment> profile = profileSegments(resolvedSegments);
    const std::vector<SketchPoint> points = profilePoints(resolvedPoints);
    const std::vector<SketchArc> arcs = profileArcs(resolvedArcs);
    const std::vector<SketchEllipseArc> ellipseArcs = profileEllipseArcs(resolvedEllipseArcs);
    const std::vector<SketchHyperbolaArc> hyperbolaArcs = profileHyperbolaArcs(resolvedHyperbolaArcs);
    const std::vector<SketchParabolaArc> parabolaArcs = profileParabolaArcs(resolvedParabolaArcs);
    const std::vector<SketchBSpline> bsplines = profileBSplines(resolvedBSplines);
    const std::vector<SketchInterpolatedSpline> interpolatedSplines =
        profileInterpolatedSplines(resolvedInterpolatedSplines);
    const std::vector<SketchBezier> beziers = profileBeziers(resolvedBeziers);
    const std::vector<SketchProfileEdge> edges
        = profileEdges(
            profile,
            arcs,
            ellipseArcs,
            hyperbolaArcs,
            parabolaArcs,
            bsplines,
            interpolatedSplines,
            beziers
        );
    const std::vector<SketchCircle> circles = profileCircles(resolvedCircles);
    const std::vector<SketchEllipse> ellipses = profileEllipses(resolvedEllipses);
    auto rawShapeBuild = buildRawSketchShape(object, context, edges, points, circles, ellipses);
    if (!rawShapeBuild) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    TopoDS_Shape rawShape = rawShapeBuild->shape;

    std::optional<TopoDS_Shape> profileShape;
    std::optional<TopoDS_Shape> internalShape;
    const ProfileFaceBuild profileFace = buildOptionalProfileFace(edges, circles, ellipses);
    if (profileFace.faceMakerFailed) {
        // FreeCAD:
        // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals() delegates split-region construction to
        // "Part::FaceMakerBuildFace"; unsupported open splitters must not silently fall
        // back to a whole-profile face.
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_profile_region",
            "Sketch open splitter geometry could not produce bounded InternalFace regions",
            object.name,
            "Geometry"
        );
    }
    if (profileFace.profileShape) {
        profileShape = *profileFace.profileShape;
    }
    if (profileFace.internalShape) {
        // This is the bounded-face subset of FreeCAD's buildInternals() path plus current
        // open-wire carry-through. Full WireJoiner ownership/history remains separate topology work.
        internalShape = *profileFace.internalShape;
    }
    const auto historyLedger = profileFace.historyLedger;

    if (hasPlacement) {
        if (!rawShape.IsNull()) {
            rawShape = base::transformShape(rawShape, placement);
        }
        if (profileShape) {
            profileShape = base::transformShape(*profileShape, placement);
        }
        if (internalShape && !internalShape->IsNull()) {
            internalShape = base::transformShape(*internalShape, placement);
        }
    }
    const RawSketchEdgeIdentityLedger rawEdgeIdentityLedger = buildRawSketchEdgeIdentityLedger(
        rawShape,
        rawShapeBuild->sourceEdges,
        rawShapeBuild->sourceEdgeIdentities,
        rawShapeBuild->sourceOrderMatchesPublishedShape
    );

    const SketchInternalResult internalResult = buildSketchInternalResult({
        object.name,
        rawShape,
        profileShape,
        sketchProfileNormalFromPlacement(hasPlacement ? placement : gp_Trsf {}),
        internalShape,
        profileFace.requiresSubshapeSelection,
        historyLedger,
        rawEdgeIdentityLedger,
    });
    context.shapes[object.name] = internalResult.shapeValue;
    if (internalResult.rawNamedShape) {
        context.namedShapes[object.name] = *internalResult.rawNamedShape;
    }
    if (internalResult.shapeValue.internalNamedShape) {
        context.namedShapes[object.name + ".InternalShape"]
            = *internalResult.shapeValue.internalNamedShape;
    }
    if (internalResult.mesh) {
        context.mesh[object.name] = *internalResult.mesh;
    }
    if (!internalResult.subshapes.empty()) {
        context.subshapes[object.name] = internalResult.subshapes;
    }

    const std::size_t rawEdgeCount = edges.size() + circles.size() + ellipses.size();
    const std::size_t profileEdgeCount = rawEdgeCount;
    const std::size_t rawPointCount = points.size();
    const std::size_t externalGeometryCount = externalGeometry->reportedGeometryCount.value_or(
        externalGeometry->segments.size() + externalGeometry->points.size()
        + externalGeometry->circles.size() + externalGeometry->arcs.size()
        + externalGeometry->ellipses.size() + externalGeometry->ellipseArcs.size()
        + externalGeometry->hyperbolaArcs.size() + externalGeometry->parabolaArcs.size()
        + externalGeometry->bsplines.size() + externalGeometry->beziers.size()
    );
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", rawShape.IsNull() ? "empty" : "occt_sketch_shape"},
        {"solver_state", solverStateName(appliedConstraints->solver.state)},
        {"solver_malformed_constraints",
         constraintIndexArray(appliedConstraints->solver.malformedConstraints)},
        {"solver_conflicting_constraints",
         constraintIndexArray(appliedConstraints->solver.conflictingConstraints)},
        {"solver_redundant_constraints",
         constraintIndexArray(appliedConstraints->solver.redundantConstraints)},
        {"solver_partially_redundant_constraints",
         constraintIndexArray(appliedConstraints->solver.partiallyRedundantConstraints)},
        {"solver_geometry_updates", appliedConstraints->solverGeometryUpdates},
        {"solver_orientation_geometry_updates", appliedConstraints->solverOrientationGeometryUpdates},
        {"solver_coordinate_geometry_updates", appliedConstraints->solverCoordinateGeometryUpdates},
        {"solver_radius_geometry_updates", appliedConstraints->solverRadiusGeometryUpdates},
        {"solver_length_geometry_updates", appliedConstraints->solverLengthGeometryUpdates},
        {"solver_arc_geometry_updates", appliedConstraints->solverArcGeometryUpdates},
        {"solver_relation_geometry_updates", appliedConstraints->solverRelationGeometryUpdates},
        {"solver_line_pair_relation_geometry_updates",
         appliedConstraints->solverLinePairRelationGeometryUpdates},
        {"solver_curve_relation_geometry_updates",
         appliedConstraints->solverCurveRelationGeometryUpdates},
        {"solver_equal_relation_geometry_updates",
         appliedConstraints->solverEqualRelationGeometryUpdates},
        {"solver_tangent_relation_geometry_updates",
         appliedConstraints->solverTangentRelationGeometryUpdates},
        {"solver_symmetric_relation_geometry_updates",
         appliedConstraints->solverSymmetricRelationGeometryUpdates},
        {"solver_symmetric_line_relation_geometry_updates",
         appliedConstraints->solverSymmetricLineRelationGeometryUpdates},
        {"solver_symmetric_center_relation_geometry_updates",
         appliedConstraints->solverSymmetricCenterRelationGeometryUpdates},
        {"solver_constraint_rank", appliedConstraints->solverConstraintRank},
        {"solver_dependent_parameter_groups", appliedConstraints->solverDependentParameterGroups},
        {"solver_blocked_dependent_parameter_groups",
         appliedConstraints->solverBlockedDependentParameterGroups},
        {"solver_dependent_parameters", appliedConstraints->solverDependentParameters},
        {"solver_geometry_update_status", solverGeometryUpdateStatus(*appliedConstraints)},
        {"solver_degrees_of_freedom",
         appliedConstraints->solverDegreesOfFreedom
             ? nlohmann::json(*appliedConstraints->solverDegreesOfFreedom)
             : nlohmann::json(nullptr)},
        {"solver_dof_status", appliedConstraints->solverDofStatus},
        {"edge_count", profileEdgeCount},
        {"raw_edge_count", rawEdgeCount},
        {"raw_point_count", rawPointCount},
        {"coincident_constraints_applied", appliedConstraints->coincident},
        {"orientation_constraints_applied", appliedConstraints->orientation},
        {"dimension_constraints_applied", appliedConstraints->dimension},
        {"relation_constraints_applied", appliedConstraints->relation},
        {"block_constraints_applied", appliedConstraints->block},
        {"external_geometry_count", externalGeometryCount},
        {"external_point_count", externalGeometry->points.size()},
        {"external_curve_count",
         externalGeometry->circles.size() + externalGeometry->arcs.size()
             + externalGeometry->ellipses.size() + externalGeometry->ellipseArcs.size()
             + externalGeometry->hyperbolaArcs.size() + externalGeometry->parabolaArcs.size()
             + externalGeometry->bsplines.size() + externalGeometry->beziers.size()},
        {"external_geometry_state_counts",
         {
             {"defining", externalGeometry->definingLinkCount},
             {"frozen", externalGeometry->frozenLinkCount},
             {"detached", externalGeometry->detachedLinkCount},
             {"missing", externalGeometry->missingLinkCount},
             {"sync", externalGeometry->syncLinkCount},
             {"recovered_missing", externalGeometry->recoveredMissingLinkCount},
         }},
    };
    for (const auto& item : internalResult.objectFields.items()) {
        context.objects[object.name][item.key()] = item.value();
    }
}

}  // namespace cad_core::sketcher
