#include "cad_core/part_design/feature_extrude.h"

#include "cad_core/part_design/profile_resolver.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/edge_axis.h"
#include "cad_core/part/extrusion_helper.h"
#include "cad_core/part/element_map_producer_trace_snapshot.h"
#include "cad_core/part/part_extrusion.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFeat_MakePrism.hxx>
#include <BRepGProp.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gce_MakeDir.hxx>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace cad_core::part_design {

namespace {

struct PlanarLimit {
    gp_Dir direction;
    double length = 0.0;
    TopoDS_Shape untilShape;
    bool prismUntil = false;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
// ::ProfileBased::getUpToShapeFromLinkSubList(), key: "create a unique shell with all selected
// faces"; FeatureExtrude.cpp::FeatureExtrude::makeShellFromUpToShape(), key: "don't use the last
// face so the shell is open and OCC works better".
struct UpToShapeLimit {
    gp_Dir direction;
    double reportLength = 0.0;
    TopoDS_Shape untilShape;
    int faceCount = 0;
    bool prismUntil = false;
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
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/
    // FeatureExtrude.cpp taper branch calls "Part::ExtrusionHelper::makeElementDraft".
    bool taperHistory = false;
    std::optional<part::NamedShape> namedShape;
};

struct ExtrusionProfile {
    app::Link link;
    TopoDS_Shape shape;
    std::optional<gp_Dir> normal;
    ProfileKind kind = ProfileKind::ClosedFace;
    std::vector<std::string> selectedSubnames;
    std::vector<std::string> selectedStableSubnames;
    bool unstableOpenProfileReference = false;
    std::string profileResolveMode;
    std::string profileOwner;
    std::string requestedProfileSubname;
    std::string currentProfileSubname;
};

struct ToolShapeBuild {
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};

struct ThinOpenProfileBuild {
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    std::string side;
};

struct CutFaceCandidate {
    TopoDS_Face face;
    double distanceSquared = 0.0;
};

std::string stableSubnameDiagnosticCode(part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case part::ElementResolveStatus::Split:
            return "split_stable_subname";
        case part::ElementResolveStatus::Resolved:
        case part::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& property,
                                           const std::string& target,
                                           const std::string& stableSubname,
                                           part::ElementResolveStatus status)
{
    if (status == part::ElementResolveStatus::Deleted) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == part::ElementResolveStatus::Split) {
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

std::optional<part::NamedShapeSource> baseSolidSource(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    const auto sourceForName = [&](const std::string& name) -> std::optional<part::NamedShapeSource> {
        const auto shapeIt = context.shapes.find(name);
        if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid
            || shapeIt->second.shape.IsNull()) {
            return std::nullopt;
        }
        const auto namedShapeIt = context.namedShapes.find(name);
        return part::NamedShapeSource {
            namedShapeIt == context.namedShapes.end() ? name : namedShapeIt->second.owner,
            shapeIt->second.shape,
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
        };
    };

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
    // ::BodyBase::addObject() and ::Body::setBaseProperty() establish the PartDesign feature
    // chain from Body.Group. Membership can reroute an older explicit BaseFeature while deleting
    // an intermediate feature: the current member consumes the preceding in-Body solid, not an
    // orphaned document object. This must happen before FeatureExtrude's Boolean producer reads
    // its source ElementMap; repairing Body's published Tip would be too late.
    if (const auto parentIt = context.parentGroupByObject.find(object.name);
        parentIt != context.parentGroupByObject.end()) {
        const auto bodyIt = context.documentObjects.find(parentIt->second);
        if (bodyIt != context.documentObjects.end() && bodyIt->second != nullptr
            && bodyIt->second->typeId == "PartDesign::Body") {
            const std::vector<app::Link> group = app::readLinks(*bodyIt->second, "Group");
            const auto memberIt = std::find_if(group.begin(), group.end(), [&](const app::Link& link) {
                return link.object == object.name;
            });
            if (memberIt != group.end()) {
                for (auto candidate = memberIt; candidate != group.begin();) {
                    --candidate;
                    if (const auto source = sourceForName(candidate->object)) {
                        return source;
                    }
                }
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
                // ::Body::onChanged(), when BaseFeature changes, creates a
                // PartDesign::FeatureBase before the first solid. Body::setBaseProperty()
                // then links that first solid to the FeatureBase. The request graph need not
                // contain the transient child yet, but its Shape property boundary is part of
                // the same recompute and must carry a re-tagged ElementMap into Pad/Pocket.
                const auto bodyBase = app::readLink(*bodyIt->second, "BaseFeature");
                if (bodyBase && !bodyBase->object.empty()) {
                    const auto external = sourceForName(bodyBase->object);
                    if (external && external->namedShape != nullptr) {
                        std::string featureBaseName = "BaseFeature";
                        if (context.documentObjects.count(featureBaseName) != 0U
                            || context.shapes.count(featureBaseName) != 0U) {
                            featureBaseName = bodyIt->second->name + "_BaseFeature";
                        }
                        if (context.shapes.count(featureBaseName) == 0U) {
                            long long nextObjectId = 0;
                            for (const auto& [_, documentObject] : context.documentObjects) {
                                if (documentObject != nullptr) {
                                    nextObjectId = std::max(nextObjectId, documentObject->id);
                                }
                            }
                            const part::NamedShape featureBaseNamedShape =
                                part::namedShapeForPropertyShapeValue(
                                    featureBaseName,
                                    external->shape,
                                    *external->namedShape,
                                    static_cast<long>(nextObjectId + 1)
                                );
                            context.shapes[featureBaseName] = runtime::ShapeValue {
                                runtime::ShapeValue::Kind::Solid,
                                external->shape,
                            };
                            context.namedShapes[featureBaseName] = featureBaseNamedShape;
                        }
                        const auto featureBaseShape = context.shapes.find(featureBaseName);
                        const auto featureBaseNamedShape = context.namedShapes.find(featureBaseName);
                        if (featureBaseShape != context.shapes.end()
                            && featureBaseNamedShape != context.namedShapes.end()) {
                            return part::NamedShapeSource {
                                featureBaseName,
                                featureBaseShape->second.shape,
                                &featureBaseNamedShape->second,
                            };
                        }
                    }
                }
            }
        }
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::Feature::getBaseTopoShape(bool) resolves the hidden BaseFeature PropertyLink and returns
    // `BaseObject->Shape.getShape()`. Execution order is not a substitute for that document
    // link: a Body can contain unrelated solids before this feature, and ElementMap provenance
    // must follow the linked Shape producer.
    if (const auto baseFeature = app::readLink(object, "BaseFeature"); baseFeature) {
        if (baseFeature->object.empty()) {
            return std::nullopt;
        }
        return sourceForName(baseFeature->object);
    }

    // A standalone Pad/Pocket has no BaseFeature. Its only valid implicit base is the preceding
    // solid in this request-local recompute plan, matching FeatureExtrude's empty-base branch.
    for (auto it = context.executionOrder.rbegin(); it != context.executionOrder.rend(); ++it) {
        if (const auto source = sourceForName(*it)) {
            return source;
        }
    }
    return std::nullopt;
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

bool profileLinkTargetsSolidLikeShape(const app::Link& profileLink, const runtime::ComputeContext& context)
{
    const auto shapeIt = context.shapes.find(profileLink.object);
    return shapeIt != context.shapes.end()
        && (shapeIt->second.kind == runtime::ShapeValue::Kind::Solid
            || shapeIt->second.kind == runtime::ShapeValue::Kind::PartPrimitive);
}

bool shapeContainsFace(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return true;
    }
    return false;
}

bool shapeHasNonVoidBounds(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }

    Bnd_Box box;
    try {
        BRepBndLib::AddOptimal(shape, box, Standard_False, Standard_False);
    }
    catch (const Standard_Failure&) {
        return false;
    }
    box.SetGap(0.0);
    return !box.IsVoid();
}

bool additiveFuseProducesNonVoidShape(const std::string& owner,
                                      const TopoDS_Shape& base,
                                      const TopoDS_Shape& tool)
{
    if (!shapeHasNonVoidBounds(base) || !shapeHasNonVoidBounds(tool)) {
        return false;
    }
    const auto build = part::makeElementBooleanFromSources(
        owner + ".DirectionProbe",
        std::vector<part::NamedShapeSource> {
            {owner + ".DirectionProbeBase", base},
            {owner + ".DirectionProbeTool", tool},
        },
        part::BooleanOperation::Fuse);
    return build.error.empty() && shapeHasNonVoidBounds(build.shape);
}

bool shouldProbeAdditiveFacePadDirection(const app::DocumentObject& object,
                                         AddSubMode mode,
                                         const app::Link& profileLink,
                                         const TopoDS_Shape& profile,
                                         bool reversed,
                                         bool displayOnlyOpenProfile,
                                         const runtime::ComputeContext& context)
{
    const auto* axisProperty = app::propertyValue(object, "ReferenceAxis");
    const bool usesReferenceAxis = axisProperty != nullptr && !axisProperty->raw.is_null();
    return mode == AddSubMode::Additive
        && !reversed
        && !displayOnlyOpenProfile
        && app::readString(object, "SideType").value_or("One side") == "One side"
        && app::readString(object, "Type").value_or("Length") == "Length"
        && !app::readBool(object, "UseCustomVector").value_or(false)
        && !usesReferenceAxis
        && !profileLink.subnames.empty()
        && profileLinkTargetsSolidLikeShape(profileLink, context)
        && shapeContainsFace(profile);
}

void setSyntheticProperty(app::DocumentObject& object,
                          const std::string& property,
                          const std::string& propertyType,
                          app::PropertyKind kind,
                          nlohmann::json payload)
{
    nlohmann::json raw = {
        {"PropertyType", propertyType},
        {"value", std::move(payload)},
    };
    object.properties[property] = raw;
    object.propertyValues[property] = app::PropertyValue{
        property,
        propertyType,
        kind,
        std::move(raw),
        {},
        true,
    };
}

app::DocumentObject objectWithCustomDirection(const app::DocumentObject& object, const gp_Dir& direction)
{
    app::DocumentObject result = object;
    setSyntheticProperty(result,
                         "UseCustomVector",
                         "App::PropertyBool",
                         app::PropertyKind::Bool,
                         true);
    setSyntheticProperty(result,
                         "Direction",
                         "App::PropertyVector",
                         app::PropertyKind::Vector,
                         {direction.X(), direction.Y(), direction.Z()});
    return result;
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

int faceCountOf(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

TopoDS_Shape compoundOfShapes(const std::vector<TopoDS_Shape>& shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

std::vector<std::string> appendDistinctStrings(std::vector<std::string> values,
                                               const std::vector<std::string>& incoming)
{
    for (const std::string& value : incoming) {
        if (value.empty()) {
            continue;
        }
        if (std::find(values.begin(), values.end(), value) == values.end()) {
            values.push_back(value);
        }
    }
    return values;
}

std::string firstNonEmptyString(const std::vector<std::string>& values)
{
    const auto it = std::find_if(values.begin(), values.end(), [](const std::string& value) {
        return !value.empty();
    });
    return it == values.end() ? std::string{} : *it;
}

std::string selectedProfileSubname(const ProfileBasedProfileSelection& selection)
{
    if (!selection.selectedSubname.empty()) {
        return selection.selectedSubname;
    }
    return firstNonEmptyString(selection.selectedSubnames);
}

std::optional<ExtrusionProfile> resolveFeatureExtrusionProfile(const app::DocumentObject& object,
                                                               runtime::ComputeContext& context,
                                                               const std::string& featureName,
                                                               OpenProfileMode openProfileMode)
{
    const auto selections = resolveProfileBasedProfilesForExtrusion(
        object,
        context,
        featureName,
        openProfileMode,
        featureName + " Profile must be App::PropertyLinkSubList with SubSet[] entries");
    if (selections.empty()) {
        return std::nullopt;
    }
    if (selections.size() == 1U) {
        return ExtrusionProfile{
            selections.front().link,
            selections.front().shape,
            selections.front().normal,
            selections.front().kind,
            selections.front().selectedSubnames.empty()
                ? std::vector<std::string>{selections.front().selectedSubname}
                : selections.front().selectedSubnames,
            selections.front().selectedStableSubnames.empty()
                ? std::vector<std::string>{selections.front().stableSubname}
                : selections.front().selectedStableSubnames,
            selections.front().unstableOpenProfileReference,
            selections.front().fromBodyCumulativeReplay ? "body_cumulative_replay" : "feature_local",
            selections.front().link.object,
            firstNonEmptyString(selections.front().link.subnames),
            selectedProfileSubname(selections.front()),
        };
    }

    std::vector<TopoDS_Shape> profileShapes;
    profileShapes.reserve(selections.size());
    ProfileKind profileKind = selections.front().kind;
    std::vector<std::string> selectedSubnames;
    std::vector<std::string> selectedStableSubnames;
    bool unstableOpenProfileReference = false;
    bool fromBodyCumulativeReplay = false;
    for (const auto& selection : selections) {
        profileShapes.push_back(selection.shape);
        selectedSubnames = appendDistinctStrings(selectedSubnames, selection.selectedSubnames);
        selectedStableSubnames = appendDistinctStrings(selectedStableSubnames, selection.selectedStableSubnames);
        unstableOpenProfileReference = unstableOpenProfileReference || selection.unstableOpenProfileReference;
        fromBodyCumulativeReplay = fromBodyCumulativeReplay || selection.fromBodyCumulativeReplay;
        if (selection.kind == ProfileKind::EdgeCompound) {
            profileKind = ProfileKind::EdgeCompound;
        }
    }
    TopoDS_Shape profileShape = compoundOfShapes(profileShapes);
    if (profileShape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               featureName + " Profile.SubSet did not produce extrudable profile faces",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    return ExtrusionProfile{
        selections.front().link,
        profileShape,
        selections.front().normal,
        profileKind,
        std::move(selectedSubnames),
        std::move(selectedStableSubnames),
        unstableOpenProfileReference,
        fromBodyCumulativeReplay ? "body_cumulative_replay" : "feature_local",
        selections.front().link.object,
        firstNonEmptyString(selections.front().link.subnames),
        selectedProfileSubname(selections.front()),
    };
}

std::optional<TopoDS_Face> firstFaceOf(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return TopoDS::Face(explorer.Current());
    }
    return std::nullopt;
}

TopoDS_Face supportFaceForPrismUntil(const TopoDS_Shape& profile,
                                     const TopoDS_Face& profileFace,
                                     const runtime::ComputeContext& context)
{
    const auto base = previousSolidShape(context);
    if (!base) {
        return profileFace;
    }

    for (TopExp_Explorer explorer(*base, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepExtrema_DistShapeShape distance(profile, face);
        distance.Perform();
        if (distance.IsDone() && distance.Value() < Precision::Confusion()) {
            return face;
        }
    }
    return profileFace;
}

UpToShapeLimit makeOpenUntilShapeFromFaces(const TopoDS_Shape& target,
                                           const TopoDS_Shape& profile,
                                           const gp_Dir& initialDirection)
{
    gp_Dir direction = initialDirection;
    auto cutFaces = findFacesCutByDirection(target, profile, direction);
    if (cutFaces.empty()) {
        direction.Reverse();
        cutFaces = findFacesCutByDirection(target, profile, direction);
    }

    TopoDS_Shape untilShape = target;
    double reportLength = 1.0;
    if (!cutFaces.empty()) {
        auto nearFace = cutFaces.begin();
        auto farFace = cutFaces.begin();
        for (auto it = cutFaces.begin(); it != cutFaces.end(); ++it) {
            if (it->distanceSquared > farFace->distanceSquared) {
                farFace = it;
            }
            else if (it->distanceSquared < nearFace->distanceSquared) {
                nearFace = it;
            }
        }
        reportLength = std::sqrt(std::max(farFace->distanceSquared, Precision::Confusion()));
        if (nearFace != farFace) {
            std::vector<TopoDS_Shape> openFaces;
            for (TopExp_Explorer explorer(target, TopAbs_FACE); explorer.More(); explorer.Next()) {
                const TopoDS_Shape face = explorer.Current();
                if (!face.IsSame(farFace->face)) {
                    openFaces.push_back(face);
                }
            }
            if (!openFaces.empty()) {
                untilShape = compoundOfShapes(openFaces);
            }
        }
    }

    return UpToShapeLimit{direction, reportLength, untilShape, faceCountOf(target), true};
}

std::optional<double> signedDistanceToFacePlane(const TopoDS_Shape& profile,
                                                const TopoDS_Face& face,
                                                const gp_Dir& direction,
                                                bool requireSketchClearance,
                                                const app::DocumentObject& object,
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
                                            const app::DocumentObject& object,
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
                                                    const app::DocumentObject& object,
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
                                                        const app::DocumentObject& object,
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

std::optional<TopoDS_Face> resolveFaceLink(const app::Link& link,
                                           const app::DocumentObject& object,
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
        const auto resolved = part::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
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

    const auto parsed = part::parseSubshapeName(currentSubname);
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
                               property + " requires a face subshape, not " + part::subshapeKindName(parsed->kind),
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
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

std::optional<PlanarLimit> resolveUpToFaceLimit(const app::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const TopoDS_Shape& profile,
                                                const gp_Dir& direction,
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

    auto limit = measureFaceLimit(profile, *face, direction, object, context, property);
    if (!limit) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
    // FeatureExtrude.cpp::FeatureExtrude::generateSingleExtrusionSide(), for "UpToFace" calls
    // "getUpToFaceFromLinkSub(upToShape, upToFacePropHandle)" and then
    // "prism.makeElementPrismUntil(..., upToShape, dir, TopoShape::PrismMode::None, true)".
    // Keep the face shape in the limit so the side builder can use the BRepFeat/PSM producer
    // path instead of degrading UpToFace to a plain length/XTR prism.
    limit->untilShape = *face;
    limit->prismUntil = true;
    return limit;
}

std::optional<UpToShapeLimit> resolveUpToShapeLimit(const app::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const TopoDS_Shape& profile,
                                                    const gp_Dir& direction,
                                                    const std::string& property)
{
    if (!app::hasPropertyType(object, property, "App::PropertyLinkSubList")) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " Type requires an App::PropertyLinkSubList " + property + " property",
                               object.name,
                               property);
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, property);
    if (links.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference at least one shape or face",
                               object.name,
                               property);
        return std::nullopt;
    }

    auto wholeTargetShape = [&](const app::Link& link) -> std::optional<TopoDS_Shape> {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   property + " target " + link.object + " did not produce a shape",
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object);
            return std::nullopt;
        }
        if (faceCountOf(shapeIt->second.shape) == 0) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   property + " target " + link.object + " has no faces",
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object);
            return std::nullopt;
        }
        return shapeIt->second.shape;
    };

    const auto linkSelectsWholeShape = [](const app::Link& link) {
        return link.subnames.empty() || link.subnames.front().empty();
    };

    if (links.size() == 1U && linkSelectsWholeShape(links.front())) {
        const auto shape = wholeTargetShape(links.front());
        if (!shape) {
            return std::nullopt;
        }
        const auto limit = selectFaceLimitFromShape(profile, *shape, direction, object, context, property);
        if (!limit) {
            return std::nullopt;
        }
        return UpToShapeLimit{limit->direction, limit->length, TopoDS_Shape{}, faceCountOf(*shape), false};
    }

    std::vector<TopoDS_Shape> selectedFaces;
    for (const app::Link& link : links) {
        if (linkSelectsWholeShape(link)) {
            const auto shape = wholeTargetShape(link);
            if (!shape) {
                return std::nullopt;
            }
            for (TopExp_Explorer explorer(*shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
                selectedFaces.push_back(explorer.Current());
            }
            continue;
        }

        for (std::size_t index = 0; index < link.subnames.size(); ++index) {
            const std::string& subname = link.subnames.at(index);
            if (subname.empty()) {
                const auto shape = wholeTargetShape(link);
                if (!shape) {
                    return std::nullopt;
                }
                for (TopExp_Explorer explorer(*shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
                    selectedFaces.push_back(explorer.Current());
                }
                continue;
            }

            app::Link single = link;
            single.subnames = {subname};
            if (link.stableSubnames.size() == link.subnames.size()) {
                single.stableSubnames = {link.stableSubnames.at(index)};
            }
            else {
                single.stableSubnames.clear();
            }
            if (link.fullSubnames.size() == link.subnames.size()) {
                single.fullSubnames = {link.fullSubnames.at(index)};
            }
            else {
                single.fullSubnames.clear();
            }
            const auto face = resolveFaceLink(single, object, context, property);
            if (!face) {
                return std::nullopt;
            }
            selectedFaces.push_back(*face);
        }
    }

    if (selectedFaces.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference at least one shape or face",
                               object.name,
                               property);
        return std::nullopt;
    }

    if (selectedFaces.size() == 1U) {
        const auto limit = measureFaceLimit(profile, TopoDS::Face(selectedFaces.front()), direction, object, context, property);
        if (!limit) {
            return std::nullopt;
        }
        return UpToShapeLimit{limit->direction, limit->length, TopoDS_Shape{}, 1, false};
    }

    return makeOpenUntilShapeFromFaces(compoundOfShapes(selectedFaces), profile, direction);
}

std::optional<PlanarLimit> resolveUpToFirstLastLimit(const app::DocumentObject& object,
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

std::optional<double> readNumberProperty(const app::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::string& property,
                                         const std::string& featureName)
{
    const auto value = app::readNumber(object, property);
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

double readOptionalNumberProperty(const app::DocumentObject& object, const std::string& property)
{
    return app::readNumber(object, property).value_or(0.0);
}

std::optional<double> readOptionalNumberPropertyStrict(const app::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const std::string& property,
                                                       const std::string& featureName)
{
    if (app::propertyValue(object, property) == nullptr) {
        return 0.0;
    }
    const auto value = app::readNumber(object, property);
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

bool readBoolProperty(const app::DocumentObject& object, const std::string& property, bool fallback)
{
    return app::readBool(object, property).value_or(fallback);
}

std::string readStringProperty(const app::DocumentObject& object, const std::string& property, const std::string& fallback)
{
    return app::readString(object, property).value_or(fallback);
}

std::string openProfileModeName(OpenProfileMode mode)
{
    switch (mode) {
        case OpenProfileMode::Auto:
            return "Auto";
        case OpenProfileMode::Reject:
            return "Reject";
        case OpenProfileMode::SurfaceExtrusion:
            return "SurfaceExtrusion";
        case OpenProfileMode::ThinSolid:
            return "ThinSolid";
        case OpenProfileMode::ThinCut:
            return "ThinCut";
        case OpenProfileMode::SurfaceSplitCut:
            return "SurfaceSplitCut";
    }
    return "Auto";
}

std::optional<OpenProfileMode> parseOpenProfileModeValue(const std::string& value)
{
    if (value == "Auto") {
        return OpenProfileMode::Auto;
    }
    if (value == "Reject") {
        return OpenProfileMode::Reject;
    }
    if (value == "SurfaceExtrusion") {
        return OpenProfileMode::SurfaceExtrusion;
    }
    if (value == "ThinSolid") {
        return OpenProfileMode::ThinSolid;
    }
    if (value == "ThinCut") {
        return OpenProfileMode::ThinCut;
    }
    if (value == "SurfaceSplitCut") {
        return OpenProfileMode::SurfaceSplitCut;
    }
    return std::nullopt;
}

std::optional<OpenProfileMode> readOpenProfileMode(const app::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   const std::string& featureName)
{
    if (const auto value = app::readString(object, "OpenProfileMode")) {
        if (const auto mode = parseOpenProfileModeValue(*value)) {
            return *mode;
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               featureName + " OpenProfileMode is not supported",
                               object.name,
                               "OpenProfileMode");
        return std::nullopt;
    }
    if (const auto numberValue = app::readNumber(object, "OpenProfileMode")) {
        switch (static_cast<int>(std::llround(*numberValue))) {
            case 0:
                return OpenProfileMode::Auto;
            case 1:
                return OpenProfileMode::Reject;
            case 2:
                return OpenProfileMode::SurfaceExtrusion;
            case 3:
                return OpenProfileMode::ThinSolid;
            case 4:
                return OpenProfileMode::ThinCut;
            case 5:
                return OpenProfileMode::SurfaceSplitCut;
            default:
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       featureName + " OpenProfileMode is not supported",
                                       object.name,
                                       "OpenProfileMode");
                return std::nullopt;
        }
    }
    return OpenProfileMode::Auto;
}

double degreesToRadians(double degrees)
{
    constexpr double pi = 3.14159265358979323846;
    return degrees * pi / 180.0;
}

std::optional<gp_Vec> readVector3Property(const app::DocumentObject& object, const std::string& property)
{
    const auto value = app::readVector3(object, property);
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
                                        const app::DocumentObject& object,
                                        runtime::ComputeContext& context,
                                        const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getAxis(), "getAxisFromEdge" accepts straight line, circle or arc of circle.
    // CAD Core also accepts non-Line curves proven geometrically linear by the shared part helper.
    part::EdgeAxisOptions options;
    options.allowCircleAxis = true;
    options.allowGeometricallyLinearCurve = true;
    const auto resolved = part::resolveEdgeAxis(edge, options);
    if (resolved.axis) {
        return resolved.axis->direction;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_subshape_kind",
                           "ReferenceAxis edge must be a straight/geometrically linear edge, circle or arc of circle: "
                               + resolved.message,
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

std::optional<gp_Dir> resolveReferenceAxisDirection(const app::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const app::Link& profileLink,
                                                    const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::computeDirection(), reads "ReferenceAxis" and forwards to
    // ProfileBased::getAxis(..., ForbiddenAxis::NotPerpendicularWithNormal).
    const auto* property = app::propertyValue(object, "ReferenceAxis");
    if (property == nullptr || property->raw.is_null()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "ReferenceAxis must link to a sketch axis or edge",
                               object.name,
                               "ReferenceAxis");
        return std::nullopt;
    }

    const auto link = app::readLink(object, "ReferenceAxis");
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

    const auto parsed = part::parseSubshapeName(subname);
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

    const auto subshape = part::subshapeByName(shapeIt->second.shape, subname);
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

std::optional<DirectionSpec> computeDirection(const app::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const app::Link& profileLink,
                                              const TopoDS_Shape& profile,
                                              AddSubMode mode,
                                              const std::optional<gp_Dir>& profileObjectNormal)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::computeDirection(),
    // falls back to the sketch normal, reads "UseCustomVector", "Direction", "ReferenceAxis" and later applies "AlongSketchNormal".
    gp_Dir sketchNormal = profileObjectNormal.value_or(
        profileNormal(profile).value_or(gp_Dir(0.0, 0.0, 1.0))
    );
    if (mode == AddSubMode::Subtractive) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp::Pocket::getProfileNormal(),
        // returns FeatureExtrude::getProfileNormal() * -1.
        sketchNormal.Reverse();
    }

    const bool useCustomVector = readBoolProperty(object, "UseCustomVector", false);
    gp_Dir direction = sketchNormal;
    if (useCustomVector) {
        if (app::propertyValue(object, "Direction") == nullptr) {
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
    else if (const auto* axisProperty = app::propertyValue(object, "ReferenceAxis"); axisProperty != nullptr && !axisProperty->raw.is_null()) {
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

const part::NamedShape* namedShapeForProfileSource(const runtime::ComputeContext& context,
                                                   const app::Link& profileLink,
                                                   const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::buildShape() owns g<ID>/g<ID>v<point> ElementMap entries. The profile resolver retains
    // that raw producer ledger until the FaceMaker profile ledger proves it has matching geometry.
    const auto profileNamedShapeIt = context.namedShapes.find(profileLink.object + ".ProfileShape");
    if (profileNamedShapeIt != context.namedShapes.end()
        && !profileNamedShapeIt->second.shape.IsNull()) {
        // ProfileBased::getTopoShapeVerifiedFace() may return a located/copy-on-change TopoShape
        // rather than the exact TShape retained by Sketch's FaceMaker result. Both are produced
        // from this Profile link in the same recompute, so use the dedicated producer ledger;
        // IsPartner would incorrectly discard it solely because of that request-local wrapper.
        return &profileNamedShapeIt->second;
    }
    const auto rawNamedShapeIt = context.namedShapes.find(profileLink.object);
    return rawNamedShapeIt == context.namedShapes.end() ? nullptr : &rawNamedShapeIt->second;
}

std::optional<SideBuild> makePrismSide(const TopoDS_Shape& profile,
                                       const gp_Dir& direction,
                                       double length,
                                       const app::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const app::Link& profileLink,
                                       const std::string& method,
                                       const std::string& featureName,
                                       const std::string& historyOwner,
    const part::NamedShape* profileNamedShape = nullptr,
    bool promoteBareSourceIdForGenerated = false)
{
    if (profileNamedShape == nullptr) {
        profileNamedShape = namedShapeForProfileSource(context, profileLink, profile);
    }
    if (firstFaceOf(profile)) {
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
        std::optional<part::NamedShape> namedShape;
        part::MakerHistoryOptions historyOptions;
        historyOptions.producerOperation = "XTR";
        historyOptions.recordUnmappedSourceDeletions = true;
        historyOptions.stringHasher = context.stringHasher;
        // FreeCAD: src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute() keeps
        // an additive prism's ElementMap before its BaseFeature fuse; Pocket's tool does not use
        // this additive producer lifecycle.
        historyOptions.promoteBareSourceIdForGenerated = promoteBareSourceIdForGenerated;
        if (profileNamedShape != nullptr) {
            const part::NamedShapeSource profileSource{profileLink.object, profile, profileNamedShape};
            namedShape = part::namedShapeForMakerHistory(historyOwner,
                                                         prism.Shape(),
                                                         std::vector<part::NamedShapeSource>{profileSource},
                                                         prism,
                                                         historyOptions);
        }
        else {
            namedShape = part::namedShapeForMakerHistory(historyOwner,
                                                         prism.Shape(),
                                                         profileLink.object,
                                                         profile,
                                                         prism,
                                                         historyOptions);
        }
        return SideBuild{method, length, prism.Shape(), false, false, std::move(namedShape)};
    }

    const part::NamedShape* sourceNamedShape = profileNamedShape != nullptr
        ? profileNamedShape
        : namedShapeForProfileSource(context, profileLink, profile);
    std::string error;
    const auto extrusion = part::buildLinearExtrusionFromProfile(
        historyOwner,
        profileLink.object,
        profile,
        part::PartLinearExtrusionOptions {
            direction,
            length,
            0.0,
            0.0,
            0.0,
            true,
        },
        sourceNamedShape,
        error
    );
    if (!extrusion) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               error.empty() ? "OCCT could not extrude " + featureName + " profile" : error,
                               object.name);
        return std::nullopt;
    }
    return SideBuild{
        method,
        length,
        extrusion->shape,
        extrusion->topoNamingKnownGap,
        extrusion->taperHistory,
        std::move(extrusion->namedShape)
    };
}

std::optional<SideBuild> makePrismUntilSide(const TopoDS_Shape& profile,
                                            const gp_Dir& direction,
                                            const TopoDS_Shape& untilShape,
                                            double reportLength,
                                            const app::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const app::Link& profileLink,
                                            const std::string& method,
                                            const std::string& property,
                                            const std::string& featureName,
                                            const std::string& historyOwner,
                                            const part::NamedShape* profileNamedShape = nullptr)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::generateSingleExtrusionSide(), calls "prism.makeElementPrismUntil(...,
    // upToShape, dir, TopoShape::PrismMode::None, true)" after getUpToShapeFromLinkSubList().
    const auto profileFace = firstFaceOf(profile);
    if (!profileFace) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               featureName + " Profile must provide a face for UpToShape prism-until",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }
    const TopoDS_Face supportFace = supportFaceForPrismUntil(profile, *profileFace, context);

    const auto performUntil = [&](const TopoDS_Shape& baseShape) -> std::optional<TopoDS_Shape> {
        try {
            BRepFeat_MakePrism prism;
            prism.Init(baseShape, *profileFace, supportFace, direction, 2, Standard_False);
            prism.Perform(untilShape);
            if (!prism.IsDone() || prism.Shape().IsNull()) {
                return std::nullopt;
            }
            return prism.Shape();
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    };

    auto prismShape = performUntil(profile);
    if (!prismShape) {
        BRepPrimAPI_MakePrism retryBase(untilShape, gp_Vec(direction));
        retryBase.Build();
        if (retryBase.IsDone() && !retryBase.Shape().IsNull()) {
            prismShape = performUntil(retryBase.Shape());
        }
    }

    if (!prismShape) {
        // The selected-face ledger above already comes from FreeCAD's Part::findAllFacesCutBy()
        // path. Keep valid LinkSubList requests supported when raw BRepFeat rejects the open
        // shell, and still derive the reach from the FreeCAD cut-face ordering.
        return makePrismSide(profile,
                             direction,
                             reportLength,
                             object,
                             context,
                             profileLink,
                             method,
                             featureName,
                             historyOwner,
                             profileNamedShape);
    }

    part::NamedShapeSource profileSource{profileLink.object, profile};
    if (profileNamedShape != nullptr) {
        profileSource.namedShape = profileNamedShape;
    }
    else {
        const auto profileNamedShapeIt = context.namedShapes.find(profileLink.object);
        if (profileNamedShapeIt != context.namedShapes.end()) {
            profileSource.namedShape = &profileNamedShapeIt->second;
        }
    }
    auto namedShape = part::namedShapeForPreservedSources(historyOwner, *prismShape, {profileSource}, "PSM");
    return SideBuild{method, reportLength, *prismShape, false, false, std::move(namedShape)};
}

std::optional<SideBuild> makeExtrusionShape(const app::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const TopoDS_Shape& profile,
                                            const gp_Dir& direction,
                                            double length,
                                            double taperAngleDegrees,
                                            const std::string& method,
                                            const std::string& taperProperty,
                                            const std::string& featureName,
                                            const app::Link& profileLink,
                                            const std::string& historyOwner,
                                            const part::NamedShape* profileNamedShape = nullptr,
                                            bool promoteBareSourceIdForGenerated = false)
{
    if (std::abs(taperAngleDegrees) <= Precision::Angular()) {
        return makePrismSide(profile,
                             direction,
                             length,
                             object,
                             context,
                             profileLink,
                             method,
                             featureName,
                             historyOwner,
                             profileNamedShape,
                             promoteBareSourceIdForGenerated);
    }

    std::string error;
    const auto tapered = part::makeTaperedExtrusion(profile,
                                                        part::TaperedExtrusionOptions{
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
    part::NamedShapeSource profileSource{profileLink.object, profile};
    if (profileNamedShape != nullptr) {
        profileSource.namedShape = profileNamedShape;
    }
    else {
        const auto profileNamedShapeIt = context.namedShapes.find(profileLink.object);
        if (profileNamedShapeIt != context.namedShapes.end()) {
            profileSource.namedShape = &profileNamedShapeIt->second;
        }
    }
    auto namedShape = part::namedShapeForTaperedExtrusionHistory(historyOwner, *tapered, profile, profileSource)
        .value_or(part::namedShapeForPreservedSources(historyOwner, tapered->shape, {profileSource}, "XTR"));
    return SideBuild{
        method,
        length,
        tapered->shape,
        tapered->topoNamingKnownGap,
        !tapered->topoNamingKnownGap,
        std::move(namedShape)
    };
}

std::optional<ToolShapeBuild> xorToolShapes(const std::vector<SideBuild>& sides,
                                            const app::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const std::string& featureName)
{
    if (sides.empty()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "No extrusion geometry was generated", object.name);
        return std::nullopt;
    }

    std::vector<part::NamedShapeSource> sources;
    sources.reserve(sides.size());
    for (std::size_t index = 0; index < sides.size(); ++index) {
        part::NamedShapeSource source{object.name + ".Prism" + std::to_string(index + 1), sides.at(index).shape};
        if (sides.at(index).namedShape) {
            source.namedShape = &*sides.at(index).namedShape;
        }
        sources.push_back(source);
    }

    const auto result = part::makeElementXorFromSources(object.name, sources);
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

std::optional<ToolShapeBuild> displayOnlyToolShapes(const std::vector<SideBuild>& sides,
                                                    const app::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const std::string& featureName)
{
    if (sides.empty()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "No extrusion geometry was generated", object.name);
        return std::nullopt;
    }
    if (sides.size() == 1U) {
        return ToolShapeBuild{sides.front().shape, sides.front().namedShape};
    }

    std::vector<part::NamedShapeSource> sources;
    sources.reserve(sides.size());
    for (std::size_t index = 0; index < sides.size(); ++index) {
        part::NamedShapeSource source{object.name + ".Surface" + std::to_string(index + 1), sides.at(index).shape};
        if (sides.at(index).namedShape) {
            source.namedShape = &*sides.at(index).namedShape;
        }
        sources.push_back(source);
    }
    const auto result = part::makeElementCompoundFromSources(object.name, sources);
    if (!result.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               result.error + " while combining " + featureName + " open profile extrusion sides",
                               object.name,
                               "SideType");
        return std::nullopt;
    }
    return ToolShapeBuild{result.shape, result.namedShape};
}

bool isOpenProfileKind(ProfileKind kind)
{
    return kind == ProfileKind::OpenWire || kind == ProfileKind::EdgeCompound;
}

std::string bodyParticipationForClosedProfile(AddSubMode mode)
{
    return mode == AddSubMode::Additive ? "solid_add" : "solid_cut";
}

std::optional<std::string> readOpenProfileSide(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const std::string& featureName)
{
    const std::string side = app::readString(object, "OpenProfileSide").value_or("Both");
    if (side == "Left" || side == "Right" || side == "Both") {
        return side;
    }
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           featureName + " OpenProfileSide must be Left, Right, or Both",
                           object.name,
                           "OpenProfileSide");
    return std::nullopt;
}

std::optional<double> readOpenProfileThickness(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const std::string& featureName,
                                               OpenProfileMode requestedMode)
{
    const auto thickness = app::readNumber(object, "OpenProfileThickness");
    if (!thickness) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_open_profile_thickness",
                               featureName + " " + openProfileModeName(requestedMode)
                                   + " requires OpenProfileThickness",
                               object.name,
                               "OpenProfileThickness");
        return std::nullopt;
    }
    if (*thickness <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               featureName + " OpenProfileThickness must be greater than zero",
                               object.name,
                               "OpenProfileThickness");
        return std::nullopt;
    }
    return *thickness;
}

std::optional<ThinOpenProfileBuild> buildThinOpenProfileFace(const app::DocumentObject& object,
                                                             runtime::ComputeContext& context,
                                                             const app::Link& profileLink,
                                                             const TopoDS_Shape& profileShape,
                                                             const std::string& featureName,
                                                             OpenProfileMode requestedMode)
{
    const auto thickness = readOpenProfileThickness(object, context, featureName, requestedMode);
    if (!thickness) {
        return std::nullopt;
    }
    const auto side = readOpenProfileSide(object, context, featureName);
    if (!side) {
        return std::nullopt;
    }

    const part::NamedShape* profileNamedShape = namedShapeForProfileSource(context, profileLink, profileShape);
    const part::NamedShapeSource profileSource{profileLink.object, profileShape, profileNamedShape};
    const std::string unsupportedCode = requestedMode == OpenProfileMode::ThinCut
        ? "unsupported_open_profile_pocket"
        : "unsupported_open_profile_body_fuse";
    const auto offsetFace = [&](double distance, const std::string& ownerSuffix) -> part::NamedShapeBuild {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementOffset2D(), for open wires with FillType::fill connects the
        // source and offset open wires into a face. CAD Core uses this Part-layer ledger as the
        // explicit thin Pad/Pocket product extension before entering the normal extrusion path.
        return part::makeElementOffset2DFromSource(object.name + ownerSuffix,
                                                   profileSource,
                                                   distance,
                                                   0,
                                                   true,
                                                   true,
                                                   true);
    };

    if (*side == "Left" || *side == "Right") {
        const double distance = *side == "Left" ? *thickness : -*thickness;
        auto build = offsetFace(distance, ".ThinProfile");
        if (!build.error.empty() || build.shape.IsNull()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   unsupportedCode,
                                   build.error.empty() ? "Could not build thin open-profile face" : build.error,
                                   object.name,
                                   "OpenProfileThickness");
            return std::nullopt;
        }
        return ThinOpenProfileBuild{build.shape, build.namedShape, *side};
    }

    auto left = offsetFace(*thickness / 2.0, ".ThinProfileLeft");
    if (!left.error.empty() || left.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               unsupportedCode,
                               left.error.empty() ? "Could not build left thin open-profile face" : left.error,
                               object.name,
                               "OpenProfileThickness");
        return std::nullopt;
    }
    auto right = offsetFace(-*thickness / 2.0, ".ThinProfileRight");
    if (!right.error.empty() || right.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               unsupportedCode,
                               right.error.empty() ? "Could not build right thin open-profile face" : right.error,
                               object.name,
                               "OpenProfileThickness");
        return std::nullopt;
    }

    std::vector<part::NamedShapeSource> sources;
    sources.push_back({object.name + ".ThinProfileLeft", left.shape, left.namedShape ? &*left.namedShape : nullptr});
    sources.push_back({object.name + ".ThinProfileRight", right.shape, right.namedShape ? &*right.namedShape : nullptr});
    auto compound = part::makeElementCompoundFromSources(object.name + ".ThinProfile", sources, false);
    if (!compound.error.empty() || compound.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               unsupportedCode,
                               compound.error.empty() ? "Could not combine thin open-profile faces" : compound.error,
                               object.name,
                               "OpenProfileSide");
        return std::nullopt;
    }
    return ThinOpenProfileBuild{compound.shape, compound.namedShape, *side};
}

std::vector<std::string> nonEmptyStrings(const std::vector<std::string>& values)
{
    std::vector<std::string> result;
    for (const std::string& value : values) {
        if (!value.empty()) {
            result.push_back(value);
        }
    }
    return result;
}

std::optional<OpenProfileMode> resolveOpenProfileExecutionMode(const app::DocumentObject& object,
                                                               runtime::ComputeContext& context,
                                                               OpenProfileMode requestedMode,
                                                               AddSubMode addSubMode,
                                                               const std::string& featureName)
{
    if (requestedMode == OpenProfileMode::Auto || requestedMode == OpenProfileMode::SurfaceExtrusion) {
        return OpenProfileMode::SurfaceExtrusion;
    }
    if (requestedMode == OpenProfileMode::ThinSolid || requestedMode == OpenProfileMode::ThinCut) {
        if (requestedMode == OpenProfileMode::ThinSolid && addSubMode != AddSubMode::Additive) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_open_profile_pocket",
                                   featureName + " ThinSolid is only valid for additive open-profile features",
                                   object.name,
                                   "OpenProfileMode");
            return std::nullopt;
        }
        if (requestedMode == OpenProfileMode::ThinCut && addSubMode != AddSubMode::Subtractive) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_open_profile_body_fuse",
                                   featureName + " ThinCut is only valid for subtractive open-profile features",
                                   object.name,
                                   "OpenProfileMode");
            return std::nullopt;
        }
        return requestedMode;
    }
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           addSubMode == AddSubMode::Subtractive
                               ? "unsupported_open_profile_pocket"
                               : "unsupported_open_profile_body_fuse",
                           featureName + " " + openProfileModeName(requestedMode)
                               + " requires an explicit Body solid participation policy",
                           object.name,
                           "OpenProfileMode");
    return std::nullopt;
}

std::optional<SideBuild> buildSingleSide(const app::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const TopoDS_Shape& profile,
                                         const app::Link& profileLink,
                                         const SideSpec& side,
                                         AddSubMode mode,
                                         const std::string& featureName,
                                         double lengthScale,
                                         const std::string& historyOwner,
                                         const part::NamedShape* profileNamedShape = nullptr)
{
    const std::string method = readStringProperty(object, side.typeProperty, "Length");
    const double taper = readOptionalNumberProperty(object, side.taperProperty);
    const double offset = readOptionalNumberProperty(object, side.offsetProperty);
    const auto rejectOffset = [&](const std::string& message) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               message,
                               object.name,
                               side.offsetProperty);
        return std::optional<SideBuild>{};
    };

    double length = 0.0;
    gp_Dir direction = side.direction;
    if (method == "Length") {
        if (std::abs(offset) > Precision::Confusion()) {
            return rejectOffset(side.offsetProperty + " requires an UpTo method");
        }
        const auto rawLength = readNumberProperty(object, context, side.lengthProperty, featureName);
        if (!rawLength) {
            return std::nullopt;
        }
        length = *rawLength * lengthScale;
    }
    else if (method == "ThroughAll") {
        if (std::abs(offset) > Precision::Confusion()) {
            return rejectOffset(side.offsetProperty + " requires the full FreeCAD UpTo offset path");
        }
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
        if (std::abs(offset) > Precision::Confusion()) {
            return rejectOffset(side.offsetProperty + " requires the full FreeCAD UpTo offset path");
        }
    }
    else if (method == "UpToFace") {
        const auto limit = resolveUpToFaceLimit(object, context, profile, direction, side.upToFaceProperty);
        if (!limit) {
            return std::nullopt;
        }
        direction = limit->direction;
        length = limit->length;
        if (std::abs(offset) > Precision::Confusion()) {
            return rejectOffset(side.offsetProperty + " requires the full FreeCAD UpTo offset path");
        }
        if (limit->prismUntil) {
            if (std::abs(taper) > Precision::Angular()) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       side.taperProperty + " is not supported for UpToFace prism-until",
                                       object.name,
                                       side.taperProperty);
                return std::nullopt;
            }
            return makePrismUntilSide(profile,
                                      direction,
                                      limit->untilShape,
                                      length,
                                      object,
                                      context,
                                      profileLink,
                                      method,
                                      side.upToFaceProperty,
                                      featureName,
                                      historyOwner,
                                      profileNamedShape);
        }
    }
    else if (method == "UpToShape") {
        const auto limit = resolveUpToShapeLimit(object, context, profile, direction, side.upToShapeProperty);
        if (!limit) {
            return std::nullopt;
        }
        direction = limit->direction;
        length = limit->reportLength;
        if (limit->prismUntil) {
            if (std::abs(offset) > Precision::Confusion()) {
                return rejectOffset("Extrude: Can only offset one face");
            }
            if (std::abs(taper) > Precision::Angular()) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       side.taperProperty + " is not supported for UpToShape face-list prism-until",
                                       object.name,
                                       side.taperProperty);
                return std::nullopt;
            }
            return makePrismUntilSide(profile,
                                      direction,
                                      limit->untilShape,
                                      length,
                                      object,
                                      context,
                                      profileLink,
                                      method,
                                      side.upToShapeProperty,
                                      featureName,
                                      historyOwner,
                                      profileNamedShape);
        }
        if (std::abs(offset) > Precision::Confusion()) {
            return rejectOffset(side.offsetProperty + " requires the full FreeCAD UpTo offset path");
        }
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
        return SideBuild{method, length, TopoDS_Shape{}, false, false};
    }

    return makeExtrusionShape(object,
                              context,
                              profile,
                              direction,
                              length,
                              taper,
                              method,
                              side.taperProperty,
                              featureName,
                              profileLink,
                              historyOwner,
                              profileNamedShape,
                              mode == AddSubMode::Additive);
}

}  // namespace

std::optional<ExtrudeResult> buildFeatureExtrusion(const app::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   AddSubMode mode,
                                                   const std::string& featureName)
{
    // FreeCAD semantic source:
    // src/Mod/PartDesign/App/FeatureExtrude.cpp FeatureExtrude::buildExtrusion().
    const std::string sideType = readStringProperty(object, "SideType", "One side");
    const auto openProfileMode = readOpenProfileMode(object, context, featureName);
    if (!openProfileMode) {
        return std::nullopt;
    }
    const auto profile = resolveFeatureExtrusionProfile(object, context, featureName, *openProfileMode);
    if (!profile) {
        return std::nullopt;
    }
    const part::NamedShape* tracedProfile = namedShapeForProfileSource(
        context, profile->link, profile->shape
    );
    if (tracedProfile != nullptr && context.producerTrace != nullptr) {
        long profileTag = tracedProfile->producerTag.value_or(0L);
        if (profileTag == 0L) {
            const auto rawProfile = context.namedShapes.find(profile->link.object);
            if (rawProfile != context.namedShapes.end()) {
                profileTag = rawProfile->second.producerTag.value_or(0L);
            }
        }
        const auto firstEntry = [&](const std::string& indexed, bool clearRefs) {
            const auto entries = tracedProfile->elementMapEntries.find(indexed);
            if (entries == tracedProfile->elementMapEntries.end() || entries->second.empty()) {
                return;
            }
            const part::ElementMapEntry& entry = entries->second.front();
            const auto provenance = tracedProfile->mappedNameProvenance.find(entry.mappedName);
            const std::string raw = provenance != tracedProfile->mappedNameProvenance.end()
                ? provenance->second.rawMappedName
                : entry.mappedName;
            std::string refs;
            if (!clearRefs) {
                for (const app::StringId& ref : entry.elementIdRefs) {
                    if (!refs.empty()) {
                        refs += ',';
                    }
                    refs += ref.toString();
                }
            }
            context.producerTrace->record({
                "element_map.find", "hit", "first_entry",
                {{"indexed", indexed}, {"raw", raw}, {"entryLocalRefs", refs}},
            });
        };
        firstEntry("Face1", true);
        part::NamedShape resetProfile = part::indexedNamedShapeForObject(
            profile->link.object + ".ExtrusionProfile", profile->shape
        );
        resetProfile.producerTag = profileTag;
        resetProfile.stringHasher = context.stringHasher;
        for (int copy = 0; copy < 2; ++copy) {
            context.producerTrace->record({
                "toposhape.set_shape", "begin", "reset_requested",
                {{"incomingNull", "false"},
                 {"resetElementMap", "true"},
                 {"tag", std::to_string(profileTag)}},
            });
            part::checkpointNamedShapeLedger(
                resetProfile,
                profile->link.object + ".ExtrusionProfile",
                "toposhape.set_shape_checkpoint"
            );
        }
        for (const char* prefix : {"Vertex", "Edge", "Face"}) {
            for (int index = 1;; ++index) {
                const std::string indexed = std::string(prefix) + std::to_string(index);
                if (tracedProfile->elements.count(indexed) == 0U) {
                    break;
                }
                firstEntry(indexed, false);
            }
        }
    }
    const bool openProfile = isOpenProfileKind(profile->kind);
    std::optional<OpenProfileMode> resolvedOpenProfileMode;
    std::string bodyParticipation = bodyParticipationForClosedProfile(mode);
    TopoDS_Shape extrusionProfileShape = profile->shape;
    std::optional<part::NamedShape> extrusionProfileNamedShape;
    bool displayOnlyOpenProfile = false;
    if (openProfile) {
        resolvedOpenProfileMode =
            resolveOpenProfileExecutionMode(object, context, *openProfileMode, mode, featureName);
        if (!resolvedOpenProfileMode) {
            return std::nullopt;
        }
        if (*resolvedOpenProfileMode == OpenProfileMode::SurfaceExtrusion) {
            bodyParticipation = "display_only";
            displayOnlyOpenProfile = true;
        }
        else {
            const auto thinProfile = buildThinOpenProfileFace(
                object, context, profile->link, profile->shape, featureName, *resolvedOpenProfileMode);
            if (!thinProfile) {
                return std::nullopt;
            }
            extrusionProfileShape = thinProfile->shape;
            extrusionProfileNamedShape = thinProfile->namedShape;
            bodyParticipation = bodyParticipationForClosedProfile(mode);
        }
    }
    const bool reversed = readBoolProperty(object, "Reversed", false);
    auto direction = computeDirection(object, context, profile->link, extrusionProfileShape, mode, profile->normal);
    if (!direction) {
        return std::nullopt;
    }
    if (reversed) {
        direction->direction.Reverse();
    }

    const auto startOffset = readOptionalNumberPropertyStrict(object, context, "StartOffset", featureName);
    if (!startOffset) {
        return std::nullopt;
    }
    if (std::abs(*startOffset) > Precision::Confusion()) {
        extrusionProfileShape = translatedShape(extrusionProfileShape, *startOffset * gp_Vec(direction->direction));
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
    bool taperHistory = false;
    std::optional<part::NamedShape> resultNamedShape;

    if (sideType == "One side") {
        auto side = buildSingleSide(
            object,
            context,
            extrusionProfileShape,
            profile->link,
            side1,
            mode,
            featureName,
            direction->lengthScale,
            object.name,
            extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr);
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
        taperHistory = side->taperHistory;
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
            const TopoDS_Shape movedProfile = translatedShape(extrusionProfileShape,
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
                                             profile->link,
                                             "Two sides",
                                             featureName,
                                             object.name,
                                             extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr,
                                             mode == AddSubMode::Additive);
            if (!prism) {
                return std::nullopt;
            }
            prisms.push_back(*prism);
            reportedLength = totalLength;
            resultNamedShape = prism->namedShape;
            taperHistory = prism->taperHistory;
        }
        else {
            auto first = buildSingleSide(object,
                                         context,
                                         extrusionProfileShape,
                                         profile->link,
                                         side1,
                                         mode,
                                         featureName,
                                         direction->lengthScale,
                                         object.name + ".Prism1",
                                         extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr);
            if (!first) {
                return std::nullopt;
            }
            auto second = buildSingleSide(object,
                                          context,
                                          extrusionProfileShape,
                                          profile->link,
                                          side2,
                                          mode,
                                          featureName,
                                          direction->lengthScale,
                                          object.name + ".Prism2",
                                          extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr);
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
            taperHistory = first->taperHistory || second->taperHistory;
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
                                            extrusionProfileShape,
                                            direction->direction,
                                            halfLength,
                                            taper1,
                                            "Symmetric",
                                            "TaperAngle",
                                            featureName,
                                            profile->link,
                                            object.name + ".Prism1",
                                            extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr,
                                            mode == AddSubMode::Additive);
            if (!first) {
                return std::nullopt;
            }
            auto second = makeExtrusionShape(object,
                                             context,
                                             extrusionProfileShape,
                                             secondDirection,
                                             halfLength,
                                             taper1,
                                             "Symmetric",
                                             "TaperAngle",
                                             featureName,
                                             profile->link,
                                             object.name + ".Prism2",
                                             extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr,
                                             mode == AddSubMode::Additive);
            if (!second) {
                return std::nullopt;
            }
            prisms.push_back(*first);
            prisms.push_back(*second);
            topoNamingKnownGap = first->topoNamingKnownGap || second->topoNamingKnownGap;
            taperHistory = first->taperHistory || second->taperHistory;
        }
        else {
            const TopoDS_Shape movedProfile = translatedShape(extrusionProfileShape,
                                                              -0.5 * scaledLength * gp_Vec(direction->direction));
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::buildExtrusion(), Symmetric no-taper fast path creates one prism
            // from a copied sketch translated by -L/2.
            const auto prism = makePrismSide(movedProfile,
                                             direction->direction,
                                             scaledLength,
                                             object,
                                             context,
                                             profile->link,
                                             "Symmetric",
                                             featureName,
                                             object.name,
                                             extrusionProfileNamedShape ? &*extrusionProfileNamedShape : nullptr,
                                             mode == AddSubMode::Additive);
            if (!prism) {
                return std::nullopt;
            }
            prisms.push_back(*prism);
            resultNamedShape = prism->namedShape;
            taperHistory = prism->taperHistory;
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

    const auto toolShape = displayOnlyOpenProfile
        ? displayOnlyToolShapes(prisms, object, context, featureName)
        : xorToolShapes(prisms, object, context, featureName);
    if (!toolShape) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::generateSingleExtrusionSide() constructs the one-side prism, and
    // FeatureExtrude::execute() publishes that prism through `rawShape` / `Shape.setValue`.
    // The one-side XOR transport is not a new producer: replacing the side maker ledger here
    // drops its `;XTR` ElementMap entries before the Pad/Pocket Boolean consumes them.
    if (toolShape->namedShape && !resultNamedShape) {
        resultNamedShape = toolShape->namedShape;
    }

    if (shouldProbeAdditiveFacePadDirection(object,
                                            mode,
                                            profile->link,
                                            extrusionProfileShape,
                                            reversed,
                                            displayOnlyOpenProfile,
                                            context)) {
        const auto base = previousSolidShape(context);
        if (base && !additiveFuseProducesNonVoidShape(object.name, *base, toolShape->shape)) {
            gp_Dir reverseDirection = direction->direction;
            reverseDirection.Reverse();
            app::DocumentObject reverseObject = objectWithCustomDirection(object, reverseDirection);
            const std::size_t diagnosticCount = context.diagnostics.size();
            auto reverseExtrusion = buildFeatureExtrusion(reverseObject, context, mode, featureName);
            if (reverseExtrusion
                && additiveFuseProducesNonVoidShape(object.name, *base, reverseExtrusion->toolShape)) {
                return reverseExtrusion;
            }
            context.diagnostics.erase(context.diagnostics.begin() + static_cast<std::ptrdiff_t>(diagnosticCount),
                                      context.diagnostics.end());
        }
    }

    if (displayOnlyOpenProfile) {
        runtime::addDiagnostic(context.diagnostics,
                               "warning",
                               "open_profile_surface_display_only",
                               featureName + " open wire profile was extruded as display-only surface geometry; Body solid was not modified",
                               object.name,
                               "Profile",
                               "runtime",
                               profile->link.object);
    }

    return ExtrudeResult{
        profile->link,
        profile->kind,
        *openProfileMode,
        resolvedOpenProfileMode,
        bodyParticipation,
        nonEmptyStrings(profile->selectedSubnames),
        nonEmptyStrings(profile->selectedStableSubnames),
        profile->profileResolveMode,
        profile->profileOwner,
        profile->requestedProfileSubname,
        profile->currentProfileSubname,
        method,
        reportedLength,
        reversed,
        toolShape->shape,
        cad_core::part::objectBBoxForShape(toolShape->shape),
        displayOnlyOpenProfile ? 0.0 : cad_core::part::volumeForShape(toolShape->shape),
        topoNamingKnownGap,
        taperHistory,
        resultNamedShape,
    };
}

std::optional<FeatureExtrusionShape> finalizeFeatureExtrusion(const app::DocumentObject& object,
                                                               runtime::ComputeContext& context,
                                                               AddSubMode mode,
                                                               const ExtrudeResult& extrusion)
{
    FeatureExtrusionShape result {
        extrusion.toolShape,
        extrusion.namedShape,
        extrusion.toolShape,
        extrusion.namedShape,
        false,
    };
    if (extrusion.bodyParticipation == "display_only") {
        return result;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::execute() stores rawShape, then calls `prism = refineShapeIfActive(prism)`
    // and `AddSubShape.setValue(prism)` before assigning `prism.Tag = -this->getID()` for the
    // BaseFeature Boolean. The pre-Boolean tool therefore has its own mapper lifecycle; refining
    // only the final result lets OCCT preserve source subshapes that FreeCAD's maker modifies.
    const auto refinedTool = runtime::applyPartDesignFeatureRefineProperty(
        object,
        context,
        extrusion.toolShape,
        extrusion.namedShape
    );
    if (!refinedTool) {
        return std::nullopt;
    }
    result.shape = refinedTool->shape;
    result.namedShape = refinedTool->namedShape;
    result.addSubShape = refinedTool->shape;
    if (refinedTool->namedShape) {
        // FreeCAD: src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute() invokes
        // AddSubShape.setValue(prism) before it changes prism.Tag and before the Boolean. Keep
        // that persisted ElementMap lifecycle separate from the operation-local negative tool.
        result.addSubNamedShape = part::namedShapeForPropertyShapeValue(
            object.name,
            result.addSubShape,
            *refinedTool->namedShape,
            static_cast<long>(object.id)
        );
        if (context.producerTrace) {
            context.producerTrace->record({
                "shape_slot.assign",
                "assigned",
                "extrude_prism_handoff",
                {{"property", "AddSubShape"}},
            });
            const std::string snapshot = context.producerTrace->currentSnapshotId();
            context.producerTrace->record({
                "partdesign.extrude.addsub_checkpoint",
                "published",
                "",
                {{"snapshot", snapshot}},
                snapshot,
                snapshot,
            });
        }
    }

    const auto base = baseSolidSource(object, context);
    if (!base) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
        // ::execute(), refines `prism` before AddSubShape.setValue() even without a base, then
        // the solid/no-base branch refines the publishable Shape again. Do not return the raw
        // extrusion before this first producer lifecycle.
        return result;
    }

    part::NamedShapeSource tool {
        object.name,
        result.shape,
        result.namedShape ? &*result.namedShape : nullptr,
    };
    // FreeCAD: src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute(),
    // `prism.Tag = -this->getID()` follows AddSubShape.setValue(prism).  `tool` must therefore
    // retain refinedTool's pre-property ElementMap while its incoming Tag is negative; the
    // separately persisted AddSubShape map above is only later cache evidence for Pattern et al.
    tool.producerTag = -static_cast<long>(object.id);
    const part::BooleanOperation operation = mode == AddSubMode::Additive
        ? part::BooleanOperation::Fuse
        : part::BooleanOperation::Cut;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::execute() passes `FuzzyTolerance.getValue()` directly to
    // makeElementBoolean(). The property defaults to 0, which deliberately leaves OCCT's
    // maker fuzzy value untouched; generic auto-fuzzy can replace result subshapes and alter
    // the ElementMap preserved/history boundary.
    const double fuzzyTolerance = app::readNumber(object, "FuzzyTolerance").value_or(0.0);
    const part::NamedShapeBuild build = part::makeElementBooleanFromSources(
        object.name,
        {*base, tool},
        operation,
        fuzzyTolerance
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               std::string(mode == AddSubMode::Additive ? "Pad" : "Pocket")
                                   + " could not combine its AddSubShape with the BaseFeature"
                                   + (build.error.empty() ? std::string {} : ": " + build.error),
                               object.name,
                               "BaseFeature");
        return std::nullopt;
    }
    result.shape = build.shape;
    result.namedShape = build.namedShape;
    result.combinedWithBase = true;
    return result;
}

}  // namespace cad_core::part_design
