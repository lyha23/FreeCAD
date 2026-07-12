#pragma once

// Part-layer TopoShape / NamedShape facade aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape*.cpp.
#include "cad_core/part/internal_shape_history_ledger.h"
#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/app/string_hasher.h"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
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

struct NamedShape;

struct NamedShapeChildMap
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), stores MappedChildElements ranges and may defer lookup
    // through "child.elementMap"; TopoShapeExpansion.cpp::TopoShape::createChildMap() fills
    // "indexedName", "offset", "count", "elementMap" and "postfix" for compound child sources.
    std::string sourceOwner;
    std::string kind;
    std::string indexedName;
    // `indexedName` is the ElementMap::findAll() query range (Vertex1/Edge1/Face1).  A
    // PropertyPartShape copy can publish the same three typed ranges as one child owner, so
    // keep that protocol owner/path identity separately instead of overloading the lookup key.
    std::string protocolPathPrefix;
    int offset = 0;
    int count = 0;
    std::string targetStart;
    std::string targetEnd;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::setupChild() stores the incoming TopoShape Tag on
    // each MappedChildElements range (or zero when it equals the receiver Tag). It is entry-local
    // child-map evidence for ElementMap::addChildElements()/hashChildMaps(), not a response tag.
    long tag = 0;
    std::string postfix;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements() stores the child's "elementMap" beside the mapped child
    // range. cad-core keeps a request-local pointer so runtime topoNamingState publication can
    // read child mapped-name provenance without re-deriving it from fixture output.
    std::shared_ptr<const NamedShape> sourceLedger;
    const NamedShape* sourceNamedShape = nullptr;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::hashChildMaps(), rewrites eligible child map postfixes into encoded
    // child-map keys using "MAPPED_CHILD_ELEMENTS_PREFIX". cad-core keeps this as request-local
    // evidence, not as a FreeCAD MappedName byte-for-byte serialization.
    std::string encodedChildMapKey;
    bool hasSourceElementMap = false;
    std::size_t sourceElementMapSize = 0;
    std::size_t sourceChildMapCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements() may expand a grandchild range internally so lookup can
    // consume the nested ledger.  That expansion is request-local resolver evidence; only the
    // direct FeatureCompound Links child belongs in the public childElementMaps projection.
    bool recursiveExpansion = false;
};

enum class MappedNameProvenanceStatus
{
    SourceBacked,
    IndexedOnly,
    MissingTag,
    MissingOperation,
    Blocked,
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildShape() records g<ID>;SKT producer evidence for a later Part maker,
// while the direct Sketch Shape and InternalShape have distinct public roles.  This scope is
// assigned by the Part producer, so runtime can project the ledger without inferring policy
// from an owner name or a mapped-name string.
enum class MappedNamePublicationScope
{
    Public,
    ProducerOnly,
};

struct MappedNameProvenance
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::mapSubElement(...), calls
    // "ensureElementMap()->encodeElementName(..., Tag, op, other.Tag)" before
    // ElementMap::setElementName(); /Users/li/Chili3DProject/FreeCAD/src/App/
    // ElementMap.cpp::ElementMap::encodeElementName(... masterTag ... postfix ... tag ...)
    // appends "POSTFIX_TAG" and the element type. This is request-local producer evidence only;
    // S2/S3 fill it from maker history instead of copying expected raw mapped-name strings.
    std::string entryKey;
    std::string currentElement;
    std::string sourceElement;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::encodeElementName(), hashes the incoming `name` before appending the
    // producer tag. Preserve that request-local input separately from sourceElement, which may
    // already be its compact StringID, and from rawMappedName, which is the encoded result.
    std::string encodeInputMappedName;
    std::string elementType;
    std::optional<long> producerTag;
    std::optional<long> masterTag;
    std::optional<long> sourceTag;
    std::string operationPostfix;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/MappedName.cpp
    // ::MappedName::findTagInElementName(), parses ";:H<tag>:<len>,<type>" from raw mapped
    // names. canonicalMappedName is the S2 codec's dehashed/canonical form, not a fallback
    // stable token.
    std::string rawMappedName;
    std::string canonicalMappedName;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::findAll() returns a MappedName together with its per-entry
    // ElementIDRefs. The same raw `#id` in two producer maps may legitimately carry different
    // refs; this request-local sidecar must not be recovered from the document StringHasher by
    // raw string alone.
    std::vector<app::StringId> elementIdRefs;
    // FreeCAD TopoShapeExpansion.cpp keeps shapeOffset=3 candidates in `newNames` during the
    // first reverse pass. Their mapped name may already be encoded, but it must not parent U
    // aliases until the delayed pass.
    bool delayedHighLevel = false;
    MappedNameProvenanceStatus status = MappedNameProvenanceStatus::IndexedOnly;
    MappedNamePublicationScope publicationScope = MappedNamePublicationScope::Public;
};

// FreeCAD: src/App/ElementMap.h keeps a per-IndexedName list of MappedName references.
// `elementMap` below remains a request-local lookup index while this ledger records only the
// encoded names actually written by ElementMap::setElementName(), in write order.
struct ElementMapEntry
{
    std::string mappedName;
    std::vector<app::StringId> elementIdRefs;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
// ::ElementMap::findAll() returns its stored raw `MappedName` and appends `child.postfix` only
// while resolving a child range. This classifies already-recorded Part provenance for the frozen
// public DTO; runtime must project it rather than reclassifying mapped-name bytes or request data.
std::string mappedNamePublicEvidenceSource(const MappedNameProvenance& provenance);

struct NamedShape
{
    std::string owner;
    TopoDS_Shape shape;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::TopoShape(long
    // Tag, ...) stores the PropertyPartShape owner ID, while
    // PropertyTopoShape.cpp::PropertyPartShape::setValue() retags a completed Feature map to
    // its DocumentObject ID. This request-local value is the incoming TopoShape Tag used by
    // TopoShapeExpansion.cpp::NameKey; it is not derived from BRep identity or display names.
    std::optional<long> producerTag;
    std::map<std::string, NamedElement> elements;
    std::map<std::string, std::string> elementMap;
    std::map<std::string, MappedNameProvenance> mappedNameProvenance;
    std::map<std::string, std::vector<ElementMapEntry>> elementMapEntries;
    std::vector<NamedShapeChildMap> childElementMaps;
    std::vector<ElementHistory> history;
    std::vector<MapperHistoryEvent> mapperHistory;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp,
    // MapperHistory can carry generated/modified/split/merge/deleted outcomes independently
    // from the currently resolvable ElementMap. cad-core exposes this request-local summary so
    // diagnostics can distinguish an indexed-only map from a partially consumed history ledger.
    std::vector<std::string> elementHistoryStatus;
    std::optional<nlohmann::json> sketchInternalHistoryDiagnostics;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp uses the document's shared
    // StringHasher for every ElementMap producer.  The pointer is request-local provenance state,
    // not geometry or a runtime response cache.
    std::shared_ptr<cad_core::app::StringHasher> stringHasher;
};

void recordElementMapEntry(NamedShape& namedShape,
                           const std::string& mappedName,
                           const std::string& currentElement,
                           bool preserveEncoding = false);


// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
// ::ElementMap::addChildElements() keeps direct child ranges and resolves their mapped names
// before callers query them.  This narrow Part finalizer consumes the already source-backed
// owner aliases plus those ranges, records canonical collisions in NamedShape::mapperHistory,
// and leaves runtime to project that ledger without synthesizing new history.
void appendProtocolChildMapCanonicalCollisionHistory(NamedShape& namedShape);

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
    // FeatureExtrude::execute() sets its local `prism.Tag = -this->getID()` immediately before
    // the final Boolean. An operation-local source may therefore override the property's
    // completed producerTag without mutating the persisted AddSubShape/Shape ledger.
    std::optional<long> producerTag;
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
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::TopoShape(long Tag, ...)
// stores the request-local `Tag`; TopoShapeExpansion.cpp::TopoShape::mapSubElement() passes
// `Tag` and `other.Tag` to ElementMap::encodeElementName(). Only a DocumentObject/property
// boundary may supply that value. A BRep fingerprint is not a TopoShape Tag and must never
// become mapped-name evidence.
std::optional<long> requestLocalProducerTagForShape(const TopoDS_Shape& shape);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap(),
// iterates InternalShape vertices/edges and records Internal* <-> raw Edge/Vertex aliases after
// "findSubShapesWithSharedVertex(..., CheckGeometry | SingleResult)". This helper builds the
// Sketch InternalShape NamedShape baseline; FaceMaker/WireJoiner history can publish stable
// aliases for generated InternalFaceN regions through the InternalShape element map.
NamedShape namedShapeForSketchInternalShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    std::optional<InternalShapeHistoryLedger> historyLedger = std::nullopt,
    std::map<std::string, std::string> internalEdgeMappedNames = {},
    const NamedShape* rawNamedShape = nullptr,
    std::shared_ptr<cad_core::app::StringHasher> stringHasher = nullptr
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::buildInternals() passes the raw Sketch Shape to makeElementFace(), whose bounded face is the
// closed Pad/Pocket profile. This builds that FaceMaker producer ledger from the raw Sketch
// ElementMap before PartDesign asks MapperMaker for Pad history.
NamedShape namedShapeForSketchProfileShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& profileShape,
    const NamedShape& rawNamedShape,
    std::shared_ptr<cad_core::app::StringHasher> stringHasher,
    std::function<void(const std::string&)> beforeSourceEntry = {}
);
void applyInternalShapeHistoryPublication(
    NamedShape& namedShape,
    const InternalShapeHistoryPublication& publication
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeShapeWithElementMap() receives the producer op code and maps only maker-backed
// preserved/generated/modified elements. Producer wrappers decide whether an unmapped source is
// a real transition and whether the result exposes operation-local IndexedName aliases.
struct MakerHistoryOptions
{
    std::string producerOperation;
    bool recordUnmappedSourceDeletions = true;
    bool addProducerLocalAliases = false;
    std::shared_ptr<cad_core::app::StringHasher> stringHasher;
    // A replacement feature's mapSubElement() starts a new ElementMap owner. It must rehash a
    // preserved incoming mapped name so a Chamfer/Fillet/Pattern does not publish the prior
    // Pad producer identity. Profile transport shapes leave this false and retain their source
    // ledger until the first consuming maker reads it.
    bool rehashPreservedMappedName = false;
    // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap() invokes
    // mapSubElement(shapes) with no op before Boolean M/G collection. Boolean-preserved entries
    // retain the full input raw mapped name and append only the incoming TopoShape Tag; DressUp
    // replacement producers use the separate rehash lifecycle above.
    bool preserveRawMappedName = false;
    // The result TopoShape Tag. PropertyPartShape::setValue() supplies the completed Feature
    // object ID; helper-only makers leave this unset rather than inventing a geometry hash.
    std::optional<long> producerTag;
    // FreeCAD: src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute() creates an
    // additive prism before it is fused into the BaseFeature. Its first Generated ElementMap
    // entry may retain a bare source StringID as its index; subtractive tools and generic Part
    // makers do not opt into that FeatureExtrude-specific lifecycle.
    bool promoteBareSourceIdForGenerated = false;
    // FreeCAD Chamfer/Fillet execute their OCCT dress-up maker directly in the feature
    // recompute scope; their later Refine owns the observable nested maker lifecycle.
    bool emitMakerScopes = true;
};

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementPrism(),
// creates BRepPrimAPI_MakePrism and then calls makeElementShape(...), which consumes
// MapperMaker::Generated/Modified history from the BRepBuilderAPI_MakeShape maker.
NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::string& sourceOwner,
    const TopoDS_Shape& sourceShape,
    BRepBuilderAPI_MakeShape& maker,
    MakerHistoryOptions options = {}
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
// calls makeElementShape(*mk, inputs, ...); MapperMaker then consumes the BRepAlgoAPI
// BooleanOperation Generated/Modified history for every input source.
NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_MakeShape& maker,
    MakerHistoryOptions options = {}
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
    const std::vector<NamedShapeSource>& sources,
    const std::string& producerOperation = {}
);
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
// ::PropertyPartShape::setValue(const TopoShape&) assigns a producer map to a document property.
// ElementMap owns ElementIDRefs per entry, so a reused document StringID cannot make a later
// property value inherit an earlier producer's endpoint ref merely because both names start
// with the same `#id`.
NamedShape namedShapeForPropertyShapeValue(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const NamedShape& source,
    long propertyTag,
    bool emitReferenceUpdate = false,
    bool emitBodyTipLifecycle = false,
    const std::vector<std::string>& referencedSubnames = {}
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
    const NamedShapeSource& source,
    std::optional<long> propertyTag = std::nullopt
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
    const NamedShapeSource& source,
    std::optional<std::string> postfix = std::nullopt,
    bool emitDirectMapLifecycle = true
);
// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementBoolean(),
// selects BRepAlgoAPI_Fuse/Cut/Common, puts the first input into Arguments and
// the rest into Tools, then calls makeElementShape(*mk, inputs, ...).
NamedShapeBuild makeElementBooleanFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    BooleanOperation operation,
    std::optional<double> tolerance = std::nullopt,
    std::optional<long> producerTag = std::nullopt
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
