#include "cad_core/sketcher/sketch_object.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/app/element_map.h"
#include "cad_core/part/topo_shape.h"
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
    std::vector<SketchBSpline> resolvedBSplines = parsed.bsplines;
    resolvedBSplines.insert(
        resolvedBSplines.end(),
        externalGeometry->bsplines.begin(),
        externalGeometry->bsplines.end()
    );
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
    const std::vector<SketchBSpline> bsplines = profileBSplines(resolvedBSplines);
    const std::vector<SketchBezier> beziers = profileBeziers(resolvedBeziers);
    const std::vector<SketchProfileEdge> edges
        = profileEdges(profile, arcs, ellipseArcs, bsplines, beziers);
    const std::vector<SketchCircle> circles = profileCircles(resolvedCircles);
    const std::vector<SketchEllipse> ellipses = profileEllipses(resolvedEllipses);
    auto rawShape = buildRawSketchShape(object, context, edges, points, circles, ellipses);
    if (!rawShape) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

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
    const auto faceMakerHistory = profileFace.faceMakerHistory;
    const auto wireJoinerLedger = profileFace.wireJoinerLedger;
    const auto wireJoinerHistory = profileFace.wireJoinerHistory;

    if (hasPlacement) {
        if (!rawShape->IsNull()) {
            rawShape = base::transformShape(*rawShape, placement);
        }
        if (profileShape) {
            profileShape = base::transformShape(*profileShape, placement);
        }
        if (internalShape && !internalShape->IsNull()) {
            internalShape = base::transformShape(*internalShape, placement);
        }
    }

    runtime::ShapeValue shapeValue {runtime::ShapeValue::Kind::Sketch, *rawShape};
    shapeValue.profileShape = profileShape;
    shapeValue.profileNormal = sketchProfileNormalFromPlacement(hasPlacement ? placement : gp_Trsf {});
    shapeValue.internalShape = internalShape;
    shapeValue.profileRequiresSubshapeSelection = profileFace.requiresSubshapeSelection;
    const bool hasNonEmptyInternalShape = internalShape && !internalShape->IsNull();
    if (hasNonEmptyInternalShape) {
        std::optional<part::SketchInternalHistoryContext> internalHistoryContext;
        if (faceMakerHistory || wireJoinerHistory) {
            // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
            // ::FaceMaker::postBuild(), consumes "MapperHistory(myPreSplitHistory)" and
            // "MapperMaker(mySplitter)" before SketchObject::getInternalElementMap() exposes
            // the request-local InternalShape. Store the FaceMakerBuildFace summary next to the
            // InternalShape NamedShape so topo consumers can see which maker-history stages
            // backed the generated/split/deleted element history.
            internalHistoryContext = part::SketchInternalHistoryContext {};
            if (faceMakerHistory) {
                internalHistoryContext->sourceEdgeCount = faceMakerHistory->sourceEdgeCount;
                internalHistoryContext->preSplitEdgeCount = faceMakerHistory->preSplitEdgeCount;
                internalHistoryContext->splitterEdgeCount = faceMakerHistory->splitterEdgeCount;
                internalHistoryContext->boundedFaceCount = faceMakerHistory->boundedFaceCount;
                internalHistoryContext->preSplitHistory = faceMakerHistory->preSplitHistory;
                internalHistoryContext->splitterHistory = faceMakerHistory->splitterHistory;
                for (const part::FaceMakerEdgeHistoryEvidence& entry :
                     faceMakerHistory->edgeEvidence) {
                    part::SketchInternalFaceMakerEdgeEvidence topoEntry;
                    topoEntry.makerStage = entry.makerStage;
                    topoEntry.relation = entry.relation;
                    topoEntry.sourceEdgeIndex = entry.sourceEdgeIndex;
                    topoEntry.targetEdgeIndex = entry.targetEdgeIndex;
                    topoEntry.targetEdge = entry.targetEdge;
                    topoEntry.preSplitHistory = entry.preSplitHistory;
                    topoEntry.splitterHistory = entry.splitterHistory;
                    internalHistoryContext->faceMakerEdgeEvidence.push_back(std::move(topoEntry));
                }
                for (const part::FaceMakerBoundedFaceHistoryEvidence& entry :
                     faceMakerHistory->boundedFaceEvidence) {
                    part::SketchInternalFaceMakerBoundedFaceEvidence topoEntry;
                    topoEntry.boundedFaceIndex = entry.boundedFaceIndex;
                    topoEntry.face = entry.face;
                    topoEntry.sourceEdgeIndices = entry.sourceEdgeIndices;
                    topoEntry.outerBoundaryTargetEdgeIndices = entry.outerBoundaryTargetEdgeIndices;
                    for (const part::FaceMakerBoundedFaceBoundaryEvidence& boundary :
                         entry.outerBoundary) {
                        part::SketchInternalFaceMakerBoundedFaceBoundaryEvidence topoBoundary;
                        topoBoundary.sourceEdgeIndex = boundary.sourceEdgeIndex;
                        topoBoundary.targetEdgeIndex = boundary.targetEdgeIndex;
                        topoBoundary.makerStage = boundary.makerStage;
                        topoBoundary.relation = boundary.relation;
                        topoBoundary.targetEdge = boundary.targetEdge;
                        topoEntry.outerBoundary.push_back(std::move(topoBoundary));
                    }
                    internalHistoryContext->faceMakerBoundedFaceEvidence.push_back(
                        std::move(topoEntry)
                    );
                }
            }
            if (wireJoinerHistory) {
                // FreeCAD:
                // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::getOpenWires(), calls makeShapeWithElementMap(...,
                // MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op).
                // This passes only WireJoiner-produced history summary into topo; topo must not
                // infer WireJoiner split/generated/deleted history from raw/internal geometry.
                internalHistoryContext->wireJoinerSourceEdgeCount = wireJoinerHistory->sourceEdgeCount;
                internalHistoryContext->wireJoinerSplitResultEdgeCount
                    = wireJoinerHistory->splitResultEdgeCount;
                for (const part::WireJoinerHistoryEvent& event : wireJoinerHistory->historyEvents) {
                    part::SketchInternalWireJoinerHistoryEvent topoEvent;
                    topoEvent.eventIndex = event.eventIndex;
                    topoEvent.openExportIndex = event.openExportIndex;
                    topoEvent.edgeInfoIndex = event.edgeInfoIndex;
                    topoEvent.openWireCompoundChildWireInfoIndex =
                        event.openWireCompoundChildWireInfoIndex;
                    topoEvent.relation = part::wireJoinerHistoryRelationName(event.relation);
                    topoEvent.relationFromChildWireLedger = event.relationFromChildWireLedger;
                    topoEvent.sourceEdgeIndices = event.sourceEdgeIndices;
                    topoEvent.sourceLineageFromSplitterHistory =
                        event.sourceLineageFromSplitterHistory;
                    topoEvent.noOriginalPurgedByLedger = event.noOriginalPurgedByLedger;
                    topoEvent.splitFragmentFromModifiedHistory =
                        event.splitFragmentFromModifiedHistory;
                    topoEvent.splitFragmentFromGeneratedHistory =
                        event.splitFragmentFromGeneratedHistory;
                    internalHistoryContext->wireJoinerHistoryEvents.push_back(
                        std::move(topoEvent)
                    );
                }
                for (const part::WireJoinerOpenExportHistoryEntry& entry :
                     wireJoinerHistory->openExportEntries) {
                    part::SketchInternalWireJoinerOpenExportHistoryEntry topoEntry;
                    topoEntry.openExportIndex = entry.openExportIndex;
                    topoEntry.edgeInfoIndex = entry.edgeInfoIndex;
                    topoEntry.openExportWire = entry.openExportWire;
                    topoEntry.openExportEdge = entry.openExportEdge;
                    topoEntry.wireJoinerHistoryRelation =
                        entry.historyRelationFromChildWireLedger
                        ? part::wireJoinerHistoryRelationName(entry.historyRelation)
                        : std::string();
                    topoEntry.wireJoinerHistoryRelationFromChildWireLedger =
                        entry.historyRelationFromChildWireLedger;
                    topoEntry.wireJoinerHistoryEventIndex =
                        entry.wireJoinerHistoryEventIndex;
                    topoEntry.wireJoinerHistoryEventFromChildWireLedger =
                        entry.wireJoinerHistoryEventFromChildWireLedger;
                    topoEntry.resultWireProducerKind = part::resultWireProducerKindName(
                        entry.resultWireProducer.kind
                    );
                    topoEntry.resultWireProducerState = part::resultWireProducerStateName(
                        entry.resultWireProducer.state
                    );
                    topoEntry.resultWireProducerBlocker = part::resultWireBlockerName(
                        entry.resultWireProducer.blocker
                    );
                    topoEntry.resultWireProducerSourceEdgeInfoIndex
                        = entry.resultWireProducer.sourceEdgeInfoIndex;
                    topoEntry.resultWireProducerRootEdgeInfoIndex
                        = entry.resultWireProducer.rootEdgeInfoIndex;
                    topoEntry.resultWireProducerCurrentMemberEdgeInfoIndex
                        = entry.resultWireProducer.currentMemberEdgeInfoIndex;
                    topoEntry.resultWireProducerChildWireInfoIndex
                        = entry.resultWireProducer.childWireInfoIndex;
                    topoEntry.openWireCompoundChildWireInfoIndex =
                        entry.openWireCompoundChildWireInfoIndex;
                    topoEntry.openWireCompoundExportSource =
                        part::openWireCompoundExportSourceName(
                            entry.openWireCompoundExportSource
                        );
                    topoEntry.openWireCompoundEdgeInfoIteration =
                        entry.openWireCompoundEdgeInfoIteration;
                    topoEntry.openWireCompoundEdgeInfoIteration2 =
                        entry.openWireCompoundEdgeInfoIteration2;
                    topoEntry.openWireCompoundOwnerWireInfo =
                        entry.openWireCompoundOwnerWireInfo;
                    topoEntry.openWireCompoundOwnerWireInfo2 =
                        entry.openWireCompoundOwnerWireInfo2;
                    topoEntry.openWireCompoundOpenLeafExport =
                        entry.openWireCompoundOpenLeafExport;
                    topoEntry.openWireCompoundUnownedOpenEdgeExport =
                        entry.openWireCompoundUnownedOpenEdgeExport;
                    topoEntry.openWireCompoundRootCurrentMemberChildProducer =
                        entry.openWireCompoundRootCurrentMemberChildProducer;
                    topoEntry.openWireCompoundChildShapeIdentityRecorded =
                        entry.openWireCompoundChildShapeIdentityRecorded;
                    topoEntry.openWireCompoundChildWireEdgeCount =
                        entry.openWireCompoundChildWireEdgeCount;
                    topoEntry.openWireCompoundChildWireVertexCount =
                        entry.openWireCompoundChildWireVertexCount;
                    topoEntry.openWireCompoundSourceEdgeIndices =
                        entry.openWireCompoundSourceEdgeIndices;
                    topoEntry.openWireCompoundSourceLineageFromSplitterHistory =
                        entry.openWireCompoundSourceLineageFromSplitterHistory;
                    topoEntry.openWireCompoundNoOriginalPurgeMatch =
                        entry.openWireCompoundNoOriginalPurgeMatch;
                    topoEntry.openWireCompoundNoOriginalPurgedByLedger =
                        entry.openWireCompoundNoOriginalPurgedByLedger;
                    topoEntry.openWireCompoundNoOriginalSharedSourceLedgerRecorded =
                        entry.openWireCompoundNoOriginalSharedSourceLedgerRecorded;
                    topoEntry.openWireCompoundNoOriginalSharedSourceEdgeCount =
                        entry.openWireCompoundNoOriginalSharedSourceEdgeCount;
                    topoEntry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount =
                        entry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount;
                    topoEntry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount =
                        entry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount;
                    topoEntry.openWireCompoundProducerLedgerWireBuilt =
                        entry.openWireCompoundProducerLedgerWireBuilt;
                    topoEntry.openWireCompoundProducerLedgerWireFromSourceVmap =
                        entry.openWireCompoundProducerLedgerWireFromSourceVmap;
                    topoEntry.openWireCompoundSourceVmapEndpointLedgerRecorded =
                        entry.openWireCompoundSourceVmapEndpointLedgerRecorded;
                    topoEntry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount =
                        entry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount;
                    topoEntry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount =
                        entry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount;
                    topoEntry.openWireCompoundEndpointProvenanceRecorded =
                        entry.openWireCompoundEndpointProvenanceRecorded;
                    topoEntry.openWireCompoundEndpointProvenanceOutputVertexCount =
                        entry.openWireCompoundEndpointProvenanceOutputVertexCount;
                    topoEntry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount =
                        entry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount;
                    topoEntry
                        .openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount =
                        entry
                            .openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount;
                    topoEntry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount =
                        entry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount;
                    topoEntry.openWireCompoundEndpointProvenanceUnmatchedVertexCount =
                        entry.openWireCompoundEndpointProvenanceUnmatchedVertexCount;
                    topoEntry.openWireCompoundVmapReplacementEventCount =
                        entry.openWireCompoundVmapReplacementEventCount;
                    for (const part::WireJoinerVmapReplacementEvent& event :
                         entry.openWireCompoundVmapReplacementEvents) {
                        part::SketchInternalWireJoinerVmapReplacementEvent topoEvent;
                        topoEvent.eventIndex = event.eventIndex;
                        topoEvent.affectedSourceEdgeIndex = event.affectedSourceEdgeIndex;
                        topoEvent.affectedChildWireEdgeInfoIndex =
                            event.affectedChildWireEdgeInfoIndex;
                        topoEvent.affectedEndpoint = event.affectedEndpoint;
                        topoEvent.affectedSourceEndpoint = event.affectedSourceEndpoint;
                        topoEvent.affectedChildWireEndpoint = event.affectedChildWireEndpoint;
                        topoEvent.replacementSourceEdgeIndex =
                            event.replacementSourceEdgeIndex;
                        topoEvent.replacementSourceEndpoint =
                            event.replacementSourceEndpoint;
                        topoEvent.replacementFromMutableSourceEdgeLedger =
                            event.replacementFromMutableSourceEdgeLedger;
                        topoEvent.replacementFromSplitFragmentLedger =
                            event.replacementFromSplitFragmentLedger;
                        topoEntry.openWireCompoundVmapReplacementEvents.push_back(
                            std::move(topoEvent)
                        );
                    }
                    topoEntry.openWireCompoundCurrentMemberProducerOutput =
                        entry.openWireCompoundCurrentMemberProducerOutput;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate =
                        entry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded =
                        entry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount;
                    topoEntry
                        .openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount =
                        entry
                            .openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount;
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount =
                        entry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount;
                    for (const part::WireJoinerEndpointIdentityDebt& debt :
                         entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt) {
                        part::SketchInternalWireJoinerEndpointIdentityDebt topoDebt;
                        topoDebt.outputVertexIndex = debt.outputVertexIndex;
                        topoDebt.matchedMemberSplitLedger = debt.matchedMemberSplitLedger;
                        topoDebt.matchedCandidateLedger = debt.matchedCandidateLedger;
                        topoDebt.currentChildWireOutputVertexMatchesOtherOutput =
                            debt.currentChildWireOutputVertexMatchesOtherOutput;
                        topoDebt.candidateWireVertexMatchesOtherOutput =
                            debt.candidateWireVertexMatchesOtherOutput;
                        topoDebt.explanation = debt.explanation;
                        topoDebt.currentChildWireOutputVertexIdentity =
                            debt.currentChildWireOutputVertexIdentity;
                        topoDebt.memberSplitLedgerVertexIdentity =
                            debt.memberSplitLedgerVertexIdentity;
                        topoDebt.candidateWireVertexIdentity = debt.candidateWireVertexIdentity;
                        topoDebt.mismatchReason = debt.mismatchReason;
                        topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt.push_back(
                            std::move(topoDebt)
                        );
                    }
                    topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked =
                        entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked;
                    topoEntry.missingOpenWireCompoundChildWire =
                        entry.missingOpenWireCompoundChildWire;
                    topoEntry.sourceEdgeIndices = entry.sourceEdgeIndices;
                    topoEntry.sourceLineageFromSplitterHistory = entry.sourceLineageFromSplitterHistory;
                    topoEntry.splitFragmentSourceEdgeIndices = entry.splitFragmentSourceEdgeIndices;
                    topoEntry.splitFragmentModifiedSourceEdgeIndices =
                        entry.splitFragmentModifiedSourceEdgeIndices;
                    topoEntry.splitFragmentGeneratedSourceEdgeIndices =
                        entry.splitFragmentGeneratedSourceEdgeIndices;
                    topoEntry.splitFragmentFromModifiedHistory =
                        entry.splitFragmentFromModifiedHistory;
                    topoEntry.splitFragmentFromGeneratedHistory =
                        entry.splitFragmentFromGeneratedHistory;
                    topoEntry.splitFragmentSourceLineageFromIdentityFallback =
                        entry.splitFragmentSourceLineageFromIdentityFallback;
                    topoEntry.splitFragmentSourceLineageFromSourceIdentityFallback =
                        entry.splitFragmentSourceLineageFromSourceIdentityFallback;
                    topoEntry.splitFragmentHistoryShapeGeometryBridge =
                        entry.splitFragmentHistoryShapeGeometryBridge;
                    topoEntry.sourceVertexIdentity = entry.sourceVertexIdentity;
                    topoEntry.sourceVertexReplacementSourceEdgeIndices
                        = entry.sourceVertexReplacementSourceEdgeIndices;
                    topoEntry.sourceVertexReplacementEndpoints
                        = entry.sourceVertexReplacementEndpoints;
                    topoEntry.sourceVertexReplacementIdentity
                        = entry.sourceVertexReplacementIdentity;
                    internalHistoryContext->wireJoinerOpenExportHistoryEntries.push_back(
                        std::move(topoEntry)
                    );
                }
                internalHistoryContext->wireJoinerModifiedSourceEdgeCount
                    = wireJoinerHistory->modifiedSourceEdgeCount;
                internalHistoryContext->wireJoinerModifiedHistoryCount
                    = wireJoinerHistory->modifiedHistoryCount;
                internalHistoryContext->wireJoinerGeneratedHistoryCount
                    = wireJoinerHistory->generatedHistoryCount;
                internalHistoryContext->wireJoinerDeletedHistoryCount
                    = wireJoinerHistory->deletedHistoryCount;
                internalHistoryContext->wireJoinerSplitterHistory = wireJoinerHistory->splitterHistory;
            }
        }
        shapeValue.internalNamedShape = part::namedShapeForSketchInternalShape(
            object.name,
            *rawShape,
            *internalShape,
            internalHistoryContext
        );
        context.namedShapes[object.name + ".InternalShape"] = *shapeValue.internalNamedShape;
    }
    context.shapes[object.name] = shapeValue;
    if (hasNonEmptyInternalShape) {
        // FreeCAD:
        // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), writes auxiliary "InternalShape"; the web
        // response renders that request-local shape with InternalFace ids matching subshapes.
        context.mesh[object.name] = cad_core::part::meshForShape(*internalShape, "InternalFace");
    }
    const nlohmann::json internalSubshapes = hasNonEmptyInternalShape
        ? part::subshapeMapForShape(*internalShape, "Internal")
        : nlohmann::json::object();
    if (!rawShape->IsNull()) {
        nlohmann::json subshapes = part::subshapeMapForShape(*rawShape);
        if (hasNonEmptyInternalShape) {
            for (const auto& item : internalSubshapes.items()) {
                subshapes[item.key()] = item.value();
            }
        }
        context.subshapes[object.name] = subshapes;
    }

    const std::size_t rawEdgeCount = edges.size() + circles.size() + ellipses.size();
    const std::size_t profileEdgeCount = rawEdgeCount;
    const std::size_t rawPointCount = points.size();
    const std::size_t internalFaceCount = countSubshapesOfKind(internalSubshapes, "face");
    const std::size_t internalEdgeCount = countSubshapesOfKind(internalSubshapes, "edge");
    const std::size_t internalVertexCount = countSubshapesOfKind(internalSubshapes, "vertex");
    const nlohmann::json internalElementMap = hasNonEmptyInternalShape
        ? app::internalElementMapForSketch(*rawShape, *internalShape)
        : nlohmann::json::object();
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", rawShape->IsNull() ? "empty" : "occt_sketch_shape"},
        {"profile", profileShapeLabel(profileShape)},
        {"profile_ready", profileShape.has_value()},
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
        {"internal_shape",
         internalShape ? (internalShape->IsNull() ? "empty" : "occt_internal_shape") : "none"},
        {"internal_face_count", internalFaceCount},
        {"internal_edge_count", internalEdgeCount},
        {"internal_vertex_count", internalVertexCount},
        {"internal_element_map", internalElementMap},
        {"coincident_constraints_applied", appliedConstraints->coincident},
        {"orientation_constraints_applied", appliedConstraints->orientation},
        {"dimension_constraints_applied", appliedConstraints->dimension},
        {"relation_constraints_applied", appliedConstraints->relation},
        {"block_constraints_applied", appliedConstraints->block},
        {"external_geometry_count",
         externalGeometry->segments.size() + externalGeometry->points.size()
             + externalGeometry->circles.size() + externalGeometry->arcs.size()
             + externalGeometry->ellipses.size() + externalGeometry->ellipseArcs.size()
             + externalGeometry->bsplines.size() + externalGeometry->beziers.size()},
        {"external_point_count", externalGeometry->points.size()},
        {"external_curve_count",
         externalGeometry->circles.size() + externalGeometry->arcs.size()
             + externalGeometry->ellipses.size() + externalGeometry->ellipseArcs.size()
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
    if (wireJoinerLedger) {
        nlohmann::json resultWireProducerLedgerEntries = nlohmann::json::array();
        for (const part::ResultWireProducerLedgerEntry& entry :
             wireJoinerLedger->resultWireProducerLedgerEntries) {
                    resultWireProducerLedgerEntries.push_back({
                        {"open_export_index", entry.openExportIndex},
                        {"source_edge_info_index", entry.sourceEdgeInfoIndex},
                        {"root_edge_info_index", entry.rootEdgeInfoIndex},
                        {"current_member_edge_info_index", entry.currentMemberEdgeInfoIndex},
                        {"child_wire_info_index", entry.childWireInfoIndex},
                        {"kind", part::resultWireProducerKindName(entry.kind)},
                        {"state", part::resultWireProducerStateName(entry.state)},
                        {"blocker", part::resultWireBlockerName(entry.blocker)},
                        {"open_wire_compound_export_source",
                         part::openWireCompoundExportSourceName(
                             entry.openWireCompoundExportSource
                         )},
                        {"open_wire_compound_edge_info_iteration",
                         entry.openWireCompoundEdgeInfoIteration},
                        {"open_wire_compound_edge_info_iteration2",
                         entry.openWireCompoundEdgeInfoIteration2},
                        {"open_wire_compound_owner_wire_info",
                         entry.openWireCompoundOwnerWireInfo},
                        {"open_wire_compound_owner_wire_info2",
                         entry.openWireCompoundOwnerWireInfo2},
                        {"open_wire_compound_open_leaf_export",
                         entry.openWireCompoundOpenLeafExport},
                        {"open_wire_compound_unowned_open_edge_export",
                         entry.openWireCompoundUnownedOpenEdgeExport},
                        {"open_wire_compound_root_current_member_child_producer",
                         entry.openWireCompoundRootCurrentMemberChildProducer},
                        {"wire_joiner_history_event_index",
                         entry.wireJoinerHistoryEventIndex},
                        {"child_shape_identity_recorded", entry.childShapeIdentityRecorded},
                        {"child_wire_edge_count", entry.childWireEdgeCount},
                        {"child_wire_vertex_count", entry.childWireVertexCount},
                        {"source_edge_indices", entry.sourceEdgeIndices},
                    });
                }
        context.objects[object.name]["wire_joiner_ledger"] = {
            {"edge_info_count", wireJoinerLedger->edgeInfoCount},
            {"split_edge_info_count", wireJoinerLedger->splitEdgeInfoCount},
            {"primary_owned_edge_info_count", wireJoinerLedger->primaryOwnedEdgeInfoCount},
            {"secondary_owned_edge_info_count", wireJoinerLedger->secondaryOwnedEdgeInfoCount},
            {"closed_wire_assigned_edge_info_count",
             wireJoinerLedger->closedWireAssignedEdgeInfoCount},
            {"closed_wire_info_count", wireJoinerLedger->closedWireInfoCount},
            {"closed_wire_vertex_count", wireJoinerLedger->closedWireVertexCount},
            {"closed_wire_search_stack_frame_count",
             wireJoinerLedger->closedWireSearchStackFrameCount},
            {"closed_wire_search_vertex_stack_count",
             wireJoinerLedger->closedWireSearchVertexStackCount},
            {"closed_wire_search_edge_set_visit_count",
             wireJoinerLedger->closedWireSearchEdgeSetVisitCount},
            {"closed_wire_search_backtrack_count", wireJoinerLedger->closedWireSearchBacktrackCount},
            {"closed_wire_search_intersect_skip_count",
             wireJoinerLedger->closedWireSearchIntersectSkipCount},
            {"tight_bound_done_wire_info_count", wireJoinerLedger->tightBoundDoneWireInfoCount},
            {"tight_bound_split_wire_info_count", wireJoinerLedger->tightBoundSplitWireInfoCount},
            {"tight_bound_new_wire_candidate_count",
             wireJoinerLedger->tightBoundNewWireCandidateCount},
            {"tight_bound_new_wire_vertex_count", wireJoinerLedger->tightBoundNewWireVertexCount},
            {"tight_bound_owner_transfer_candidate_edge_info_count",
             wireJoinerLedger->tightBoundOwnerTransferCandidateEdgeInfoCount},
            {"tight_bound_transfer_wire_info_count",
             wireJoinerLedger->tightBoundTransferWireInfoCount},
            {"tight_bound_transfer_wire_vertex_count",
             wireJoinerLedger->tightBoundTransferWireVertexCount},
            {"tight_bound_transferred_owner_edge_info_count",
             wireJoinerLedger->tightBoundTransferredOwnerEdgeInfoCount},
            {"tight_bound_split_owner_wire_info_count",
             wireJoinerLedger->tightBoundSplitOwnerWireInfoCount},
            {"tight_bound_split_owner_vertex_count",
             wireJoinerLedger->tightBoundSplitOwnerVertexCount},
            {"tight_bound_split_owner_built_wire_count",
             wireJoinerLedger->tightBoundSplitOwnerBuiltWireCount},
            {"tight_bound_split_wire_vertex_count", wireJoinerLedger->tightBoundSplitWireVertexCount},
            {"tight_bound_split_wire_built_count", wireJoinerLedger->tightBoundSplitWireBuiltCount},
            {"tight_bound_existing_wire_search_count",
             wireJoinerLedger->tightBoundExistingWireSearchCount},
            {"tight_bound_existing_wire_hit_count", wireJoinerLedger->tightBoundExistingWireHitCount},
            {"tight_bound_existing_wire_reverse_hit_count",
             wireJoinerLedger->tightBoundExistingWireReverseHitCount},
            {"tight_bound_existing_wire_purge_count",
             wireJoinerLedger->tightBoundExistingWirePurgeCount},
            {"tight_bound_purged_wire_info_count", wireJoinerLedger->tightBoundPurgedWireInfoCount},
            {"tight_bound_exhaust_visited_wire_info_count",
             wireJoinerLedger->tightBoundExhaustVisitedWireInfoCount},
            {"tight_bound_exhaust_done_wire_info_count",
             wireJoinerLedger->tightBoundExhaustDoneWireInfoCount},
            {"tight_bound_exhaust_discarded_purged_wire_info_count",
             wireJoinerLedger->tightBoundExhaustDiscardedPurgedWireInfoCount},
            {"tight_bound_exhaust_primary_reset_edge_info_count",
             wireJoinerLedger->tightBoundExhaustPrimaryResetEdgeInfoCount},
            {"tight_bound_full_wire_set_insert_count",
             wireJoinerLedger->tightBoundFullWireSetInsertCount},
            {"tight_bound_full_wire_set_erase_count",
             wireJoinerLedger->tightBoundFullWireSetEraseCount},
            {"tight_bound_full_wire_set_abort_count",
             wireJoinerLedger->tightBoundFullWireSetAbortCount},
            {"tight_bound_full_wire_set_purge_candidate_count",
             wireJoinerLedger->tightBoundFullWireSetPurgeCandidateCount},
            {"tight_bound_full_wire_set_blocked_transfer_count",
             wireJoinerLedger->tightBoundFullWireSetBlockedTransferCount},
            {"tight_bound_full_wire_set_abort_search_count",
             wireJoinerLedger->tightBoundFullWireSetAbortSearchCount},
            {"tight_bound_full_wire_set_abort_resolved_by_hit_count",
             wireJoinerLedger->tightBoundFullWireSetAbortResolvedByHitCount},
            {"tight_bound_full_wire_set_abort_blocked_search_count",
             wireJoinerLedger->tightBoundFullWireSetAbortBlockedSearchCount},
            {"tight_bound_existing_wire_multi_round_wire_info_count",
             wireJoinerLedger->tightBoundExistingWireMultiRoundWireInfoCount},
            {"tight_bound_existing_wire_multi_round_search_count",
             wireJoinerLedger->tightBoundExistingWireMultiRoundSearchCount},
            {"exhaust_adjacent_search_count", wireJoinerLedger->exhaustAdjacentSearchCount},
            {"exhaust_adjacent_search_hit_count", wireJoinerLedger->exhaustAdjacentSearchHitCount},
            {"exhaust_adjacent_search_miss_count", wireJoinerLedger->exhaustAdjacentSearchMissCount},
            {"exhaust_adjacent_search_stack_frame_count",
             wireJoinerLedger->exhaustAdjacentSearchStackFrameCount},
            {"exhaust_adjacent_search_vertex_stack_count",
             wireJoinerLedger->exhaustAdjacentSearchVertexStackCount},
            {"exhaust_adjacent_search_edge_set_visit_count",
             wireJoinerLedger->exhaustAdjacentSearchEdgeSetVisitCount},
            {"exhaust_adjacent_search_backtrack_count",
             wireJoinerLedger->exhaustAdjacentSearchBacktrackCount},
            {"exhaust_adjacent_wire_set_insert_count",
             wireJoinerLedger->exhaustAdjacentWireSetInsertCount},
            {"exhaust_adjacent_wire_set_erase_count",
             wireJoinerLedger->exhaustAdjacentWireSetEraseCount},
            {"exhaust_adjacent_wire_set_abort_count",
             wireJoinerLedger->exhaustAdjacentWireSetAbortCount},
            {"exhaust_adjacent_wire_info2_abort_count",
             wireJoinerLedger->exhaustAdjacentWireInfo2AbortCount},
            {"repeated_split_exhaust_cycle_count", wireJoinerLedger->repeatedSplitExhaustCycleCount},
            {"repeated_split_exhaust_removed_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRemovedEdgeInfoCount},
            {"repeated_split_exhaust_removed_unowned_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRemovedUnownedEdgeInfoCount},
            {"repeated_split_exhaust_removed_secondary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRemovedSecondaryEdgeInfoCount},
            {"repeated_split_exhaust_removed_primary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRemovedPrimaryEdgeInfoCount},
            {"repeated_split_exhaust_rerun_active_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunActiveEdgeInfoCount},
            {"repeated_split_exhaust_rerun_owned_active_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount},
            {"repeated_split_exhaust_rerun_reset_primary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount},
            {"repeated_split_exhaust_rerun_reset_secondary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount},
            {"repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount},
            {"repeated_split_exhaust_rerun_no_active_search_count",
             wireJoinerLedger->repeatedSplitExhaustRerunNoActiveSearchCount},
            {"repeated_split_exhaust_rerun_closed_wire_search_count",
             wireJoinerLedger->repeatedSplitExhaustRerunClosedWireSearchCount},
            {"repeated_split_exhaust_rerun_closed_wire_miss_count",
             wireJoinerLedger->repeatedSplitExhaustRerunClosedWireMissCount},
            {"repeated_split_exhaust_rerun_miss_live_reset_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount},
            {"repeated_split_exhaust_rerun_closed_wire_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunClosedWireInfoCount},
            {"repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount},
            {"repeated_split_exhaust_rerun_closed_wire_vertex_count",
             wireJoinerLedger->repeatedSplitExhaustRerunClosedWireVertexCount},
            {"repeated_split_exhaust_rerun_resettable_closed_wire_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunResettableClosedWireInfoCount},
            {"repeated_split_exhaust_rerun_resettable_assigned_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount},
            {"repeated_split_exhaust_rerun_live_reset_primary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount},
            {"repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount},
            {"repeated_split_exhaust_rerun_live_closed_wire_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveClosedWireInfoCount},
            {"repeated_split_exhaust_rerun_live_assigned_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount},
            {"repeated_split_exhaust_rerun_live_closed_wire_vertex_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveClosedWireVertexCount},
            {"repeated_split_exhaust_rerun_live_branch_search_candidate_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveBranchSearchCandidateCount},
            {"repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount},
            {"repeated_split_exhaust_rerun_live_done_wire_info_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLiveDoneWireInfoCount},
            {"repeated_split_exhaust_rerun_removal_scan_count",
             wireJoinerLedger->repeatedSplitExhaustRerunRemovalScanCount},
            {"repeated_split_exhaust_rerun_loop_exit_no_removal_count",
             wireJoinerLedger->repeatedSplitExhaustRerunLoopExitNoRemovalCount},
            {"repeated_split_exhaust_rerun_branch_search_candidate_count",
             wireJoinerLedger->repeatedSplitExhaustRerunBranchSearchCandidateCount},
            {"repeated_split_exhaust_rerun_branch_search_inside_candidate_count",
             wireJoinerLedger->repeatedSplitExhaustRerunBranchSearchInsideCandidateCount},
            {"repeated_split_exhaust_rerun_new_wire_seed_candidate_count",
             wireJoinerLedger->repeatedSplitExhaustRerunNewWireSeedCandidateCount},
            {"repeated_split_exhaust_generated_identity_blocked_edge_info_count",
             wireJoinerLedger->repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount},
            {"tight_bound_existing_wire_search_stack_frame_count",
             wireJoinerLedger->tightBoundExistingWireSearchStackFrameCount},
            {"tight_bound_existing_wire_search_vertex_stack_count",
             wireJoinerLedger->tightBoundExistingWireSearchVertexStackCount},
            {"tight_bound_existing_wire_search_edge_set_visit_count",
             wireJoinerLedger->tightBoundExistingWireSearchEdgeSetVisitCount},
            {"tight_bound_existing_wire_search_backtrack_count",
             wireJoinerLedger->tightBoundExistingWireSearchBacktrackCount},
            {"tight_bound_existing_wire_search_idx_vertex_count",
             wireJoinerLedger->tightBoundExistingWireSearchIdxVertexCount},
            {"tight_bound_existing_wire_search_stack_pos_count",
             wireJoinerLedger->tightBoundExistingWireSearchStackPosCount},
            {"tight_bound_existing_wire_search_path_vertex_count",
             wireJoinerLedger->tightBoundExistingWireSearchPathVertexCount},
            {"tight_bound_existing_wire_selected_hit_count",
             wireJoinerLedger->tightBoundExistingWireSelectedHitCount},
            {"tight_bound_existing_wire_search_only_hit_count",
             wireJoinerLedger->tightBoundExistingWireSearchOnlyHitCount},
            {"tight_bound_existing_wire_search_only_idx_vertex_count",
             wireJoinerLedger->tightBoundExistingWireSearchOnlyIdxVertexCount},
            {"tight_bound_existing_wire_search_only_stack_pos_count",
             wireJoinerLedger->tightBoundExistingWireSearchOnlyStackPosCount},
            {"tight_bound_existing_wire_search_only_path_blocked_count",
             wireJoinerLedger->tightBoundExistingWireSearchOnlyPathBlockedCount},
            {"tight_bound_existing_wire_search_only_order_blocked_count",
             wireJoinerLedger->tightBoundExistingWireSearchOnlyOrderBlockedCount},
            {"tight_bound_existing_wire_idx_vertex_count",
             wireJoinerLedger->tightBoundExistingWireIdxVertexCount},
            {"tight_bound_existing_wire_stack_pos_count",
             wireJoinerLedger->tightBoundExistingWireStackPosCount},
            {"result_wire_producer_ledger_entries", resultWireProducerLedgerEntries},
            {"source_identity_shared_vertex_edge_info_count",
             wireJoinerLedger->sourceIdentitySharedVertexEdgeInfoCount},
            {"source_identity_only_source_vertices_edge_info_count",
             wireJoinerLedger->sourceIdentityOnlySourceVerticesEdgeInfoCount},
            {"source_identity_open_export_shared_vertex_edge_info_count",
             wireJoinerLedger->sourceIdentityOpenExportSharedVertexEdgeInfoCount},
            {"source_identity_open_export_only_source_vertices_edge_info_count",
             wireJoinerLedger->sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount},
            {"source_lineage_edge_info_count", wireJoinerLedger->sourceLineageEdgeInfoCount},
            {"source_lineage_split_edge_info_count",
             wireJoinerLedger->sourceLineageSplitEdgeInfoCount},
            {"source_lineage_open_export_edge_info_count",
             wireJoinerLedger->sourceLineageOpenExportEdgeInfoCount},
            {"source_lineage_missing_open_export_edge_info_count",
             wireJoinerLedger->sourceLineageMissingOpenExportEdgeInfoCount},
            {"source_lineage_multi_source_edge_info_count",
             wireJoinerLedger->sourceLineageMultiSourceEdgeInfoCount},
            {"split_fragment_source_lineage_edge_info_count",
             wireJoinerLedger->splitFragmentSourceLineageEdgeInfoCount},
            {"split_fragment_modified_history_edge_info_count",
             wireJoinerLedger->splitFragmentModifiedHistoryEdgeInfoCount},
            {"split_fragment_generated_history_edge_info_count",
             wireJoinerLedger->splitFragmentGeneratedHistoryEdgeInfoCount},
            {"split_fragment_identity_fallback_edge_info_count",
             wireJoinerLedger->splitFragmentIdentityFallbackEdgeInfoCount},
            {"split_fragment_source_identity_fallback_edge_info_count",
             wireJoinerLedger->splitFragmentSourceIdentityFallbackEdgeInfoCount},
            {"split_fragment_history_shape_geometry_bridge_edge_info_count",
             wireJoinerLedger->splitFragmentHistoryShapeGeometryBridgeEdgeInfoCount},
            {"closed_wire_cycle_split_ledger_source_edge_count",
             wireJoinerLedger->closedWireCycleSplitLedgerSourceEdgeCount},
            {"closed_wire_cycle_split_ledger_open_export_decision_count",
             wireJoinerLedger->closedWireCycleSplitLedgerOpenExportDecisionCount},
            {"super_edge_candidate_count", wireJoinerLedger->superEdgeCandidateCount},
            {"super_edge_candidate_edge_info_count",
             wireJoinerLedger->superEdgeCandidateEdgeInfoCount},
            {"super_edge_root_edge_info_count", wireJoinerLedger->superEdgeRootEdgeInfoCount},
            {"super_edge_closed_candidate_count", wireJoinerLedger->superEdgeClosedCandidateCount},
            {"super_edge_open_candidate_count", wireJoinerLedger->superEdgeOpenCandidateCount},
            {"super_edge_materialized_root_edge_info_count",
             wireJoinerLedger->superEdgeMaterializedRootEdgeInfoCount},
            {"super_edge_materialized_edge_info_count",
             wireJoinerLedger->superEdgeMaterializedEdgeInfoCount},
            {"super_edge_shadowed_member_edge_info_count",
             wireJoinerLedger->superEdgeShadowedMemberEdgeInfoCount},
            {"super_edge_lifecycle_member_minus_one_edge_info_count",
             wireJoinerLedger->superEdgeLifecycleMemberMinusOneEdgeInfoCount},
            {"super_edge_lifecycle_open_root_edge_info_count",
             wireJoinerLedger->superEdgeLifecycleOpenRootEdgeInfoCount},
            {"super_edge_lifecycle_closed_root_edge_info_count",
             wireJoinerLedger->superEdgeLifecycleClosedRootEdgeInfoCount},
            {"super_edge_lifecycle_adjacent_range_rewrite_count",
             wireJoinerLedger->superEdgeLifecycleAdjacentRangeRewriteCount},
            {"super_edge_lifecycle_endpoint_rewrite_count",
             wireJoinerLedger->superEdgeLifecycleEndpointRewriteCount},
            {"super_edge_lifecycle_adjacent_range_source_edge_info_count",
             wireJoinerLedger->superEdgeLifecycleAdjacentRangeSourceEdgeInfoCount},
            {"super_edge_lifecycle_adjacent_range_vertex_count",
             wireJoinerLedger->superEdgeLifecycleAdjacentRangeVertexCount},
            {"open_export_edge_info_count", wireJoinerLedger->openExportEdgeInfoCount},
            {"open_wire_compound_wire_info_count", wireJoinerLedger->openWireCompoundWireInfoCount},
            {"open_wire_compound_built_wire_info_count",
             wireJoinerLedger->openWireCompoundBuiltWireInfoCount},
            {"open_wire_compound_edge_info_count", wireJoinerLedger->openWireCompoundEdgeInfoCount},
            {"open_wire_compound_super_edge_wire_info_count",
             wireJoinerLedger->openWireCompoundSuperEdgeWireInfoCount},
            {"open_wire_compound_source_lineage_wire_info_count",
             wireJoinerLedger->openWireCompoundSourceLineageWireInfoCount},
            {"open_wire_compound_splitter_lineage_wire_info_count",
             wireJoinerLedger->openWireCompoundSplitterLineageWireInfoCount},
            {"open_wire_compound_no_original_purge_match_wire_info_count",
             wireJoinerLedger->openWireCompoundNoOriginalPurgeMatchWireInfoCount},
            {"open_wire_compound_no_original_shared_source_ledger_wire_info_count",
             wireJoinerLedger->openWireCompoundNoOriginalSharedSourceLedgerWireInfoCount},
            {"open_wire_compound_no_original_shared_source_edge_count",
             wireJoinerLedger->openWireCompoundNoOriginalSharedSourceEdgeCount},
            {"open_wire_compound_no_original_shared_source_matched_edge_count",
             wireJoinerLedger->openWireCompoundNoOriginalSharedSourceMatchedEdgeCount},
            {"open_wire_compound_no_original_shared_source_unmatched_edge_count",
             wireJoinerLedger->openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount},
            {"open_wire_compound_producer_ledger_wire_built_wire_info_count",
             wireJoinerLedger->openWireCompoundProducerLedgerWireBuiltWireInfoCount},
            {"open_wire_compound_producer_ledger_wire_from_source_vmap_wire_info_count",
             wireJoinerLedger->openWireCompoundProducerLedgerWireFromSourceVmapWireInfoCount},
            {"open_wire_compound_source_vmap_endpoint_ledger_wire_info_count",
             wireJoinerLedger->openWireCompoundSourceVmapEndpointLedgerWireInfoCount},
            {"open_wire_compound_source_vmap_endpoint_ledger_output_vertex_count",
             wireJoinerLedger->openWireCompoundSourceVmapEndpointLedgerOutputVertexCount},
            {"open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
             wireJoinerLedger->openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_wire_info_count",
             wireJoinerLedger->openWireCompoundEndpointProvenanceWireInfoCount},
            {"open_wire_compound_endpoint_provenance_output_vertex_count",
             wireJoinerLedger->openWireCompoundEndpointProvenanceOutputVertexCount},
            {"open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
             wireJoinerLedger->openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_candidate_matched_vertex_count",
             wireJoinerLedger->openWireCompoundEndpointProvenanceCandidateMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_unmatched_vertex_count",
             wireJoinerLedger->openWireCompoundEndpointProvenanceUnmatchedVertexCount},
            {"open_wire_compound_vmap_replacement_event_wire_info_count",
             wireJoinerLedger->openWireCompoundVmapReplacementEventWireInfoCount},
            {"open_wire_compound_vmap_replacement_event_count",
             wireJoinerLedger->openWireCompoundVmapReplacementEventCount},
            {"open_wire_compound_current_member_split_ledger_vertex_candidate_wire_info_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerVertexCandidateWireInfoCount},
            {"open_wire_compound_current_member_split_ledger_vertex_debt_wire_info_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerVertexDebtWireInfoCount},
            {"open_wire_compound_current_member_split_ledger_member_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerMemberVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount},
            {"open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_distinct_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount},
            {"open_wire_compound_current_member_split_ledger_candidate_distinct_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount},
            {"open_wire_compound_current_member_split_ledger_candidate_vertex_multiplicity_loss_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerCandidateVertexMultiplicityLossCount},
            {"open_wire_compound_current_member_split_ledger_output_other_output_matched_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerOutputOtherOutputMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_candidate_other_output_matched_vertex_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerCandidateOtherOutputMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_candidate_vertex_reuse_risk_count",
             wireJoinerLedger
                 ->openWireCompoundCurrentMemberSplitLedgerCandidateVertexReuseRiskCount},
            {"open_wire_compound_missing_child_wire_history_edge_info_count",
             wireJoinerLedger->openWireCompoundMissingChildWireHistoryEdgeInfoCount},
            {"open_wire_compound_root_current_member_producer_output_wire_info_count",
             wireJoinerLedger->openWireCompoundRootCurrentMemberProducerOutputWireInfoCount},
            {"open_wire_compound_source_shared_vertex_wire_info_count",
             wireJoinerLedger->openWireCompoundSourceSharedVertexWireInfoCount},
            {"ordered_wire_info_count", wireJoinerLedger->orderedWireInfoCount},
            {"ordered_vertex_count", wireJoinerLedger->orderedVertexCount},
            {"iteration2_marked_edge_info_count", wireJoinerLedger->iteration2MarkedEdgeInfoCount},
            {"branch_search_candidate_count", wireJoinerLedger->branchSearchCandidateCount},
            {"branch_search_seed_wire_info_count", wireJoinerLedger->branchSearchSeedWireInfoCount},
            {"branch_search_inside_candidate_count",
             wireJoinerLedger->branchSearchInsideCandidateCount},
            {"new_wire_seed_candidate_count", wireJoinerLedger->newWireSeedCandidateCount},
            {"new_wire_seed_wire_info_count", wireJoinerLedger->newWireSeedWireInfoCount},
            {"split_wire_candidate_count", wireJoinerLedger->splitWireCandidateCount},
            {"split_wire_edge_info_count", wireJoinerLedger->splitWireEdgeInfoCount},
            {"done_wire_info_count", wireJoinerLedger->doneWireInfoCount},
            {"done_owned_edge_info_count", wireJoinerLedger->doneOwnedEdgeInfoCount},
            {"owner_propagation_candidate_count", wireJoinerLedger->ownerPropagationCandidateCount},
            {"owner_propagation_other_wire_candidate_count",
             wireJoinerLedger->ownerPropagationOtherWireCandidateCount},
            {"owner_propagation_other_wire_live_edge_info_count",
             wireJoinerLedger->ownerPropagationOtherWireLiveEdgeInfoCount},
            {"exhaust_seed_edge_info_count", wireJoinerLedger->exhaustSeedEdgeInfoCount},
            {"exhaust_shared_owner_edge_info_count",
             wireJoinerLedger->exhaustSharedOwnerEdgeInfoCount},
            {"exhaust_done_secondary_edge_info_count",
             wireJoinerLedger->exhaustDoneSecondaryEdgeInfoCount},
            {"exhaust_search_candidate_edge_info_count",
             wireJoinerLedger->exhaustSearchCandidateEdgeInfoCount},
            {"exhaust_secondary_owner_edge_info_count",
             wireJoinerLedger->exhaustSecondaryOwnerEdgeInfoCount},
        };
        context.objects[object.name]["wire_joiner_history"]
            = "history_partial:edge_info_wire_info_split_done_exhaust";
    }
    if (wireJoinerHistory) {
        nlohmann::json wireJoinerHistoryEvents = nlohmann::json::array();
        for (const part::WireJoinerHistoryEvent& event : wireJoinerHistory->historyEvents) {
            wireJoinerHistoryEvents.push_back({
                {"event_index", event.eventIndex},
                {"open_export_index", event.openExportIndex},
                {"edge_info_index", event.edgeInfoIndex},
                {"open_wire_compound_child_wire_info_index",
                 event.openWireCompoundChildWireInfoIndex},
                {"relation", part::wireJoinerHistoryRelationName(event.relation)},
                {"relation_from_child_wire_ledger", event.relationFromChildWireLedger},
                {"source_edge_indices", event.sourceEdgeIndices},
                {"source_lineage_from_splitter_history",
                 event.sourceLineageFromSplitterHistory},
                {"no_original_purged_by_ledger", event.noOriginalPurgedByLedger},
                {"split_fragment_from_modified_history",
                 event.splitFragmentFromModifiedHistory},
                {"split_fragment_from_generated_history",
                 event.splitFragmentFromGeneratedHistory},
            });
        }
        nlohmann::json openExportHistoryEntries = nlohmann::json::array();
        for (const part::WireJoinerOpenExportHistoryEntry& entry :
             wireJoinerHistory->openExportEntries) {
            nlohmann::json currentMemberSplitOutputVertexDebt = nlohmann::json::array();
            for (const part::WireJoinerEndpointIdentityDebt& debt :
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt) {
                currentMemberSplitOutputVertexDebt.push_back({
                    {"output_vertex_index", debt.outputVertexIndex},
                    {"matched_member_split_ledger", debt.matchedMemberSplitLedger},
                    {"matched_candidate_ledger", debt.matchedCandidateLedger},
                    {"current_child_wire_output_vertex_matches_other_output",
                     debt.currentChildWireOutputVertexMatchesOtherOutput},
                    {"candidate_wire_vertex_matches_other_output",
                     debt.candidateWireVertexMatchesOtherOutput},
                    {"explanation", debt.explanation},
                    {"current_child_wire_output_vertex_identity",
                     debt.currentChildWireOutputVertexIdentity},
                    {"member_split_ledger_vertex_identity",
                     debt.memberSplitLedgerVertexIdentity},
                    {"candidate_wire_vertex_identity", debt.candidateWireVertexIdentity},
                    {"mismatch_reason", debt.mismatchReason},
                });
            }
            nlohmann::json vmapReplacementEvents = nlohmann::json::array();
            for (const part::WireJoinerVmapReplacementEvent& event :
                 entry.openWireCompoundVmapReplacementEvents) {
                vmapReplacementEvents.push_back({
                    {"event_index", event.eventIndex},
                    {"affected_source_edge_index", event.affectedSourceEdgeIndex},
                    {"affected_child_wire_edge_info_index",
                     event.affectedChildWireEdgeInfoIndex},
                    {"affected_endpoint", event.affectedEndpoint},
                    {"affected_source_endpoint", event.affectedSourceEndpoint},
                    {"affected_child_wire_endpoint", event.affectedChildWireEndpoint},
                    {"replacement_source_edge_index", event.replacementSourceEdgeIndex},
                    {"replacement_source_endpoint", event.replacementSourceEndpoint},
                    {"replacement_from_mutable_source_edge_ledger",
                     event.replacementFromMutableSourceEdgeLedger},
                    {"replacement_from_split_fragment_ledger",
                     event.replacementFromSplitFragmentLedger},
                });
            }
            openExportHistoryEntries.push_back({
                {"open_export_index", entry.openExportIndex},
                {"edge_info_index", entry.edgeInfoIndex},
                {"open_wire_compound_export_source",
                 part::openWireCompoundExportSourceName(entry.openWireCompoundExportSource)},
                {"open_wire_compound_edge_info_iteration",
                 entry.openWireCompoundEdgeInfoIteration},
                {"open_wire_compound_edge_info_iteration2",
                 entry.openWireCompoundEdgeInfoIteration2},
                {"open_wire_compound_owner_wire_info",
                 entry.openWireCompoundOwnerWireInfo},
                {"open_wire_compound_owner_wire_info2",
                 entry.openWireCompoundOwnerWireInfo2},
                {"open_wire_compound_open_leaf_export",
                 entry.openWireCompoundOpenLeafExport},
                {"open_wire_compound_unowned_open_edge_export",
                 entry.openWireCompoundUnownedOpenEdgeExport},
                {"open_wire_compound_root_current_member_child_producer",
                 entry.openWireCompoundRootCurrentMemberChildProducer},
                {"open_wire_compound_child_shape_identity_recorded",
                 entry.openWireCompoundChildShapeIdentityRecorded},
                {"open_wire_compound_child_wire_edge_count",
                 entry.openWireCompoundChildWireEdgeCount},
                {"open_wire_compound_child_wire_vertex_count",
                 entry.openWireCompoundChildWireVertexCount},
                {"wire_joiner_history_relation",
                 entry.historyRelationFromChildWireLedger
                     ? part::wireJoinerHistoryRelationName(entry.historyRelation)
                     : ""},
                {"wire_joiner_history_relation_from_child_wire_ledger",
                 entry.historyRelationFromChildWireLedger},
                {"wire_joiner_history_event_index", entry.wireJoinerHistoryEventIndex},
                {"wire_joiner_history_event_from_child_wire_ledger",
                 entry.wireJoinerHistoryEventFromChildWireLedger},
                {"result_wire_producer_kind",
                 part::resultWireProducerKindName(entry.resultWireProducer.kind)},
                {"result_wire_producer_state",
                 part::resultWireProducerStateName(entry.resultWireProducer.state)},
                {"result_wire_producer_blocker",
                 part::resultWireBlockerName(entry.resultWireProducer.blocker)},
                {"result_wire_producer_source_edge_info_index",
                 entry.resultWireProducer.sourceEdgeInfoIndex},
                {"result_wire_producer_root_edge_info_index",
                 entry.resultWireProducer.rootEdgeInfoIndex},
                {"result_wire_producer_current_member_edge_info_index",
                 entry.resultWireProducer.currentMemberEdgeInfoIndex},
                {"result_wire_producer_child_wire_info_index",
                 entry.resultWireProducer.childWireInfoIndex},
                {"open_wire_compound_child_wire_info_index",
                 entry.openWireCompoundChildWireInfoIndex},
                {"open_wire_compound_source_edge_indices",
                 entry.openWireCompoundSourceEdgeIndices},
                {"open_wire_compound_source_lineage_from_splitter_history",
                 entry.openWireCompoundSourceLineageFromSplitterHistory},
                {"open_wire_compound_no_original_purge_match",
                 entry.openWireCompoundNoOriginalPurgeMatch},
                {"open_wire_compound_no_original_purged_by_ledger",
                 entry.openWireCompoundNoOriginalPurgedByLedger},
                {"open_wire_compound_no_original_shared_source_ledger_recorded",
                 entry.openWireCompoundNoOriginalSharedSourceLedgerRecorded},
                {"open_wire_compound_no_original_shared_source_edge_count",
                 entry.openWireCompoundNoOriginalSharedSourceEdgeCount},
                {"open_wire_compound_no_original_shared_source_matched_edge_count",
                 entry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount},
                {"open_wire_compound_no_original_shared_source_unmatched_edge_count",
                 entry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount},
                {"open_wire_compound_producer_ledger_wire_built",
                 entry.openWireCompoundProducerLedgerWireBuilt},
                {"open_wire_compound_producer_ledger_wire_from_source_vmap",
                 entry.openWireCompoundProducerLedgerWireFromSourceVmap},
                {"open_wire_compound_source_vmap_endpoint_ledger_recorded",
                 entry.openWireCompoundSourceVmapEndpointLedgerRecorded},
                {"open_wire_compound_source_vmap_endpoint_ledger_output_vertex_count",
                 entry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount},
                {"open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
                 entry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount},
                {"open_wire_compound_endpoint_provenance_recorded",
                 entry.openWireCompoundEndpointProvenanceRecorded},
                {"open_wire_compound_endpoint_provenance_output_vertex_count",
                 entry.openWireCompoundEndpointProvenanceOutputVertexCount},
                {"open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
                 entry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount},
                {"open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
                 entry.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount},
                {"open_wire_compound_endpoint_provenance_candidate_matched_vertex_count",
                 entry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount},
                {"open_wire_compound_endpoint_provenance_unmatched_vertex_count",
                 entry.openWireCompoundEndpointProvenanceUnmatchedVertexCount},
                {"open_wire_compound_vmap_replacement_event_count",
                 entry.openWireCompoundVmapReplacementEventCount},
                {"open_wire_compound_vmap_replacement_events", vmapReplacementEvents},
                {"open_wire_compound_current_member_producer_output",
                 entry.openWireCompoundCurrentMemberProducerOutput},
                {"open_wire_compound_current_member_split_ledger_vertex_candidate",
                 entry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate},
                {"open_wire_compound_current_member_split_ledger_vertex_debt_recorded",
                 entry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded},
                {"open_wire_compound_current_member_split_ledger_member_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount},
                {"open_wire_compound_current_member_split_ledger_candidate_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount},
                {"open_wire_compound_current_member_split_ledger_output_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount},
                {"open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount},
                {"open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount},
                {"open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount},
                {"open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count",
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount},
                {"open_wire_compound_current_member_split_ledger_output_vertex_debt",
                 currentMemberSplitOutputVertexDebt},
                {"open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked",
                 entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked},
                {"missing_open_wire_compound_child_wire", entry.missingOpenWireCompoundChildWire},
                {"source_edge_indices", entry.sourceEdgeIndices},
                {"source_lineage_from_splitter_history", entry.sourceLineageFromSplitterHistory},
                {"split_fragment_source_edge_indices", entry.splitFragmentSourceEdgeIndices},
                {"split_fragment_modified_source_edge_indices",
                 entry.splitFragmentModifiedSourceEdgeIndices},
                {"split_fragment_generated_source_edge_indices",
                 entry.splitFragmentGeneratedSourceEdgeIndices},
                {"split_fragment_from_modified_history", entry.splitFragmentFromModifiedHistory},
                {"split_fragment_from_generated_history", entry.splitFragmentFromGeneratedHistory},
                {"split_fragment_source_lineage_from_identity_fallback",
                 entry.splitFragmentSourceLineageFromIdentityFallback},
                {"split_fragment_source_lineage_from_source_identity_fallback",
                 entry.splitFragmentSourceLineageFromSourceIdentityFallback},
                {"split_fragment_history_shape_geometry_bridge",
                 entry.splitFragmentHistoryShapeGeometryBridge},
                {"source_vertex_identity", entry.sourceVertexIdentity},
                {"source_vertex_identity_any",
                 entry.sourceVertexIdentity[0] || entry.sourceVertexIdentity[1]},
                {"source_vertex_identity_all",
                 entry.sourceVertexIdentity[0] && entry.sourceVertexIdentity[1]},
                {"source_vertex_replacement_source_edge_indices",
                 entry.sourceVertexReplacementSourceEdgeIndices},
                {"source_vertex_replacement_endpoints", entry.sourceVertexReplacementEndpoints},
                {"source_vertex_replacement_identity", entry.sourceVertexReplacementIdentity},
            });
        }
        context.objects[object.name]["wire_joiner_history_detail"] = {
            {"source_edge_count", wireJoinerHistory->sourceEdgeCount},
            {"split_result_edge_count", wireJoinerHistory->splitResultEdgeCount},
            {"wire_joiner_history_event_count", wireJoinerHistory->historyEvents.size()},
            {"wire_joiner_history_event_from_child_wire_ledger_count",
             wireJoinerHistory->historyEventFromChildWireLedgerCount},
            {"wire_joiner_history_events", std::move(wireJoinerHistoryEvents)},
            {"open_export_history_entries", std::move(openExportHistoryEntries)},
            {"modified_source_edge_count", wireJoinerHistory->modifiedSourceEdgeCount},
            {"modified_history_count", wireJoinerHistory->modifiedHistoryCount},
            {"generated_history_count", wireJoinerHistory->generatedHistoryCount},
            {"deleted_history_count", wireJoinerHistory->deletedHistoryCount},
            {"splitter_history", wireJoinerHistory->splitterHistory},
        };
    }
    if (faceMakerHistory) {
        nlohmann::json edgeEvidence = nlohmann::json::array();
        for (const part::FaceMakerEdgeHistoryEvidence& entry : faceMakerHistory->edgeEvidence) {
            edgeEvidence.push_back({
                {"maker_stage", entry.makerStage},
                {"relation", entry.relation},
                {"source_edge_index", entry.sourceEdgeIndex},
                {"target_edge_index", entry.targetEdgeIndex},
                {"pre_split_history", entry.preSplitHistory},
                {"splitter_history", entry.splitterHistory},
            });
        }
        nlohmann::json boundedFaceEvidence = nlohmann::json::array();
        for (const part::FaceMakerBoundedFaceHistoryEvidence& entry :
             faceMakerHistory->boundedFaceEvidence) {
            nlohmann::json boundary = nlohmann::json::array();
            for (const part::FaceMakerBoundedFaceBoundaryEvidence& boundaryEntry :
                 entry.outerBoundary) {
                boundary.push_back({
                    {"source_edge_index", boundaryEntry.sourceEdgeIndex},
                    {"target_edge_index", boundaryEntry.targetEdgeIndex},
                    {"maker_stage", boundaryEntry.makerStage},
                    {"relation", boundaryEntry.relation},
                });
            }
            boundedFaceEvidence.push_back({
                {"bounded_face_index", entry.boundedFaceIndex},
                {"source_edge_indices", entry.sourceEdgeIndices},
                {"outer_boundary_target_edge_indices", entry.outerBoundaryTargetEdgeIndices},
                {"outer_boundary", std::move(boundary)},
            });
        }
        context.objects[object.name]["facemaker_history"] = {
            {"source_edge_count", faceMakerHistory->sourceEdgeCount},
            {"pre_split_edge_count", faceMakerHistory->preSplitEdgeCount},
            {"splitter_edge_count", faceMakerHistory->splitterEdgeCount},
            {"bounded_face_count", faceMakerHistory->boundedFaceCount},
            {"pre_split_history", faceMakerHistory->preSplitHistory},
            {"splitter_history", faceMakerHistory->splitterHistory},
            {"profile_result_source",
             faceMakerRuntimeSourceName(faceMakerHistory->profileResultSource)},
            {"internal_result_source",
             faceMakerRuntimeSourceName(faceMakerHistory->internalResultSource)},
            {"topology_switch_used", faceMakerHistory->topologySwitchUsed},
            {"edge_evidence", std::move(edgeEvidence)},
            {"bounded_face_evidence", std::move(boundedFaceEvidence)},
        };
        context.objects[object.name]["facemaker_history_status"]
            = "history_evidence:facemaker_buildface";
    }
}

}  // namespace cad_core::sketcher
