#pragma once

// Part-layer TopoShape / NamedShape facade aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape*.cpp.
#include "cad_core/part/internal_shape_history_ledger.h"
#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <string>
#include <vector>

class BRepOffsetAPI_ThruSections;
class BRepBuilderAPI_Sewing;
class ShapeFix_Root;

namespace cad_core::part
{
class ShapeFixHistory;
struct TaperedExtrusionResult;
}  // namespace cad_core::part

namespace cad_core::part
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

struct NamedElement
{
    std::string name;
    SubshapeName subshape;
    ElementHistoryKind status = ElementHistoryKind::Indexed;
    std::vector<std::string> sources;
};

struct NamedShapeChildMap
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), stores MappedChildElements ranges and may defer lookup
    // through "child.elementMap"; TopoShapeExpansion.cpp::TopoShape::createChildMap() fills
    // "indexedName", "offset", "count", "elementMap" and "postfix" for compound child sources.
    std::string sourceOwner;
    std::string kind;
    std::string indexedName;
    int offset = 0;
    int count = 0;
    std::string targetStart;
    std::string targetEnd;
    std::string postfix;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::hashChildMaps(), rewrites eligible child map postfixes into encoded
    // child-map keys using "MAPPED_CHILD_ELEMENTS_PREFIX". cad-core keeps this as request-local
    // evidence, not as a FreeCAD MappedName byte-for-byte serialization.
    std::string encodedChildMapKey;
    bool hasSourceElementMap = false;
    std::size_t sourceElementMapSize = 0;
    std::size_t sourceChildMapCount = 0;
};

struct NamedShape
{
    std::string owner;
    TopoDS_Shape shape;
    std::map<std::string, NamedElement> elements;
    std::map<std::string, std::string> elementMap;
    std::vector<NamedShapeChildMap> childElementMaps;
    std::vector<ElementHistory> history;
    std::vector<MapperHistoryEvent> mapperHistory;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp,
    // MapperHistory can carry generated/modified/split/merge/deleted outcomes independently
    // from the currently resolvable ElementMap. cad-core exposes this request-local summary so
    // diagnostics can distinguish an indexed-only map from a partially consumed history ledger.
    std::vector<std::string> elementHistoryStatus;
    std::optional<InternalShapeHistoryLedger> sketchInternalHistory;
};

struct NamedShapeSource
{
    std::string owner;
    TopoDS_Shape shape;
    const NamedShape* namedShape = nullptr;
    std::vector<std::string> ownerAliases;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::createChildMap(), when an operation string is supplied, stores it in
    // MappedChildElements::postfix before ElementMap::addChildElements() encodes the child map.
    std::string childElementMapPostfix;
    bool expandCompoundForBoolean = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // FCBRepAlgoAPI_BooleanOperation.cpp::RecursiveCutFusedTools(), "cut argument and compound
    // tool" then "if tool consists of two or more shapes, fuse them together".
    bool fuseCompoundForCut = false;
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
    std::optional<InternalShapeHistoryLedger> historyLedger = std::nullopt
);
void consumeInternalShapeHistoryLedger(
    NamedShape& namedShape,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    const nlohmann::json& internalElementMap,
    const InternalShapeHistoryLedger& ledger
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
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperSewing,
// "modified()" first asks "maker.Modified(s)" and then "maker.ModifiedSubShape(s)" before
// makeShapeWithElementMap() consumes the sewing history for front/back faces and shells.
NamedShape namedShapeForSewingHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_Sewing& maker
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp::Extrusion::extrudeShape(),
// calls "ExtrusionHelper::makeElementDraft(params, myShape, drafts, result.Hasher)" for taper;
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections,
// adds "GeneratedFace(s)", "FirstShape()" and "LastShape()" to the loft maker history.
std::optional<NamedShape> namedShapeForTaperedExtrusionHistory(
    const std::string& owner,
    const part::TaperedExtrusionResult& tapered,
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
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementBoolean(), routes OpCodes::Compound to
// "return makeElementCompound(shapes, op, SingleShapeCompoundCreationPolicy::returnShape)".
NamedShapeBuild makeElementCompoundFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool returnSingleShape = true
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementWires(), after BRepBuilderAPI_MakeWire may replace connected
// vertices, updates the edge shape so ElementMapPolicy::Propagate can call
// "wires.back().mapSubElement(edges, op)" with the post-maker edge identity.
NamedShapeBuild makeElementWiresWithPropagatedSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    const std::string& op
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementShell(), builds a TopoDS_Shell from source faces, then for
// ElementMapPolicy::Propagate calls "tmp.mapSubElement(*this, op)" before resetting the shell
// ElementMap.
NamedShapeBuild makeElementShellWithPropagatedSource(
    const std::string& owner,
    const NamedShapeSource& source,
    const std::string& op
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
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementBoolean(), selects FCBRepAlgoAPI_Section for OpCodes::Section,
// appends the first source to Arguments and the remaining sources to Tools, and keeps
// "buildShell = false" so section edge/wire output stays non-solid.
NamedShapeBuild makeElementSectionFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool approximate
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementOffset(), creates "BRepOffsetAPI_MakeOffsetShape mkOffset",
// calls "PerformByJoin(...)" and when "FillType::fill" is requested, sews the original
// shape, offset result, and free-bound perimeter faces before makeShapeWithElementMap().
NamedShapeBuild makeElementOffsetFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    double tolerance,
    bool intersection,
    bool selfIntersection,
    short offsetMode,
    short join,
    bool fill = false
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp
// ::Offset2D::execute(), calls "TopoShape(0).makeElementOffset2D(shape, offset, join, fill,
// openresult, inter)"; /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
// TopoShapeExpansion.cpp::TopoShape::makeElementOffset2D(), for face offsets "extracts source
// wires", calls "BRepOffsetAPI_MakeOffsetFix", remakes no-fill faces from offset wires, and for
// FillType::fill handles the closed-wire case by making a face from source wire + offset wire.
// cad-core currently covers the OCCT MakeOffset-compatible planar face no-fill and closed-fill
// subsets.
NamedShapeBuild makeElementOffset2DFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult,
    bool intersection
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
// ::Thickness::execute(), calls "TopoShape(0, ...).makeElementThickSolid(base, shapes,
// thickness, tol, inter, self, mode, static_cast<JoinType>(join))"; /Users/li/.../src/Mod/Part/
// App/TopoShapeExpansion.cpp::TopoShape::makeElementThickSolid(), calls
// "mkThick.MakeThickSolidByJoin(...)" and then makeElementShape(mkThick, shape, op).
NamedShapeBuild makeElementThickSolidFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    const std::vector<TopoDS_Face>& faces,
    double offset,
    double tolerance,
    bool intersection,
    bool selfIntersection,
    short offsetMode,
    short join
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementSolid(), accepts one compsolid or all shells through
// "BRepBuilderAPI_MakeSolid", then calls makeElementShape(mkSolid, shape, op).
NamedShapeBuild makeElementSolidFromSource(const std::string& owner, const NamedShapeSource& source);
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
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
// ::TopoShape::fix(), calls "makeShapeWithElementMap(fixThis.Shape(), MapperHistory(fixThis),
// {*this})" because "ShapeFix_Shape may delete (e.g. small edges) or modify the input shape".
NamedShapeBuild makeElementShapeFixFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double precision = 0.0,
    double smallEdgeTolerance = 0.0
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/tests/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::MapperHistoryModified constructs "MapperHistory(*fix)" from ShapeFix_Wireframe, and
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::MapperHistory::MapperHistory(ShapeFix_Root& fix), reads "fix.Context()->History()".
NamedShape namedShapeForShapeFixRootHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    ShapeFix_Root& fix
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeShapeWithElementMap(), checks "ElementMapPolicy::Drop", calls
// "dropElementNaming()" and returns before preserving aliases.
NamedShape namedShapeForElementMapPolicyDrop(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
);
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

}  // namespace cad_core::part
