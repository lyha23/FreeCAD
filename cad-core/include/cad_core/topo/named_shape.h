#pragma once

#include "cad_core/topo/mapper_history.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
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
    TopoDS_Wire openExportWire;
    TopoDS_Edge openExportEdge;
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
    bool purgeBridge = false;
};

struct SketchInternalFaceMakerEdgeEvidence
{
    std::string makerStage;
    std::string relation;
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    TopoDS_Edge targetEdge;
    bool preSplitHistory = false;
    bool splitterHistory = false;
};

struct SketchInternalFaceMakerBoundedFaceBoundaryEvidence
{
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    std::string makerStage;
    std::string relation;
    TopoDS_Edge targetEdge;
};

struct SketchInternalFaceMakerBoundedFaceEvidence
{
    std::size_t boundedFaceIndex = 0;
    TopoDS_Face face;
    std::vector<std::size_t> sourceEdgeIndices;
    std::vector<std::size_t> outerBoundaryTargetEdgeIndices;
    std::vector<SketchInternalFaceMakerBoundedFaceBoundaryEvidence> outerBoundary;
};

struct SketchInternalHistoryContext
{
    std::size_t sourceEdgeCount = 0;
    std::size_t preSplitEdgeCount = 0;
    std::size_t splitterEdgeCount = 0;
    std::size_t boundedFaceCount = 0;
    bool preSplitHistory = false;
    bool splitterHistory = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
    // ::FaceMaker::postBuild(), maps pre-split and splitter history before generated face naming.
    // These entries are producer evidence; InternalShape topo consumers must not rebuild them from
    // bbox, area, geometry type, or output order.
    std::vector<SketchInternalFaceMakerEdgeEvidence> faceMakerEdgeEvidence;
    std::vector<SketchInternalFaceMakerBoundedFaceEvidence> faceMakerBoundedFaceEvidence;
    std::size_t wireJoinerSourceEdgeCount = 0;
    std::size_t wireJoinerSplitResultEdgeCount = 0;
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
    std::vector<MapperHistoryEvent> mapperHistory;
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
