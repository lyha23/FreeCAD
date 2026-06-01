#include "cad_core/features/feature_extrude.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/extrusion_helper.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/reference_matcher.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gce_MakeDir.hxx>

#include <cmath>
#include <utility>
#include <vector>

namespace cad_core::features {

namespace {

struct PlanarLimit {
    gp_Dir direction;
    double length = 0.0;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion(),
// reads "SideType", "Type2", "Length2", "TaperAngle2", "UpToFace2" and "UpToShape2" before generating side prisms.
struct SideSpec {
    std::string typeProperty;
    std::string lengthProperty;
    std::string upToFaceProperty;
    std::string upToShapeProperty;
    std::string taperProperty;
    std::string offsetProperty;
    gp_Dir direction;
};

struct SideBuild {
    std::string method;
    double length = 0.0;
    TopoDS_Shape shape;
    bool topoNamingKnownGap = false;
    std::optional<topo::NamedShape> namedShape;
};

struct ToolShapeBuild {
    TopoDS_Shape shape;
    std::optional<topo::NamedShape> namedShape;
};

struct CutFaceCandidate {
    TopoDS_Face face;
    double distanceSquared = 0.0;
};

std::string stableSubnameDiagnosticCode(topo::ElementResolveStatus status)
{
    switch (status) {
        case topo::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case topo::ElementResolveStatus::Split:
            return "split_stable_subname";
        case topo::ElementResolveStatus::Resolved:
        case topo::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& property,
                                           const std::string& target,
                                           const std::string& stableSubname,
                                           topo::ElementResolveStatus status)
{
    if (status == topo::ElementResolveStatus::Deleted) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == topo::ElementResolveStatus::Split) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return property + " target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

struct DirectionSpec {
    gp_Dir direction;
    gp_Dir sketchNormal;
    double lengthScale = 1.0;
};

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

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::getElementTypes() exposes "InternalFace"; PartDesign Profile is a LinkSub
// to the sketch, so cad-core resolves Profile.SubList InternalFaceN against InternalShape.
std::optional<TopoDS_Shape> resolveSketchInternalFaceProfile(const document::DocumentObject& object,
                                                             runtime::ComputeContext& context,
                                                             const document::Link& profileLink,
                                                             const runtime::ShapeValue& shapeValue,
                                                             const std::string& featureName)
{
    if (profileLink.subnames.empty()) {
        if (shapeValue.profileRequiresSubshapeSelection) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   featureName + " Profile target " + profileLink.object
                                       + " has split InternalFace regions; Profile.SubList must select one InternalFaceN",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink.object);
            return std::nullopt;
        }
        if (shapeValue.profileShape && !shapeValue.profileShape->IsNull()) {
            return *shapeValue.profileShape;
        }
        return std::nullopt;
    }

    if (profileLink.subnames.size() != 1U || profileLink.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile.SubList must select exactly one InternalFaceN",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::getTopoShapeVerifiedFace(), throws "Cannot make face from profile" when the linked sketch
        // cannot provide a closed face. An explicit InternalFaceN selection against an empty
        // Sketch InternalShape is a profile error, not a missing object/link target.
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               featureName + " Profile target " + profileLink.object
                                   + " has no closed InternalFace profile for " + profileLink.subnames.front(),
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               profileLink.subnames.front());
        return std::nullopt;
    }

    const std::string& subname = profileLink.subnames.front();
    const auto parsed = topo::parseInternalSubshapeName(subname);
    if (!parsed || parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               featureName + " Profile.SubList requires an InternalFaceN subshape",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               subname);
        return std::nullopt;
    }
    if (profileLink.stableSubnamesExplicit) {
        const std::string stableSubname =
            profileLink.stableSubnames.size() == 1U ? profileLink.stableSubnames.front() : std::string{};
        if (!stableSubname.empty()) {
            const bool requestLocalStableSubname = topo::parseInternalSubshapeName(stableSubname).has_value();
            if (!requestLocalStableSubname && !profileLink.referenceShadows.empty()) {
                // ReferenceShadow is the approved stateless evidence channel while Sketch
                // InternalShape ElementMap is still incomplete; topo/reference_matcher owns
                // the actual fingerprint check, not the Pad executor.
            }
            else {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
                // ::getInternalElementMap() is what makes Internal* names traceable through ElementMap.
                // cad-core has not yet migrated that Sketch InternalShape NamedShape/ElementMap, so an
                // explicit StableSubList for InternalFaceN would persist a request-local selector as stable.
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_stable_subname",
                                       featureName + " Profile.StableSubList cannot reference request-local " + stableSubname
                                           + " before Sketch InternalShape ElementMap is available",
                                       object.name,
                                       "Profile",
                                       "runtime",
                                       profileLink.object,
                                       stableSubname);
                return std::nullopt;
            }
        }
    }

    const auto subshape = topo::subshapeByName(*shapeValue.internalShape, *parsed);
    if (!subshape || subshape->IsNull()) {
        for (const auto& shadow : profileLink.referenceShadows) {
            if (!shadow.target.empty() && shadow.target != profileLink.object) {
                continue;
            }
            const auto targetObjectIt = context.documentObjects.find(profileLink.object);
            if (targetObjectIt != context.documentObjects.end() && shadow.targetId != targetObjectIt->second->id) {
                continue;
            }
            topo::ReferenceMatchResult match;
            if (shadow.brep && shadow.brep->format == "brep-text") {
                std::string brepError;
                match = topo::findUniqueSubshapeByReferenceBrepText(*shapeValue.internalShape,
                                                                    "Internal",
                                                                    shadow.brep->data,
                                                                    shadow.brep->byteLength,
                                                                    shadow.shapeType,
                                                                    brepError);
            }
            else {
                match = topo::findUniqueSubshapeByReferenceFingerprint(*shapeValue.internalShape,
                                                                       "Internal",
                                                                       shadow.fingerprint,
                                                                       shadow.shapeType);
            }
            if (match.status == topo::ReferenceMatchStatus::Unique && match.shape && !match.shape->IsNull()) {
                return *match.shape;
            }
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile target " + profileLink.object + " has no subshape " + subname,
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               subname);
        return std::nullopt;
    }
    for (const auto& shadow : profileLink.referenceShadows) {
        if (!shadow.brep || shadow.brep->format != "brep-text") {
            continue;
        }
        if (!shadow.target.empty() && shadow.target != profileLink.object) {
            continue;
        }
        const auto targetObjectIt = context.documentObjects.find(profileLink.object);
        if (targetObjectIt != context.documentObjects.end() && shadow.targetId != targetObjectIt->second->id) {
            continue;
        }
        if (!topo::referenceFingerprintDriftReason(*subshape, shadow.fingerprint, shadow.shapeType)) {
            continue;
        }

        std::string brepError;
        const auto match = topo::findUniqueSubshapeByReferenceBrepText(*shapeValue.internalShape,
                                                                       "Internal",
                                                                       shadow.brep->data,
                                                                       shadow.brep->byteLength,
                                                                       shadow.shapeType,
                                                                       brepError);
        if (match.status == topo::ReferenceMatchStatus::Unique && match.shape && !match.shape->IsNull()
            && !topo::referenceFingerprintDriftReason(*match.shape, shadow.fingerprint, shadow.shapeType)) {
            return *match.shape;
        }
    }
    return *subshape;
}

double throughAllLength(const TopoDS_Shape& base, const TopoDS_Shape& profile)
{
    // FreeCAD semantic source:
    // src/Mod/PartDesign/App/FeatureSketchBased.cpp ProfileBased::getThroughAllLength().
    Bnd_Box box;
    BRepBndLib::Add(base, box);
    if (!profile.IsNull()) {
        BRepBndLib::Add(profile, box);
    }
    box.SetGap(0.0);
    return 2.02 * std::sqrt(box.SquareExtent());
}

std::optional<gp_Pnt> shapeCenter(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (box.IsVoid()) {
        return std::nullopt;
    }

    double xmin = 0.0;
    double ymin = 0.0;
    double zmin = 0.0;
    double xmax = 0.0;
    double ymax = 0.0;
    double zmax = 0.0;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    return gp_Pnt((xmin + xmax) / 2.0, (ymin + ymax) / 2.0, (zmin + zmax) / 2.0);
}

std::optional<gp_Pnt> profileSurfaceCenter(const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::findAllFacesCutBy(),
    // "Find the centre of gravity of the face" via BRepGProp::SurfaceProperties(face.getShape(), props).
    GProp_GProps props;
    BRepGProp::SurfaceProperties(profile, props);
    if (props.Mass() > Precision::Confusion()) {
        return props.CentreOfMass();
    }

    return shapeCenter(profile);
}

std::vector<CutFaceCandidate> findFacesCutByDirection(const TopoDS_Shape& target,
                                                      const TopoDS_Shape& profile,
                                                      const gp_Dir& direction)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::findAllFacesCutBy(),
    // creates "a line through the centre of gravity" and stores each hit face with "newF.distsq = dsq".
    std::vector<CutFaceCandidate> result;
    const auto center = profileSurfaceCenter(profile);
    if (!center) {
        return result;
    }

    const gp_Lin line(*center, direction);
    BRepIntCurveSurface_Inter intersection;
    for (intersection.Init(target, line, Precision::Confusion()); intersection.More(); intersection.Next()) {
        const gp_Pnt point = intersection.Pnt();
        const double distanceSquared = center->SquareDistance(point);
        if (distanceSquared < Precision::Confusion()) {
            continue;
        }

        gce_MakeDir pointDirection(*center, point);
        if (!pointDirection.IsDone()) {
            continue;
        }
        if (pointDirection.Value().IsOpposite(direction, Precision::Confusion())) {
            continue;
        }

        result.push_back(CutFaceCandidate{intersection.Face(), distanceSquared});
    }

    return result;
}

std::optional<double> signedDistanceToFacePlane(const TopoDS_Shape& profile,
                                                const TopoDS_Face& face,
                                                const gp_Dir& direction,
                                                bool requireSketchClearance,
                                                const document::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const std::string& property)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Only planar up-to faces are supported before full FreeCAD makeElementPrismUntil migration",
                               object.name,
                               property);
        return std::nullopt;
    }

    const gp_Dir normal = surface.Plane().Axis().Direction();
    const double denominator = gp_Vec(direction).Dot(gp_Vec(normal));
    if (std::abs(denominator) < Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Up-to face must not be parallel to extrusion direction",
                               object.name,
                               property);
        return std::nullopt;
    }

    if (requireSketchClearance) {
        BRepExtrema_DistShapeShape distance(profile, face);
        distance.Perform();
        if (!distance.IsDone() || distance.Value() < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Up-to face must not intersect the sketch profile",
                                   object.name,
                                   property);
            return std::nullopt;
        }
    }

    const auto center = shapeCenter(profile);
    if (!center) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Could not measure profile bounds", object.name, "Profile");
        return std::nullopt;
    }

    const gp_Pnt planePoint = surface.Plane().Location();
    const double numerator = gp_Vec(*center, planePoint).Dot(gp_Vec(normal));
    return numerator / denominator;
}

std::optional<PlanarLimit> measureFaceLimit(const TopoDS_Shape& profile,
                                            const TopoDS_Face& face,
                                            const gp_Dir& initialDirection,
                                            const document::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const std::string& property)
{
    gp_Dir direction = initialDirection;
    auto length = signedDistanceToFacePlane(profile, face, direction, true, object, context, property);
    if (!length) {
        return std::nullopt;
    }

    if (*length < -Precision::Confusion()) {
        direction.Reverse();
        *length = -*length;
    }
    if (*length <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Up-to face is not reachable from the sketch profile",
                               object.name,
                               property);
        return std::nullopt;
    }

    return PlanarLimit{direction, *length};
}

std::optional<PlanarLimit> selectFaceLimitFromShape(const TopoDS_Shape& profile,
                                                    const TopoDS_Shape& target,
                                                    const gp_Dir& initialDirection,
                                                    const document::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const std::string& property)
{
    struct Candidate {
        TopoDS_Face face;
        gp_Dir direction;
        double length = 0.0;
    };

    auto findCandidate = [&](const gp_Dir& direction) -> std::optional<Candidate> {
        std::optional<Candidate> best;
        for (TopExp_Explorer explorer(target, TopAbs_FACE); explorer.More(); explorer.Next()) {
            const TopoDS_Face face = TopoDS::Face(explorer.Current());
            BRepAdaptor_Surface surface(face);
            if (surface.GetType() != GeomAbs_Plane) {
                continue;
            }

            BRepExtrema_DistShapeShape distance(profile, face);
            distance.Perform();
            if (!distance.IsDone() || distance.Value() < Precision::Confusion()) {
                continue;
            }

            const gp_Dir normal = surface.Plane().Axis().Direction();
            const double denominator = gp_Vec(direction).Dot(gp_Vec(normal));
            if (std::abs(denominator) < Precision::Confusion()) {
                continue;
            }

            const auto center = shapeCenter(profile);
            if (!center) {
                continue;
            }
            const double numerator = gp_Vec(*center, surface.Plane().Location()).Dot(gp_Vec(normal));
            const double length = numerator / denominator;
            if (length <= Precision::Confusion()) {
                continue;
            }
            if (!best || length < best->length) {
                best = Candidate{face, direction, length};
            }
        }
        return best;
    };

    auto candidate = findCandidate(initialDirection);
    if (!candidate) {
        gp_Dir reversed = initialDirection;
        reversed.Reverse();
        candidate = findCandidate(reversed);
    }
    if (!candidate) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Up-to shape did not provide a reachable planar limit face",
                               object.name,
                               property);
        return std::nullopt;
    }

    return PlanarLimit{candidate->direction, candidate->length};
}

std::optional<PlanarLimit> selectFirstLastLimitFromBase(const TopoDS_Shape& profile,
                                                        const TopoDS_Shape& base,
                                                        const gp_Dir& direction,
                                                        const std::string& method,
                                                        const document::DocumentObject& object,
                                                        runtime::ComputeContext& context,
                                                        const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getUpToFace(), for "UpToLast" / "UpToFirst" calls Part::findAllFacesCutBy(support,
    // sketchshape, dir), then chooses the face with greatest / smallest "distsq".
    const auto candidates = findFacesCutByDirection(base, profile, direction);
    if (candidates.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "No faces found in this direction",
                               object.name,
                               property);
        return std::nullopt;
    }

    auto selected = candidates.begin();
    for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        if (method == "UpToLast") {
            if (it->distanceSquared > selected->distanceSquared) {
                selected = it;
            }
        }
        else if (it->distanceSquared < selected->distanceSquared) {
            selected = it;
        }
    }

    return measureFaceLimit(profile, selected->face, direction, object, context, property);
}

std::optional<TopoDS_Face> resolveFaceLink(const document::Link& link,
                                           const document::DocumentObject& object,
                                           runtime::ComputeContext& context,
                                           const std::string& property)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference exactly one FaceN subshape",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }

    const std::string& subname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front() : std::string{};
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " did not produce a solid",
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }

    std::string currentSubname = subname;
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved = topo::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != subname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(property, link.object, stableSubname, resolved.status),
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    const auto parsed = topo::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid subshape name " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " requires a face subshape, not " + topo::subshapeKindName(parsed->kind),
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = topo::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = topo::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + link.object + " has no subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    return TopoDS::Face(*subshape);
}

std::optional<PlanarLimit> resolveUpToFaceLimit(const document::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const TopoDS_Shape& profile,
                                                const gp_Dir& direction,
                                                const std::string& property)
{
    if (!document::hasPropertyType(object, property, "App::PropertyLinkSub")) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " Type requires an App::PropertyLinkSub " + property + " property",
                               object.name,
                               property);
        return std::nullopt;
    }

    const auto link = document::readLink(object, property);
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " must link to a target face",
                               object.name,
                               property);
        return std::nullopt;
    }

    const auto face = resolveFaceLink(*link, object, context, property);
    if (!face) {
        return std::nullopt;
    }

    return measureFaceLimit(profile, *face, direction, object, context, property);
}

std::optional<PlanarLimit> resolveUpToShapeLimit(const document::DocumentObject& object,
                                                 runtime::ComputeContext& context,
                                                 const TopoDS_Shape& profile,
                                                 const gp_Dir& direction,
                                                 const std::string& property)
{
    if (!document::hasPropertyType(object, property, "App::PropertyLinkSubList")) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " Type requires an App::PropertyLinkSubList " + property + " property",
                               object.name,
                               property);
        return std::nullopt;
    }

    const std::vector<document::Link> links = document::readLinks(object, property);
    if (links.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference one shape or one face",
                               object.name,
                               property);
        return std::nullopt;
    }
    if (links.size() != 1U || links.front().subnames.size() > 1U) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "Only a single " + property + " target shape or face is supported before multi-face UpToShape",
                               object.name,
                               property);
        return std::nullopt;
    }

    const document::Link& link = links.front();
    if (!link.subnames.empty() && !link.subnames.front().empty()) {
        const auto face = resolveFaceLink(link, object, context, property);
        if (!face) {
            return std::nullopt;
        }
        return measureFaceLimit(profile, *face, direction, object, context, property);
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " did not produce a solid",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }

    return selectFaceLimitFromShape(profile, shapeIt->second.shape, direction, object, context, property);
}

std::optional<PlanarLimit> resolveUpToFirstLastLimit(const document::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const TopoDS_Shape& profile,
                                                     const gp_Dir& direction,
                                                     const std::string& method,
                                                     const std::string& property,
                                                     const std::string& featureName)
{
    const auto base = previousSolidShape(context);
    if (!base) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               featureName + " " + method + " requires a previous base solid",
                               object.name,
                               property);
        return std::nullopt;
    }

    return selectFirstLastLimitFromBase(profile, *base, direction, method, object, context, property);
}

std::optional<double> readNumberProperty(const document::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::string& property,
                                         const std::string& featureName)
{
    const auto value = document::readNumber(object, property);
    if (!value) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               featureName + " " + property + " must be a number",
                               object.name,
                               property);
        return std::nullopt;
    }
    return *value;
}

double readOptionalNumberProperty(const document::DocumentObject& object, const std::string& property)
{
    return document::readNumber(object, property).value_or(0.0);
}

bool readBoolProperty(const document::DocumentObject& object, const std::string& property, bool fallback)
{
    return document::readBool(object, property).value_or(fallback);
}

std::string readStringProperty(const document::DocumentObject& object, const std::string& property, const std::string& fallback)
{
    return document::readString(object, property).value_or(fallback);
}

double degreesToRadians(double degrees)
{
    constexpr double pi = 3.14159265358979323846;
    return degrees * pi / 180.0;
}

std::optional<gp_Vec> readVector3Property(const document::DocumentObject& object, const std::string& property)
{
    const auto value = document::readVector3(object, property);
    if (!value) {
        return std::nullopt;
    }
    return gp_Vec(value->at(0), value->at(1), value->at(2));
}

std::optional<gp_Dir> profileNormal(const TopoDS_Shape& profile)
{
    for (TopExp_Explorer explorer(profile, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Plane) {
            return surface.Plane().Axis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> sketchAxisDirection(const TopoDS_Shape& profile, const std::string& subname)
{
    for (TopExp_Explorer explorer(profile, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            continue;
        }

        const gp_Pln plane = surface.Plane();
        if (subname == "N_Axis") {
            return plane.Axis().Direction();
        }
        if (subname == "H_Axis") {
            return plane.XAxis().Direction();
        }
        if (subname == "V_Axis") {
            return plane.YAxis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> edgeAxisDirection(const TopoDS_Edge& edge,
                                        const document::DocumentObject& object,
                                        runtime::ComputeContext& context,
                                        const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getAxis(), "getAxisFromEdge" accepts straight line, circle or arc of circle.
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Line) {
        return curve.Line().Direction();
    }
    if (curve.GetType() == GeomAbs_Circle) {
        return curve.Circle().Axis().Direction();
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_subshape_kind",
                           "ReferenceAxis edge must be a straight line, circle or arc of circle",
                           object.name,
                           property,
                           "runtime");
    return std::nullopt;
}

std::optional<gp_Dir> coordinateSystemAxisDirection(const runtime::ComputeContext& context,
                                                    const std::string& objectName,
                                                    const std::string& subname)
{
    gp_Dir direction(0, 0, 1);
    if (subname == "X" || subname == "X_Axis") {
        direction = gp_Dir(1, 0, 0);
    }
    else if (subname == "Y" || subname == "Y_Axis") {
        direction = gp_Dir(0, 1, 0);
    }
    else if (subname == "Z" || subname == "Z_Axis") {
        direction = gp_Dir(0, 0, 1);
    }
    else {
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(objectName);
    if (placementIt != context.globalPlacements.end()) {
        direction.Transform(placementIt->second);
    }
    return direction;
}

std::optional<gp_Dir> resolveReferenceAxisDirection(const document::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const document::Link& profileLink,
                                                    const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::computeDirection(), reads "ReferenceAxis" and forwards to
    // ProfileBased::getAxis(..., ForbiddenAxis::NotPerpendicularWithNormal).
    const auto* property = document::propertyValue(object, "ReferenceAxis");
    if (property == nullptr || property->raw.is_null()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "ReferenceAxis must link to a sketch axis or edge",
                               object.name,
                               "ReferenceAxis");
        return std::nullopt;
    }

    const auto link = document::readLink(object, "ReferenceAxis");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "ReferenceAxis must link to a sketch axis, datum line or edge",
                               object.name,
                               "ReferenceAxis");
        return std::nullopt;
    }

    const std::string subname = link->subnames.empty() ? std::string{} : link->subnames.front();
    if (link->object == profileLink.object && !subname.empty()) {
        auto axis = sketchAxisDirection(profile, subname);
        if (axis) {
            return axis;
        }
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "ReferenceAxis target " + link->object + " did not produce a shape",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::ProfileBased::getAxis(), for "PartDesign::Line" uses line->getDirection() directly
        // without requiring a sub-element name.
        return edgeAxisDirection(TopoDS::Edge(shapeIt->second.shape), object, context, "ReferenceAxis");
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumCoordinateSystem) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumCS.cpp
        // ::CoordinateSystem::getXAxis/getYAxis/getZAxis() rotate unit axes by Placement.
        const auto axis = coordinateSystemAxisDirection(context, link->object, subname);
        if (axis) {
            return axis;
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ReferenceAxis CoordinateSystem supports X_Axis, Y_Axis or Z_Axis",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    if (link->subnames.size() != 1U || subname.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "ReferenceAxis must reference exactly one subname unless it links to a DatumLine",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    const auto parsed = topo::parseSubshapeName(subname);
    if (!parsed || parsed->kind != TopAbs_EDGE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ReferenceAxis supports sketch N_Axis/H_Axis/V_Axis or EdgeN",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    const auto subshape = topo::subshapeByName(shapeIt->second.shape, subname);
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ReferenceAxis target " + link->object + " has no subshape " + subname,
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               link->object,
                               subname);
        return std::nullopt;
    }

    return edgeAxisDirection(TopoDS::Edge(*subshape), object, context, "ReferenceAxis");
}

std::optional<DirectionSpec> computeDirection(const document::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const document::Link& profileLink,
                                              const TopoDS_Shape& profile,
                                              AddSubMode mode)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::computeDirection(),
    // falls back to the sketch normal, reads "UseCustomVector", "Direction", "ReferenceAxis" and later applies "AlongSketchNormal".
    gp_Dir sketchNormal = profileNormal(profile).value_or(gp_Dir(0.0, 0.0, 1.0));
    if (mode == AddSubMode::Subtractive) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp::Pocket::getProfileNormal(),
        // returns FeatureExtrude::getProfileNormal() * -1.
        sketchNormal.Reverse();
    }

    const bool useCustomVector = readBoolProperty(object, "UseCustomVector", false);
    gp_Dir direction = sketchNormal;
    if (useCustomVector) {
        if (document::propertyValue(object, "Direction") == nullptr) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_direction",
                                   "UseCustomVector requires a Direction vector",
                                   object.name,
                                   "Direction");
            return std::nullopt;
        }
        const auto vector = readVector3Property(object, "Direction");
        if (!vector || vector->Magnitude() < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_direction",
                                   "Direction must be a non-zero 3D vector",
                                   object.name,
                                   "Direction");
            return std::nullopt;
        }
        direction = gp_Dir(*vector);
    }
    else if (const auto* axisProperty = document::propertyValue(object, "ReferenceAxis"); axisProperty != nullptr && !axisProperty->raw.is_null()) {
        const auto axis = resolveReferenceAxisDirection(object, context, profileLink, profile);
        if (!axis) {
            return std::nullopt;
        }
        direction = *axis;
        if (mode == AddSubMode::Subtractive) {
            direction.Reverse();
        }
    }

    const double factor = std::abs(gp_Vec(direction).Dot(gp_Vec(sketchNormal)));
    if (factor < Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_direction",
                               "Extrusion direction must not be orthogonal to the sketch normal",
                               object.name,
                               useCustomVector ? "Direction" : "ReferenceAxis");
        return std::nullopt;
    }

    const bool alongSketchNormal = readBoolProperty(object, "AlongSketchNormal", true);
    return DirectionSpec{direction, sketchNormal, alongSketchNormal ? 1.0 / factor : 1.0};
}

TopoDS_Shape translatedShape(const TopoDS_Shape& shape, const gp_Vec& translation)
{
    gp_Trsf transform;
    transform.SetTranslation(translation);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

std::optional<SideBuild> makePrismSide(const TopoDS_Shape& profile,
                                       const gp_Dir& direction,
                                       double length,
                                       const document::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const document::Link& profileLink,
                                       const std::string& method,
                                       const std::string& featureName,
                                       const std::string& historyOwner)
{
    BRepPrimAPI_MakePrism prism(profile, length * gp_Vec(direction));
    prism.Build();
    if (!prism.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not extrude " + featureName + " profile",
                               object.name);
        return std::nullopt;
    }
    auto namedShape = topo::namedShapeForMakerHistory(historyOwner,
                                                      prism.Shape(),
                                                      profileLink.object,
                                                      profile,
                                                      prism);
    return SideBuild{method, length, prism.Shape(), false, std::move(namedShape)};
}

std::string taperComponentOwner(const std::string& historyOwner, std::size_t index, std::size_t count)
{
    if (count <= 1U) {
        return historyOwner;
    }
    if (index == 0U) {
        return historyOwner + ".Outer";
    }
    return historyOwner + ".Inner" + std::to_string(index);
}

topo::NamedShape namedShapeForTaperComponent(const std::string& componentOwner,
                                             const geometry::TaperedExtrusionHistoryComponent& component,
                                             const TopoDS_Shape& profile,
                                             const topo::NamedShapeSource& profileSource)
{
    if (component.historyMaker && !component.historySources.empty()) {
        std::vector<topo::NamedShapeSource> sources;
        sources.reserve(component.historySources.size());
        sources.push_back(topo::NamedShapeSource{profileSource.owner, profile, profileSource.namedShape});
        for (std::size_t index = 1; index < component.historySources.size(); ++index) {
            sources.push_back(topo::NamedShapeSource{componentOwner + ".TaperSection" + std::to_string(index + 1),
                                                     component.historySources.at(index)});
        }
        if (auto* thruSections = dynamic_cast<BRepOffsetAPI_ThruSections*>(component.historyMaker.get())) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::MapperThruSections::generated(), adds GeneratedFace(), FirstShape() and LastShape()
            // to the generic BRepBuilderAPI_MakeShape mapper used by makeElementShape().
            return topo::namedShapeForThruSectionsHistory(componentOwner,
                                                          component.shape,
                                                          sources,
                                                          *thruSections,
                                                          component.historySources.front(),
                                                          component.historySources.back());
        }
        return topo::namedShapeForMakerHistory(componentOwner, component.shape, sources, *component.historyMaker);
    }
    return topo::namedShapeForPreservedSources(componentOwner, component.shape, {profileSource});
}

std::optional<topo::NamedShape> namedShapeForTaperedExtrusion(const std::string& historyOwner,
                                                             const geometry::TaperedExtrusionResult& tapered,
                                                             const TopoDS_Shape& profile,
                                                             const topo::NamedShapeSource& profileSource)
{
    if (tapered.historyComponents.empty()) {
        return std::nullopt;
    }

    const std::size_t count = tapered.historyComponents.size();
    std::string currentOwner = taperComponentOwner(historyOwner, 0, count);
    TopoDS_Shape currentShape = tapered.historyComponents.front().shape;
    topo::NamedShape currentNamedShape =
        namedShapeForTaperComponent(currentOwner, tapered.historyComponents.front(), profile, profileSource);

    for (std::size_t index = 1; index < count; ++index) {
        const std::string innerOwner = taperComponentOwner(historyOwner, index, count);
        topo::NamedShape innerNamedShape =
            namedShapeForTaperComponent(innerOwner, tapered.historyComponents.at(index), profile, profileSource);
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
        // ::ExtrusionHelper::makeElementDraft(), "Inner wires are lofted into separate solids and
        // then cut from the outer solid"; cad-core routes the same owner chain through topo boolean
        // history so inner-wire generated sources survive the final taper result.
        const auto cut = topo::makeElementBooleanFromSources(
            historyOwner,
            {
                topo::NamedShapeSource{currentOwner, currentShape, &currentNamedShape},
                topo::NamedShapeSource{innerOwner, tapered.historyComponents.at(index).shape, &innerNamedShape},
            },
            topo::BooleanOperation::Cut);
        if (cut.error.empty() && cut.namedShape) {
            currentOwner = historyOwner + ".InnerCut" + std::to_string(index);
            currentShape = cut.shape;
            currentNamedShape = *cut.namedShape;
        }
    }

    currentNamedShape.owner = historyOwner;
    currentNamedShape.shape = tapered.shape;
    return currentNamedShape;
}

std::optional<SideBuild> makeExtrusionShape(const document::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const TopoDS_Shape& profile,
                                            const gp_Dir& direction,
                                            double length,
                                            double taperAngleDegrees,
                                            const std::string& method,
                                            const std::string& taperProperty,
                                            const std::string& featureName,
                                            const document::Link& profileLink,
                                            const std::string& historyOwner)
{
    if (std::abs(taperAngleDegrees) <= Precision::Angular()) {
        return makePrismSide(profile, direction, length, object, context, profileLink, method, featureName, historyOwner);
    }

    std::string error;
    const auto tapered = geometry::makeTaperedExtrusion(profile,
                                                        geometry::TaperedExtrusionOptions{
                                                            direction,
                                                            length,
                                                            degreesToRadians(taperAngleDegrees),
                                                            true,
                                                        },
                                                        error);
    if (!tapered) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_taper",
                               error.empty() ? "Could not build tapered extrusion" : error,
                               object.name,
                               taperProperty);
        return std::nullopt;
    }
    topo::NamedShapeSource profileSource{profileLink.object, profile};
    const auto profileNamedShapeIt = context.namedShapes.find(profileLink.object);
    if (profileNamedShapeIt != context.namedShapes.end()) {
        profileSource.namedShape = &profileNamedShapeIt->second;
    }
    auto namedShape = namedShapeForTaperedExtrusion(historyOwner, *tapered, profile, profileSource)
        .value_or(topo::namedShapeForPreservedSources(historyOwner, tapered->shape, {profileSource}));
    return SideBuild{method, length, tapered->shape, tapered->topoNamingKnownGap, std::move(namedShape)};
}

std::optional<ToolShapeBuild> xorToolShapes(const std::vector<SideBuild>& sides,
                                            const document::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const std::string& featureName)
{
    if (sides.empty()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "No extrusion geometry was generated", object.name);
        return std::nullopt;
    }

    std::vector<topo::NamedShapeSource> sources;
    sources.reserve(sides.size());
    for (std::size_t index = 0; index < sides.size(); ++index) {
        topo::NamedShapeSource source{object.name + ".Prism" + std::to_string(index + 1), sides.at(index).shape};
        if (sides.at(index).namedShape) {
            source.namedShape = &*sides.at(index).namedShape;
        }
        sources.push_back(source);
    }

    const auto result = topo::makeElementXorFromSources(object.name, sources);
    if (!result.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               result.error + " while combining " + featureName + " extrusion sides",
                               object.name,
                               "SideType");
        return std::nullopt;
    }
    return ToolShapeBuild{result.shape, result.namedShape};
}

std::optional<SideBuild> buildSingleSide(const document::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const TopoDS_Shape& profile,
                                         const document::Link& profileLink,
                                         const SideSpec& side,
                                         AddSubMode mode,
                                         const std::string& featureName,
                                         double lengthScale,
                                         const std::string& historyOwner)
{
    const std::string method = readStringProperty(object, side.typeProperty, "Length");
    const double taper = readOptionalNumberProperty(object, side.taperProperty);
    const double offset = readOptionalNumberProperty(object, side.offsetProperty);
    if (std::abs(offset) > Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               side.offsetProperty + " requires the full FreeCAD UpTo offset path",
                               object.name,
                               side.offsetProperty);
        return std::nullopt;
    }

    double length = 0.0;
    gp_Dir direction = side.direction;
    if (method == "Length") {
        const auto rawLength = readNumberProperty(object, context, side.lengthProperty, featureName);
        if (!rawLength) {
            return std::nullopt;
        }
        length = *rawLength * lengthScale;
    }
    else if (method == "ThroughAll") {
        if (mode != AddSubMode::Subtractive) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Pad does not support Type=ThroughAll",
                                   object.name,
                                   side.typeProperty);
            return std::nullopt;
        }
        const auto base = previousSolidShape(context);
        if (!base) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Pocket ThroughAll requires a previous base solid",
                                   object.name,
                                   side.typeProperty);
            return std::nullopt;
        }
        length = throughAllLength(*base, profile) * lengthScale;
    }
    else if (method == "UpToFirst" || method == "UpToLast") {
        const auto limit = resolveUpToFirstLastLimit(object, context, profile, direction, method, side.typeProperty, featureName);
        if (!limit) {
            return std::nullopt;
        }
        direction = limit->direction;
        length = limit->length;
    }
    else if (method == "UpToFace") {
        const auto limit = resolveUpToFaceLimit(object, context, profile, direction, side.upToFaceProperty);
        if (!limit) {
            return std::nullopt;
        }
        direction = limit->direction;
        length = limit->length;
    }
    else if (method == "UpToShape") {
        const auto limit = resolveUpToShapeLimit(object, context, profile, direction, side.upToShapeProperty);
        if (!limit) {
            return std::nullopt;
        }
        direction = limit->direction;
        length = limit->length;
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported " + featureName + " " + side.typeProperty + " " + method + " in P3b",
                               object.name,
                               side.typeProperty);
        return std::nullopt;
    }

    if (std::abs(length) < Precision::Confusion()) {
        return SideBuild{method, length, TopoDS_Shape{}, false};
    }

    return makeExtrusionShape(
        object, context, profile, direction, length, taper, method, side.taperProperty, featureName, profileLink, historyOwner);
}

}  // namespace

std::optional<ExtrudeResult> buildFeatureExtrusion(const document::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   AddSubMode mode,
                                                   const std::string& featureName)
{
    // FreeCAD semantic source:
    // src/Mod/PartDesign/App/FeatureExtrude.cpp FeatureExtrude::buildExtrusion().
    if (document::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto profileLink = document::readLink(object, "Profile");
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const std::string sideType = readStringProperty(object, "SideType", "One side");
    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end()
        || (shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Profile)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a profile",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }
    std::optional<TopoDS_Shape> selectedProfileShape;
    const TopoDS_Shape* profileShape = nullptr;
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch) {
        selectedProfileShape = resolveSketchInternalFaceProfile(object, context, *profileLink, shapeIt->second, featureName);
        profileShape = selectedProfileShape ? &*selectedProfileShape : nullptr;
        const bool explicitSubshape = !profileLink->subnames.empty();
        const bool ambiguousMultiFace = shapeIt->second.profileRequiresSubshapeSelection;
        if (profileShape == nullptr && (explicitSubshape || ambiguousMultiFace)) {
            return std::nullopt;
        }
    }
    else {
        profileShape = &shapeIt->second.shape;
    }
    if (profileShape == nullptr || profileShape->IsNull()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::getTopoShapeVerifiedFace(),
        // throws "Cannot make face from profile" when linked sketch wires cannot become a face.
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               "Profile target " + profileLink->object + " did not produce a closed profile face",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const bool reversed = readBoolProperty(object, "Reversed", false);
    auto direction = computeDirection(object, context, *profileLink, *profileShape, mode);
    if (!direction) {
        return std::nullopt;
    }
    if (reversed) {
        direction->direction.Reverse();
    }

    gp_Dir secondDirection = direction->direction;
    secondDirection.Reverse();
    const SideSpec side1{
        "Type",
        "Length",
        "UpToFace",
        "UpToShape",
        "TaperAngle",
        "Offset",
        direction->direction,
    };
    const SideSpec side2{
        "Type2",
        "Length2",
        "UpToFace2",
        "UpToShape2",
        "TaperAngle2",
        "Offset2",
        secondDirection,
    };

    std::vector<SideBuild> prisms;
    std::string method = readStringProperty(object, "Type", "Length");
    double reportedLength = 0.0;
    bool topoNamingKnownGap = false;
    std::optional<topo::NamedShape> resultNamedShape;

    if (sideType == "One side") {
        auto side = buildSingleSide(
            object, context, *profileShape, *profileLink, side1, mode, featureName, direction->lengthScale, object.name);
        if (!side) {
            return std::nullopt;
        }
        if (std::abs(side->length) < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   featureName + " Length must not be zero",
                                   object.name,
                                   "Length");
            return std::nullopt;
        }
        prisms.push_back(*side);
        method = side->method;
        reportedLength = side->length;
        topoNamingKnownGap = side->topoNamingKnownGap;
        resultNamedShape = side->namedShape;
    }
    else if (sideType == "Two sides") {
        const std::string method1 = readStringProperty(object, "Type", "Length");
        const std::string method2 = readStringProperty(object, "Type2", "Length");
        const double taper1 = readOptionalNumberProperty(object, "TaperAngle");
        const double taper2 = readOptionalNumberProperty(object, "TaperAngle2");
        if (method1 == "Length" && method2 == "Length" && std::abs(taper1) <= Precision::Angular()
            && std::abs(taper2) <= Precision::Angular()) {
            const auto length1 = readNumberProperty(object, context, "Length", featureName);
            const auto length2 = readNumberProperty(object, context, "Length2", featureName);
            if (!length1 || !length2) {
                return std::nullopt;
            }
            const double scaledLength1 = *length1 * direction->lengthScale;
            const double scaledLength2 = *length2 * direction->lengthScale;
            const double totalLength = scaledLength1 + scaledLength2;
            if (std::abs(totalLength) < Precision::Confusion()) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_length",
                                       featureName + " Two sides total length must not be zero",
                                       object.name,
                                       "Length2");
                return std::nullopt;
            }
            const TopoDS_Shape movedProfile = translatedShape(*profileShape,
                                                              -scaledLength2 * gp_Vec(direction->direction));
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::buildExtrusion(), Two sides no-taper fast path copies and moves
            // "moved_sketch" before generateSingleExtrusionSide(); keep that single-prism maker
            // history instead of falling back to indexed-only topo names.
            const auto prism = makePrismSide(movedProfile,
                                             direction->direction,
                                             totalLength,
                                             object,
                                             context,
                                             *profileLink,
                                             "Two sides",
                                             featureName,
                                             object.name);
            if (!prism) {
                return std::nullopt;
            }
            prisms.push_back(*prism);
            reportedLength = totalLength;
            resultNamedShape = prism->namedShape;
        }
        else {
            auto first = buildSingleSide(object,
                                         context,
                                         *profileShape,
                                         *profileLink,
                                         side1,
                                         mode,
                                         featureName,
                                         direction->lengthScale,
                                         object.name + ".Prism1");
            if (!first) {
                return std::nullopt;
            }
            auto second = buildSingleSide(object,
                                          context,
                                          *profileShape,
                                          *profileLink,
                                          side2,
                                          mode,
                                          featureName,
                                          direction->lengthScale,
                                          object.name + ".Prism2");
            if (!second) {
                return std::nullopt;
            }
            if (!first->shape.IsNull()) {
                prisms.push_back(*first);
            }
            if (!second->shape.IsNull()) {
                prisms.push_back(*second);
            }
            reportedLength = first->length + second->length;
            topoNamingKnownGap = first->topoNamingKnownGap || second->topoNamingKnownGap;
        }
        method = "Two sides";
    }
    else if (sideType == "Symmetric") {
        const std::string method1 = readStringProperty(object, "Type", "Length");
        if (method1 != "Length") {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Symmetric currently supports Type=Length; mirrored UpTo is tracked for later P3b work",
                                   object.name,
                                   "SideType");
            return std::nullopt;
        }
        const auto length = readNumberProperty(object, context, "Length", featureName);
        if (!length) {
            return std::nullopt;
        }
        const double scaledLength = *length * direction->lengthScale;
        if (std::abs(scaledLength) < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   featureName + " Symmetric length must not be zero",
                                   object.name,
                                   "Length");
            return std::nullopt;
        }
        const double taper1 = readOptionalNumberProperty(object, "TaperAngle");
        if (std::abs(taper1) > Precision::Angular()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::buildExtrusion(), "TAPERED case: We must create two separate
            // prisms and fuse them" for SideType == "Symmetric".
            const double halfLength = scaledLength / 2.0;
            auto first = makeExtrusionShape(object,
                                            context,
                                            *profileShape,
                                            direction->direction,
                                            halfLength,
                                            taper1,
                                            "Symmetric",
                                            "TaperAngle",
                                            featureName,
                                            *profileLink,
                                            object.name + ".Prism1");
            if (!first) {
                return std::nullopt;
            }
            auto second = makeExtrusionShape(object,
                                             context,
                                             *profileShape,
                                             secondDirection,
                                             halfLength,
                                             taper1,
                                             "Symmetric",
                                             "TaperAngle",
                                             featureName,
                                             *profileLink,
                                             object.name + ".Prism2");
            if (!second) {
                return std::nullopt;
            }
            prisms.push_back(*first);
            prisms.push_back(*second);
            topoNamingKnownGap = first->topoNamingKnownGap || second->topoNamingKnownGap;
        }
        else {
            const TopoDS_Shape movedProfile = translatedShape(*profileShape,
                                                              -0.5 * scaledLength * gp_Vec(direction->direction));
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::buildExtrusion(), Symmetric no-taper fast path creates one prism
            // from a copied sketch translated by -L/2.
            const auto prism = makePrismSide(movedProfile,
                                             direction->direction,
                                             scaledLength,
                                             object,
                                             context,
                                             *profileLink,
                                             "Symmetric",
                                             featureName,
                                             object.name);
            if (!prism) {
                return std::nullopt;
            }
            prisms.push_back(*prism);
            resultNamedShape = prism->namedShape;
        }
        method = "Symmetric";
        reportedLength = scaledLength;
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported " + featureName + " SideType " + sideType,
                               object.name,
                               "SideType");
        return std::nullopt;
    }

    const auto toolShape = xorToolShapes(prisms, object, context, featureName);
    if (!toolShape) {
        return std::nullopt;
    }
    if (toolShape->namedShape && (!topoNamingKnownGap || !resultNamedShape)) {
        resultNamedShape = toolShape->namedShape;
    }

    return ExtrudeResult{
        *profileLink,
        method,
        reportedLength,
        reversed,
        toolShape->shape,
        geometry::bboxForShape(toolShape->shape),
        geometry::volumeForShape(toolShape->shape),
        topoNamingKnownGap,
        resultNamedShape,
    };
}

}  // namespace cad_core::features
