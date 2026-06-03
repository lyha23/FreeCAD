#pragma once

#include "cad_core/topo/subshape_map.h"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <utility>
#include <string>
#include <vector>

class BRepOffsetAPI_ThruSections;

namespace cad_core::geometry
{
struct TaperedExtrusionResult;
}

namespace cad_core::topo
{

enum class ElementHistoryKind
{
    Indexed,
    Generated,
    Modified,
    Deleted,
    Split,
    Merge,
};

struct ElementHistory
{
    ElementHistoryKind kind = ElementHistoryKind::Indexed;
    std::string element;
    std::vector<std::string> sources;
};

struct SketchInternalWireJoinerOpenExportHistoryEntry
{
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = 0;
    std::string resultWireProducerKind;
    std::string resultWireProducerState;
    std::string resultWireProducerBlocker;
    std::size_t resultWireProducerSourceEdgeInfoIndex = 0;
    std::size_t resultWireProducerRootEdgeInfoIndex = 0;
    std::size_t resultWireProducerCurrentMemberEdgeInfoIndex = 0;
    std::size_t resultWireProducerChildWireInfoIndex = 0;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    bool helperOpenExportOverride = false;
    std::string helperOpenExportOverrideReason;
    bool helperOpenExportOverrideSourceEdgeInfo = false;
    std::size_t helperOpenExportOverrideSourceEdgeInfoIndex = 0;
    bool helperOpenExportOverrideSourceEdgeInfoConsumed = false;
    bool helperOpenExportOverrideOpenWireCompoundEligibleEdgeInfo = false;
    bool helperOpenExportOverrideForcedOpenWireCompoundEdgeInfo = false;
    bool helperOpenExportOverrideSourceEdgeExportShape = false;
    bool helperOpenExportOverrideSourceEdgeProducerOutput = false;
    bool helperOpenExportOverrideFullAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeMemberEdgeInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootEdgeInfo = false;
    std::size_t helperOpenExportOverrideSuperEdgeRootEdgeInfoIndex = 0;
    bool helperOpenExportOverrideSuperEdgeRootOpenWireCompoundEligibleEdgeInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootOpenLifecycleEdgeInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootClosedLifecycleEdgeInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedByUnowned = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedByPrimaryOwner = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedBySecondaryOwner = false;
    bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidence = false;
    int helperOpenExportOverrideSuperEdgeRootSelectedIteration = 0;
    std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo = 0;
    std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo2 = 0;
    bool helperOpenExportOverrideSuperEdgeRootExportBlockedByIteration = false;
    bool helperOpenExportOverrideSuperEdgeRootExportBlockedByWireInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootMissingSafeAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedPrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedMissingRemovalBranch = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidate = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateFullAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReady = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidatePrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingRemovalBranch = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranch = false;
    int helperOpenExportOverrideSelectedIteration = 0;
    std::size_t helperOpenExportOverrideSelectedWireInfo = 0;
    std::size_t helperOpenExportOverrideSelectedWireInfo2 = 0;
    bool helperOpenExportOverrideExportBlockedByIteration = false;
    bool helperOpenExportOverrideExportBlockedByWireInfo = false;
    std::vector<std::size_t> helperOpenExportOverrideCandidateEdgeInfoIndices;
    std::vector<std::size_t> helperOpenExportOverrideOpenWireCompoundEligibleCandidateEdgeInfoIndices;
    bool helperOpenExportOverrideRemovedSourceEdgeInfo = false;
    bool helperOpenExportOverrideRemovedTargetEdgeInfo = false;
    bool helperOpenExportOverrideAHistoryRemoveSourceEdgeInfo = false;
    std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeInfoIndices;
    std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeIndices;
    bool helperOpenExportOverrideAHistoryRemoveSourceLineage = false;
    bool helperOpenExportOverrideAHistoryRemoveSameSourceLineage = false;
    bool helperOpenExportOverrideAHistoryRemoveForeignSourceLineage = false;
    bool helperOpenExportOverrideSafeAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfo = false;
    std::vector<std::size_t> helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfoIndices;
    bool purgeBridge = false;
};

struct SketchInternalHistoryContext
{
    std::size_t sourceEdgeCount = 0;
    std::size_t preSplitEdgeCount = 0;
    std::size_t splitterEdgeCount = 0;
    std::size_t boundedFaceCount = 0;
    bool preSplitHistory = false;
    bool splitterHistory = false;
    std::size_t wireJoinerSourceEdgeCount = 0;
    std::size_t wireJoinerSplitResultEdgeCount = 0;
    std::size_t wireJoinerOpenExportEdgeCount = 0;
    std::size_t wireJoinerOpenExportSourceLineageEdgeCount = 0;
    std::size_t wireJoinerOpenExportMissingSourceLineageEdgeCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideEdgeCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSourceEdgeInfoConsumedCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSourceEdgeExportShapeEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSourceEdgeProducerOutputEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports natural final-gate EdgeInfo wires through
    // "openWireCompound"; this count marks entries still blocked from source-edge shape export by
    // the M2 child-wire/source-vertex boundary.
    std::size_t wireJoinerOpenExportHelperOverrideOpenWireCompoundEligibleWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideFullAHistoryProducerEvidenceWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() produces Remove evidence through "aHistory->Remove(info.edge)",
    // while ::build() exports only final-gate EdgeInfo wires. Topo only receives this split from
    // WireJoiner; it does not infer producer lifecycle from raw sketch geometry.
    std::size_t wireJoinerOpenExportHelperOverrideFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSafeAHistoryProducerEvidenceWithoutFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets non-root members with
    // "current->iteration = -1" and keeps one root with "first->superEdge = makeCleanWire(false)".
    // Topo consumes this producer ledger from WireJoiner instead of classifying raw edge geometry.
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberWithRootEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootOpenLifecycleEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootClosedLifecycleEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleAndSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleMissingSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootMissingSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootIterationBlockedUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootIterationBlockedPrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootIterationBlockedSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootIterationBlockedMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalChildWireProducerReadyEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidatePrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t wireJoinerOpenExportHelperOverrideSuperEdgeMemberForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        wireJoinerOpenExportHelperOverrideSuperEdgeMemberMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    std::size_t wireJoinerOpenExportHelperOverrideExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideBindingCandidateEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideWithOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideRemovedSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingRemovedSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideRemovedTargetEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingRemovedTargetEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideAHistoryRemoveSameSourceLineageEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideAHistoryRemoveForeignSourceLineageEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records "aHistory->Remove(info.edge)" producer evidence,
    // and ::build() only exports final-gate EdgeInfo wires to "openWireCompound". Topo keeps the
    // split so M4 can tell safe producer evidence from true final-wire export.
    std::size_t wireJoinerOpenExportHelperOverrideSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideSourceLineageRemovedSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingSourceLineageRemovedSourceEdgeInfoCount = 0;
    std::size_t wireJoinerOpenExportHelperOverrideMissingSourceLineageEdgeCount = 0;
    std::size_t wireJoinerOpenExportPurgeBridgeEdgeCount = 0;
    std::vector<SketchInternalWireJoinerOpenExportHistoryEntry> wireJoinerOpenExportHistoryEntries;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports result wires into "openWireCompound", then calls
    // "shape.makeShapeWithElementMap(..., MapperHistory(aHistory), ...)"; topo must consume the
    // same producer identity in history instead of re-inferring source ownership from geometry.
    std::size_t namedShapeHistoryMissingResultWireIdentityCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap(), calls "mapSubElement(shapes)" before mapper history.
    // A result-wire source lineage is accepted only when ElementMap or terminal history carries the
    // same source edge evidence.
    std::size_t elementMapResultWireIdentityMismatchCount = 0;
    std::size_t wireJoinerModifiedSourceEdgeCount = 0;
    std::size_t wireJoinerModifiedHistoryCount = 0;
    std::size_t wireJoinerGeneratedHistoryCount = 0;
    std::size_t wireJoinerDeletedHistoryCount = 0;
    bool wireJoinerSplitterHistory = false;
    bool wireJoinerFinalExportHistory = false;
};

struct NamedElement
{
    std::string name;
    SubshapeName subshape;
    ElementHistoryKind status = ElementHistoryKind::Indexed;
    std::vector<std::string> sources;
};

struct NamedShape
{
    std::string owner;
    TopoDS_Shape shape;
    std::map<std::string, NamedElement> elements;
    std::map<std::string, std::string> elementMap;
    std::vector<ElementHistory> history;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp,
    // MapperHistory can carry generated/modified/split/merge/deleted outcomes independently
    // from the currently resolvable ElementMap. cad-core exposes this request-local summary so
    // diagnostics can distinguish an indexed-only map from a partially consumed history ledger.
    std::vector<std::string> elementHistoryStatus;
    std::optional<SketchInternalHistoryContext> sketchInternalHistory;
};

struct NamedShapeSource
{
    std::string owner;
    TopoDS_Shape shape;
    const NamedShape* namedShape = nullptr;
    std::vector<std::string> ownerAliases;
};

struct LinkedSubshapeRetag
{
    std::string sourceElementName;
    std::string targetElementName;
    std::vector<std::string> exactAliases;
};

struct NamedShapeBuild
{
    TopoDS_Shape shape;
    std::optional<NamedShape> namedShape;
    std::string error;
};

enum class BooleanOperation
{
    Fuse,
    Cut,
    Common,
};

enum class ElementResolveStatus
{
    Resolved,
    Unresolved,
    Deleted,
    Split,
};

struct ElementResolveResult
{
    ElementResolveStatus status = ElementResolveStatus::Unresolved;
    std::optional<std::string> element;
};

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp::getElementTypes(),
// returns "Face", "Edge", "Vertex", while PropertyPartShape stores TopoShape as the shape property
// and tracks ElementMap versioning for later GeoFeature link updates.
NamedShape indexedNamedShapeForObject(const std::string& owner, const TopoDS_Shape& shape);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap(),
// iterates InternalShape vertices/edges and records Internal* <-> raw Edge/Vertex aliases after
// "findSubShapesWithSharedVertex(..., CheckGeometry | SingleResult)". This helper builds the
// Sketch InternalShape NamedShape baseline and deliberately leaves InternalFaceN without a stable
// alias until FaceMaker/WireJoiner history is migrated.
NamedShape namedShapeForSketchInternalShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    std::optional<SketchInternalHistoryContext> historyContext = std::nullopt
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementPrism(),
// creates BRepPrimAPI_MakePrism and then calls makeElementShape(...), which consumes
// MapperMaker::Generated/Modified history from the BRepBuilderAPI_MakeShape maker.
NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::string& sourceOwner,
    const TopoDS_Shape& sourceShape,
    BRepBuilderAPI_MakeShape& maker
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
// calls makeElementShape(*mk, inputs, ...); MapperMaker then consumes the BRepAlgoAPI
// BooleanOperation Generated/Modified history for every input source.
NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_MakeShape& maker
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections,
// extends MapperMaker::generated() with "GeneratedFace(s)", "FirstShape()" and "LastShape()"
// for BRepOffsetAPI_ThruSections loft history.
NamedShape namedShapeForThruSectionsHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepOffsetAPI_ThruSections& maker,
    const TopoDS_Shape& firstProfile,
    const TopoDS_Shape& lastProfile
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp::Extrusion::extrudeShape(),
// calls "ExtrusionHelper::makeElementDraft(params, myShape, drafts, result.Hasher)" for taper;
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections,
// adds "GeneratedFace(s)", "FirstShape()" and "LastShape()" to the loft maker history.
std::optional<NamedShape> namedShapeForTaperedExtrusionHistory(
    const std::string& owner,
    const geometry::TaperedExtrusionResult& tapered,
    const TopoDS_Shape& profile,
    const NamedShapeSource& profileSource
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap(),
// calls "mapSubElement(shapes)" before mapper history. This helper exposes only that
// source-preserved subset for makers whose full Generated/Modified ledger is not migrated yet.
NamedShape namedShapeForPreservedSources(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
// ::LinkBaseExtension::checkGeoElementMap(), calls "geoData->reTagElementMap(obj->getID(), ...)"
// after resolving the linked object. cad-core exposes the same source-alias retag as ElementMap.
NamedShape namedShapeForLinkedShape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
);
NamedShape namedShapeForLinkedSubshape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::string& sourceElementName,
    const std::string& targetElementName
);
NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<std::pair<std::string, std::string>>& sourceToTargetElements
);
NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<LinkedSubshapeRetag>& sourceToTargetElements
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementTransform(), after BRepBuilderAPI_Transform, calls
// "copyElementMap(tmp, op)" so transformed copies keep source stable aliases.
NamedShape namedShapeForTransformedCopy(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementBoolean(),
// selects BRepAlgoAPI_Fuse/Cut/Common, puts the first input into Arguments and
// the rest into Tools, then calls makeElementShape(*mk, inputs, ...).
NamedShapeBuild makeElementBooleanFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    BooleanOperation operation
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementXor(),
// executes "Step 1: Union(A, B)", "Step 2: Common(A, B)", then "Cut(Union,
// Common)" when the intersection exists, and routes every maker through
// makeElementBoolean(...)/makeElementShape(...).
NamedShapeBuild makeElementXorFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources
);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartSection.cpp
// ::Section::makeOperation(), configures FCBRepAlgoAPI_Section with Base, Tool and
// Approximation, then Part::Boolean::execute() routes it through makeElementShape(*mkBool,
// shapes, opCode()).
NamedShapeBuild makeElementSectionFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool approximate
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementGeneralFuse(), builds "BRepAlgoAPI_BuilderAlgo mkGFA",
// calls SetArguments(...), then makeElementShape(mkGFA, shapes, OpCodes::GeneralFuse).
NamedShapeBuild makeElementGeneralFuseFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    double tolerance
);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
// ::TopoShape::makeElementRefine(), "BRepBuilderAPI_RefineModel mkRefine(getShape())"
// then "GenericShapeMapper mapper; mkRefine.populate(mapper); mapper.init(shape,
// mkRefine.Shape())" before makeShapeWithElementMap(...).
NamedShapeBuild makeElementRefineFromSource(const std::string& owner, const NamedShapeSource& source);
std::optional<std::string> resolveElementName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
);
ElementResolveResult resolveElementReference(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
);
std::optional<TopoDS_Shape> subshapeByName(const NamedShape& namedShape, const std::string& name);
std::optional<TopoDS_Shape> subshapeByName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
);
nlohmann::json namedShapeToJson(const NamedShape& namedShape);
nlohmann::json namedShapesToJson(const std::map<std::string, NamedShape>& namedShapes);

}  // namespace cad_core::topo
