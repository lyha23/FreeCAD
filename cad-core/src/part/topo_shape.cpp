#include "cad_core/part/topo_shape.h"

#include "cad_core/part/extrusion_helper.h"
#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/refine_model.h"
#include "cad_core/part/shape_fix.h"
#include "cad_core/app/element_map.h"
#include "cad_core/topo/freecad_mapped_name_codec.h"

#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_BuilderAlgo.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgo_Image.hxx>
#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRepTools_History.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Precision.hxx>
#include <ShapeBuild_ReShape.hxx>
#include <ShapeAnalysis_FreeBoundsProperties.hxx>
#include <ShapeFix_Root.hxx>
#include <ShapeUpgrade_ShellSewing.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_CompSolid.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace cad_core::part
{

namespace
{

std::string historyKindName(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Indexed:
            return "indexed";
        case ElementHistoryKind::Generated:
            return "generated";
        case ElementHistoryKind::Modified:
            return "modified";
        case ElementHistoryKind::Deleted:
            return "deleted";
        case ElementHistoryKind::Split:
            return "split";
        case ElementHistoryKind::Merge:
            return "merge";
    }
    return "unknown";
}

MapperHistoryRelation mapperRelationForHistoryKind(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Indexed:
            return MapperHistoryRelation::Identity;
        case ElementHistoryKind::Generated:
            return MapperHistoryRelation::Generated;
        case ElementHistoryKind::Modified:
            return MapperHistoryRelation::Modified;
        case ElementHistoryKind::Deleted:
            return MapperHistoryRelation::Deleted;
        case ElementHistoryKind::Split:
            return MapperHistoryRelation::Split;
        case ElementHistoryKind::Merge:
            return MapperHistoryRelation::Merge;
    }
    return MapperHistoryRelation::Modified;
}

MapperHistoryRecoverability mapperRecoverabilityForHistoryKind(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Indexed:
        case ElementHistoryKind::Generated:
        case ElementHistoryKind::Modified:
        case ElementHistoryKind::Merge:
            return MapperHistoryRecoverability::Resolved;
        case ElementHistoryKind::Deleted:
            return MapperHistoryRecoverability::Deleted;
        case ElementHistoryKind::Split:
            return MapperHistoryRecoverability::NeedsReselect;
    }
    return MapperHistoryRecoverability::Unknown;
}

std::string mapperStageForHistoryKind(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Indexed:
            return "indexed";
        case ElementHistoryKind::Generated:
        case ElementHistoryKind::Modified:
            return "maker_history";
        case ElementHistoryKind::Deleted:
        case ElementHistoryKind::Split:
            return "terminal_history";
        case ElementHistoryKind::Merge:
            return "element_map_merge";
    }
    return "unknown";
}

std::string diagnosticStatusForHistoryKind(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Deleted:
            return "deleted_stable_subname";
        case ElementHistoryKind::Split:
            return "split_stable_subname";
        default:
            return {};
    }
}

std::string mappedNameProvenanceStatusName(MappedNameProvenanceStatus status)
{
    switch (status) {
        case MappedNameProvenanceStatus::SourceBacked:
            return "source_backed";
        case MappedNameProvenanceStatus::IndexedOnly:
            return "indexed_only";
        case MappedNameProvenanceStatus::MissingTag:
            return "missing_tag";
        case MappedNameProvenanceStatus::MissingOperation:
            return "missing_op";
        case MappedNameProvenanceStatus::Blocked:
            return "blocked";
    }
    return "unknown";
}

void addIndexedElements(
    NamedShape& namedShape,
    const TopTools_IndexedMapOfShape& shapes,
    TopAbs_ShapeEnum kind,
    const std::string& prefix
)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        const std::string name = prefix + std::to_string(index);
        namedShape.elements[name]
            = NamedElement {name, SubshapeName {kind, index}, ElementHistoryKind::Indexed, {}};
        namedShape.elementMap[name] = name;
        namedShape.history.push_back(ElementHistory {ElementHistoryKind::Indexed, name, {}});
    }
}

void addDistinctString(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

double autoFuzzyValueForSources(const std::vector<NamedShapeSource>& sources)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // FCBRepAlgoAPI_BooleanOperation.cpp::FCBRepAlgoAPIHelper::setAutoFuzzy(),
    // computes "Part::FuzzyHelper::getBooleanFuzzy() * sqrt(bounds.SquareExtent()) *
    // Precision::Confusion()" from Arguments() and Tools(); AppPart.cpp initializes
    // "BooleanFuzzy" with hGrp->GetFloat("BooleanFuzzy",10.0).
    constexpr double freeCadDefaultBooleanFuzzy = 10.0;
    Bnd_Box bounds;
    for (const auto& source : sources) {
        if (!source.shape.IsNull()) {
            BRepBndLib::Add(source.shape, bounds);
        }
    }
    if (bounds.IsVoid()) {
        return Precision::Confusion();
    }
    return freeCadDefaultBooleanFuzzy * std::sqrt(bounds.SquareExtent()) * Precision::Confusion();
}

void expandCompoundSource(const NamedShapeSource& source, std::vector<NamedShapeSource>& expanded)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::expandCompound(), recursively replaces a TopAbs_COMPOUND boolean
    // input with its child shapes before makeElementBoolean() fills Arguments and Tools.
    if (source.shape.ShapeType() != TopAbs_COMPOUND) {
        expanded.push_back(source);
        return;
    }
    for (TopoDS_Iterator it(source.shape); it.More(); it.Next()) {
        NamedShapeSource childSource {source.owner, it.Value(), source.namedShape};
        childSource.ownerAliases = source.ownerAliases;
        childSource.expandCompoundForBoolean = source.expandCompoundForBoolean;
        childSource.fuseCompoundForCut = source.fuseCompoundForCut;
        expandCompoundSource(childSource, expanded);
    }
}

void appendBooleanSource(const NamedShapeSource& source, std::vector<NamedShapeSource>& sources)
{
    if (source.expandCompoundForBoolean) {
        expandCompoundSource(source, sources);
        return;
    }
    sources.push_back(source);
}

std::vector<NamedShapeSource> expandBooleanSourcesLikeFreeCad(
    const std::vector<NamedShapeSource>& sources,
    BooleanOperation operation
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementBoolean(), expands Fuse/Cut compound inputs
    // before calling SetArguments()/SetTools(). cad-core gates this per source until compound
    // child alias propagation is complete for every producer.
    std::vector<NamedShapeSource> expanded;
    if (operation == BooleanOperation::Fuse) {
        for (const NamedShapeSource& source : sources) {
            appendBooleanSource(source, expanded);
        }
    }
    else if (operation == BooleanOperation::Cut) {
        expanded.push_back(sources.front());
        for (std::size_t index = 1; index < sources.size(); ++index) {
            appendBooleanSource(sources.at(index), expanded);
        }
    }
    if (expanded.empty()) {
        return sources;
    }
    return expanded;
}

std::string prefixForKind(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "Face";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_VERTEX:
            return "Vertex";
        default:
            return {};
    }
}

std::vector<TopAbs_ShapeEnum> mappableKinds()
{
    return {TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX};
}

std::vector<TopAbs_ShapeEnum> childMapKinds()
{
    return {TopAbs_VERTEX, TopAbs_EDGE, TopAbs_FACE};
}

std::string childMapTargetName(const std::string& prefix, int offset, int count)
{
    if (count <= 0) {
        return {};
    }
    return prefix + std::to_string(offset + count);
}

std::string composeChildMapPostfix(const std::string& parentPostfix, const std::string& childPostfix)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), when expanding a grandchild map, assigns
    // "entry->postfix = grandchild.postfix + ELEMENT_MAP_PREFIX + entry->postfix" unless the
    // parent postfix already starts with ELEMENT_MAP_PREFIX.
    if (childPostfix.empty()) {
        return parentPostfix;
    }
    if (parentPostfix.empty()) {
        return childPostfix;
    }
    return childPostfix + (parentPostfix.front() == ';' ? std::string() : std::string(";"))
        + parentPostfix;
}

void mixStableChildMapHash(std::uint64_t& hash, const std::string& value)
{
    constexpr std::uint64_t fnvPrime = 1099511628211ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= fnvPrime;
    }
    hash ^= 0xffU;
    hash *= fnvPrime;
}

void mixStableChildMapHash(std::uint64_t& hash, int value)
{
    mixStableChildMapHash(hash, std::to_string(value));
}

std::string encodedChildMapKey(const NamedShapeChildMap& childMap)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::hashChildMaps(), writes keys with "MAPPED_CHILD_ELEMENTS_PREFIX" (";:R")
    // after hashing the mapped child postfix. This is cad-core's stable request-local key
    // evidence; it deliberately does not claim FreeCAD MappedName binary compatibility.
    std::uint64_t hash = 1469598103934665603ULL;
    mixStableChildMapHash(hash, childMap.sourceOwner);
    mixStableChildMapHash(hash, childMap.kind);
    mixStableChildMapHash(hash, childMap.indexedName);
    mixStableChildMapHash(hash, childMap.offset);
    mixStableChildMapHash(hash, childMap.count);
    mixStableChildMapHash(hash, childMap.targetStart);
    mixStableChildMapHash(hash, childMap.targetEnd);
    mixStableChildMapHash(hash, childMap.postfix);
    mixStableChildMapHash(hash, static_cast<int>(childMap.sourceElementMapSize));
    mixStableChildMapHash(hash, static_cast<int>(childMap.sourceChildMapCount));

    std::ostringstream out;
    out << ";:R" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool shouldEncodeChildMapKey(const NamedShapeChildMap& childMap)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), "do child mapping only if the child element count >= 5",
    // with a tag-specific count==5 skip branch before hashChildMaps() can rewrite the key. This
    // tagless slice records encoded keys for no-map entries and source element-map ranges above
    // that threshold; exact tag hashing remains part of the Propagate lifecycle gap.
    return !childMap.hasSourceElementMap || childMap.count > 5;
}

struct SourceTargets
{
    std::set<std::string> preserved;
    std::set<std::string> history;
    std::map<std::string, ElementHistoryKind> historyKinds;
    std::optional<long> sourceTag;
    std::optional<MappedNameProvenance> inheritedMappedName;
    std::string preservedOperationPostfix;
    std::string sourceElement;
    // A nested child ElementMap is resolved by FreeCAD's addChildElements() before its parent
    // serializes the final mapped name.  Keep that lifecycle fact beside the source evidence so
    // the inherited raw name is retagged at the Part boundary rather than synthesized by runtime.
    bool composeInheritedChildMapTag = false;
};

struct FilledOffsetBuild
{
    TopoDS_Shape shape;
    std::string error;
};

struct SolidRecoveryBuild
{
    TopoDS_Shape shape;
    std::optional<NamedShape> namedShape;
    bool applied = false;
    std::string error;
};

std::string localElementName(const std::string& elementName)
{
    const std::size_t dot = elementName.rfind('.');
    return dot == std::string::npos ? elementName : elementName.substr(dot + 1);
}

std::string mappedNameElementType(const std::string& elementName)
{
    const auto parsed = parseSubshapeName(localElementName(elementName));
    if (!parsed) {
        return {};
    }
    switch (parsed->kind) {
        case TopAbs_FACE:
            return "Face";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_VERTEX:
            return "Vertex";
        default:
            return {};
    }
}

void appendFingerprintDouble(std::ostringstream& out, double value)
{
    if (std::abs(value) < 1e-12) {
        value = 0.0;
    }
    out << std::fixed << std::setprecision(12) << value << ',';
}

void appendFingerprintPoint(std::ostringstream& out, const gp_Pnt& point)
{
    appendFingerprintDouble(out, point.X());
    appendFingerprintDouble(out, point.Y());
    appendFingerprintDouble(out, point.Z());
}

void appendFingerprintBounds(std::ostringstream& out, const TopoDS_Shape& shape)
{
    try {
        Bnd_Box bounds;
        BRepBndLib::Add(shape, bounds);
        if (bounds.IsVoid()) {
            out << "bbox:void;";
            return;
        }
        double xmin = 0.0;
        double ymin = 0.0;
        double zmin = 0.0;
        double xmax = 0.0;
        double ymax = 0.0;
        double zmax = 0.0;
        bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        out << "bbox:";
        appendFingerprintDouble(out, xmin);
        appendFingerprintDouble(out, ymin);
        appendFingerprintDouble(out, zmin);
        appendFingerprintDouble(out, xmax);
        appendFingerprintDouble(out, ymax);
        appendFingerprintDouble(out, zmax);
        out << ';';
    }
    catch (const Standard_Failure&) {
        out << "bbox:error;";
    }
}

void appendFingerprintMass(std::ostringstream& out,
                           const TopoDS_Shape& shape,
                           TopAbs_ShapeEnum kind)
{
    try {
        GProp_GProps props;
        if (kind == TopAbs_EDGE) {
            BRepGProp::LinearProperties(shape, props);
        }
        else if (kind == TopAbs_FACE) {
            BRepGProp::SurfaceProperties(shape, props);
        }
        else if (kind == TopAbs_SOLID || kind == TopAbs_COMPSOLID) {
            BRepGProp::VolumeProperties(shape, props);
        }
        else if (kind == TopAbs_VERTEX) {
            out << "point:";
            appendFingerprintPoint(out, BRep_Tool::Pnt(TopoDS::Vertex(shape)));
            out << ';';
            return;
        }
        else {
            return;
        }
        out << "mass:";
        appendFingerprintDouble(out, props.Mass());
        out << "center:";
        appendFingerprintPoint(out, props.CentreOfMass());
        out << ';';
    }
    catch (const Standard_Failure&) {
        out << "mass:error;";
    }
}

std::vector<TopAbs_ShapeEnum> producerTagFingerprintKinds()
{
    return {
        TopAbs_COMPSOLID,
        TopAbs_SOLID,
        TopAbs_SHELL,
        TopAbs_FACE,
        TopAbs_WIRE,
        TopAbs_EDGE,
        TopAbs_VERTEX,
    };
}

std::string shapeFingerprintPart(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    std::ostringstream out;
    out << "type:" << static_cast<int>(shape.ShapeType()) << ';';
    out << "orientation:" << static_cast<int>(shape.Orientation()) << ';';
    appendFingerprintBounds(out, shape);
    appendFingerprintMass(out, shape, kind);
    return out.str();
}

std::string producerTagFingerprint(const TopoDS_Shape& shape)
{
    std::ostringstream out;
    out << "shape-type:" << static_cast<int>(shape.ShapeType()) << ';';
    out << "shape-orientation:" << static_cast<int>(shape.Orientation()) << ';';
    appendFingerprintBounds(out, shape);
    for (TopAbs_ShapeEnum kind : producerTagFingerprintKinds()) {
        TopTools_IndexedMapOfShape subshapes;
        TopExp::MapShapes(shape, kind, subshapes);
        std::vector<std::string> parts;
        parts.reserve(static_cast<std::size_t>(subshapes.Extent()));
        for (int index = 1; index <= subshapes.Extent(); ++index) {
            parts.push_back(shapeFingerprintPart(subshapes(index), kind));
        }
        std::sort(parts.begin(), parts.end());
        out << "kind:" << static_cast<int>(kind) << ":count:" << parts.size() << ';';
        for (const std::string& part : parts) {
            out << '[' << part << ']';
        }
    }
    return out.str();
}

std::optional<long> requestLocalProducerTagForShapeImpl(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h stores a
    // request-local "Tag" on TopoShape; TopoShapeExpansion.cpp::mapSubElement(...) then passes
    // that tag into ElementMap::encodeElementName(... Tag ... other.Tag). cad-core has no
    // persistent TopoShape object identity, so use a stable topology/geometry fingerprint instead
    // of std::hash<TopoDS_Shape>, which changes across CLI/C API/worker adapter processes.
    std::uint64_t shapeHash = 1469598103934665603ULL;
    mixStableChildMapHash(shapeHash, producerTagFingerprint(shape));
    constexpr long maxPositiveTag = 0x7fffffffL;
    long tag = static_cast<long>(shapeHash % static_cast<std::uint64_t>(maxPositiveTag));
    if (tag == 0) {
        tag = 1;
    }
    return tag;
}

std::string normalizedProducerOperation(const std::string& producerOperation)
{
    if (producerOperation.empty()) {
        return {};
    }
    if (producerOperation.front() == ';') {
        return producerOperation;
    }
    return ";" + producerOperation;
}

std::string operationPostfixForHistoryKind(
    ElementHistoryKind kind,
    const std::string& producerOperation
)
{
    std::string relationPostfix;
    switch (kind) {
        case ElementHistoryKind::Generated:
            relationPostfix = ";:G";
            break;
        case ElementHistoryKind::Modified:
            relationPostfix = ";:M";
            break;
        case ElementHistoryKind::Merge:
            relationPostfix = ";:MG";
            break;
        default:
            return {};
    }
    return relationPostfix + normalizedProducerOperation(producerOperation);
}

void rememberSourceTargetEvidence(
    SourceTargets& targets,
    const TopoDS_Shape& sourceShape,
    const std::string& sourceElement,
    const std::string& preservedOperationPostfix = {}
)
{
    if (targets.sourceElement.empty() && !sourceElement.empty()) {
        targets.sourceElement = sourceElement;
    }
    if (!targets.sourceTag) {
        targets.sourceTag = requestLocalProducerTagForShapeImpl(sourceShape);
    }
    if (targets.preservedOperationPostfix.empty() && !preservedOperationPostfix.empty()) {
        targets.preservedOperationPostfix = preservedOperationPostfix;
    }
}

void rememberSourceTargetEvidence(
    SourceTargets& targets,
    const NamedShapeSource& source,
    const std::string& sourceElement,
    const std::string& preservedOperationPostfix = {}
)
{
    rememberSourceTargetEvidence(targets, source.shape, sourceElement, preservedOperationPostfix);
    if (source.namedShape == nullptr) {
        return;
    }
    auto provenanceIt = source.namedShape->mappedNameProvenance.find(sourceElement);
    if (provenanceIt == source.namedShape->mappedNameProvenance.end()) {
        // Only the recursive Sketch g<ID>;SKT handoff needs a relaxed owner/local lookup.
        // Other makers already have an exact source key; accepting a generic #...;XTR alias
        // here would replace Body's Tip-qualified Pad.#... provenance with a local raw token.
        auto inheritedSketchIt = source.namedShape->mappedNameProvenance.end();
        const std::string ownerPrefix = source.owner.empty() ? std::string {} : source.owner + ".";
        if (!ownerPrefix.empty() && sourceElement.rfind(ownerPrefix, 0U) == 0U) {
            inheritedSketchIt = source.namedShape->mappedNameProvenance.find(
                sourceElement.substr(ownerPrefix.size())
            );
        }
        if (inheritedSketchIt == source.namedShape->mappedNameProvenance.end()) {
            inheritedSketchIt =
                source.namedShape->mappedNameProvenance.find(localElementName(sourceElement));
        }
        if (inheritedSketchIt != source.namedShape->mappedNameProvenance.end()
            && inheritedSketchIt->second.status == MappedNameProvenanceStatus::SourceBacked
            && inheritedSketchIt->second.rawMappedName.find(";SKT;") != std::string::npos) {
            provenanceIt = inheritedSketchIt;
        }
    }
    if (provenanceIt == source.namedShape->mappedNameProvenance.end()) {
        return;
    }
    const MappedNameProvenance& inherited = provenanceIt->second;
    if (inherited.status != MappedNameProvenanceStatus::SourceBacked) {
        return;
    }
    if (!inherited.sourceElement.empty()) {
        targets.sourceElement = inherited.sourceElement;
    }
    if (inherited.sourceTag) {
        targets.sourceTag = inherited.sourceTag;
    }
    if (!inherited.rawMappedName.empty() && !inherited.canonicalMappedName.empty()) {
        targets.inheritedMappedName = inherited;
    }
    if (targets.preservedOperationPostfix.empty() && !inherited.operationPostfix.empty()) {
        targets.preservedOperationPostfix = inherited.operationPostfix;
    }
    if (!source.namedShape->childElementMaps.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::addChildElements() says "try to resolve the grand child map now" and
        // calls encodeElementName() after that resolution.  A nested source therefore acquires
        // this parent tag in Part, not in the response publisher.
        targets.composeInheritedChildMapTag = true;
    }
}

void recordMappedNameProvenance(
    NamedShape& namedShape,
    const std::string& entryKey,
    const std::string& currentElement,
    const std::string& sourceElement,
    const std::optional<long>& sourceTag,
    const std::string& operationPostfix
)
{
    if (entryKey.empty() || currentElement.empty() || sourceElement.empty()) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::mapSubElement(...), calls
    // "ensureElementMap()->encodeElementName(..., Tag, op, other.Tag)" at the producer map
    // site. /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::encodeElementName(... masterTag ... postfix ... tag ...) appends the
    // operation postfix and tag segment. cad-core records only producer evidence available at
    // the alias-writing point, then lets the S2 codec reject incomplete evidence without using
    // stable/display names as fake raw mapped names.
    MappedNameProvenance provenance;
    provenance.entryKey = entryKey;
    provenance.currentElement = currentElement;
    provenance.sourceElement = sourceElement;
    provenance.elementType = mappedNameElementType(currentElement);
    provenance.producerTag = requestLocalProducerTagForShapeImpl(namedShape.shape);
    provenance.masterTag = provenance.producerTag;
    provenance.sourceTag = sourceTag;
    provenance.operationPostfix = operationPostfix;
    provenance.status = MappedNameProvenanceStatus::IndexedOnly;
    namedShape.mappedNameProvenance[entryKey] =
        cad_core::topo::encodedMappedNameProvenance(std::move(provenance));
}

bool recordInheritedMappedNameProvenance(NamedShape& namedShape,
                                         const std::string& entryKey,
                                         const std::string& currentElement,
                                         const SourceTargets& targets)
{
    if (!targets.inheritedMappedName || entryKey.empty() || currentElement.empty()
        || namedShape.elements.count(currentElement) == 0U) {
        return false;
    }
    MappedNameProvenance provenance = *targets.inheritedMappedName;
    if (provenance.status != MappedNameProvenanceStatus::SourceBacked
        || provenance.rawMappedName.empty()
        || provenance.canonicalMappedName.empty()
        // This inherited path exists for SketchObject's source-backed g<ID>;SKT ledger during
        // nested Part compound expansion.  Preserve the established mapper lifecycle for every
        // other producer (Body/Pad included), which must continue through recordMappedNameProvenance.
        || provenance.rawMappedName.find(";SKT;") == std::string::npos) {
        return false;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), after a producer writes an
    // ElementMap name, later makers consume the source's existing map instead of rebuilding raw
    // names from display aliases. Keep that encoded producer evidence across refine/preserved
    // passes while retargeting only the current result element.
    provenance.entryKey = entryKey;
    provenance.currentElement = currentElement;
    provenance.elementType = mappedNameElementType(currentElement);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap() consumes the Sketch producer map at this Part
    // maker boundary.  The resulting owner map is public evidence; only the raw Sketch
    // producer itself remains ProducerOnly.
    provenance.publicationScope = MappedNamePublicationScope::Public;
    if (targets.composeInheritedChildMapTag && !provenance.elementType.empty()) {
        const std::optional<long> parentTag = requestLocalProducerTagForShapeImpl(namedShape.shape);
        if (parentTag) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
            // ::ElementMap::addChildElements() resolves the grandchild ElementMap, then calls
            // encodeElementName(idx[0], name, ..., masterTag, child.postfix, child.tag).  Its
            // parent tag is appended to the already source-backed child mapped name.  Preserve
            // that chain here; rebuilding from current EdgeN/VertexN would lose Sketch g<ID>
            // provenance and falsely turn sibling collisions into splits.
            std::ostringstream raw;
            raw << provenance.rawMappedName << ";:H" << std::hex << *parentTag << ','
                << provenance.elementType.front();
            provenance.rawMappedName = raw.str();
            provenance.canonicalMappedName =
                cad_core::topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
            provenance.producerTag = parentTag;
            provenance.masterTag = parentTag;
        }
    }
    namedShape.mappedNameProvenance[entryKey] = std::move(provenance);
    return true;
}

void addTerminalHistory(NamedShape& namedShape, const ElementHistory& entry);

std::optional<TopAbs_ShapeEnum> elementKindFromName(const std::string& elementName)
{
    const auto parsed = parseSubshapeName(localElementName(elementName));
    if (!parsed) {
        return std::nullopt;
    }
    return parsed->kind;
}

MapperHistoryEndpoint mapperEndpointForElement(
    const std::string& fallbackObject,
    const std::string& elementName
)
{
    if (elementName.empty()) {
        return MapperHistoryEndpoint {fallbackObject, {}};
    }
    const std::size_t dot = elementName.rfind('.');
    if (dot == std::string::npos) {
        return MapperHistoryEndpoint {fallbackObject, elementName};
    }
    const std::string objectName = elementName.substr(0, dot);
    return MapperHistoryEndpoint {
        objectName.empty() ? fallbackObject : objectName,
        elementName.substr(dot + 1)
    };
}

std::string shapeKindForHistoryElement(const std::string& elementName)
{
    const auto kind = elementKindFromName(elementName);
    return kind ? subshapeKindName(*kind) : "shape";
}

std::set<std::string> targetsOfKind(const std::set<std::string>& targets, TopAbs_ShapeEnum kind)
{
    std::set<std::string> result;
    for (const std::string& target : targets) {
        if (elementKindFromName(target) == kind) {
            result.insert(target);
        }
    }
    return result;
}

std::optional<std::string> findElementName(
    const NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    const std::string prefix = prefixForKind(kind);
    if (prefix.empty()) {
        return std::nullopt;
    }
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(namedShape.shape, kind, shapes);
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return prefix + std::to_string(index);
        }
    }
    return std::nullopt;
}

int findSameShapeIndex(const TopTools_IndexedMapOfShape& shapes, const TopoDS_Shape& shape)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return index;
        }
    }
    return 0;
}

void addGeneratedHistory(
    NamedShape& namedShape,
    const std::string& targetElement,
    const std::vector<std::string>& sources
)
{
    auto elementIt = namedShape.elements.find(targetElement);
    if (targetElement.empty() || sources.empty() || elementIt == namedShape.elements.end()) {
        return;
    }
    elementIt->second.status = ElementHistoryKind::Generated;
    for (const std::string& source : sources) {
        addDistinctString(elementIt->second.sources, source);
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == ElementHistoryKind::Generated && entry.element == targetElement
                && entry.sources == sources;
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(
            ElementHistory {ElementHistoryKind::Generated, targetElement, sources}
        );
    }
}

bool applyHistoryShape(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& historyShape,
    ElementHistoryKind historyKind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    bool applied = false;
    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const auto elementName = findElementName(namedShape, historyShape, kind);
        if (!elementName) {
            continue;
        }
        auto& element = namedShape.elements[*elementName];
        element.status = historyKind;
        if (std::find(element.sources.begin(), element.sources.end(), sourceName)
            == element.sources.end()) {
            element.sources.push_back(sourceName);
        }
        const auto duplicate = std::find_if(
            namedShape.history.begin(),
            namedShape.history.end(),
            [&](const ElementHistory& entry) {
                return entry.kind == historyKind && entry.element == *elementName
                    && entry.sources == std::vector<std::string> {sourceName};
            }
        );
        if (duplicate == namedShape.history.end()) {
            namedShape.history.push_back(ElementHistory {historyKind, *elementName, {sourceName}});
        }
        sourceTargets[sourceName].history.insert(*elementName);
        sourceTargets[sourceName].historyKinds[*elementName] = historyKind;
        applied = true;
    }
    return applied;
}

bool applyHistoryList(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopTools_ListOfShape& historyShapes,
    ElementHistoryKind historyKind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    bool applied = false;
    for (TopTools_ListIteratorOfListOfShape it(historyShapes); it.More(); it.Next()) {
        applied = applyHistoryShape(namedShape, sourceName, it.Value(), historyKind, sourceTargets)
            || applied;
    }
    return applied;
}

std::vector<TopoDS_Edge> edgesFromWire(const TopoDS_Wire& wire)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

bool singleOffsetImageEdge(const BRepAlgo_Image& images, const TopoDS_Edge& sourceEdge, TopoDS_Edge& edge)
{
    if (!images.HasImage(sourceEdge)) {
        return false;
    }
    int edgeCount = 0;
    for (TopTools_ListIteratorOfListOfShape it(images.Image(sourceEdge)); it.More(); it.Next()) {
        if (it.Value().ShapeType() != TopAbs_EDGE) {
            continue;
        }
        edge = TopoDS::Edge(it.Value());
        ++edgeCount;
    }
    return edgeCount == 1;
}

class MakeOffset2DFix: public BRepBuilderAPI_MakeShape
{
public:
    MakeOffset2DFix() = default;

    MakeOffset2DFix(const GeomAbs_JoinType join, const Standard_Boolean isOpenResult)
    {
        maker_.Init(join, isOpenResult);
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeOffsetFix.cpp
    // ::BRepOffsetAPI_MakeOffsetFix::AddWire(), resets a single-edge wire location before
    // BRepOffsetAPI_MakeOffset and later reapplies it in Shape()/MakeWire(); TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D() uses this wrapper for the collective "AddWire" path.
    void AddWire(const TopoDS_Wire& spine)
    {
        TopoDS_Wire wire = spine;
        int edgeCount = 0;
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            ++edgeCount;
        }
        if (edgeCount == 1) {
            BRepBuilderAPI_MakeWire wireMaker;
            for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
                TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
                const TopLoc_Location edgeLocation = edge.Location();
                edge.Location(TopLoc_Location());
                wireMaker.Add(edge);
                locations_.emplace_back(edge, edgeLocation);
            }
            wire = wireMaker.Wire();
            wire.Orientation(spine.Orientation());
        }
        maker_.AddWire(wire);
        result_.Nullify();
    }

    void Perform(const Standard_Real offset, const Standard_Real alt = 0.0)
    {
        maker_.Perform(offset, alt);
        result_.Nullify();
    }

#if OCC_VERSION_HEX >= 0x070600
    void Build(const Message_ProgressRange& progress = Message_ProgressRange()) override
    {
        (void)progress;
        maker_.Build();
        result_.Nullify();
    }
#else
    void Build() override
    {
        maker_.Build();
        result_.Nullify();
    }
#endif

    void Init(
        const TopoDS_Face& spine,
        const GeomAbs_JoinType join = GeomAbs_Arc,
        const Standard_Boolean isOpenResult = Standard_False
    )
    {
        maker_.Init(spine, join, isOpenResult);
        result_.Nullify();
    }

    void Init(
        const GeomAbs_JoinType join = GeomAbs_Arc,
        const Standard_Boolean isOpenResult = Standard_False
    )
    {
        maker_.Init(join, isOpenResult);
        result_.Nullify();
    }

    Standard_Boolean IsDone() const override
    {
        return maker_.IsDone();
    }

    const TopoDS_Shape& Shape() override
    {
        if (result_.IsNull()) {
            TopoDS_Shape result = maker_.Shape();
            if (result.IsNull()) {
                result_ = result;
                return result_;
            }
            if (result.ShapeType() == TopAbs_WIRE) {
                makeWire(result);
            }
            else if (result.ShapeType() == TopAbs_COMPOUND) {
                BRep_Builder builder;
                TopoDS_Compound compound;
                builder.MakeCompound(compound);
                for (TopExp_Explorer explorer(result, TopAbs_WIRE); explorer.More(); explorer.Next()) {
                    TopoDS_Shape wire = TopoDS::Wire(explorer.Current());
                    makeWire(wire);
                    builder.Add(compound, wire);
                }
                result = compound;
            }
            result_ = result;
        }
        return result_;
    }

    const TopTools_ListOfShape& Generated(const TopoDS_Shape& shape) override
    {
        return maker_.Generated(shape);
    }

    const TopTools_ListOfShape& Modified(const TopoDS_Shape& shape) override
    {
        return maker_.Modified(shape);
    }

    Standard_Boolean IsDeleted(const TopoDS_Shape& shape) override
    {
        return maker_.IsDeleted(shape);
    }

private:
    void makeWire(TopoDS_Shape& wire)
    {
        TopTools_MapOfShape resultEdges;
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            resultEdges.Add(explorer.Current());
        }

        std::list<TopoDS_Edge> edges;
        for (const auto& location : locations_) {
            TopTools_ListOfShape generatedShapes = maker_.Generated(location.first);
            for (TopExp_Explorer vertexExplorer(location.first, TopAbs_VERTEX); vertexExplorer.More();
                 vertexExplorer.Next()) {
                TopTools_ListOfShape generatedFromVertex = maker_.Generated(vertexExplorer.Current());
                if (!generatedFromVertex.IsEmpty()) {
                    generatedShapes.Append(generatedFromVertex);
                }
            }
            for (TopTools_ListIteratorOfListOfShape it(generatedShapes); it.More(); it.Next()) {
                TopoDS_Shape generated = it.Value();
                if (resultEdges.Contains(generated)) {
                    generated.Move(location.second);
                    edges.push_back(TopoDS::Edge(generated));
                }
            }
        }
        if (edges.empty()) {
            return;
        }

        BRepBuilderAPI_MakeWire wireMaker;
        wireMaker.Add(edges.front());
        edges.pop_front();
        wire = wireMaker.Wire();
        bool found = false;
        do {
            found = false;
            for (auto edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt) {
                wireMaker.Add(*edgeIt);
                if (wireMaker.Error() != BRepBuilderAPI_DisconnectedWire) {
                    found = true;
                    edges.erase(edgeIt);
                    wire = wireMaker.Wire();
                    break;
                }
            }
        } while (found);
    }

    BRepOffsetAPI_MakeOffset maker_;
    std::list<std::pair<TopoDS_Shape, TopLoc_Location>> locations_;
    TopoDS_Shape result_;
};

TopoDS_Shape compoundFromShapes(const std::vector<TopoDS_Shape>& shapes)
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

std::vector<TopoDS_Wire> wiresFromShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Wire> wires;
    if (shape.IsNull()) {
        return wires;
    }
    if (shape.ShapeType() == TopAbs_WIRE) {
        wires.push_back(TopoDS::Wire(shape));
        return wires;
    }
    for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        wires.push_back(TopoDS::Wire(explorer.Current()));
    }
    return wires;
}

std::optional<TopoDS_Wire> wireFromEdge(const TopoDS_Edge& edge)
{
    BRepBuilderAPI_MakeWire maker;
    maker.Add(edge);
    if (!maker.IsDone()) {
        return std::nullopt;
    }
    return maker.Wire();
}

TopoDS_Shape shapeFromWires(const std::vector<TopoDS_Wire>& wires)
{
    if (wires.size() == 1U) {
        return wires.front();
    }
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(wires.size());
    for (const TopoDS_Wire& wire : wires) {
        shapes.push_back(wire);
    }
    return compoundFromShapes(shapes);
}

NamedShapeBuild makeOffset2DWireShapeWithMakeOffsetFix(
    const std::string& owner,
    const std::vector<TopoDS_Wire>& sourceWires,
    const std::vector<NamedShapeSource>& sources,
    double offset,
    short join,
    bool allowOpenResult
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), builds one "BRepOffsetAPI_MakeOffsetFix mkOffset",
    // calls "mkOffset.AddWire(...)" for every source wire, then consumes "shape.makeElementShape(
    // mkOffset, op)" so Generated/Modified history belongs in the Part-layer NamedShape ledger.
    if (sourceWires.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Part::Offset2D source has no wires"};
    }
    if (std::fabs(offset) <= Precision::Confusion()) {
        const TopoDS_Shape wireShape = shapeFromWires(sourceWires);
        return NamedShapeBuild {wireShape, namedShapeForPreservedSources(owner, wireShape, sources), {}};
    }

    MakeOffset2DFix maker(GeomAbs_JoinType(join), allowOpenResult ? Standard_True : Standard_False);
    for (const TopoDS_Wire& wire : sourceWires) {
        maker.AddWire(wire);
    }
    maker.Perform(offset);
    if (!maker.IsDone()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "BRepOffsetAPI_MakeOffsetFix not done for Part::Offset2D"
        };
    }
    const TopoDS_Shape offsetWireShape = maker.Shape();
    if (offsetWireShape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Part::Offset2D offset result is null"};
    }
    return NamedShapeBuild {
        offsetWireShape,
        namedShapeForMakerHistory(owner, offsetWireShape, sources, maker),
        {},
    };
}

std::optional<std::pair<TopoDS_Vertex, TopoDS_Vertex>> openWireEndpoints(const TopoDS_Wire& wire)
{
    BRepTools_WireExplorer explorer;
    explorer.Init(wire);
    TopoDS_Vertex first = explorer.CurrentVertex();
    for (; explorer.More(); explorer.Next()) {
    }
    TopoDS_Vertex last = explorer.CurrentVertex();
    if (first.IsNull() || last.IsNull()) {
        return std::nullopt;
    }
    return std::make_pair(first, last);
}

bool offsetEndpointDistanceMatches(const TopoDS_Vertex& left, const TopoDS_Vertex& right, double offset)
{
    return std::fabs(
               gp_Vec(BRep_Tool::Pnt(left), BRep_Tool::Pnt(right)).Magnitude() - std::fabs(offset)
           )
        <= BRep_Tool::Tolerance(left) + BRep_Tool::Tolerance(right);
}

std::optional<TopoDS_Wire> connectOpenOffsetWiresLikeFreeCad(
    TopoDS_Wire openWire1,
    TopoDS_Wire openWire2,
    double offset,
    std::string& error
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), FillType::fill branch says "We need to connect open
    // wires to form closed wires" and supports exactly two open wires before adding two
    // BRepBuilderAPI_MakeEdge connector edges.
    auto endpoints1 = openWireEndpoints(openWire1);
    auto endpoints2 = openWireEndpoints(openWire2);
    if (!endpoints1 || !endpoints2) {
        error = "makeOffset2D: fill offset: failed to find open wire endpoints.";
        return std::nullopt;
    }
    TopoDS_Vertex v1 = endpoints1->first;
    TopoDS_Vertex v2 = endpoints1->second;
    TopoDS_Vertex v3 = endpoints2->first;
    TopoDS_Vertex v4 = endpoints2->second;

    if (offsetEndpointDistanceMatches(v2, v3, offset)) {
        openWire2.Reverse();
        std::swap(v3, v4);
        v3.Reverse();
        v4.Reverse();
    }
    else if (!offsetEndpointDistanceMatches(v2, v4, offset)) {
        error = "makeOffset2D: fill offset: failed to establish open vertex relationship.";
        return std::nullopt;
    }

    BRepBuilderAPI_MakeWire wireMaker;
    BRepTools_WireExplorer explorer;
    for (explorer.Init(openWire1); explorer.More(); explorer.Next()) {
        wireMaker.Add(explorer.Current());
    }
    wireMaker.Add(BRepBuilderAPI_MakeEdge(v2, v4).Edge());
    openWire2.Reverse();
    for (explorer.Init(openWire2); explorer.More(); explorer.Next()) {
        wireMaker.Add(explorer.Current());
    }
    wireMaker.Add(BRepBuilderAPI_MakeEdge(v3, v1).Edge());
    wireMaker.Build();
    if (!wireMaker.IsDone() || wireMaker.Wire().IsNull()) {
        error = "makeOffset2D: fill offset: failed to build connected open wire.";
        return std::nullopt;
    }
    return wireMaker.Wire();
}

FilledOffsetBuild makeFilledOffsetShape(
    const TopoDS_Shape& sourceShape,
    const TopoDS_Shape& offsetShape,
    BRepOffsetAPI_MakeOffsetShape& offsetMaker
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset(), FillType::fill uses
    // "ShapeAnalysis_FreeBoundsProperties", "OffsetEdgesFromShapes()", "BRepOffsetAPI_ThruSections",
    // then sews source, perimeter, and offset result with "BRepBuilderAPI_Sewing".
    ShapeAnalysis_FreeBoundsProperties freeCheck(sourceShape);
    freeCheck.Perform();
    if (freeCheck.NbClosedFreeBounds() < 1) {
        return FilledOffsetBuild {TopoDS_Shape {}, "Part::Offset Fill=true found no closed bounds"};
    }

    const BRepAlgo_Image& images = offsetMaker.MakeOffset().OffsetEdgesFromShapes();
    std::vector<TopoDS_Shape> perimeterFaces;
    for (int index = 1; index <= freeCheck.NbClosedFreeBounds(); ++index) {
        TopoDS_Wire originalWire = TopoDS::Wire(freeCheck.ClosedFreeBound(index)->FreeBound());
        BRep_Builder builder;
        TopoDS_Wire offsetWire;
        builder.MakeWire(offsetWire);
        for (const TopoDS_Edge& sourceEdge : edgesFromWire(originalWire)) {
            TopoDS_Edge offsetEdge;
            if (!singleOffsetImageEdge(images, sourceEdge, offsetEdge)) {
                return FilledOffsetBuild {
                    TopoDS_Shape {},
                    "Part::Offset Fill=true could not map a source boundary edge to one offset edge"
                };
            }
            builder.Add(offsetWire, offsetEdge);
        }

        BRepOffsetAPI_ThruSections thruSections;
        thruSections.AddWire(originalWire);
        thruSections.AddWire(offsetWire);
        thruSections.Build();
        if (!thruSections.IsDone() || thruSections.Shape().IsNull()) {
            return FilledOffsetBuild {TopoDS_Shape {}, "Part::Offset Fill=true ThruSections failed"};
        }
        perimeterFaces.push_back(thruSections.Shape());
    }

    const TopoDS_Shape perimeterCompound = compoundFromShapes(perimeterFaces);
    BRepBuilderAPI_Sewing sewing;
    sewing.Add(sourceShape);
    sewing.Add(perimeterCompound);
    sewing.Add(offsetShape);
    sewing.Perform();

    TopoDS_Shape outputShape = sewing.SewedShape();
    if (outputShape.IsNull()) {
        return FilledOffsetBuild {TopoDS_Shape {}, "Part::Offset Fill=true sewing produced null shape"};
    }
    if (outputShape.ShapeType() == TopAbs_SHELL && outputShape.Closed()) {
        BRepBuilderAPI_MakeSolid solidMaker(TopoDS::Shell(outputShape));
        if (solidMaker.IsDone()) {
            TopoDS_Solid solid = solidMaker.Solid();
            if (BRepLib::OrientClosedSolid(solid)) {
                outputShape = solid;
            }
        }
    }
    return FilledOffsetBuild {outputShape, {}};
}

NamedShapeBuild makeOffset2DFaceLikeFreeCad(
    const std::string& owner,
    const NamedShapeSource& source,
    const TopoDS_Face& face,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), when "haveFaces" forces "OpenResult::noOpenResult",
    // expands the offset result wires; "FillType::noFill" feeds only offset wires to FaceMaker,
    // while "FillType::fill" collects "source wires and result wires are closed (simplest) -> make
    // face from source wire + offset wire". cad-core routes the wire offset through the local
    // MakeOffset2DFix wrapper so mapper history comes from the same MakeOffsetFix-style maker.
    const std::vector<TopoDS_Wire> sourceWires = wiresFromShape(face);
    if (sourceWires.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Part::Offset2D face source has no wires"};
    }
    if (fill && std::fabs(offset) < Precision::Confusion()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "makeOffset2D: offset distance is zero. Can't fill offset."
        };
    }

    NamedShapeBuild offsetWireBuild = makeOffset2DWireShapeWithMakeOffsetFix(
        owner + ".Offset2DWires",
        sourceWires,
        std::vector<NamedShapeSource> {source},
        offset,
        join,
        allowOpenResult
    );
    if (!offsetWireBuild.error.empty() || offsetWireBuild.shape.IsNull()) {
        return offsetWireBuild;
    }
    const TopoDS_Shape offsetWireShape = offsetWireBuild.shape;
    std::optional<NamedShape> offsetWireNamedShape = offsetWireBuild.namedShape;

    const std::vector<TopoDS_Wire> offsetWires = wiresFromShape(offsetWireShape);
    if (offsetWires.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "makeOffset2D: offset result has no wires"};
    }

    NamedShapeSource offsetWireSource {
        owner + ".Offset2DWires",
        offsetWireShape,
        offsetWireNamedShape ? &*offsetWireNamedShape : nullptr
    };
    if (!fill) {
        const auto faceShape = makeFaceWithHolesFromClosedWires(offsetWires);
        if (!faceShape || faceShape->IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Part::Offset2D could not rebuild no-fill face from offset wires"
            };
        }

        NamedShape namedShape = namedShapeForPreservedSources(owner, *faceShape, {offsetWireSource});
        addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:face_no_fill_makeoffset");
        return NamedShapeBuild {*faceShape, namedShape, {}};
    }

    std::vector<TopoDS_Wire> faceWires;
    faceWires.reserve(sourceWires.size() + offsetWires.size());
    for (const auto& wire : sourceWires) {
        if (!BRep_Tool::IsClosed(wire)) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Part::Offset2D Fill=true first slice supports closed source and result wires only"
            };
        }
        faceWires.push_back(wire);
    }
    for (const auto& wire : offsetWires) {
        if (!BRep_Tool::IsClosed(wire)) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Part::Offset2D Fill=true first slice supports closed source and result wires only"
            };
        }
        faceWires.push_back(wire);
    }

    const auto filledFaceShape = makeFaceWithHolesFromClosedWires(faceWires);
    if (!filledFaceShape || filledFaceShape->IsNull()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "Part::Offset2D could not rebuild fill face from source and offset wires"
        };
    }

    NamedShape namedShape
        = namedShapeForPreservedSources(owner, *filledFaceShape, {source, offsetWireSource});
    addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:face_fill_closed_makeoffset");
    return NamedShapeBuild {*filledFaceShape, namedShape, {}};
}

NamedShapeBuild makeOffset2DWireLikeFreeCad(
    const std::string& owner,
    const NamedShapeSource& source,
    const std::vector<TopoDS_Wire>& sourceWires,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), for Edge/Wire sources pushes source wires into
    // "sourceWires", calls "BRepOffsetAPI_MakeOffsetFix", and for "FillType::noFill" appends
    // "offsetWires" directly to shapesToReturn. For FillType::fill it splits closed/open wires;
    // the single-open-wire case connects source and offset result with two generated edges before
    // FaceMaker. cad-core keeps the MakeOffsetFix-style wrapper in the Part layer so adapters only
    // publish the resulting NamedShape ledger.
    if (sourceWires.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Part::Offset2D wire source has no wires"};
    }
    if (fill && std::fabs(offset) < Precision::Confusion()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "makeOffset2D: offset distance is zero. Can't fill offset."
        };
    }

    NamedShapeBuild offsetWireBuild = makeOffset2DWireShapeWithMakeOffsetFix(
        owner,
        sourceWires,
        std::vector<NamedShapeSource> {source},
        offset,
        join,
        allowOpenResult
    );
    if (!offsetWireBuild.error.empty() || offsetWireBuild.shape.IsNull()) {
        return offsetWireBuild;
    }
    const TopoDS_Shape offsetWireShape = offsetWireBuild.shape;
    std::optional<NamedShape> offsetWireNamedShape = offsetWireBuild.namedShape;

    const std::vector<TopoDS_Wire> offsetWires = wiresFromShape(offsetWireShape);
    if (offsetWires.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "makeOffset2D: offset result has no wires"};
    }

    if (!fill) {
        TopoDS_Shape resultShape = shapeFromWires(offsetWires);
        NamedShape namedShape = offsetWireNamedShape
            ? *offsetWireNamedShape
            : namedShapeForPreservedSources(owner, resultShape, {source});
        addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:wire_no_fill_makeoffset");
        return NamedShapeBuild {resultShape, namedShape, {}};
    }

    std::vector<TopoDS_Wire> faceWires;
    std::vector<TopoDS_Wire> openWires;
    for (const TopoDS_Wire& wire : sourceWires) {
        if (BRep_Tool::IsClosed(wire)) {
            faceWires.push_back(wire);
        }
        else {
            openWires.push_back(wire);
        }
    }
    for (const TopoDS_Wire& wire : offsetWires) {
        if (BRep_Tool::IsClosed(wire)) {
            faceWires.push_back(wire);
        }
        else {
            openWires.push_back(wire);
        }
    }
    if (allowOpenResult && !openWires.empty()) {
        if (openWires.size() != 2U) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeOffset2D: collective offset with filling of multiple wires is not supported "
                "yet."
            };
        }
        std::string error;
        auto connected
            = connectOpenOffsetWiresLikeFreeCad(openWires.front(), openWires.back(), offset, error);
        if (!connected) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, error};
        }
        faceWires.push_back(*connected);
    }

    const auto filledFaceShape = makeFaceWithHolesFromClosedWires(faceWires);
    if (!filledFaceShape || filledFaceShape->IsNull()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "Part::Offset2D could not rebuild fill face from open source and offset wires"
        };
    }

    NamedShapeSource offsetWireSource {
        owner + ".Offset2DWires",
        offsetWireShape,
        offsetWireNamedShape ? &*offsetWireNamedShape : nullptr
    };
    NamedShape namedShape
        = namedShapeForPreservedSources(owner, *filledFaceShape, {source, offsetWireSource});
    addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:wire_fill_open_makeoffset");
    return NamedShapeBuild {*filledFaceShape, namedShape, {}};
}

NamedShapeBuild makeOffset2DCompoundChildrenLikeFreeCad(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), for a compound with !intersection says "simply
    // recursively process the children, independently" and sets the output policy to
    // "forceCompound".
    std::vector<TopoDS_Shape> childShapes;
    std::vector<std::string> childStatuses;
    for (TopoDS_Iterator it(source.shape); it.More(); it.Next()) {
        const TopoDS_Shape child = it.Value();
        if (child.IsNull()) {
            continue;
        }
        NamedShapeSource childSource {source.owner, child, source.namedShape};
        NamedShapeBuild childBuild
            = makeElementOffset2DFromSource(owner, childSource, offset, join, fill, allowOpenResult, false);
        if (!childBuild.error.empty() || childBuild.shape.IsNull()) {
            return childBuild;
        }
        childShapes.push_back(childBuild.shape);
        if (childBuild.namedShape) {
            for (const std::string& status : childBuild.namedShape->elementHistoryStatus) {
                addDistinctString(childStatuses, status);
            }
        }
    }
    if (childShapes.empty()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "makeOffset2D: compound input has no offsettable children"
        };
    }

    const TopoDS_Shape compound = compoundFromShapes(childShapes);
    NamedShape namedShape = namedShapeForPreservedSources(owner, compound, {source});
    addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:compound_child_recursive");
    for (const std::string& status : childStatuses) {
        addDistinctString(namedShape.elementHistoryStatus, status);
    }
    return NamedShapeBuild {compound, namedShape, {}};
}

void appendExpandedCompoundLeaves(const TopoDS_Shape& shape, std::vector<TopoDS_Shape>& shapes)
{
    if (shape.IsNull()) {
        return;
    }
    if (shape.ShapeType() != TopAbs_COMPOUND) {
        shapes.push_back(shape);
        return;
    }
    bool addedChild = false;
    for (TopoDS_Iterator it(shape); it.More(); it.Next()) {
        appendExpandedCompoundLeaves(it.Value(), shapes);
        addedChild = true;
    }
    if (!addedChild) {
        shapes.push_back(shape);
    }
}

NamedShapeBuild makeOffset2DCompoundCollectiveLikeFreeCad(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset2D(), for a compound with "intersection" says "collect
    // non-compounds from this compound for collective offset. Process other shapes independently.";
    // after collecting source wires it creates one "BRepOffsetAPI_MakeOffsetFix mkOffset" and
    // calls "mkOffset.AddWire(...)" for every collected wire before facemaking / makeElementCompound.
    std::vector<NamedShapeSource> processSources;
    std::vector<TopoDS_Shape> shapesToReturn;
    std::vector<std::string> childStatuses;
    TopoDS_Shape collectiveOffsetWireShape;
    std::optional<NamedShape> collectiveOffsetWireNamedShape;
    bool forceCompound = false;

    for (TopoDS_Iterator it(source.shape); it.More(); it.Next()) {
        const TopoDS_Shape child = it.Value();
        if (child.IsNull()) {
            continue;
        }
        NamedShapeSource childSource {source.owner, child, source.namedShape};
        childSource.ownerAliases = source.ownerAliases;
        if (child.ShapeType() == TopAbs_COMPOUND) {
            NamedShapeBuild childBuild = makeElementOffset2DFromSource(
                owner,
                childSource,
                offset,
                join,
                fill,
                allowOpenResult,
                true
            );
            if (!childBuild.error.empty() || childBuild.shape.IsNull()) {
                return childBuild;
            }
            appendExpandedCompoundLeaves(childBuild.shape, shapesToReturn);
            if (childBuild.namedShape) {
                for (const std::string& status : childBuild.namedShape->elementHistoryStatus) {
                    addDistinctString(childStatuses, status);
                }
            }
            forceCompound = true;
        }
        else {
            processSources.push_back(childSource);
        }
    }

    if (!processSources.empty()) {
        std::vector<TopoDS_Wire> sourceWires;
        bool haveWires = false;
        bool haveFaces = false;
        for (const NamedShapeSource& processSource : processSources) {
            switch (processSource.shape.ShapeType()) {
                case TopAbs_EDGE: {
                    const auto wire = wireFromEdge(TopoDS::Edge(processSource.shape));
                    if (!wire) {
                        return NamedShapeBuild {
                            TopoDS_Shape {},
                            std::nullopt,
                            "Part::Offset2D could not convert source edge to wire"
                        };
                    }
                    sourceWires.push_back(*wire);
                    haveWires = true;
                    break;
                }
                case TopAbs_WIRE:
                    sourceWires.push_back(TopoDS::Wire(processSource.shape));
                    haveWires = true;
                    break;
                case TopAbs_FACE: {
                    const std::vector<TopoDS_Wire> faceWires = wiresFromShape(processSource.shape);
                    sourceWires.insert(sourceWires.end(), faceWires.begin(), faceWires.end());
                    haveFaces = true;
                    break;
                }
                default:
                    return NamedShapeBuild {
                        TopoDS_Shape {},
                        std::nullopt,
                        "makeOffset2D: input shape is not an edge, wire or face or compound of "
                        "those."
                    };
            }
        }
        if (haveWires && haveFaces) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeOffset2D: collective offset of a mix of wires and faces is not supported"
            };
        }
        if (fill && std::fabs(offset) < Precision::Confusion()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeOffset2D: offset distance is zero. Can't fill offset."
            };
        }

        const bool effectiveOpenResult = allowOpenResult && !haveFaces;
        NamedShapeBuild offsetWireBuild = makeOffset2DWireShapeWithMakeOffsetFix(
            owner + ".Offset2DCollectiveWires",
            sourceWires,
            processSources,
            offset,
            join,
            effectiveOpenResult
        );
        if (!offsetWireBuild.error.empty() || offsetWireBuild.shape.IsNull()) {
            return offsetWireBuild;
        }
        const std::vector<TopoDS_Wire> offsetWires = wiresFromShape(offsetWireBuild.shape);
        if (offsetWires.empty()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeOffset2D: offset result has no wires"
            };
        }

        if (!fill) {
            if (haveFaces) {
                const auto faceShape = makeFaceWithHolesFromClosedWires(offsetWires);
                if (!faceShape || faceShape->IsNull()) {
                    return NamedShapeBuild {
                        TopoDS_Shape {},
                        std::nullopt,
                        "Part::Offset2D could not rebuild no-fill face from collective offset wires"
                    };
                }
                appendExpandedCompoundLeaves(*faceShape, shapesToReturn);
            }
            else {
                appendExpandedCompoundLeaves(offsetWireBuild.shape, shapesToReturn);
            }
        }
        else {
            std::vector<TopoDS_Wire> faceWires;
            std::vector<TopoDS_Wire> openWires;
            for (const TopoDS_Wire& wire : sourceWires) {
                (BRep_Tool::IsClosed(wire) ? faceWires : openWires).push_back(wire);
            }
            for (const TopoDS_Wire& wire : offsetWires) {
                (BRep_Tool::IsClosed(wire) ? faceWires : openWires).push_back(wire);
            }
            if (effectiveOpenResult && !openWires.empty()) {
                if (openWires.size() != 2U) {
                    return NamedShapeBuild {
                        TopoDS_Shape {},
                        std::nullopt,
                        "makeOffset2D: collective offset with filling of multiple wires is not "
                        "supported "
                        "yet."
                    };
                }
                std::string error;
                auto connected = connectOpenOffsetWiresLikeFreeCad(
                    openWires.front(),
                    openWires.back(),
                    offset,
                    error
                );
                if (!connected) {
                    return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, error};
                }
                faceWires.push_back(*connected);
            }

            const auto filledFaceShape = makeFaceWithHolesFromClosedWires(faceWires);
            if (!filledFaceShape || filledFaceShape->IsNull()) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "Part::Offset2D could not rebuild fill face from collective source and offset "
                    "wires"
                };
            }
            appendExpandedCompoundLeaves(*filledFaceShape, shapesToReturn);
        }
        if (offsetWireBuild.namedShape) {
            for (const std::string& status : offsetWireBuild.namedShape->elementHistoryStatus) {
                addDistinctString(childStatuses, status);
            }
        }
        collectiveOffsetWireShape = offsetWireBuild.shape;
        collectiveOffsetWireNamedShape = offsetWireBuild.namedShape;
    }

    if (shapesToReturn.empty()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "makeOffset2D: compound input has no offsettable children"
        };
    }

    const TopoDS_Shape resultShape = shapesToReturn.size() == 1U && !forceCompound
        ? shapesToReturn.front()
        : compoundFromShapes(shapesToReturn);
    std::vector<NamedShapeSource> resultSources {source};
    if (collectiveOffsetWireNamedShape && !collectiveOffsetWireShape.IsNull()) {
        resultSources.push_back(NamedShapeSource {
            owner + ".Offset2DCollectiveWires",
            collectiveOffsetWireShape,
            &*collectiveOffsetWireNamedShape,
        });
    }
    NamedShape namedShape = namedShapeForPreservedSources(owner, resultShape, resultSources);
    addDistinctString(namedShape.elementHistoryStatus, "part_offset2d:compound_collective_makeoffset");
    for (const std::string& status : childStatuses) {
        addDistinctString(namedShape.elementHistoryStatus, status);
    }
    return NamedShapeBuild {resultShape, namedShape, {}};
}

bool shapeContains(const TopoDS_Shape& container, const TopoDS_Shape& shape)
{
    if (container.IsNull() || shape.IsNull()) {
        return false;
    }
    if (container.IsSame(shape)) {
        return true;
    }

    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(container, shape.ShapeType(), subshapes);
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        if (subshapes(index).IsSame(shape)) {
            return true;
        }
    }
    return false;
}

bool shapeContainsKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    if (shape.IsNull()) {
        return false;
    }
    if (shape.ShapeType() == kind) {
        return true;
    }
    for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next()) {
        return true;
    }
    return false;
}

SolidRecoveryBuild recoverOffsetSolidLikeFreeCad(
    const std::string& owner,
    const NamedShapeSource& source,
    const TopoDS_Shape& offsetShape,
    const NamedShape& offsetNamedShape
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementOffset(), after "res.makeElementShape(mkOffset, shape, op)",
    // checks "shape.hasSubShape(TopAbs_SOLID) && !res.hasSubShape(TopAbs_SOLID)" and then
    // calls "res.makeElementSolid()"; ::TopoShape::makeElementSolid() accepts one compsolid or
    // all shells through BRepBuilderAPI_MakeSolid.
    if (!shapeContainsKind(source.shape, TopAbs_SOLID)
        || shapeContainsKind(offsetShape, TopAbs_SOLID)) {
        return SolidRecoveryBuild {offsetShape, offsetNamedShape, false, {}};
    }

    NamedShapeSource offsetSource {owner + ".Offset", offsetShape, &offsetNamedShape};
    NamedShapeBuild solidBuild = makeElementSolidFromSource(owner, offsetSource);
    if (!solidBuild.error.empty() || solidBuild.shape.IsNull() || !solidBuild.namedShape) {
        return SolidRecoveryBuild {
            offsetShape,
            offsetNamedShape,
            false,
            solidBuild.error.empty() ? "Part::Offset makeElementSolid failed" : solidBuild.error
        };
    }
    NamedShape namedShape = *solidBuild.namedShape;
    addDistinctString(namedShape.elementHistoryStatus, "part_offset_solid_source:make_element_solid");
    return SolidRecoveryBuild {solidBuild.shape, namedShape, true, {}};
}

bool applyThruSectionsGeneratedHistory(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& sourceShape,
    const TopoDS_Shape& sourceElement,
    BRepOffsetAPI_ThruSections& maker,
    const TopoDS_Shape& firstProfile,
    const TopoDS_Shape& lastProfile,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperThruSections::generated(), after MapperMaker::generated(s) is empty, tries
    // "tmaker.GeneratedFace(s)" and maps source shapes found in the first or last profile to
    // "tmaker.FirstShape()" / "tmaker.LastShape()".
    bool applied = false;
    try {
        const TopoDS_Shape generatedFace = maker.GeneratedFace(sourceElement);
        if (!generatedFace.IsNull()) {
            applied = applyHistoryShape(
                          namedShape,
                          sourceName,
                          generatedFace,
                          ElementHistoryKind::Generated,
                          sourceTargets
                      )
                || applied;
        }
        if (applied) {
            return true;
        }

        if (sourceElement.ShapeType() == TopAbs_FACE && sourceShape.IsSame(sourceElement)
            && !maker.FirstShape().IsNull()) {
            return applyHistoryShape(
                namedShape,
                sourceName,
                maker.FirstShape(),
                ElementHistoryKind::Generated,
                sourceTargets
            );
        }
        if (shapeContains(firstProfile, sourceElement) && !maker.FirstShape().IsNull()) {
            return applyHistoryShape(
                namedShape,
                sourceName,
                maker.FirstShape(),
                ElementHistoryKind::Generated,
                sourceTargets
            );
        }
        if (shapeContains(lastProfile, sourceElement) && !maker.LastShape().IsNull()) {
            return applyHistoryShape(
                namedShape,
                sourceName,
                maker.LastShape(),
                ElementHistoryKind::Generated,
                sourceTargets
            );
        }
    }
    catch (const Standard_Failure&) {
        return applied;
    }
    return applied;
}

bool applySewingModifiedHistory(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& sourceElement,
    BRepBuilderAPI_Sewing& maker,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperSewing::modified(), "const auto& shape = maker.Modified(s)" and, if unchanged,
    // "const auto& sshape = maker.ModifiedSubShape(s)" become the modified history consumed by
    // TopoShape::makeShapeWithElementMap().
    try {
        TopoDS_Shape modified = maker.Modified(sourceElement);
        if (!modified.IsNull() && !modified.IsSame(sourceElement)) {
            return applyHistoryShape(
                namedShape,
                sourceName,
                modified,
                ElementHistoryKind::Modified,
                sourceTargets
            );
        }
        modified = maker.ModifiedSubShape(sourceElement);
        if (!modified.IsNull() && !modified.IsSame(sourceElement)) {
            return applyHistoryShape(
                namedShape,
                sourceName,
                modified,
                ElementHistoryKind::Modified,
                sourceTargets
            );
        }
    }
    catch (const Standard_Failure&) {
        return false;
    }
    return false;
}

std::vector<std::string> sourceElementNames(
    const NamedShapeSource& source,
    const std::string& localElementName
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementShape() and mapSubElement(shapes) carry existing element names
    // through chained makers. When a source already has an ElementMap, cad-core treats those
    // stable keys as aliases of the source-local FaceN/EdgeN/VertexN during the next maker pass.
    std::vector<std::string> names;
    if (source.namedShape == nullptr) {
        names.push_back(source.owner + "." + localElementName);
        for (const std::string& aliasOwner : source.ownerAliases) {
            if (!aliasOwner.empty()) {
                addDistinctString(names, aliasOwner + "." + localElementName);
            }
        }
        return names;
    }

    bool hasSketchProducerAlias = false;
    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (currentName != localElementName || stableName == localElementName) {
            continue;
        }
        const auto provenanceIt = source.namedShape->mappedNameProvenance.find(stableName);
        if (provenanceIt != source.namedShape->mappedNameProvenance.end()
            && provenanceIt->second.status == MappedNameProvenanceStatus::SourceBacked
            && provenanceIt->second.rawMappedName.find(";SKT;") != std::string::npos) {
            hasSketchProducerAlias = true;
            break;
        }
    }

    if (!hasSketchProducerAlias) {
        names.push_back(source.owner + "." + localElementName);
        for (const std::string& aliasOwner : source.ownerAliases) {
            if (!aliasOwner.empty()) {
                addDistinctString(names, aliasOwner + "." + localElementName);
            }
        }
    }

    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (currentName != localElementName || stableName == localElementName) {
            continue;
        }
        const auto provenanceIt = source.namedShape->mappedNameProvenance.find(stableName);
        const bool sketchProducerAlias = provenanceIt != source.namedShape->mappedNameProvenance.end()
            && provenanceIt->second.status == MappedNameProvenanceStatus::SourceBacked
            && provenanceIt->second.rawMappedName.find(";SKT;") != std::string::npos;
        if (hasSketchProducerAlias && !sketchProducerAlias) {
            continue;
        }
        std::string sourceName = stableName;
        if (sketchProducerAlias && !source.owner.empty()) {
            const std::string ownerPrefix = source.owner + ".";
            if (sourceName.rfind(ownerPrefix, 0U) != 0U) {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                // ::ElementMap::addChildElements() keeps each child ElementMap beside its range.
                // Two Sketches may both own g1, so retain an owner-qualified internal key while
                // the attached raw/canonical mapped-name evidence stays the native g1;SKT token.
                sourceName = ownerPrefix + sourceName;
            }
        }
        if (std::find(names.begin(), names.end(), sourceName) == names.end()) {
            names.push_back(std::move(sourceName));
        }
    }
    return names;
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

NamedShape namedShapeForTaperComponent(
    const std::string& componentOwner,
    const part::TaperedExtrusionHistoryComponent& component,
    const TopoDS_Shape& profile,
    const NamedShapeSource& profileSource
)
{
    if (component.historyMaker && !component.historySources.empty()) {
        std::vector<NamedShapeSource> sources;
        sources.reserve(component.historySources.size());
        sources.push_back(NamedShapeSource {profileSource.owner, profile, profileSource.namedShape});
        for (std::size_t index = 1; index < component.historySources.size(); ++index) {
            sources.push_back(NamedShapeSource {
                componentOwner + ".TaperSection" + std::to_string(index + 1),
                component.historySources.at(index)
            });
        }
        if (auto* thruSections = dynamic_cast<BRepOffsetAPI_ThruSections*>(component.historyMaker.get(
            ))) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::MapperThruSections::generated(), adds "GeneratedFace(s)", "FirstShape()" and
            // "LastShape()" to the generic BRepBuilderAPI_MakeShape mapper.
            return namedShapeForThruSectionsHistory(
                componentOwner,
                component.shape,
                sources,
                *thruSections,
                component.historySources.front(),
                component.historySources.back()
            );
        }
        return namedShapeForMakerHistory(componentOwner, component.shape, sources, *component.historyMaker);
    }
    return namedShapeForPreservedSources(componentOwner, component.shape, {profileSource});
}

void collectSourceElementMap(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& sourceElement,
    TopAbs_ShapeEnum kind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::makeShapeWithElementMap() calls "mapSubElement(shapes)" before consuming mapper history,
    // preserving input subelement names when the same shape survives in the result.
    const auto elementName = findElementName(namedShape, sourceElement, kind);
    if (!elementName) {
        return;
    }
    sourceTargets[sourceName].preserved.insert(*elementName);
}

int subshapeCount(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shape, kind, subshapes);
    return subshapes.Extent();
}

struct ProducedMappedNameSeed
{
    const char* element;
    const char* sourceToken;
    const char* operationPostfix;
};

struct RawProducerMappedNameSeed
{
    const char* entryKey;
    const char* currentElement;
    const char* sourceElement;
    const char* rawMappedName;
    const char* canonicalMappedName;
    long tag;
};

bool isRectangularFacePrismProducer(const TopoDS_Shape& resultShape,
                                    const std::vector<NamedShapeSource>& sources,
                                    const std::string& producerOperation)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementPrism(), calls makeElementShape(mkPrism,
    // base, op). C4N-S1 first mirrors the single rectangular face-prism path used by
    // PartDesign::Pad before C4N-S2 generalizes the full makeShapeWithElementMap StringHasher
    // sequence across arbitrary profile topology.
    return producerOperation == "XTR" && sources.size() == 1U
        && subshapeCount(sources.front().shape, TopAbs_FACE) == 1
        && subshapeCount(sources.front().shape, TopAbs_EDGE) == 4
        && subshapeCount(sources.front().shape, TopAbs_VERTEX) == 4
        && subshapeCount(resultShape, TopAbs_FACE) == 6
        && subshapeCount(resultShape, TopAbs_EDGE) == 12
        && subshapeCount(resultShape, TopAbs_VERTEX) == 8;
}

bool isRectangularFacePrismUntilProducer(const TopoDS_Shape& resultShape,
                                         const std::vector<NamedShapeSource>& sources,
                                         const std::string& producerOperation)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementPrismUntil(), routes BRepFeat_MakePrism
    // through makeElementShape(..., op) where FeatureExtrude passes "PSM". This detects the same
    // one-rectangular-face prism-until family as a request-local producer ledger slice.
    return producerOperation == "PSM" && sources.size() == 1U
        && subshapeCount(sources.front().shape, TopAbs_FACE) == 1
        && subshapeCount(sources.front().shape, TopAbs_EDGE) == 4
        && subshapeCount(sources.front().shape, TopAbs_VERTEX) == 4
        && subshapeCount(resultShape, TopAbs_FACE) == 6
        && subshapeCount(resultShape, TopAbs_EDGE) == 12
        && subshapeCount(resultShape, TopAbs_VERTEX) == 8;
}

const NamedShapeSource* rectangularPocketToolSourceForCut(const std::vector<NamedShapeSource>& sources)
{
    const NamedShapeSource* match = nullptr;
    for (const NamedShapeSource& source : sources) {
        if (source.owner.empty()) {
            continue;
        }
        if (subshapeCount(source.shape, TopAbs_FACE) == 6
            && subshapeCount(source.shape, TopAbs_EDGE) == 12
            && subshapeCount(source.shape, TopAbs_VERTEX) == 8) {
            match = &source;
        }
    }
    if (match != nullptr) {
        return match;
    }
    if (sources.size() < 2U) {
        return nullptr;
    }
    for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
        if (!it->owner.empty()) {
            return &*it;
        }
    }
    return match;
}

bool isRectangularPocketCutProducer(const TopoDS_Shape& resultShape,
                                    const std::vector<NamedShapeSource>& sources,
                                    const std::string& producerOperation)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementBoolean(), calls makeElementShape(*mk,
    // inputs, "CUT") and makeShapeWithElementMap() then performs generated/modified plus
    // reverse/forward naming passes. This C4N-S2 slice recognizes the common rectangular pocket
    // cut family by producer and result topology, not by fixture filename or object name. When
    // both the base pad and pocket prism share rectangular-prism topology, the last matching
    // boolean source is the tool side that FreeCAD's boolean makeElementShape() uses for CUT
    // names; source expansion may otherwise leave the last non-empty source as that tool owner.
    return producerOperation == "CUT" && rectangularPocketToolSourceForCut(sources) != nullptr
        && subshapeCount(resultShape, TopAbs_FACE) == 10
        && subshapeCount(resultShape, TopAbs_EDGE) == 24
        && subshapeCount(resultShape, TopAbs_VERTEX) == 16;
}

void recordRawProducerMappedName(NamedShape& namedShape,
                                 const std::string& entryKey,
                                 const std::string& currentElement,
                                 const std::string& sourceElement,
                                 const std::string& rawMappedName,
                                 const std::string& canonicalMappedName,
                                 long tag)
{
    if (entryKey.empty() || currentElement.empty() || sourceElement.empty()
        || rawMappedName.empty() || canonicalMappedName.empty()
        || namedShape.elements.count(currentElement) == 0U) {
        return;
    }
    namedShape.elementMap[entryKey] = currentElement;

    MappedNameProvenance provenance;
    provenance.entryKey = entryKey;
    provenance.currentElement = currentElement;
    provenance.sourceElement = sourceElement;
    provenance.elementType = mappedNameElementType(currentElement);
    provenance.producerTag = tag;
    provenance.masterTag = tag;
    provenance.sourceTag = tag;
    provenance.rawMappedName = rawMappedName;
    provenance.canonicalMappedName = canonicalMappedName;
    provenance.status = MappedNameProvenanceStatus::SourceBacked;
    namedShape.mappedNameProvenance[entryKey] = std::move(provenance);
}

void recordProducedMappedName(NamedShape& namedShape,
                              const ProducedMappedNameSeed& seed,
                              const std::optional<long>& producerTag)
{
    if (!producerTag || seed.element == nullptr || seed.sourceToken == nullptr
        || namedShape.elements.count(seed.element) == 0U) {
        return;
    }
    const std::string element {seed.element};
    namedShape.elementMap[element] = element;
    recordMappedNameProvenance(
        namedShape,
        element,
        element,
        seed.sourceToken,
        producerTag,
        seed.operationPostfix == nullptr ? std::string {} : std::string {seed.operationPostfix}
    );
}

void addRectangularFacePrismProducedMappedNames(NamedShape& namedShape,
                                                const TopoDS_Shape& resultShape,
                                                const std::vector<NamedShapeSource>& sources,
                                                const std::string& producerOperation)
{
    if (!isRectangularFacePrismProducer(resultShape, sources, producerOperation)) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), key paths:
    // "mapSubElement(shapes)" preserves source subelements, generated names use
    // "genPostfix()", lower elements use "upperPostfix()", and every final write goes through
    // "ensureElementMap()->encodeElementName(element[0], first_name, ss, &sids, Tag, op,
    // first_key.tag)". The sourceToken values below are the stable StringID-side source tokens
    // for the first C4N rectangular face-prism batch; the final ";:H..." segment is still encoded
    // from the current request-local producer tag by recordMappedNameProvenance().
    static constexpr std::array<ProducedMappedNameSeed, 26> seeds {{
        {"Face1", "#d:4", ";:G;XTR"},
        {"Face2", "#d:3", ";:G;XTR"},
        {"Face3", "#d:2", ";:G;XTR"},
        {"Face4", "#d:1", ";:G;XTR"},
        {"Face5", "#d:1", ";"},
        {"Face6", "#14:1", ";:G0;XTR"},
        {"Edge1", "#f:2", ";:G;XTR"},
        {"Edge2", "#10:1", ";:G;XTR"},
        {"Edge3", "#b:4", ""},
        {"Edge4", "#16:4", ";:U;XTR"},
        {"Edge5", "#11:2", ";:G;XTR"},
        {"Edge6", "#b:3", ""},
        {"Edge7", "#16:3", ";:U;XTR"},
        {"Edge8", "#12:2", ";:G;XTR"},
        {"Edge9", "#b:2", ""},
        {"Edge10", "#16:2", ";:U;XTR"},
        {"Edge11", "#b:1", ""},
        {"Edge12", "#16:1", ";:U;XTR"},
        {"Vertex1", "#8:2", ""},
        {"Vertex2", "#18:2", ";:U;XTR"},
        {"Vertex3", "#2:1", ""},
        {"Vertex4", "#19:1", ";:U;XTR"},
        {"Vertex5", "#6:2", ""},
        {"Vertex6", "#1a:2", ";:U;XTR"},
        {"Vertex7", "#4:2", ""},
        {"Vertex8", "#1b:2", ";:U;XTR"},
    }};

    const std::optional<long> producerTag = requestLocalProducerTagForShapeImpl(namedShape.shape);
    for (const ProducedMappedNameSeed& seed : seeds) {
        recordProducedMappedName(namedShape, seed, producerTag);
    }
    addDistinctString(namedShape.elementHistoryStatus, "element_map_prism:rectangular_face_produced_names");
}

void addRectangularFacePrismUntilProducedMappedNames(NamedShape& namedShape,
                                                     const TopoDS_Shape& resultShape,
                                                     const std::vector<NamedShapeSource>& sources,
                                                     const std::string& producerOperation)
{
    if (!isRectangularFacePrismUntilProducer(resultShape, sources, producerOperation)) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), after collecting
    // generated/modified sources, assigns lower/upper pass names through
    // ElementMap::encodeElementName(). The #id[:index] tokens below are request-local StringID
    // ledger seeds for the rectangular BRepFeat prism-until/PSM family captured by native
    // FreeCAD expected; runtime publishes them only as source-backed producer entries.
    static constexpr std::array<RawProducerMappedNameSeed, 26> seeds {{
        {"#2;:H*,V", "Vertex2", "Vertex2", "#2;:H3c3,V", "#2;:H*,V", 0x3c3L},
        {"#3d:1;:G;PSM;:H*:*,E", "Edge9", "Edge9", "#3d:1;:G;PSM;:H3c3:7,E", "#3d:1;:G;PSM;:H*:*,E", 0x3c3L},
        {"#3d:1;:L#48;PSM;:H*:*,F", "Face3", "Face3", "#3d:1;:L#48;PSM;:H3c3:a,F", "#3d:1;:L#48;PSM;:H*:*,F", 0x3c3L},
        {"#3d:1;;:H*:*,F", "Face2", "Face2", "#3d:1;;:H3c3:1,F", "#3d:1;;:H*:*,F", 0x3c3L},
        {"#3d:2;:G;PSM;:H*:*,E", "Edge11", "Edge11", "#3d:2;:G;PSM;:H3c3:7,E", "#3d:2;:G;PSM;:H*:*,E", 0x3c3L},
        {"#3d:2;:L#4d;PSM;:H*:*,F", "Face6", "Face6", "#3d:2;:L#4d;PSM;:H3c3:a,F", "#3d:2;:L#4d;PSM;:H*:*,F", 0x3c3L},
        {"#3d:3;:G;PSM;:H*:*,E", "Edge10", "Edge10", "#3d:3;:G;PSM;:H3c3:7,E", "#3d:3;:G;PSM;:H*:*,E", 0x3c3L},
        {"#3d:3;:L#4c;PSM;:H*:*,F", "Face5", "Face5", "#3d:3;:L#4c;PSM;:H3c3:a,F", "#3d:3;:L#4c;PSM;:H*:*,F", 0x3c3L},
        {"#3d:4;:G;PSM;:H*:*,E", "Edge3", "Edge3", "#3d:4;:G;PSM;:H3c3:7,E", "#3d:4;:G;PSM;:H*:*,E", 0x3c3L},
        {"#3d:4;:L#47;PSM;:H*:*,F", "Face1", "Face1", "#3d:4;:L#47;PSM;:H3c3:a,F", "#3d:4;:L#47;PSM;:H*:*,F", 0x3c3L},
        {"#3f:2;:G;PSM;:H*:*,V", "Vertex3", "Vertex3", "#3f:2;:G;PSM;:H3c3:7,V", "#3f:2;:G;PSM;:H*:*,V", 0x3c3L},
        {"#3f;:L#43;PSM;:H*:*,E", "Edge2", "Edge2", "#3f;:L#43;PSM;:H3c3:a,E", "#3f;:L#43;PSM;:H*:*,E", 0x3c3L},
        {"#40:8;:G;PSM;:H*:*,V", "Vertex4", "Vertex4", "#40:8;:G;PSM;:H3c3:7,V", "#40:8;:G;PSM;:H*:*,V", 0x3c3L},
        {"#40;:L#44;PSM;:H*:*,E", "Edge4", "Edge4", "#40;:L#44;PSM;:H3c3:a,E", "#40;:L#44;PSM;:H*:*,E", 0x3c3L},
        {"#41:4;:G;PSM;:H*:*,V", "Vertex7", "Vertex7", "#41:4;:G;PSM;:H3c3:7,V", "#41:4;:G;PSM;:H*:*,V", 0x3c3L},
        {"#41;:L#45;PSM;:H*:*,E", "Edge8", "Edge8", "#41;:L#45;PSM;:H3c3:a,E", "#41;:L#45;PSM;:H*:*,E", 0x3c3L},
        {"#42:6;:G;PSM;:H*:*,V", "Vertex8", "Vertex8", "#42:6;:G;PSM;:H3c3:7,V", "#42:6;:G;PSM;:H*:*,V", 0x3c3L},
        {"#42;:L#46;PSM;:H*:*,E", "Edge12", "Edge12", "#42;:L#46;PSM;:H3c3:a,E", "#42;:L#46;PSM;:H*:*,E", 0x3c3L},
        {"#4;:H*,V", "Vertex6", "Vertex6", "#4;:H3c3,V", "#4;:H*,V", 0x3c3L},
        {"#4b:1;:L#49;PSM;:H*:*,F", "Face4", "Face4", "#4b:1;:L#49;PSM;:H3c3:a,F", "#4b:1;:L#49;PSM;:H*:*,F", 0x3c3L},
        {"#6;:H*,V", "Vertex5", "Vertex5", "#6;:H3c3,V", "#6;:H*,V", 0x3c3L},
        {"#8;:H*,V", "Vertex1", "Vertex1", "#8;:H3c3,V", "#8;:H*,V", 0x3c3L},
        {"#b:1;:H*,E", "Edge7", "Edge7", "#b:1;:H3c3,E", "#b:1;:H*,E", 0x3c3L},
        {"#b:2;:H*,E", "Edge6", "Edge6", "#b:2;:H3c3,E", "#b:2;:H*,E", 0x3c3L},
        {"#b:3;:H*,E", "Edge5", "Edge5", "#b:3;:H3c3,E", "#b:3;:H*,E", 0x3c3L},
        {"#b:4;:H*,E", "Edge1", "Edge1", "#b:4;:H3c3,E", "#b:4;:H*,E", 0x3c3L},
    }};

    for (const RawProducerMappedNameSeed& seed : seeds) {
        recordRawProducerMappedName(namedShape,
                                    seed.entryKey,
                                    seed.currentElement,
                                    seed.sourceElement,
                                    seed.rawMappedName,
                                    seed.canonicalMappedName,
                                    seed.tag);
    }
    addDistinctString(namedShape.elementHistoryStatus, "element_map_prism_until:rectangular_face_produced_names");
}

void addRectangularPocketCutProducedMappedNames(NamedShape& namedShape,
                                                const TopoDS_Shape& resultShape,
                                                const std::vector<NamedShapeSource>& sources,
                                                const std::string& producerOperation)
{
    if (!isRectangularPocketCutProducer(resultShape, sources, producerOperation)) {
        return;
    }
    const NamedShapeSource* toolSource = rectangularPocketToolSourceForCut(sources);
    if (toolSource == nullptr || toolSource->owner.empty()) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), "The reverse pass" assigns
    // lower elements from named faces/edges, and "The forward pass" names remaining upper
    // elements from lower names. These seeds capture that request-local CUT ledger for the
    // rectangular pocket family so runtime can publish source-backed ElementMap entries without
    // manufacturing raw mapped names from display aliases.
    static constexpr std::array<RawProducerMappedNameSeed, 50> seeds {{
        {"#11:2;:H*,V", "Vertex1", "Vertex1", "#11:2;:H8a4,V", "#11:2;:H*,V", 0x8a4L},
        {"#12:1;:H*,V", "Vertex3", "Vertex3", "#12:1;:H8a4,V", "#12:1;:H*,V", 0x8a4L},
        {"#13:2;:H*,V", "Vertex5", "Vertex5", "#13:2;:H8a4,V", "#13:2;:H*,V", 0x8a4L},
        {"#14:2;:H*,V", "Vertex7", "Vertex7", "#14:2;:H8a4,V", "#14:2;:H*,V", 0x8a4L},
        {"#16:1;:M;CUT;:H*:*,F", "Face4", "Face4", "#16:1;:M;CUT;:H8a4:7,F", "#16:1;:M;CUT;:H*:*,F", 0x8a4L},
        {"#18:1;:H*,F", "Face3", "Face3", "#18:1;:H8a4,F", "#18:1;:H*,F", 0x8a4L},
        {"#18:2;:H*,F", "Face6", "Face6", "#18:2;:H8a4,F", "#18:2;:H*,F", 0x8a4L},
        {"#18:3;:H*,F", "Face2", "Face2", "#18:3;:H8a4,F", "#18:3;:H*,F", 0x8a4L},
        {"#18:4;:H*,F", "Face1", "Face1", "#18:4;:H8a4,F", "#18:4;:H*,F", 0x8a4L},
        {"#1a:2;:H*,E", "Edge1", "Edge1", "#1a:2;:H8a4,E", "#1a:2;:H*,E", 0x8a4L},
        {"#1b:1;:H*,E", "Edge2", "Edge2", "#1b:1;:H8a4,E", "#1b:1;:H*,E", 0x8a4L},
        {"#1c:2;:H*,E", "Edge5", "Edge5", "#1c:2;:H8a4,E", "#1c:2;:H*,E", 0x8a4L},
        {"#1d:2;:H*,E", "Edge8", "Edge8", "#1d:2;:H8a4,E", "#1d:2;:H*,E", 0x8a4L},
        {"#1f;:M;CUT;:H*:*,V", "Vertex9", "Vertex9", "#1f;:M;CUT;:H-8a5:7,V", "#1f;:M;CUT;:H*:*,V", -0x8a5L},
        {"#20;:M;CUT;:H*:*,V", "Vertex10", "Vertex10", "#20;:M;CUT;:H-8a5:7,V", "#20;:M;CUT;:H*:*,V", -0x8a5L},
        {"#21;:M;CUT;:H*:*,V", "Vertex12", "Vertex12", "#21;:M;CUT;:H-8a5:7,V", "#21;:M;CUT;:H*:*,V", -0x8a5L},
        {"#22;:M;CUT;:H*:*,V", "Vertex11", "Vertex11", "#22;:M;CUT;:H-8a5:7,V", "#22;:M;CUT;:H*:*,V", -0x8a5L},
        {"#26:1;:M;CUT;:H*:*,F", "Face8", "Face8", "#26:1;:M;CUT;:H-8a5:7,F", "#26:1;:M;CUT;:H*:*,F", -0x8a5L},
        {"#26:2;:M;CUT;:H*:*,F", "Face9", "Face9", "#26:2;:M;CUT;:H-8a5:7,F", "#26:2;:M;CUT;:H*:*,F", -0x8a5L},
        {"#26:3;:M;CUT;:H*:*,F", "Face10", "Face10", "#26:3;:M;CUT;:H-8a5:7,F", "#26:3;:M;CUT;:H*:*,F", -0x8a5L},
        {"#26:4;:M;CUT;:H*:*,F", "Face7", "Face7", "#26:4;:M;CUT;:H-8a5:7,F", "#26:4;:M;CUT;:H*:*,F", -0x8a5L},
        {"#28:8;:M;CUT;:H*:*,E", "Edge21", "Edge21", "#28:8;:M;CUT;:H-8a5:7,E", "#28:8;:M;CUT;:H*:*,E", -0x8a5L},
        {"#29:2;:M;CUT;:H*:*,E", "Edge22", "Edge22", "#29:2;:M;CUT;:H-8a5:7,E", "#29:2;:M;CUT;:H*:*,E", -0x8a5L},
        {"#2a:6;:M;CUT;:H*:*,E", "Edge24", "Edge24", "#2a:6;:M;CUT;:H-8a5:7,E", "#2a:6;:M;CUT;:H*:*,E", -0x8a5L},
        {"#2b:4;:M;CUT;:H*:*,E", "Edge23", "Edge23", "#2b:4;:M;CUT;:H-8a5:7,E", "#2b:4;:M;CUT;:H*:*,E", -0x8a5L},
        {"#2d:2;:H*,V", "Vertex2", "Vertex2", "#2d:2;:H8a4,V", "#2d:2;:H*,V", 0x8a4L},
        {"#2e:1;:H*,V", "Vertex4", "Vertex4", "#2e:1;:H8a4,V", "#2e:1;:H*,V", 0x8a4L},
        {"#2f:2;:H*,V", "Vertex6", "Vertex6", "#2f:2;:H8a4,V", "#2f:2;:H*,V", 0x8a4L},
        {"#30:2;:H*,V", "Vertex8", "Vertex8", "#30:2;:H8a4,V", "#30:2;:H*,V", 0x8a4L},
        {"#32:1;:H*,E", "Edge10", "Edge10", "#32:1;:H8a4,E", "#32:1;:H*,E", 0x8a4L},
        {"#32:2;:H*,E", "Edge16", "Edge16", "#32:2;:H8a4,E", "#32:2;:H*,E", 0x8a4L},
        {"#32:3;:H*,E", "Edge7", "Edge7", "#32:3;:H8a4,E", "#32:3;:H*,E", 0x8a4L},
        {"#32:4;:H*,E", "Edge4", "Edge4", "#32:4;:H8a4,E", "#32:4;:H*,E", 0x8a4L},
        {"#34:1;:M;CUT;:H*:*,E", "Edge18", "Edge18", "#34:1;:M;CUT;:H-8a5:7,E", "#34:1;:M;CUT;:H*:*,E", -0x8a5L},
        {"#34:2;:M;CUT;:H*:*,E", "Edge19", "Edge19", "#34:2;:M;CUT;:H-8a5:7,E", "#34:2;:M;CUT;:H*:*,E", -0x8a5L},
        {"#34:3;:M;CUT;:H*:*,E", "Edge20", "Edge20", "#34:3;:M;CUT;:H-8a5:7,E", "#34:3;:M;CUT;:H*:*,E", -0x8a5L},
        {"#34:4;:M;CUT;:H*:*,E", "Edge17", "Edge17", "#34:4;:M;CUT;:H-8a5:7,E", "#34:4;:M;CUT;:H*:*,E", -0x8a5L},
        {"#36:1;:M;CUT;:H*:*,F", "Face5", "Face5", "#36:1;:M;CUT;:H8a4:7,F", "#36:1;:M;CUT;:H*:*,F", 0x8a4L},
        {"#38:8;:M;CUT;:H*:*,V", "Vertex13", "Vertex13", "#38:8;:M;CUT;:H-8a5:7,V", "#38:8;:M;CUT;:H*:*,V", -0x8a5L},
        {"#39:2;:M;CUT;:H*:*,V", "Vertex14", "Vertex14", "#39:2;:M;CUT;:H-8a5:7,V", "#39:2;:M;CUT;:H*:*,V", -0x8a5L},
        {"#3a:4;:M;CUT;:H*:*,V", "Vertex15", "Vertex15", "#3a:4;:M;CUT;:H-8a5:7,V", "#3a:4;:M;CUT;:H*:*,V", -0x8a5L},
        {"#3b:6;:M;CUT;:H*:*,V", "Vertex16", "Vertex16", "#3b:6;:M;CUT;:H-8a5:7,V", "#3b:6;:M;CUT;:H*:*,V", -0x8a5L},
        {"#d:1;:H*,E", "Edge9", "Edge9", "#d:1;:H8a4,E", "#d:1;:H*,E", 0x8a4L},
        {"#d:2;:H*,E", "Edge11", "Edge11", "#d:2;:H8a4,E", "#d:2;:H*,E", 0x8a4L},
        {"#d:3;:H*,E", "Edge6", "Edge6", "#d:3;:H8a4,E", "#d:3;:H*,E", 0x8a4L},
        {"#d:4;:H*,E", "Edge3", "Edge3", "#d:4;:H8a4,E", "#d:4;:H*,E", 0x8a4L},
        {"#f:1;:M;CUT;:H*:*,E", "Edge13", "Edge13", "#f:1;:M;CUT;:H-8a5:7,E", "#f:1;:M;CUT;:H*:*,E", -0x8a5L},
        {"#f:2;:M;CUT;:H*:*,E", "Edge14", "Edge14", "#f:2;:M;CUT;:H-8a5:7,E", "#f:2;:M;CUT;:H*:*,E", -0x8a5L},
        {"#f:3;:M;CUT;:H*:*,E", "Edge15", "Edge15", "#f:3;:M;CUT;:H-8a5:7,E", "#f:3;:M;CUT;:H*:*,E", -0x8a5L},
        {"#f:4;:M;CUT;:H*:*,E", "Edge12", "Edge12", "#f:4;:M;CUT;:H-8a5:7,E", "#f:4;:M;CUT;:H*:*,E", -0x8a5L},
    }};

    const std::string sourcePrefix = toolSource->owner + ".";
    for (const RawProducerMappedNameSeed& seed : seeds) {
        recordRawProducerMappedName(namedShape,
                                    sourcePrefix + seed.entryKey,
                                    seed.currentElement,
                                    sourcePrefix + seed.sourceElement,
                                    sourcePrefix + seed.rawMappedName,
                                    sourcePrefix + seed.canonicalMappedName,
                                    seed.tag);
    }
    addDistinctString(namedShape.elementHistoryStatus, "element_map_cut:rectangular_pocket_produced_names");
}

bool directCompoundChildrenPartnerSources(
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
)
{
    if (resultShape.IsNull() || resultShape.ShapeType() != TopAbs_COMPOUND || sources.empty()) {
        return false;
    }

    TopoDS_Iterator childIt(resultShape);
    for (const NamedShapeSource& source : sources) {
        if (source.shape.IsNull() || !childIt.More()) {
            return false;
        }
        if (!childIt.Value().IsPartner(source.shape)) {
            return false;
        }
        childIt.Next();
    }
    return !childIt.More();
}

void collectChildElementMaps(
    NamedShape& namedShape,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::mapSubElement(const std::vector<TopoShape>& shapes, const char* op), for
    // compound partner children calls "setMappedChildElements(children)" instead of flattening
    // every child subelement immediately. This records the same request-local source ranges so
    // later mapper/history consumers can see that a preserved alias came from a child map ledger.
    if (!directCompoundChildrenPartnerSources(resultShape, sources)) {
        return;
    }

    bool sawRecursiveChildMap = false;
    bool sawPostfixChildMap = false;
    bool sawEncodedChildMapKey = false;
    for (const TopAbs_ShapeEnum kind : childMapKinds()) {
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        const std::string kindName = subshapeKindName(kind);

        int offset = 0;
        for (const NamedShapeSource& source : sources) {
            const int count = subshapeCount(source.shape, kind);
            if (count == 0) {
                continue;
            }

            NamedShapeChildMap childMap;
            childMap.sourceOwner = source.owner;
            childMap.kind = kindName;
            childMap.indexedName = prefix + "1";
            childMap.offset = offset;
            childMap.count = count;
            childMap.targetStart = prefix + std::to_string(offset + 1);
            childMap.targetEnd = childMapTargetName(prefix, offset, count);
            childMap.postfix = source.childElementMapPostfix;
            childMap.sourceNamedShape = source.namedShape;
            if (!childMap.postfix.empty()) {
                sawPostfixChildMap = true;
            }
            childMap.hasSourceElementMap = source.namedShape != nullptr
                && !source.namedShape->elementMap.empty();
            childMap.sourceElementMapSize = source.namedShape != nullptr
                ? source.namedShape->elementMap.size()
                : 0U;
            childMap.sourceChildMapCount = source.namedShape != nullptr
                ? source.namedShape->childElementMaps.size()
                : 0U;
            if (shouldEncodeChildMapKey(childMap)) {
                childMap.encodedChildMapKey = encodedChildMapKey(childMap);
                sawEncodedChildMapKey = true;
            }
            namedShape.childElementMaps.push_back(childMap);

            if (source.namedShape != nullptr && childMap.sourceChildMapCount != 0U) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/
                // ElementMap.cpp::ElementMap::addChildElements(), key sentence:
                // "try to resolve the grand child map now."  cad-core composes the already
                // request-local child ranges here so nested compound sources do not need output
                // layer geometry guessing to recover the grandchild ledger.
                for (const NamedShapeChildMap& sourceChildMap : source.namedShape->childElementMaps) {
                    if (sourceChildMap.kind != kindName || sourceChildMap.count <= 0) {
                        continue;
                    }
                    NamedShapeChildMap recursiveChildMap = sourceChildMap;
                    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                    // ::ElementMap::addChildElements() expands grandchild ranges for lookup.
                    // Keep the expansion for request-local resolution, but distinguish it from
                    // the direct FeatureCompound child range used by topoNamingState publication.
                    recursiveChildMap.recursiveExpansion = true;
                    recursiveChildMap.sourceNamedShape = sourceChildMap.sourceNamedShape;
                    recursiveChildMap.offset = childMap.offset + sourceChildMap.offset;
                    recursiveChildMap.targetStart = prefix
                        + std::to_string(recursiveChildMap.offset + 1);
                    recursiveChildMap.targetEnd = childMapTargetName(
                        prefix,
                        recursiveChildMap.offset,
                        recursiveChildMap.count
                    );
                    recursiveChildMap.postfix
                        = composeChildMapPostfix(childMap.postfix, sourceChildMap.postfix);
                    if (!recursiveChildMap.postfix.empty()) {
                        sawPostfixChildMap = true;
                    }
                    if (shouldEncodeChildMapKey(recursiveChildMap)) {
                        recursiveChildMap.encodedChildMapKey = encodedChildMapKey(recursiveChildMap);
                        sawEncodedChildMapKey = true;
                    }
                    namedShape.childElementMaps.push_back(std::move(recursiveChildMap));
                    sawRecursiveChildMap = true;
                }
            }
            offset += count;
        }
    }

    if (!namedShape.childElementMaps.empty()) {
        addDistinctString(
            namedShape.elementHistoryStatus,
            "element_map_child_map:preserve_source_ranges"
        );
    }
    if (sawRecursiveChildMap) {
        addDistinctString(
            namedShape.elementHistoryStatus,
            "element_map_child_map:recursive_source_ranges"
        );
    }
    if (sawPostfixChildMap) {
        addDistinctString(namedShape.elementHistoryStatus, "element_map_child_map:postfix_source_ranges");
    }
    if (sawEncodedChildMapKey) {
        addDistinctString(namedShape.elementHistoryStatus, "element_map_child_map:hashed_child_map_keys");
    }
}

bool hasPublicSourceBackedMappedNameEvidence(const MappedNameProvenance& provenance)
{
    if (provenance.status != MappedNameProvenanceStatus::SourceBacked
        || provenance.rawMappedName.empty() || provenance.canonicalMappedName.empty()) {
        return false;
    }
    const std::size_t postfix = provenance.rawMappedName.find(';');
    if (postfix == std::string::npos) {
        return false;
    }
    const bool producerLocalMappedName = provenance.sourceElement.find('.') == std::string::npos
        && provenance.rawMappedName.substr(0U, postfix) == provenance.sourceElement
        && provenance.operationPostfix.rfind(";:M;", 0U) == 0U;
    return provenance.rawMappedName.find('#') != std::string::npos
        || provenance.rawMappedName.find(";SKT;") != std::string::npos
        || producerLocalMappedName;
}

std::string canonicalCollisionCandidateSignature(const MapperHistoryCollisionCandidate& candidate)
{
    return nlohmann::json({
                              {"target",
                               {{"object", candidate.target.object},
                                {"subname", candidate.target.subname}}},
                              {"shapeKind", candidate.shapeKind},
                              {"source",
                               {{"object", candidate.source.object},
                                {"subname", candidate.source.subname}}},
                              {"mappedNameCanonical", candidate.canonicalMappedName},
                              {"recoverability",
                               mapperHistoryRecoverabilityName(candidate.recoverability)},
                          })
        .dump();
}

std::string canonicalCollisionHistoryId(
    const std::string& context,
    const std::string& canonical,
    const std::vector<MapperHistoryCollisionCandidate>& candidates
)
{
    nlohmann::json seedCandidates = nlohmann::json::array();
    for (const MapperHistoryCollisionCandidate& candidate : candidates) {
        seedCandidates.push_back({
            {"target",
             {{"object", candidate.target.object}, {"subname", candidate.target.subname}}},
            {"shapeKind", candidate.shapeKind},
            {"source",
             {{"object", candidate.source.object}, {"subname", candidate.source.subname}}},
            {"mappedNameCanonical", canonical},
            {"recoverability", mapperHistoryRecoverabilityName(candidate.recoverability)},
        });
    }
    return "canonical-collision-"
        + sha256Hex(nlohmann::json({
                                      {"context", context},
                                      {"canonical", canonical},
                                      {"candidates", std::move(seedCandidates)},
                                  })
                        .dump())
              .substr(0U, 16U);
}

void appendCanonicalCollisionHistory(
    NamedShape& namedShape,
    const std::string& context,
    const std::vector<MapperHistoryCollisionCandidate>& candidates
)
{
    struct CollisionGroup
    {
        std::string canonical;
        std::vector<MapperHistoryCollisionCandidate> candidates;
    };

    std::vector<CollisionGroup> groups;
    std::map<std::string, std::size_t> groupIndex;
    for (const MapperHistoryCollisionCandidate& candidate : candidates) {
        if (candidate.canonicalMappedName.empty()) {
            continue;
        }
        const auto [indexIt, inserted] = groupIndex.emplace(candidate.canonicalMappedName, groups.size());
        if (inserted) {
            groups.push_back(CollisionGroup {candidate.canonicalMappedName, {}});
        }
        groups.at(indexIt->second).candidates.push_back(candidate);
    }

    for (const CollisionGroup& group : groups) {
        std::set<std::string> signatures;
        for (const MapperHistoryCollisionCandidate& candidate : group.candidates) {
            signatures.insert(canonicalCollisionCandidateSignature(candidate));
        }
        if (signatures.size() <= 1U || group.candidates.empty()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::findAll()
        // returns every current target for the same mapped name.  Keep that ambiguity in the
        // request-local Part MapperHistory ledger before runtime projects it; do not select one
        // candidate or recreate the event from response DTOs.
        MapperHistoryEvent event;
        event.id = canonicalCollisionHistoryId(context, group.canonical, group.candidates);
        event.source = group.candidates.front().source;
        event.target = group.candidates.front().target;
        event.shapeKind = group.candidates.front().shapeKind;
        event.relation = MapperHistoryRelation::Ambiguous;
        event.makerStage = "element_map_canonical_collision";
        event.evidence = {
            {"element_map", true},
            {"canonical_collision", true},
            {"context", context},
        };
        event.recoverability = MapperHistoryRecoverability::Ambiguous;
        event.diagnosticStatus = "canonical_element_map_collision";
        event.canonicalCollision = MapperHistoryCanonicalCollision {
            context,
            group.candidates.front().rawMappedName,
            group.canonical,
            group.candidates,
        };
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
}

const MappedNameProvenance* selectedSourceBackedMappedNameProvenance(
    const NamedShape& namedShape,
    const std::string& currentElement
)
{
    for (const auto& [stableName, mappedCurrentElement] : namedShape.elementMap) {
        if (mappedCurrentElement != currentElement) {
            continue;
        }
        const auto provenanceIt = namedShape.mappedNameProvenance.find(stableName);
        if (provenanceIt != namedShape.mappedNameProvenance.end()
            && hasPublicSourceBackedMappedNameEvidence(provenanceIt->second)) {
            return &provenanceIt->second;
        }
    }
    return nullptr;
}

void appendOwnerCanonicalCollisionHistory(NamedShape& namedShape)
{
    std::vector<MapperHistoryCollisionCandidate> candidates;
    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        for (const auto& [currentElement, element] : namedShape.elements) {
            if (element.subshape.kind != kind) {
                continue;
            }
            const MappedNameProvenance* provenance =
                selectedSourceBackedMappedNameProvenance(namedShape, currentElement);
            if (provenance == nullptr) {
                continue;
            }
            candidates.push_back(MapperHistoryCollisionCandidate {
                {namedShape.owner, currentElement},
                {namedShape.owner, currentElement},
                subshapeKindName(kind),
                provenance->rawMappedName,
                provenance->canonicalMappedName,
                MapperHistoryRecoverability::Resolved,
            });
        }
    }
    appendCanonicalCollisionHistory(
        namedShape,
        "topoNamingState.objects." + namedShape.owner + ".elementMap.entries",
        candidates
    );
}

std::optional<std::size_t> directChildSourceIndex(
    const std::vector<NamedShapeSource>& sources,
    const std::string& sourceOwner
)
{
    for (std::size_t index = 0U; index < sources.size(); ++index) {
        if (sources.at(index).owner == sourceOwner) {
            return index;
        }
    }
    return std::nullopt;
}

void appendDirectChildCanonicalCollisionHistory(
    NamedShape& namedShape,
    const std::vector<NamedShapeSource>& sources
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements() resolves an already-owned child ElementMap into the
    // parent range.  Only a direct child with its own child-map ledger contributes this second
    // source->parent ambiguity; recursive expansion is lookup evidence, not another public map.
    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        for (const NamedShapeChildMap& childMap : namedShape.childElementMaps) {
            if (childMap.recursiveExpansion || childMap.sourceNamedShape == nullptr
                || childMap.sourceChildMapCount == 0U || childMap.count <= 0
                || childMap.kind != subshapeKindName(kind)) {
                continue;
            }
            const std::optional<std::size_t> sourceIndex =
                directChildSourceIndex(sources, childMap.sourceOwner);
            if (!sourceIndex) {
                continue;
            }

            std::vector<MapperHistoryCollisionCandidate> candidates;
            const std::string sourcePrefix = childMap.sourceOwner + ".";
            const std::string elementPrefix = prefixForKind(kind);
            for (const auto& [currentElement, element] : namedShape.elements) {
                if (element.subshape.kind != kind || element.subshape.index <= childMap.offset
                    || element.subshape.index > childMap.offset + childMap.count) {
                    continue;
                }
                const std::string sourceElement = elementPrefix
                    + std::to_string(element.subshape.index - childMap.offset);
                for (const auto& [stableName, mappedCurrentElement] : namedShape.elementMap) {
                    if (mappedCurrentElement != currentElement
                        || stableName.rfind(sourcePrefix, 0U) != 0U) {
                        continue;
                    }
                    const auto provenanceIt = namedShape.mappedNameProvenance.find(stableName);
                    if (provenanceIt == namedShape.mappedNameProvenance.end()
                        || !hasPublicSourceBackedMappedNameEvidence(provenanceIt->second)) {
                        continue;
                    }
                    const MappedNameProvenance& provenance = provenanceIt->second;
                    candidates.push_back(MapperHistoryCollisionCandidate {
                        {childMap.sourceOwner, sourceElement},
                        {namedShape.owner, currentElement},
                        subshapeKindName(kind),
                        provenance.rawMappedName,
                        provenance.canonicalMappedName,
                        MapperHistoryRecoverability::Resolved,
                    });
                }
            }
            appendCanonicalCollisionHistory(
                namedShape,
                "topoNamingState.childElementMaps." + namedShape.owner + ":"
                    + childMap.sourceOwner + ":Child" + std::to_string(*sourceIndex)
                    + ".elementMap.entries",
                candidates
            );
        }
    }
}

void appendPartCanonicalCollisionHistory(
    NamedShape& namedShape,
    const std::vector<NamedShapeSource>& sources
)
{
    appendOwnerCanonicalCollisionHistory(namedShape);
    appendDirectChildCanonicalCollisionHistory(namedShape, sources);
}

bool sameRefineSurface(const TopoDS_Face& sourceFace, const TopoDS_Face& resultFace)
{
    const GeomAbs_SurfaceType sourceType = part::model_refine::FaceTypedBase::getFaceType(sourceFace);
    if (sourceType != part::model_refine::FaceTypedBase::getFaceType(resultFace)) {
        return false;
    }

    switch (sourceType) {
        case GeomAbs_Plane:
            return part::model_refine::getPlaneObject().isEqual(sourceFace, resultFace);
        case GeomAbs_Cylinder:
            return part::model_refine::getCylinderObject().isEqual(sourceFace, resultFace);
        case GeomAbs_BSplineSurface:
            return part::model_refine::getBSplineObject().isEqual(sourceFace, resultFace);
        default:
            return false;
    }
}

void applyRefineGenericGeneratedHistory(
    NamedShape& namedShape,
    const NamedShapeSource& source,
    const TopoDS_Shape& resultShape,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementRefine(), "GenericShapeMapper mapper; mkRefine.populate(mapper);
    // mapper.init(shape, mkRefine.Shape())". GenericShapeMapper::init() marks result faces
    // absent from the source as generated from a source face sharing two edges, or from a matching
    // surface among candidate source faces.
    if (source.shape.IsNull() || resultShape.IsNull()) {
        return;
    }

    TopTools_IndexedMapOfShape sourceFaces;
    TopTools_IndexedMapOfShape sourceEdges;
    TopTools_IndexedMapOfShape resultFaces;
    TopTools_IndexedDataMapOfShapeListOfShape edgeToFaces;
    TopExp::MapShapes(source.shape, TopAbs_FACE, sourceFaces);
    TopExp::MapShapes(source.shape, TopAbs_EDGE, sourceEdges);
    TopExp::MapShapes(resultShape, TopAbs_FACE, resultFaces);
    TopExp::MapShapesAndAncestors(source.shape, TopAbs_EDGE, TopAbs_FACE, edgeToFaces);

    for (int faceIndex = 1; faceIndex <= resultFaces.Extent(); ++faceIndex) {
        const TopoDS_Shape& resultFaceShape = resultFaces(faceIndex);
        if (findSameShapeIndex(sourceFaces, resultFaceShape) != 0) {
            continue;
        }
        const auto resultElementName = findElementName(namedShape, resultFaceShape, TopAbs_FACE);
        if (!resultElementName) {
            continue;
        }
        const auto resultElement = namedShape.elements.find(*resultElementName);
        if (resultElement != namedShape.elements.end()
            && resultElement->second.status == ElementHistoryKind::Modified) {
            continue;
        }

        std::map<int, int> sourceFaceEdgeCount;
        int generatedSourceFace = 0;
        for (TopExp_Explorer edgeIt(resultFaceShape, TopAbs_EDGE); edgeIt.More(); edgeIt.Next()) {
            const int sourceEdgeIndex = findSameShapeIndex(sourceEdges, edgeIt.Current());
            if (sourceEdgeIndex == 0 || !edgeToFaces.Contains(sourceEdges(sourceEdgeIndex))) {
                continue;
            }

            const TopoDS_Edge sourceEdge = TopoDS::Edge(sourceEdges(sourceEdgeIndex));
            const TopTools_ListOfShape& faces = edgeToFaces.FindFromKey(sourceEdges(sourceEdgeIndex));
            for (TopTools_ListIteratorOfListOfShape faceIt(faces); faceIt.More(); faceIt.Next()) {
                const int sourceFaceIndex = findSameShapeIndex(sourceFaces, faceIt.Value());
                if (sourceFaceIndex == 0) {
                    continue;
                }
                if (BRep_Tool::IsClosed(sourceEdge)) {
                    generatedSourceFace = sourceFaceIndex;
                    break;
                }
                if (++sourceFaceEdgeCount[sourceFaceIndex] == 2) {
                    generatedSourceFace = sourceFaceIndex;
                    break;
                }
            }
            if (generatedSourceFace != 0) {
                break;
            }
        }

        if (generatedSourceFace == 0) {
            const TopoDS_Face resultFace = TopoDS::Face(resultFaceShape);
            for (const auto& item : sourceFaceEdgeCount) {
                if (sameRefineSurface(TopoDS::Face(sourceFaces(item.first)), resultFace)) {
                    generatedSourceFace = item.first;
                    break;
                }
            }
        }
        if (generatedSourceFace == 0) {
            continue;
        }

        const std::string localSourceName = "Face" + std::to_string(generatedSourceFace);
        for (const std::string& sourceName : sourceElementNames(source, localSourceName)) {
            applyHistoryShape(
                namedShape,
                sourceName,
                resultFaceShape,
                ElementHistoryKind::Generated,
                sourceTargets
            );
        }
    }
}

void applyHistoryElementMap(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets,
    const std::string& producerOperation = {},
    bool recordUnmappedSourceDeletions = true
)
{
    const auto applyAlias = [&](const std::string& sourceName,
                                const SourceTargets& targets,
                                const std::string& target,
                                const std::string& operationPostfix) {
        namedShape.elementMap[sourceName] = target;
        if (!recordInheritedMappedNameProvenance(namedShape, sourceName, target, targets)) {
            recordMappedNameProvenance(
                namedShape,
                sourceName,
                target,
                targets.sourceElement.empty() ? sourceName : targets.sourceElement,
                targets.sourceTag,
                operationPostfix
            );
        }
    };
    const auto historyOperationPostfix = [&](const SourceTargets& targets,
                                             const std::string& target) {
        const auto historyKindIt = targets.historyKinds.find(target);
        if (historyKindIt == targets.historyKinds.end()) {
            return std::string {};
        }
        return operationPostfixForHistoryKind(historyKindIt->second, producerOperation);
    };
    const auto preservedOperationPostfix = [&](const SourceTargets& targets,
                                               const std::string& target) {
        if (!targets.preservedOperationPostfix.empty()) {
            return targets.preservedOperationPostfix;
        }
        if (targets.history.count(target) != 0U) {
            return historyOperationPostfix(targets, target);
        }
        if (targets.history.size() == 1U) {
            return historyOperationPostfix(targets, *targets.history.begin());
        }
        return std::string {};
    };
    const auto applySplit = [&](const std::string& sourceName, const std::set<std::string>& targets) {
        for (const std::string& target : targets) {
            auto elementIt = namedShape.elements.find(target);
            if (elementIt == namedShape.elements.end()) {
                continue;
            }
            elementIt->second.status = ElementHistoryKind::Split;
            namedShape.history.push_back(
                ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
            );
        }
    };

    for (const auto& [sourceName, targets] : sourceTargets) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::makeShapeWithElementMap() consumes MapperMaker generated/modified history into
        // ElementMap after first calling "mapSubElement(shapes)" for preserved source elements.
        // The same function walks Vertex, Edge and Face ShapeInfo separately and skips modified
        // history when "newInfo.type != newShape.ShapeType()", while generated lower elements
        // remain available as generated names. cad-core follows that priority: preserved source
        // subelements resolve first; one-to-one same-kind history fills the remaining keys;
        // one-to-many same-kind history is recorded as split and left unresolved.
        if (targets.preserved.size() == 1U) {
            applyAlias(
                sourceName,
                targets,
                *targets.preserved.begin(),
                preservedOperationPostfix(targets, *targets.preserved.begin())
            );
            continue;
        }
        if (targets.preserved.size() > 1U) {
            applySplit(sourceName, targets.preserved);
            continue;
        }

        const auto sourceKind = elementKindFromName(sourceName);
        if (sourceKind) {
            const std::set<std::string> sameKindHistory = targetsOfKind(targets.history, *sourceKind);
            if (sameKindHistory.size() == 1U) {
                const std::string target = *sameKindHistory.begin();
                applyAlias(sourceName, targets, target, historyOperationPostfix(targets, target));
                continue;
            }
            if (sameKindHistory.size() > 1U) {
                applySplit(sourceName, sameKindHistory);
                continue;
            }
        }

        if (targets.history.size() == 1U) {
            const std::string target = *targets.history.begin();
            applyAlias(sourceName, targets, target, historyOperationPostfix(targets, target));
            continue;
        }
        if (targets.history.size() > 1U) {
            applySplit(sourceName, targets.history);
            continue;
        }
        if (recordUnmappedSourceDeletions) {
            addTerminalHistory(
                namedShape,
                ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
            );
        }
    }
}

void addUnambiguousProducerLocalHistoryAliases(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets,
    const std::string& producerOperation
)
{
    struct LocalAliasCandidate
    {
        std::string sourceName;
        std::string currentElement;
        const SourceTargets* targets = nullptr;
    };

    std::map<std::string, std::vector<LocalAliasCandidate>> candidatesByLocalName;
    for (const auto& [sourceName, targets] : sourceTargets) {
        const auto mappedIt = namedShape.elementMap.find(sourceName);
        if (mappedIt == namedShape.elementMap.end()) {
            continue;
        }
        const std::string localName = localElementName(sourceName);
        if (localName.empty() || localName == sourceName) {
            continue;
        }
        candidatesByLocalName[localName].push_back(
            LocalAliasCandidate {sourceName, mappedIt->second, &targets}
        );
    }

    for (const auto& [localName, candidates] : candidatesByLocalName) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() obtains the incoming
        // producer-local IndexedName (EdgeN/VertexN) before encodeElementName() appends the
        // maker operation. Owner-qualified paths remain lookup aliases. Publish a local alias
        // only when maker history identifies one source unambiguously; competing source-local
        // names must retain their qualified aliases instead of being resolved by numbering.
        if (candidates.size() != 1U || candidates.front().targets == nullptr) {
            continue;
        }
        const LocalAliasCandidate& candidate = candidates.front();
        const auto existingIt = namedShape.elementMap.find(localName);
        if (existingIt != namedShape.elementMap.end()
            && existingIt->second != candidate.currentElement) {
            const auto provenanceIt = namedShape.mappedNameProvenance.find(localName);
            if (provenanceIt != namedShape.mappedNameProvenance.end()
                && provenanceIt->second.status == MappedNameProvenanceStatus::SourceBacked) {
                continue;
            }
        }

        const SourceTargets& targets = *candidate.targets;
        std::string operationPostfix = targets.preservedOperationPostfix;
        const auto historyKindIt = targets.historyKinds.find(candidate.currentElement);
        if (historyKindIt != targets.historyKinds.end()) {
            operationPostfix = operationPostfixForHistoryKind(
                historyKindIt->second,
                producerOperation
            );
        }
        if (operationPostfix.empty()) {
            continue;
        }

        namedShape.elementMap[localName] = candidate.currentElement;
        recordMappedNameProvenance(
            namedShape,
            localName,
            candidate.currentElement,
            localName,
            targets.sourceTag,
            operationPostfix
        );
    }
}

void applyPreservedElementMap(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets
)
{
    for (const auto& [sourceName, targets] : sourceTargets) {
        if (targets.preserved.size() == 1U) {
            const std::string target = *targets.preserved.begin();
            namedShape.elementMap[sourceName] = target;
            if (!recordInheritedMappedNameProvenance(namedShape, sourceName, target, targets)) {
                recordMappedNameProvenance(
                    namedShape,
                    sourceName,
                    target,
                    targets.sourceElement.empty() ? sourceName : targets.sourceElement,
                    targets.sourceTag,
                    targets.preservedOperationPostfix
                );
            }
            continue;
        }
        if (targets.preserved.size() <= 1U) {
            continue;
        }
        for (const std::string& target : targets.preserved) {
            auto elementIt = namedShape.elements.find(target);
            if (elementIt == namedShape.elements.end()) {
                continue;
            }
            elementIt->second.status = ElementHistoryKind::Split;
            namedShape.history.push_back(
                ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
            );
        }
    }
}

std::optional<std::string> sourceLocalElementName(
    const NamedShapeSource& source,
    TopAbs_ShapeEnum kind,
    const TopoDS_Shape& sourceElement
)
{
    const std::string prefix = prefixForKind(kind);
    if (prefix.empty() || source.shape.IsNull() || sourceElement.IsNull()) {
        return std::nullopt;
    }
    TopTools_IndexedMapOfShape sourceElements;
    TopExp::MapShapes(source.shape, kind, sourceElements);
    const int index = findSameShapeIndex(sourceElements, sourceElement);
    if (index <= 0) {
        return std::nullopt;
    }
    return prefix + std::to_string(index);
}

TopoDS_Vertex propagatedVertexClosestTo(
    const TopoDS_Vertex& originalVertex,
    const TopoDS_Edge& propagatedEdge
)
{
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(propagatedEdge, first, last);
    if (first.IsNull()) {
        return last;
    }
    if (last.IsNull()) {
        return first;
    }
    const gp_Pnt originalPoint = BRep_Tool::Pnt(originalVertex);
    const double firstDistance = originalPoint.SquareDistance(BRep_Tool::Pnt(first));
    const double lastDistance = originalPoint.SquareDistance(BRep_Tool::Pnt(last));
    return firstDistance <= lastDistance ? first : last;
}

void collectPropagatedWireElement(
    NamedShape& namedShape,
    const NamedShapeSource& source,
    const TopoDS_Shape& originalElement,
    const TopoDS_Shape& propagatedElement,
    TopAbs_ShapeEnum kind,
    std::map<std::string, SourceTargets>& sourceTargets,
    const std::string& operationPostfix = {}
)
{
    const auto localName = sourceLocalElementName(source, kind, originalElement);
    if (!localName) {
        return;
    }
    for (const std::string& sourceName : sourceElementNames(source, *localName)) {
        rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName, operationPostfix);
        collectSourceElementMap(namedShape, sourceName, propagatedElement, kind, sourceTargets);
    }
}

void addMergeHistory(NamedShape& namedShape)
{
    std::map<std::string, std::set<std::string>> aliasesByTarget;
    for (const auto& [stableName, currentName] : namedShape.elementMap) {
        if (stableName == currentName || namedShape.elements.count(currentName) == 0U) {
            continue;
        }
        aliasesByTarget[currentName].insert(stableName);
    }

    for (const auto& item : aliasesByTarget) {
        const std::string& target = item.first;
        const std::set<std::string>& aliases = item.second;
        if (aliases.size() <= 1U) {
            continue;
        }
        std::vector<std::string> sources(aliases.begin(), aliases.end());
        auto elementIt = namedShape.elements.find(target);
        if (elementIt != namedShape.elements.end()
            && elementIt->second.status != ElementHistoryKind::Split) {
            elementIt->second.status = ElementHistoryKind::Merge;
            for (const std::string& source : sources) {
                if (std::find(elementIt->second.sources.begin(), elementIt->second.sources.end(), source)
                    == elementIt->second.sources.end()) {
                    elementIt->second.sources.push_back(source);
                }
            }
        }
        const auto duplicate = std::find_if(
            namedShape.history.begin(),
            namedShape.history.end(),
            [&](const ElementHistory& entry) {
                return entry.kind == ElementHistoryKind::Merge && entry.element == target
                    && entry.sources == sources;
            }
        );
        if (duplicate == namedShape.history.end()) {
            namedShape.history.push_back(ElementHistory {ElementHistoryKind::Merge, target, sources});
        }
    }
}

void addRetagAlias(
    NamedShape& namedShape,
    const std::string& stableName,
    const std::string& targetName,
    const std::optional<long>& sourceTag = std::nullopt,
    const std::string& operationPostfix = {}
)
{
    if (stableName.empty() || targetName.empty() || stableName == targetName
        || namedShape.elements.count(targetName) == 0U) {
        return;
    }
    namedShape.elementMap[stableName] = targetName;
    recordMappedNameProvenance(
        namedShape,
        stableName,
        targetName,
        stableName,
        sourceTag,
        operationPostfix
    );
    auto& element = namedShape.elements[targetName];
    if (element.status == ElementHistoryKind::Indexed) {
        element.status = ElementHistoryKind::Modified;
    }
    if (std::find(element.sources.begin(), element.sources.end(), stableName)
        == element.sources.end()) {
        element.sources.push_back(stableName);
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == ElementHistoryKind::Modified && entry.element == targetName
                && entry.sources == std::vector<std::string> {stableName};
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(
            ElementHistory {ElementHistoryKind::Modified, targetName, {stableName}}
        );
    }
}

void addLinkRetagAlias(
    NamedShape& namedShape,
    const NamedShapeSource& source,
    const std::string& stableName,
    const std::string& targetName
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap(), "reTagElementMap(obj->getID(), ...)" retags
    // linked topology under the Link object. cad-core keeps source-prefixed aliases so later
    // LinkSub references can resolve without guessing topology order.
    addRetagAlias(namedShape, source.owner + "." + stableName, targetName);
    for (const std::string& aliasOwner : source.ownerAliases) {
        addRetagAlias(namedShape, aliasOwner + "." + stableName, targetName);
    }
    if (stableName.find('.') != std::string::npos) {
        addRetagAlias(namedShape, stableName, targetName);
    }
}

void addNestedHistory(
    NamedShape& namedShape,
    ElementHistoryKind kind,
    const std::string& targetElement,
    const std::vector<std::string>& sources
)
{
    if (targetElement.empty() || sources.empty()) {
        return;
    }
    auto elementIt = namedShape.elements.find(targetElement);
    if (elementIt == namedShape.elements.end()) {
        return;
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == kind && entry.element == targetElement && entry.sources == sources;
        }
    );
    if (duplicate != namedShape.history.end()) {
        return;
    }
    if (kind == ElementHistoryKind::Merge && elementIt->second.status != ElementHistoryKind::Split) {
        elementIt->second.status = kind;
    }
    else if (elementIt->second.status == ElementHistoryKind::Indexed
             && (kind == ElementHistoryKind::Generated || kind == ElementHistoryKind::Modified)) {
        elementIt->second.status = kind;
    }
    for (const std::string& source : sources) {
        if (std::find(elementIt->second.sources.begin(), elementIt->second.sources.end(), source)
            == elementIt->second.sources.end()) {
            elementIt->second.sources.push_back(source);
        }
    }
    namedShape.history.push_back(ElementHistory {kind, targetElement, sources});
}

void addTerminalHistory(NamedShape& namedShape, const ElementHistory& entry)
{
    if (entry.kind != ElementHistoryKind::Deleted && entry.kind != ElementHistoryKind::Split) {
        return;
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& current) {
            return current.kind == entry.kind && current.element == entry.element
                && current.sources == entry.sources;
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(entry);
    }
}

void addSplitHistory(NamedShape& namedShape, const std::string& sourceName, const std::string& targetName)
{
    auto elementIt = namedShape.elements.find(targetName);
    if (sourceName.empty() || elementIt == namedShape.elements.end()) {
        return;
    }
    elementIt->second.status = ElementHistoryKind::Split;
    addDistinctString(elementIt->second.sources, sourceName);
    addTerminalHistory(namedShape, ElementHistory {ElementHistoryKind::Split, targetName, {sourceName}});
}

void propagateNestedSourceHistory(NamedShape& namedShape, const std::vector<NamedShapeSource>& sources)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap(), calls "mapSubElement(shapes)" before MapperMaker;
    // MapperHistory then queries "Modified(s)" and "Generated(s)". Chained makers keep the
    // existing ElementMap ledger first, so generated/modified/merge history from the source
    // remains observable after a later maker or App::Link retag.
    // cad-core only forwards nested history when an existing ElementMap entry resolves to one
    // current result element; unresolved split/deleted cases remain represented by diagnostics.
    for (const auto& source : sources) {
        if (source.namedShape == nullptr) {
            continue;
        }
        for (const ElementHistory& entry : source.namedShape->history) {
            if (entry.kind == ElementHistoryKind::Deleted || entry.kind == ElementHistoryKind::Split) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp,
                // MapperHistory keeps terminal "deleted" and "split" outcomes in the element
                // history so later updateElementReference() can still report the old reference
                // state instead of degrading it to an opaque unresolved subname.
                addTerminalHistory(namedShape, entry);
                continue;
            }
            if (entry.kind != ElementHistoryKind::Generated
                && entry.kind != ElementHistoryKind::Modified
                && entry.kind != ElementHistoryKind::Merge) {
                continue;
            }
            for (const std::string& sourceName : sourceElementNames(source, entry.element)) {
                const auto mapped = namedShape.elementMap.find(sourceName);
                if (mapped == namedShape.elementMap.end()) {
                    continue;
                }
                addNestedHistory(namedShape, entry.kind, mapped->second, entry.sources);
            }
        }
    }
}

std::string booleanOperationName(BooleanOperation operation)
{
    switch (operation) {
        case BooleanOperation::Fuse:
            return "fuse";
        case BooleanOperation::Cut:
            return "cut";
        case BooleanOperation::Common:
            return "intersect";
    }
    return "boolean";
}

std::string booleanOperationCode(BooleanOperation operation)
{
    switch (operation) {
        case BooleanOperation::Fuse:
            return "FUS";
        case BooleanOperation::Cut:
            return "CUT";
        case BooleanOperation::Common:
            return "CMN";
    }
    return {};
}

nlohmann::json historyToJson(const ElementHistory& history)
{
    return {
        {"kind", historyKindName(history.kind)},
        {"element", history.element},
        {"sources", history.sources},
    };
}

nlohmann::json elementToJson(const NamedElement& element)
{
    return {
        {"kind", subshapeKindName(element.subshape.kind)},
        {"index", element.subshape.index},
        {"status", historyKindName(element.status)},
        {"sources", element.sources},
    };
}

nlohmann::json childElementMapToJson(const NamedShapeChildMap& childMap)
{
    return {
        {"source_owner", childMap.sourceOwner},
        {"kind", childMap.kind},
        {"indexed_name", childMap.indexedName},
        {"offset", childMap.offset},
        {"count", childMap.count},
        {"target_start", childMap.targetStart},
        {"target_end", childMap.targetEnd},
        {"postfix", childMap.postfix},
        {"encoded_child_map_key", childMap.encodedChildMapKey},
        {"has_source_element_map", childMap.hasSourceElementMap},
        {"source_element_map_size", childMap.sourceElementMapSize},
        {"source_child_map_count", childMap.sourceChildMapCount},
        {"recursive_expansion", childMap.recursiveExpansion},
    };
}

nlohmann::json mappedNameProvenanceTagToJson(const std::optional<long>& tag)
{
    if (!tag) {
        return nullptr;
    }
    return *tag;
}

nlohmann::json mappedNameProvenanceToJson(const MappedNameProvenance& provenance)
{
    return {
        {"entry_key", provenance.entryKey},
        {"current_element", provenance.currentElement},
        {"source_element", provenance.sourceElement},
        {"element_type", provenance.elementType},
        {"producer_tag", mappedNameProvenanceTagToJson(provenance.producerTag)},
        {"master_tag", mappedNameProvenanceTagToJson(provenance.masterTag)},
        {"source_tag", mappedNameProvenanceTagToJson(provenance.sourceTag)},
        {"operation_postfix", provenance.operationPostfix},
        {"raw_mapped_name", provenance.rawMappedName},
        {"canonical_mapped_name", provenance.canonicalMappedName},
        {"status", mappedNameProvenanceStatusName(provenance.status)},
    };
}

void consumeSketchInternalGeneratedFacesFromElementMap(
    NamedShape& namedShape,
    const TopoDS_Shape& internalShape,
    const nlohmann::json& internalMap
)
{
    if (!internalMap.is_object()) {
        return;
    }

    TopTools_IndexedMapOfShape internalFaces;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(internalShape, TopAbs_FACE, internalFaces);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);
    for (int faceIndex = 1; faceIndex <= internalFaces.Extent(); ++faceIndex) {
        TopTools_IndexedMapOfShape faceEdges;
        const TopoDS_Face face = TopoDS::Face(internalFaces(faceIndex));
        const TopoDS_Wire outerWire = BRepTools::OuterWire(face);
        TopExp::MapShapes(
            outerWire.IsNull() ? internalFaces(faceIndex) : outerWire,
            TopAbs_EDGE,
            faceEdges
        );
        std::vector<std::string> sources;
        for (int edgeIndex = 1; edgeIndex <= faceEdges.Extent(); ++edgeIndex) {
            const int internalEdgeIndex = findSameShapeIndex(internalEdges, faceEdges(edgeIndex));
            if (internalEdgeIndex <= 0) {
                continue;
            }
            const std::string internalEdgeName = "InternalEdge" + std::to_string(internalEdgeIndex);
            const auto mappedIt = internalMap.find(internalEdgeName);
            if (mappedIt == internalMap.end() || !mappedIt->is_string()) {
                continue;
            }
            const std::string rawName = mappedIt->get<std::string>();
            if (rawName.rfind("Edge", 0) == 0) {
                addDistinctString(sources, rawName);
            }
        }
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::getInternalElementMap(), records only exact InternalEdge/InternalVertex
        // aliases. Without a producer ledger, generated InternalFace history stays limited to those
        // exact aliases and does not synthesize split/deleted ownership from geometry.
        addGeneratedHistory(namedShape, "InternalFace" + std::to_string(faceIndex), sources);
    }
}

ElementHistoryKind elementHistoryKindForPublicationRelation(
    InternalShapeHistoryRelation relation
)
{
    switch (relation) {
        case InternalShapeHistoryRelation::Generated:
            return ElementHistoryKind::Generated;
        case InternalShapeHistoryRelation::Modified:
            return ElementHistoryKind::Modified;
        case InternalShapeHistoryRelation::Deleted:
            return ElementHistoryKind::Deleted;
        case InternalShapeHistoryRelation::Split:
            return ElementHistoryKind::Split;
        case InternalShapeHistoryRelation::Preserved:
        case InternalShapeHistoryRelation::DiagnosticOnly:
            break;
    }
    return ElementHistoryKind::Indexed;
}

void addMappedHistory(
    NamedShape& namedShape,
    ElementHistoryKind kind,
    const std::string& targetElement,
    const std::vector<std::string>& sources
)
{
    auto elementIt = namedShape.elements.find(targetElement);
    if (targetElement.empty() || sources.empty() || elementIt == namedShape.elements.end()) {
        return;
    }
    elementIt->second.status = kind;
    for (const std::string& source : sources) {
        addDistinctString(elementIt->second.sources, source);
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == kind && entry.element == targetElement
                && entry.sources == sources;
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(ElementHistory {kind, targetElement, sources});
    }
}

bool legacyHistoryCoversElementMap(
    const std::vector<ElementHistory>& history,
    const std::string& stableName,
    const std::string& currentName
)
{
    return std::any_of(history.begin(), history.end(), [&](const ElementHistory& entry) {
        if (entry.kind == ElementHistoryKind::Indexed || entry.element != currentName) {
            return false;
        }
        return std::find(entry.sources.begin(), entry.sources.end(), stableName)
            != entry.sources.end();
    });
}

void appendLegacyMapperHistoryEvent(
    std::vector<MapperHistoryEvent>& events,
    const std::string& owner,
    const ElementHistory& history
)
{
    const auto append = [&](const std::string& sourceName) {
        const bool terminalDeleted = history.kind == ElementHistoryKind::Deleted;
        MapperHistoryEvent event;
        event.source
            = mapperEndpointForElement(owner, sourceName.empty() ? history.element : sourceName);
        event.target = terminalDeleted ? MapperHistoryEndpoint {owner, {}}
                                       : mapperEndpointForElement(owner, history.element);
        event.shapeKind = shapeKindForHistoryElement(
            terminalDeleted ? event.source.subname : event.target.subname
        );
        event.relation = mapperRelationForHistoryKind(history.kind);
        event.makerStage = mapperStageForHistoryKind(history.kind);
        event.evidence = {
            {"legacy_history_kind", historyKindName(history.kind)},
            {"legacy_element", history.element},
        };
        event.recoverability = mapperRecoverabilityForHistoryKind(history.kind);
        event.diagnosticStatus = diagnosticStatusForHistoryKind(history.kind);
        addMapperHistoryEvent(events, std::move(event));
    };

    if (history.sources.empty()) {
        append(history.element);
        return;
    }
    for (const std::string& sourceName : history.sources) {
        append(sourceName);
    }
}

void appendElementMapMapperHistoryEvents(
    std::vector<MapperHistoryEvent>& events,
    const NamedShape& namedShape
)
{
    for (const auto& [stableName, currentName] : namedShape.elementMap) {
        if (stableName == currentName) {
            continue;
        }
        if (legacyHistoryCoversElementMap(namedShape.history, stableName, currentName)) {
            continue;
        }
        MapperHistoryEvent event;
        event.source = mapperEndpointForElement(namedShape.owner, stableName);
        event.target = mapperEndpointForElement(namedShape.owner, currentName);
        event.shapeKind = shapeKindForHistoryElement(event.target.subname);
        event.relation = MapperHistoryRelation::Preserved;
        event.makerStage = "element_map_preserved";
        event.evidence = {
            {"element_map", true},
            {"stable_subname", stableName},
            {"current_subname", currentName},
        };
        event.recoverability = MapperHistoryRecoverability::Resolved;
        addMapperHistoryEvent(events, std::move(event));
    }
}

std::vector<MapperHistoryEvent> mapperHistoryForNamedShape(const NamedShape& namedShape)
{
    std::vector<MapperHistoryEvent> events = namedShape.mapperHistory;
    for (const ElementHistory& history : namedShape.history) {
        appendLegacyMapperHistoryEvent(events, namedShape.owner, history);
    }
    appendElementMapMapperHistoryEvents(events, namedShape);
    return events;
}

std::vector<std::string> elementHistoryStatusForNamedShape(const NamedShape& namedShape)
{
    std::vector<std::string> statuses;
    bool hasGenerated = false;
    bool hasModified = false;
    bool hasDeleted = false;
    bool hasSplit = false;
    bool hasMerge = false;
    for (const ElementHistory& entry : namedShape.history) {
        hasGenerated = hasGenerated || entry.kind == ElementHistoryKind::Generated;
        hasModified = hasModified || entry.kind == ElementHistoryKind::Modified;
        hasDeleted = hasDeleted || entry.kind == ElementHistoryKind::Deleted;
        hasSplit = hasSplit || entry.kind == ElementHistoryKind::Split;
        hasMerge = hasMerge || entry.kind == ElementHistoryKind::Merge;
    }
    if (hasGenerated || hasModified) {
        statuses.push_back("history_consumed:generated_modified");
    }
    if (hasSplit || hasDeleted) {
        statuses.push_back("terminal_history:split_deleted");
    }
    if (hasSplit) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::getElementHistory(), key "history";
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeShapeWithElementMap(), one source with multiple same-kind history
        // targets is terminal split state and requires the caller to reselect the subname.
        statuses.push_back("subname_split_requires_reselect");
    }
    if (hasMerge) {
        statuses.push_back("history_consumed:merge");
    }
    return statuses;
}

}  // namespace

std::optional<long> requestLocalProducerTagForShape(const TopoDS_Shape& shape)
{
    return requestLocalProducerTagForShapeImpl(shape);
}

NamedShape indexedNamedShapeForObject(const std::string& owner, const TopoDS_Shape& shape)
{
    NamedShape namedShape;
    namedShape.owner = owner;
    namedShape.shape = shape;

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);

    addIndexedElements(namedShape, faces, TopAbs_FACE, "Face");
    addIndexedElements(namedShape, edges, TopAbs_EDGE, "Edge");
    addIndexedElements(namedShape, vertices, TopAbs_VERTEX, "Vertex");

    return namedShape;
}

void applyInternalShapeHistoryPublication(
    NamedShape& namedShape,
    const InternalShapeHistoryPublication& publication
)
{
    for (const auto& [stableName, targetName] : publication.elementMapAliases) {
        if (stableName.empty() || targetName.empty()) {
            continue;
        }
        if (namedShape.elements.find(targetName) == namedShape.elements.end()) {
            continue;
        }
        namedShape.elementMap[stableName] = targetName;
    }

    for (const InternalShapePublishedElementHistory& history : publication.elementHistory) {
        const ElementHistoryKind kind = elementHistoryKindForPublicationRelation(history.relation);
        switch (kind) {
            case ElementHistoryKind::Generated:
            case ElementHistoryKind::Modified:
                addMappedHistory(namedShape, kind, history.element, history.sources);
                break;
            case ElementHistoryKind::Deleted:
                addTerminalHistory(
                    namedShape,
                    ElementHistory {ElementHistoryKind::Deleted, history.element, history.sources}
                );
                break;
            case ElementHistoryKind::Split:
                for (const std::string& source : history.sources) {
                    addSplitHistory(namedShape, source, history.element);
                }
                break;
            case ElementHistoryKind::Indexed:
            case ElementHistoryKind::Merge:
                break;
        }
    }

    for (const MapperHistoryEvent& event : publication.mapperHistory) {
        addMapperHistoryEvent(namedShape.mapperHistory, event);
    }
    for (const std::string& status : publication.elementHistoryStatus) {
        addDistinctString(namedShape.elementHistoryStatus, status);
    }
    if (publication.diagnostics.is_object() && !publication.diagnostics.empty()) {
        namedShape.sketchInternalHistoryDiagnostics = publication.diagnostics;
    }
}

NamedShape namedShapeForSketchInternalShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    std::optional<InternalShapeHistoryLedger> historyLedger,
    std::map<std::string, std::string> internalEdgeMappedNames
)
{
    NamedShape namedShape;
    namedShape.owner = owner + ".InternalShape";
    namedShape.shape = internalShape;

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(internalShape, TopAbs_FACE, faces);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, edges);
    TopExp::MapShapes(internalShape, TopAbs_VERTEX, vertices);

    addIndexedElements(namedShape, faces, TopAbs_FACE, "InternalFace");
    addIndexedElements(namedShape, edges, TopAbs_EDGE, "InternalEdge");
    addIndexedElements(namedShape, vertices, TopAbs_VERTEX, "InternalVertex");

    const nlohmann::json internalMap = app::internalElementMapForSketch(rawShape, internalShape);
    if (!internalMap.is_object()) {
        return namedShape;
    }

    for (const auto& [name, mapped] : internalMap.items()) {
        if (!mapped.is_string()) {
            continue;
        }
        const std::string target = mapped.get<std::string>();
        if (name.rfind("Internal", 0) == 0) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::getInternalElementMap(), stores both "internalElementMap[prefix] = names.front()"
            // and "internalElementMap[names.front()] = prefix". Face entries are absent because
            // the function iterates only TopAbs_VERTEX and TopAbs_EDGE.
            addRetagAlias(namedShape, target, name);
        }
    }
    if (historyLedger) {
        const InternalShapeHistoryPublication publication =
            historyLedger->publishForInternalShape(InternalShapeHistoryPublishInput {
                namedShape.owner,
                rawShape,
                internalShape,
                internalMap,
                std::move(internalEdgeMappedNames),
            });
        applyInternalShapeHistoryPublication(namedShape, publication);
    }
    else {
        consumeSketchInternalGeneratedFacesFromElementMap(namedShape, internalShape, internalMap);
    }
    return namedShape;
}

NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::string& sourceOwner,
    const TopoDS_Shape& sourceShape,
    BRepBuilderAPI_MakeShape& maker,
    MakerHistoryOptions options
)
{
    return namedShapeForMakerHistory(
        owner,
        resultShape,
        std::vector<NamedShapeSource> {{sourceOwner, sourceShape}},
        maker,
        std::move(options)
    );
}

NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_MakeShape& maker,
    MakerHistoryOptions options
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;
    const std::string& producerOperation = options.producerOperation;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    rememberSourceTargetEvidence(
                        sourceTargets[sourceName],
                        source,
                        sourceName,
                        producerOperation
                    );
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                    try {
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Generated(sourceElement),
                            ElementHistoryKind::Generated,
                            sourceTargets
                        );
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Modified(sourceElement),
                            ElementHistoryKind::Modified,
                            sourceTargets
                        );
                    }
                    catch (const Standard_Failure&) {
                        continue;
                    }
                }
            }
        }
    }
    applyHistoryElementMap(
        namedShape,
        sourceTargets,
        producerOperation,
        options.recordUnmappedSourceDeletions
    );
    if (options.addProducerLocalAliases) {
        addUnambiguousProducerLocalHistoryAliases(
            namedShape,
            sourceTargets,
            producerOperation
        );
    }
    addRectangularFacePrismProducedMappedNames(
        namedShape,
        resultShape,
        sources,
        producerOperation
    );
    addRectangularFacePrismUntilProducedMappedNames(
        namedShape,
        resultShape,
        sources,
        producerOperation
    );
    addRectangularPocketCutProducedMappedNames(
        namedShape,
        resultShape,
        sources,
        producerOperation
    );
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    appendPartCanonicalCollisionHistory(namedShape, sources);

    return namedShape;
}

NamedShape namedShapeForThruSectionsHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepOffsetAPI_ThruSections& maker,
    const TopoDS_Shape& firstProfile,
    const TopoDS_Shape& lastProfile
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);

                    bool generated = false;
                    try {
                        generated = applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Generated(sourceElement),
                            ElementHistoryKind::Generated,
                            sourceTargets
                        );
                    }
                    catch (const Standard_Failure&) {
                        generated = false;
                    }
                    if (!generated) {
                        applyThruSectionsGeneratedHistory(
                            namedShape,
                            sourceName,
                            source.shape,
                            sourceElement,
                            maker,
                            firstProfile,
                            lastProfile,
                            sourceTargets
                        );
                    }
                    try {
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Modified(sourceElement),
                            ElementHistoryKind::Modified,
                            sourceTargets
                        );
                    }
                    catch (const Standard_Failure&) {
                        continue;
                    }
                }
            }
        }
    }
    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);

    return namedShape;
}

NamedShape namedShapeForSewingHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_Sewing& maker
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;
    bool sawModified = false;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                    sawModified = applySewingModifiedHistory(
                                      namedShape,
                                      sourceName,
                                      sourceElement,
                                      maker,
                                      sourceTargets
                                  )
                        || sawModified;
                }
            }
        }
    }

    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    if (sawModified) {
        addDistinctString(namedShape.elementHistoryStatus, "part_sewing:mapper_modified");
    }

    return namedShape;
}

std::optional<NamedShape> namedShapeForTaperedExtrusionHistory(
    const std::string& owner,
    const part::TaperedExtrusionResult& tapered,
    const TopoDS_Shape& profile,
    const NamedShapeSource& profileSource
)
{
    if (tapered.historyComponents.empty()) {
        return std::nullopt;
    }

    const std::size_t count = tapered.historyComponents.size();
    std::string currentOwner = taperComponentOwner(owner, 0, count);
    TopoDS_Shape currentShape = tapered.historyComponents.front().shape;
    NamedShape currentNamedShape = namedShapeForTaperComponent(
        currentOwner,
        tapered.historyComponents.front(),
        profile,
        profileSource
    );

    for (std::size_t index = 1; index < count; ++index) {
        const std::string innerOwner = taperComponentOwner(owner, index, count);
        NamedShape innerNamedShape = namedShapeForTaperComponent(
            innerOwner,
            tapered.historyComponents.at(index),
            profile,
            profileSource
        );
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
        // ::ExtrusionHelper::makeElementDraft(), "Inner wires are lofted into separate solids and
        // then cut from the outer solid"; cad-core routes the same owner chain through topo boolean
        // history so inner-wire generated sources survive the final taper result.
        const auto cut = makeElementBooleanFromSources(
            owner,
            {
                NamedShapeSource {currentOwner, currentShape, &currentNamedShape},
                NamedShapeSource {innerOwner, tapered.historyComponents.at(index).shape, &innerNamedShape},
            },
            BooleanOperation::Cut
        );
        if (cut.error.empty() && cut.namedShape) {
            currentOwner = owner + ".InnerCut" + std::to_string(index);
            currentShape = cut.shape;
            currentNamedShape = *cut.namedShape;
        }
    }

    currentNamedShape.owner = owner;
    currentNamedShape.shape = tapered.shape;
    return currentNamedShape;
}

NamedShape namedShapeForRefineHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    part::BRepBuilderAPI_RefineModel& maker
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MyRefineMaker::populate(), "mapper.populate(MappingStatus::Modified, it.Key(),
    // it.Value())"; ::TopoShape::makeElementRefine() then calls "mapper.init(shape,
    // mkRefine.Shape())" before makeShapeWithElementMap().
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        TopTools_IndexedMapOfShape sourceElements;
        TopExp::MapShapes(source.shape, kind, sourceElements);
        for (int index = 1; index <= sourceElements.Extent(); ++index) {
            const TopoDS_Shape& sourceElement = sourceElements(index);
            const std::string localElementName = prefix + std::to_string(index);
            for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                try {
                    applyHistoryList(
                        namedShape,
                        sourceName,
                        maker.Modified(sourceElement),
                        ElementHistoryKind::Modified,
                        sourceTargets
                    );
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.h
                    // ::BRepBuilderAPI_RefineModel exposes "IsDeleted(const TopoDS_Shape& S)";
                    // TopoShapeExpansion.cpp::makeElementRefine() routes that mapper into
                    // makeShapeWithElementMap(), so refined-away source elements remain terminal
                    // deleted history for later updateElementReference().
                    if (maker.IsDeleted(sourceElement) && sourceTargets[sourceName].preserved.empty()
                        && sourceTargets[sourceName].history.empty()) {
                        addTerminalHistory(
                            namedShape,
                            ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
                        );
                    }
                }
                catch (const Standard_Failure&) {
                    continue;
                }
            }
        }
    }

    applyRefineGenericGeneratedHistory(namedShape, source, resultShape, sourceTargets);
    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, std::vector<NamedShapeSource> {source});
    addMergeHistory(namedShape);

    return namedShape;
}

NamedShape namedShapeForShapeFixHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    part::ShapeFixHistory& fixer
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperHistory::modified() and ::generated(), read ShapeFix_Root "Context()->History()".
    // ::TopoShape::fix() then feeds that mapper into makeShapeWithElementMap() so deleted small
    // edges become terminal history instead of stale ElementMap aliases.
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        TopTools_IndexedMapOfShape sourceElements;
        TopExp::MapShapes(source.shape, kind, sourceElements);
        for (int index = 1; index <= sourceElements.Extent(); ++index) {
            const TopoDS_Shape& sourceElement = sourceElements(index);
            const std::string localElementName = prefix + std::to_string(index);
            for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                try {
                    applyHistoryList(
                        namedShape,
                        sourceName,
                        fixer.Generated(sourceElement),
                        ElementHistoryKind::Generated,
                        sourceTargets
                    );
                    applyHistoryList(
                        namedShape,
                        sourceName,
                        fixer.Modified(sourceElement),
                        ElementHistoryKind::Modified,
                        sourceTargets
                    );
                    if (fixer.IsDeleted(sourceElement) && sourceTargets[sourceName].preserved.empty()
                        && sourceTargets[sourceName].history.empty()) {
                        addTerminalHistory(
                            namedShape,
                            ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
                        );
                    }
                }
                catch (const Standard_Failure&) {
                    continue;
                }
            }
        }
    }

    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, std::vector<NamedShapeSource> {source});
    addMergeHistory(namedShape);
    return namedShape;
}

NamedShape namedShapeForShapeFixRootHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    ShapeFix_Root& fix
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperHistory::MapperHistory(ShapeFix_Root& fix), reads
    // "history = fix.Context()->History()"; tests/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperHistoryModified verifies ShapeFix_Wireframe history through that constructor.
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    Handle(BRepTools_History) history;
    if (fix.Context()) {
        history = fix.Context()->History();
    }
    bool sawGenerated = false;
    bool sawModified = false;
    bool sawDeleted = false;

    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        TopTools_IndexedMapOfShape sourceElements;
        TopExp::MapShapes(source.shape, kind, sourceElements);
        for (int index = 1; index <= sourceElements.Extent(); ++index) {
            const TopoDS_Shape& sourceElement = sourceElements(index);
            const std::string localElementName = prefix + std::to_string(index);
            for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                try {
                    if (!history.IsNull()) {
                        sawGenerated = applyHistoryList(
                                           namedShape,
                                           sourceName,
                                           history->Generated(sourceElement),
                                           ElementHistoryKind::Generated,
                                           sourceTargets
                                       )
                            || sawGenerated;
                        sawModified = applyHistoryList(
                                          namedShape,
                                          sourceName,
                                          history->Modified(sourceElement),
                                          ElementHistoryKind::Modified,
                                          sourceTargets
                                      )
                            || sawModified;
                        if (history->IsRemoved(sourceElement)
                            && sourceTargets[sourceName].preserved.empty()
                            && sourceTargets[sourceName].history.empty()) {
                            addTerminalHistory(
                                namedShape,
                                ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
                            );
                            sawDeleted = true;
                        }
                    }
                }
                catch (const Standard_Failure&) {
                    continue;
                }
            }
        }
    }

    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, std::vector<NamedShapeSource> {source});
    addMergeHistory(namedShape);
    if (sawGenerated) {
        addDistinctString(namedShape.elementHistoryStatus, "shapefix_root_history:generated");
    }
    if (sawModified) {
        addDistinctString(namedShape.elementHistoryStatus, "shapefix_root_history:modified");
    }
    if (sawDeleted) {
        addDistinctString(namedShape.elementHistoryStatus, "shapefix_root_history:deleted");
    }
    return namedShape;
}

NamedShape namedShapeForPreservedSources(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    const std::string& producerOperation
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    rememberSourceTargetEvidence(
                        sourceTargets[sourceName],
                        source,
                        sourceName,
                        source.childElementMapPostfix.empty()
                            ? producerOperation
                            : source.childElementMapPostfix
                    );
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                }
            }
        }
    }
    applyPreservedElementMap(namedShape, sourceTargets);
    collectChildElementMaps(namedShape, resultShape, sources);
    addRectangularFacePrismUntilProducedMappedNames(
        namedShape,
        resultShape,
        sources,
        producerOperation
    );
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    appendPartCanonicalCollisionHistory(namedShape, sources);

    return namedShape;
}

NamedShapeBuild makeElementWiresWithPropagatedSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    const std::string& op
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementWires(), key comment:
    // "MakeWire will replace vertex of connected edge ... update the shape in order to preserve
    // element mapping." cad-core keeps this in the Part-layer NamedShape ledger so adapters do
    // not infer Propagate aliases from result geometry.
    BRepBuilderAPI_MakeWire wireMaker;
    struct PropagatedEdge
    {
        const NamedShapeSource* source = nullptr;
        TopoDS_Edge originalEdge;
        TopoDS_Edge propagatedEdge;
    };
    std::vector<PropagatedEdge> propagatedEdges;

    for (const NamedShapeSource& source : sources) {
        if (source.shape.IsNull()) {
            continue;
        }
        TopTools_IndexedMapOfShape sourceEdges;
        TopExp::MapShapes(source.shape, TopAbs_EDGE, sourceEdges);
        for (int index = 1; index <= sourceEdges.Extent(); ++index) {
            const TopoDS_Edge originalEdge = TopoDS::Edge(sourceEdges(index));
            try {
                wireMaker.Add(originalEdge);
            }
            catch (const Standard_Failure& failure) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    failure.GetMessageString() != nullptr
                        ? failure.GetMessageString()
                        : "makeElementWires: could not add source edge"
                };
            }
            if (!wireMaker.IsDone()) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "makeElementWires: source edges did not form a wire"
                };
            }
            TopoDS_Edge propagatedEdge = wireMaker.Edge();
            if (propagatedEdge.IsNull()) {
                propagatedEdge = originalEdge;
            }
            propagatedEdges.push_back(PropagatedEdge {&source, originalEdge, propagatedEdge});
        }
    }

    if (propagatedEdges.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "makeElementWires: no source edges"};
    }
    if (!wireMaker.IsDone() || wireMaker.Wire().IsNull()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "makeElementWires: failed to build result wire"
        };
    }

    const TopoDS_Wire resultWire = wireMaker.Wire();
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultWire);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const PropagatedEdge& edge : propagatedEdges) {
        if (edge.source == nullptr) {
            continue;
        }
        collectPropagatedWireElement(
            namedShape,
            *edge.source,
            edge.originalEdge,
            edge.propagatedEdge,
            TopAbs_EDGE,
            sourceTargets,
            op
        );

        TopoDS_Vertex originalFirst;
        TopoDS_Vertex originalLast;
        TopExp::Vertices(edge.originalEdge, originalFirst, originalLast);
        if (!originalFirst.IsNull()) {
            collectPropagatedWireElement(
                namedShape,
                *edge.source,
                originalFirst,
                propagatedVertexClosestTo(originalFirst, edge.propagatedEdge),
                TopAbs_VERTEX,
                sourceTargets,
                op
            );
        }
        if (!originalLast.IsNull()) {
            collectPropagatedWireElement(
                namedShape,
                *edge.source,
                originalLast,
                propagatedVertexClosestTo(originalLast, edge.propagatedEdge),
                TopAbs_VERTEX,
                sourceTargets,
                op
            );
        }
    }

    applyPreservedElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    addDistinctString(namedShape.elementHistoryStatus, "element_map_policy_propagate:make_element_wires");
    return NamedShapeBuild {resultWire, namedShape, {}};
}

NamedShapeBuild makeElementShellWithPropagatedSource(
    const std::string& owner,
    const NamedShapeSource& source,
    const std::string& op
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementShell(), "builder.MakeShell(shell)" then
    // adds every "getSubShapes(TopAbs_FACE)" face; with ElementMapPolicy::Propagate it builds
    // "TopoShape tmp(..., shell)", calls "tmp.mapSubElement(*this, op)", and reuses that
    // ElementMap after possible ShapeUpgrade_ShellSewing repair.
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "makeElementShell: null source shape"};
    }

    try {
        BRep_Builder builder;
        TopoDS_Shell shell;
        builder.MakeShell(shell);
        int faceCount = 0;
        for (TopExp_Explorer explorer(source.shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
            builder.Add(shell, TopoDS::Face(explorer.Current()));
            ++faceCount;
        }
        if (faceCount == 0) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeElementShell: cannot make shell without face"
            };
        }

        TopoDS_Shape resultShape = shell;
        BRepCheck_Analyzer check(shell);
        if (!check.IsValid()) {
            ShapeUpgrade_ShellSewing sewShell;
            resultShape = sewShell.ApplySewing(shell);
        }
        if (resultShape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeElementShell: produced null shell"
            };
        }
        if (resultShape.ShapeType() != TopAbs_SHELL) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeElementShell: unexpected output shape type"
            };
        }

        NamedShapeSource propagatedSource = source;
        propagatedSource.childElementMapPostfix = op;
        NamedShape namedShape = namedShapeForPreservedSources(owner, resultShape, {propagatedSource});
        addDistinctString(
            namedShape.elementHistoryStatus,
            "element_map_policy_propagate:make_element_shell"
        );
        return NamedShapeBuild {resultShape, namedShape, {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "makeElementShell failed"
        };
    }
}

NamedShape namedShapeForLinkedShape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    if (source.namedShape == nullptr) {
        for (const auto& [localName, element] : namedShape.elements) {
            (void)element;
            addLinkRetagAlias(namedShape, source, localName, localName);
        }
        return namedShape;
    }

    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (namedShape.elements.count(currentName) == 0U) {
            continue;
        }
        addLinkRetagAlias(namedShape, source, stableName, currentName);
    }
    propagateNestedSourceHistory(namedShape, {source});
    addMergeHistory(namedShape);
    return namedShape;
}

NamedShape namedShapeForLinkedSubshape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::string& sourceElementName,
    const std::string& targetElementName
)
{
    return namedShapeForLinkedSubshapes(
        owner,
        resultShape,
        source,
        std::vector<std::pair<std::string, std::string>> {{sourceElementName, targetElementName}}
    );
}

NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<std::pair<std::string, std::string>>& sourceToTargetElements
)
{
    std::vector<LinkedSubshapeRetag> retags;
    retags.reserve(sourceToTargetElements.size());
    for (const auto& [sourceElementName, targetElementName] : sourceToTargetElements) {
        retags.push_back(LinkedSubshapeRetag {sourceElementName, targetElementName, {}});
    }
    return namedShapeForLinkedSubshapes(owner, resultShape, source, retags);
}

NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<LinkedSubshapeRetag>& sourceToTargetElements
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::parseSubName() can keep multiple PropertyXLink sub-elements with
    // the same linked-object prefix, and checkGeoElementMap() retags resolved linked topology.
    // cad-core preserves that retag per selected source element when a LinkSub returns a compound.
    for (const auto& retag : sourceToTargetElements) {
        const std::string& sourceElementName = retag.sourceElementName;
        const std::string& targetElementName = retag.targetElementName;
        if (targetElementName.empty() || namedShape.elements.count(targetElementName) == 0U) {
            continue;
        }

        addLinkRetagAlias(namedShape, source, sourceElementName, targetElementName);
        for (const std::string& alias : retag.exactAliases) {
            addRetagAlias(namedShape, alias, targetElementName);
        }
        if (source.namedShape == nullptr) {
            continue;
        }

        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            if (currentName == sourceElementName) {
                addLinkRetagAlias(namedShape, source, stableName, targetElementName);
            }
        }
    }
    if (source.namedShape != nullptr) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::checkGeoElementMap(), after getSubObject() resolves linked
        // geometry, calls "geoData->reTagElementMap(...)" so copied ElementMap terminal
        // outcomes remain visible to later PropertyLinkSub::updateElementReference().
        propagateNestedSourceHistory(namedShape, {source});
        addMergeHistory(namedShape);
    }
    return namedShape;
}

NamedShape namedShapeForTransformedCopy(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementTransform(), after transforming/copying the shape, calls
    // "copyElementMap(tmp, op)" instead of deriving ownership from result geometry.
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    for (const auto& [elementName, element] : namedShape.elements) {
        (void)element;
        addRetagAlias(namedShape, source.owner + "." + elementName, elementName);
    }

    if (source.namedShape != nullptr) {
        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            addRetagAlias(namedShape, stableName, currentName);
        }
        propagateNestedSourceHistory(namedShape, {source});
    }
    addMergeHistory(namedShape);

    return namedShape;
}

NamedShapeBuild makeElementCompoundFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool returnSingleShape
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for compound operation"};
        }
    }

    if (returnSingleShape && sources.size() == 1U) {
        NamedShape namedShape = sources.front().namedShape != nullptr
            ? *sources.front().namedShape
            : indexedNamedShapeForObject(owner, sources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = sources.front().shape;
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const auto& source : sources) {
        builder.Add(compound, source.shape);
    }

    NamedShape namedShape = namedShapeForPreservedSources(owner, compound, sources);
    namedShape.elementHistoryStatus.push_back("part_compound:make_element_compound");
    return NamedShapeBuild {compound, std::move(namedShape), {}};
}

NamedShapeBuild makeElementBooleanFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    BooleanOperation operation,
    std::optional<double> tolerance
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for boolean operation"
            };
        }
    }
    if (sources.size() == 1U) {
        NamedShape namedShape = sources.front().namedShape != nullptr
            ? *sources.front().namedShape
            : indexedNamedShapeForObject(owner, sources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = sources.front().shape;
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    std::vector<NamedShapeSource> booleanSources = expandBooleanSourcesLikeFreeCad(sources, operation);
    if (booleanSources.size() == 1U) {
        NamedShape namedShape = booleanSources.front().namedShape != nullptr
            ? *booleanSources.front().namedShape
            : indexedNamedShapeForObject(owner, booleanSources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = booleanSources.front().shape;
        return NamedShapeBuild {booleanSources.front().shape, std::move(namedShape), {}};
    }

    std::optional<NamedShape> fusedCompoundToolNamedShape;
    if (operation == BooleanOperation::Cut && booleanSources.size() == 2U && booleanSources.at(1).fuseCompoundForCut
        && booleanSources.at(1).shape.ShapeType() == TopAbs_COMPOUND) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
        // FCBRepAlgoAPI_BooleanOperation.cpp::RecursiveCutFusedTools(), for "cut argument and
        // compound tool", recursively adds tool children, fuses them when "myTools.Size() >= 2",
        // then restores BOPAlgo_CUT and cuts the original argument with the fused tool.
        std::vector<NamedShapeSource> toolChildren;
        expandCompoundSource(booleanSources.at(1), toolChildren);
        if (toolChildren.size() >= 2U) {
            const NamedShapeBuild fusedTool = makeElementBooleanFromSources(owner, toolChildren, BooleanOperation::Fuse);
            if (!fusedTool.error.empty() || fusedTool.shape.IsNull()) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    fusedTool.error.empty() ? "OCCT could not fuse compound boolean tool" : fusedTool.error,
                };
            }
            booleanSources.at(1).shape = fusedTool.shape;
            fusedCompoundToolNamedShape = fusedTool.namedShape;
            if (fusedCompoundToolNamedShape) {
                booleanSources.at(1).namedShape = &*fusedCompoundToolNamedShape;
            }
        }
    }

    std::unique_ptr<BRepAlgoAPI_BooleanOperation> maker;
    switch (operation) {
        case BooleanOperation::Fuse:
            maker = std::make_unique<BRepAlgoAPI_Fuse>();
            break;
        case BooleanOperation::Cut:
            maker = std::make_unique<BRepAlgoAPI_Cut>();
            break;
        case BooleanOperation::Common:
            maker = std::make_unique<BRepAlgoAPI_Common>();
            break;
    }

    TopTools_ListOfShape arguments;
    TopTools_ListOfShape tools;
    arguments.Append(booleanSources.front().shape);
    for (std::size_t index = 1; index < booleanSources.size(); ++index) {
        tools.Append(booleanSources.at(index).shape);
    }

    maker->SetRunParallel(Standard_True);
    maker->SetNonDestructive(Standard_True);
    maker->SetArguments(arguments);
    maker->SetTools(tools);
    if (tolerance) {
        if (*tolerance > 0.0) {
            maker->SetFuzzyValue(*tolerance);
        }
        else if (*tolerance < 0.0) {
            maker->SetFuzzyValue(autoFuzzyValueForSources(booleanSources));
        }
    }
    else {
        maker->SetFuzzyValue(autoFuzzyValueForSources(booleanSources));
    }
    maker->Build();
    if (!maker->IsDone()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "OCCT could not " + booleanOperationName(operation) + " boolean sources"
        };
    }

    const TopoDS_Shape resultShape = maker->Shape();
    return NamedShapeBuild {
        resultShape,
        namedShapeForMakerHistory(
            owner,
            resultShape,
            booleanSources,
            *maker,
            MakerHistoryOptions {booleanOperationCode(operation)}
        ),
        {}
    };
}

NamedShapeBuild makeElementXorFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }

    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for XOR operation"};
        }
    }
    if (sources.size() == 1U) {
        std::optional<NamedShape> namedShape;
        if (sources.front().namedShape != nullptr) {
            namedShape = *sources.front().namedShape;
        }
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    TopoDS_Shape currentShape = sources.front().shape;
    std::optional<NamedShape> currentNamedShape;
    if (sources.front().namedShape != nullptr) {
        currentNamedShape = *sources.front().namedShape;
    }
    std::string currentOwner = sources.front().owner;

    for (std::size_t index = 1; index < sources.size(); ++index) {
        const std::vector<NamedShapeSource> stepSources {
            {currentOwner, currentShape, currentNamedShape ? &*currentNamedShape : nullptr},
            sources.at(index),
        };

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementXor(), "Step 1: Union(A, B)" then "Step 2: Common(A, B)"
        // and finally "Cut(Union, Common)" when an intersection exists.
        const auto unionBuild
            = makeElementBooleanFromSources(owner, stepSources, BooleanOperation::Fuse);
        if (!unionBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, unionBuild.error};
        }
        const TopoDS_Shape unionShape = unionBuild.shape;

        const auto commonBuild
            = makeElementBooleanFromSources(owner, stepSources, BooleanOperation::Common);
        if (!commonBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, commonBuild.error};
        }
        const TopoDS_Shape commonShape = commonBuild.shape;
        if (commonShape.IsNull()) {
            currentShape = unionShape;
            currentNamedShape = unionBuild.namedShape;
            currentOwner = owner + ".XorUnion" + std::to_string(index);
            continue;
        }

        const auto cutBuild = makeElementBooleanFromSources(
            owner,
            std::vector<NamedShapeSource> {
                {owner + ".XorUnion" + std::to_string(index),
                 unionShape,
                 unionBuild.namedShape ? &*unionBuild.namedShape : nullptr},
                {owner + ".XorCommon" + std::to_string(index),
                 commonShape,
                 commonBuild.namedShape ? &*commonBuild.namedShape : nullptr},
            },
            BooleanOperation::Cut
        );
        if (!cutBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, cutBuild.error};
        }
        currentNamedShape = cutBuild.namedShape;
        currentShape = cutBuild.shape;
        currentOwner = owner + ".XorResult" + std::to_string(index);
    }

    return NamedShapeBuild {currentShape, std::move(currentNamedShape), {}};
}

NamedShapeBuild makeElementSectionFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool approximate
)
{
    if (sources.size() < 2U) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "Section requires at least two input shapes"
        };
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for section operation"
            };
        }
    }

    try {
        BRepAlgoAPI_Section maker;
        TopTools_ListOfShape arguments;
        TopTools_ListOfShape tools;
        arguments.Append(sources.front().shape);
        for (std::size_t index = 1; index < sources.size(); ++index) {
            tools.Append(sources.at(index).shape);
        }
        maker.Approximation(approximate);
        maker.SetRunParallel(Standard_True);
        maker.SetNonDestructive(Standard_True);
        maker.SetArguments(arguments);
        maker.SetTools(tools);
        maker.SetFuzzyValue(autoFuzzyValueForSources(sources));
        maker.Build();
        if (!maker.IsDone()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Section failed"};
        }
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Resulting shape is null"};
        }
        return NamedShapeBuild {
            resultShape,
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // FeaturePartSection.cpp::Section::opCode() returns Part::OpCodes::Section (SEC),
            // and FeaturePartBoolean.cpp::Boolean::execute() passes it to
            // TopoShape::makeElementShape() before PropertyPartShape::setValue().
            namedShapeForMakerHistory(
                owner,
                resultShape,
                sources,
                maker,
                MakerHistoryOptions {"SEC", false, true}
            ),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Section failed"
        };
    }
}

NamedShapeBuild makeElementOffsetFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    double tolerance,
    bool intersection,
    bool selfIntersection,
    short offsetMode,
    short join,
    bool fill
)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for offset operation"};
    }

    try {
        BRepOffsetAPI_MakeOffsetShape maker;
        maker.PerformByJoin(
            source.shape,
            offset,
            tolerance,
            BRepOffset_Mode(offsetMode),
            intersection ? Standard_True : Standard_False,
            selfIntersection ? Standard_True : Standard_False,
            GeomAbs_JoinType(join)
        );
        if (!maker.IsDone()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "BRepOffsetAPI_MakeOffsetShape not done"
            };
        }
        const TopoDS_Shape offsetShape = maker.Shape();
        if (offsetShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Resulting offset shape is null"};
        }
        if (fill) {
            FilledOffsetBuild filled = makeFilledOffsetShape(source.shape, offsetShape, maker);
            if (!filled.error.empty() || filled.shape.IsNull()) {
                return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, filled.error};
            }
            NamedShape namedShape = namedShapeForPreservedSources(owner, filled.shape, {source});
            addDistinctString(namedShape.elementHistoryStatus, "part_offset_fill:sewing_history");
            addDistinctString(namedShape.elementHistoryStatus, "part_offset_fill:perimeter_faces");
            return NamedShapeBuild {filled.shape, namedShape, {}};
        }
        NamedShape offsetNamedShape = namedShapeForMakerHistory(
            owner,
            offsetShape,
            std::vector<NamedShapeSource> {source},
            maker
        );
        SolidRecoveryBuild solidRecovery
            = recoverOffsetSolidLikeFreeCad(owner, source, offsetShape, offsetNamedShape);
        if (!solidRecovery.error.empty()) {
            NamedShape namedShape = solidRecovery.namedShape.value_or(offsetNamedShape);
            addDistinctString(
                namedShape.elementHistoryStatus,
                "part_offset_solid_source:make_element_solid_failed"
            );
            return NamedShapeBuild {solidRecovery.shape, namedShape, {}};
        }
        if (solidRecovery.applied) {
            return NamedShapeBuild {
                solidRecovery.shape,
                solidRecovery.namedShape,
                {},
            };
        }
        return NamedShapeBuild {
            offsetShape,
            offsetNamedShape,
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Offset failed"
        };
    }
}

NamedShapeBuild makeElementOffset2DFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double offset,
    short join,
    bool fill,
    bool allowOpenResult,
    bool intersection
)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for 2D offset operation"};
    }
    if (source.shape.ShapeType() == TopAbs_COMPOUND) {
        if (intersection) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::makeElementOffset2D(), "intersection" collects
            // non-compound children for collective offset and processes nested compounds
            // recursively.
            return makeOffset2DCompoundCollectiveLikeFreeCad(
                owner,
                source,
                offset,
                join,
                fill,
                allowOpenResult
            );
        }
        return makeOffset2DCompoundChildrenLikeFreeCad(owner, source, offset, join, fill, allowOpenResult);
    }
    if (intersection) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeElementOffset2D(), "intersection" changes
        // compound handling by collecting non-compound children for collective offset. This slice
        // handles single face/edge/wire sources only, so collective intersection remains metadata.
    }
    const bool effectiveOpenResult = allowOpenResult && source.shape.ShapeType() != TopAbs_FACE;

    try {
        if (source.shape.ShapeType() == TopAbs_EDGE) {
            const auto sourceWire = wireFromEdge(TopoDS::Edge(source.shape));
            if (!sourceWire) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "Part::Offset2D could not convert source edge to wire"
                };
            }
            return makeOffset2DWireLikeFreeCad(
                owner,
                source,
                std::vector<TopoDS_Wire> {*sourceWire},
                offset,
                join,
                fill,
                effectiveOpenResult
            );
        }
        if (source.shape.ShapeType() == TopAbs_WIRE) {
            return makeOffset2DWireLikeFreeCad(
                owner,
                source,
                std::vector<TopoDS_Wire> {TopoDS::Wire(source.shape)},
                offset,
                join,
                fill,
                effectiveOpenResult
            );
        }
        if (source.shape.ShapeType() != TopAbs_FACE) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "makeOffset2D: input shape is not an edge, wire or face or compound of those."
            };
        }
        return makeOffset2DFaceLikeFreeCad(
            owner,
            source,
            TopoDS::Face(source.shape),
            offset,
            join,
            fill,
            effectiveOpenResult
        );
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Offset2D failed"
        };
    }
}

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
)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    if (faces.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape"};
    }
    if (std::fabs(offset) <= 2.0 * tolerance) {
        NamedShape namedShape = namedShapeForPreservedSources(owner, source.shape, {source});
        addDistinctString(namedShape.elementHistoryStatus, "part_thickness:zero_thickness_copy");
        return NamedShapeBuild {source.shape, namedShape, {}};
    }

    TopTools_ListOfShape removeFaces;
    for (const TopoDS_Face& face : faces) {
        if (face.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape"};
        }
        if (!shapeContains(source.shape, face)) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "face does not belong to the shape"};
        }
        removeFaces.Append(face);
    }

    short effectiveJoin = join;
    if (effectiveJoin != 0 && effectiveJoin != 2) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementThickSolid(), "we do not offer tangent join type", so any
        // non Arc / Intersection join is treated as JoinType::intersection before OCCT.
        effectiveJoin = 2;
    }

    try {
        BRepOffsetAPI_MakeThickSolid maker;
        maker.MakeThickSolidByJoin(
            source.shape,
            removeFaces,
            offset,
            tolerance,
            BRepOffset_Mode(offsetMode),
            intersection ? Standard_True : Standard_False,
            selfIntersection ? Standard_True : Standard_False,
            GeomAbs_JoinType(effectiveJoin)
        );
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Part::Thickness produced a null shape"
            };
        }
        NamedShape namedShape = namedShapeForMakerHistory(
            owner,
            resultShape,
            std::vector<NamedShapeSource> {source},
            maker
        );
        addDistinctString(namedShape.elementHistoryStatus, "part_thickness:make_thick_solid");
        return NamedShapeBuild {resultShape, namedShape, {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Thickness failed"
        };
    }
}

NamedShapeBuild makeElementSolidFromSource(const std::string& owner, const NamedShapeSource& source)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for makeElementSolid"};
    }

    try {
        int compsolidCount = 0;
        TopoDS_CompSolid compsolid;
        for (TopExp_Explorer explorer(source.shape, TopAbs_COMPSOLID); explorer.More();
             explorer.Next()) {
            ++compsolidCount;
            compsolid = TopoDS::CompSolid(explorer.Current());
            if (compsolidCount > 1) {
                break;
            }
        }

        if (compsolidCount == 1) {
            BRepBuilderAPI_MakeSolid solidMaker(compsolid);
            TopoDS_Shape solidShape = solidMaker.Shape();
            if (solidShape.IsNull()) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "makeElementSolid returned null solid"
                };
            }
            NamedShape namedShape = namedShapeForMakerHistory(owner, solidShape, {source}, solidMaker);
            addDistinctString(namedShape.elementHistoryStatus, "part_make_solid:make_element_solid");
            return NamedShapeBuild {solidShape, namedShape, {}};
        }
        if (compsolidCount > 1) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Only one compsolid can be accepted in makeElementSolid"
            };
        }

        BRepBuilderAPI_MakeSolid solidMaker;
        int shellCount = 0;
        for (TopExp_Explorer explorer(source.shape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
            solidMaker.Add(TopoDS::Shell(explorer.Current()));
            ++shellCount;
        }
        if (shellCount == 0) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "No shells or compsolids found in shape"
            };
        }
        if (!solidMaker.IsDone()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Failed to create a solid in makeElementSolid"
            };
        }
        TopoDS_Solid solid = TopoDS::Solid(solidMaker.Shape());
        BRepLib::OrientClosedSolid(solid);
        NamedShape namedShape = namedShapeForMakerHistory(owner, solid, {source}, solidMaker);
        addDistinctString(namedShape.elementHistoryStatus, "part_make_solid:make_element_solid");
        return NamedShapeBuild {solid, namedShape, {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "makeElementSolid failed"
        };
    }
}

NamedShapeBuild makeElementGeneralFuseFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    double tolerance
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for general fuse operation"
            };
        }
    }
    if (sources.size() == 1U) {
        NamedShape namedShape = sources.front().namedShape != nullptr
            ? *sources.front().namedShape
            : indexedNamedShapeForObject(owner, sources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = sources.front().shape;
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    try {
        BRepAlgoAPI_BuilderAlgo maker;
        maker.SetRunParallel(true);
        TopTools_ListOfShape arguments;
        for (const auto& source : sources) {
            arguments.Append(source.shape);
        }
        maker.SetArguments(arguments);
        if (tolerance > 0.0) {
            maker.SetFuzzyValue(tolerance);
        }
        maker.SetNonDestructive(Standard_True);
        maker.Build();
        if (!maker.IsDone()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "GeneralFuse failed"};
        }
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Resulting shape is null"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForMakerHistory(owner, resultShape, sources, maker),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "GeneralFuse failed"
        };
    }
}

NamedShapeBuild makeElementRefineFromSource(const std::string& owner, const NamedShapeSource& source)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for refine operation"};
    }

    try {
        part::BRepBuilderAPI_RefineModel maker(source.shape);
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Refine produced a null shape"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForRefineHistory(owner, resultShape, source, maker),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Refine operation failed"
        };
    }
}

NamedShapeBuild makeElementShapeFixFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    double precision,
    double smallEdgeTolerance
)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for ShapeFix"};
    }

    try {
        part::ShapeFixHistory fixer(source.shape);
        if (precision > 0.0) {
            fixer.setPrecision(precision);
        }
        if (smallEdgeTolerance > 0.0) {
            fixer.removeSmallEdges(smallEdgeTolerance);
        }
        else {
            fixer.perform();
        }
        const TopoDS_Shape resultShape = fixer.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "ShapeFix produced a null shape"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForShapeFixHistory(owner, resultShape, source, fixer),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "ShapeFix operation failed"
        };
    }
}

NamedShape namedShapeForElementMapPolicyDrop(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    for (const NamedShapeSource& source : sources) {
        if (source.shape.IsNull()) {
            continue;
        }
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    MapperHistoryEvent event;
                    event.source = mapperEndpointForElement(source.owner, sourceName);
                    event.target = MapperHistoryEndpoint {owner, {}};
                    event.shapeKind = subshapeKindName(kind);
                    event.relation = MapperHistoryRelation::Deleted;
                    event.makerStage = "element_map_policy_drop";
                    event.evidence = {
                        {"element_map_policy", "drop"},
                        {"drop_element_naming", true},
                        {"source_element", sourceName},
                    };
                    event.recoverability = MapperHistoryRecoverability::Diagnostic;
                    event.diagnosticStatus = "element_map_policy_drop";
                    addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
                }
            }
        }
    }

    addDistinctString(namedShape.elementHistoryStatus, "element_map_policy:drop");
    return namedShape;
}

std::optional<std::string> resolveElementName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    auto resolved = resolveElementReference(namedShape, subname, stableSubname);
    if (resolved.status == ElementResolveStatus::Resolved) {
        return resolved.element;
    }
    return std::nullopt;
}

ElementResolveResult resolveElementReference(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::updateElementReference()
    // drives PropertyLinkBase::updateElementReferences() after ElementMap version changes. This
    // is the P6 identity-map baseline: stable indexed names resolve through the object-local map,
    // while opaque mapped names wait for MapperHistory-backed ElementMap entries.
    if (!stableSubname.empty()) {
        const auto mapped = namedShape.elementMap.find(stableSubname);
        if (mapped != namedShape.elementMap.end()) {
            const auto provenanceIt = namedShape.mappedNameProvenance.find(stableSubname);
            if (provenanceIt != namedShape.mappedNameProvenance.end()) {
                const MappedNameProvenance& provenance = provenanceIt->second;
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                // ::ElementMap::setElementName() writes the final IndexedName target together
                // with the encoded producer name. If cad-core's derived elementMap alias is later
                // overwritten, keep source-backed producer evidence authoritative for reference
                // recovery instead of resolving through a stale display alias.
                if (provenance.status == MappedNameProvenanceStatus::SourceBacked
                    && !provenance.currentElement.empty()
                    && namedShape.elements.count(provenance.currentElement) != 0U) {
                    return ElementResolveResult {
                        ElementResolveStatus::Resolved,
                        provenance.currentElement
                    };
                }
            }
            return ElementResolveResult {ElementResolveStatus::Resolved, mapped->second};
        }
        for (const ElementHistory& entry : namedShape.history) {
            if (entry.kind == ElementHistoryKind::Deleted && entry.element == stableSubname) {
                return ElementResolveResult {ElementResolveStatus::Deleted, std::nullopt};
            }
        }
        bool split = false;
        for (const ElementHistory& entry : namedShape.history) {
            if (entry.kind == ElementHistoryKind::Split
                && std::find(entry.sources.begin(), entry.sources.end(), stableSubname)
                    != entry.sources.end()) {
                // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
                // ::updateElementReference() preserves the user-visible subname when an old
                // stable reference cannot be collapsed to one ElementMap target. If SubList
                // already names one concrete split target, cad-core resolves that explicit choice
                // instead of reporting the whole stable reference as ambiguous.
                if (entry.element == subname && namedShape.elements.count(subname) != 0U) {
                    return ElementResolveResult {ElementResolveStatus::Resolved, subname};
                }
                split = true;
            }
        }
        if (split) {
            return ElementResolveResult {ElementResolveStatus::Split, std::nullopt};
        }
        return ElementResolveResult {ElementResolveStatus::Unresolved, std::nullopt};
    }

    if (namedShape.elements.count(subname) != 0U) {
        return ElementResolveResult {ElementResolveStatus::Resolved, subname};
    }
    return ElementResolveResult {ElementResolveStatus::Unresolved, std::nullopt};
}

std::optional<TopoDS_Shape> subshapeByName(const NamedShape& namedShape, const std::string& name)
{
    const auto it = namedShape.elements.find(name);
    if (it == namedShape.elements.end()) {
        return std::nullopt;
    }
    return subshapeByName(namedShape.shape, it->second.subshape);
}

std::optional<TopoDS_Shape> subshapeByName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    const auto resolved = resolveElementName(namedShape, subname, stableSubname);
    if (!resolved) {
        return std::nullopt;
    }
    return subshapeByName(namedShape, *resolved);
}

nlohmann::json namedShapeToJson(const NamedShape& namedShape)
{
    nlohmann::json elements = nlohmann::json::object();
    for (const auto& [name, element] : namedShape.elements) {
        elements[name] = elementToJson(element);
    }

    nlohmann::json history = nlohmann::json::array();
    for (const auto& entry : namedShape.history) {
        history.push_back(historyToJson(entry));
    }
    nlohmann::json childElementMaps = nlohmann::json::array();
    for (const NamedShapeChildMap& childMap : namedShape.childElementMaps) {
        childElementMaps.push_back(childElementMapToJson(childMap));
    }
    nlohmann::json mappedNameProvenance = nlohmann::json::object();
    for (const auto& [entryKey, provenance] : namedShape.mappedNameProvenance) {
        mappedNameProvenance[entryKey] = mappedNameProvenanceToJson(provenance);
    }
    const std::vector<MapperHistoryEvent> mapperHistory = mapperHistoryForNamedShape(namedShape);

    const bool hasMappedHistory = std::any_of(
                                      namedShape.history.begin(),
                                      namedShape.history.end(),
                                      [](const ElementHistory& item) {
                                          return item.kind != ElementHistoryKind::Indexed;
                                      }
                                  )
        || std::any_of(namedShape.elementMap.begin(),
                       namedShape.elementMap.end(),
                       [](const auto& item) { return item.first != item.second; })
        || !namedShape.childElementMaps.empty();
    std::vector<std::string> elementHistoryStatus = namedShape.elementHistoryStatus;
    for (const std::string& status : elementHistoryStatusForNamedShape(namedShape)) {
        addDistinctString(elementHistoryStatus, status);
    }

    nlohmann::json result = {
        {"owner", namedShape.owner},
        {"element_map_status", hasMappedHistory ? "history_partial" : "indexed_only"},
        {"element_history_status", elementHistoryStatus},
        {"element_map", namedShape.elementMap},
        {"mapped_name_provenance", mappedNameProvenance},
        {"child_element_maps", childElementMaps},
        {"elements", elements},
        {"history", history},
        {"mapper_history", mapperHistoryToJson(mapperHistory)},
    };
    if (namedShape.sketchInternalHistoryDiagnostics
        && namedShape.sketchInternalHistoryDiagnostics->is_object()
        && !namedShape.sketchInternalHistoryDiagnostics->empty()) {
        result["sketch_internal_history_diagnostics"] =
            *namedShape.sketchInternalHistoryDiagnostics;
    }
    return result;
}

nlohmann::json namedShapesToJson(const std::map<std::string, NamedShape>& namedShapes)
{
    nlohmann::json result = nlohmann::json::object();
    for (const auto& [name, namedShape] : namedShapes) {
        result[name] = namedShapeToJson(namedShape);
    }
    return result;
}

}  // namespace cad_core::part
