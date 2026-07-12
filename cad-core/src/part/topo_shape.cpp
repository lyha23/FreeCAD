#include "cad_core/part/topo_shape.h"

#include "cad_core/app/string_hasher.h"

#include "cad_core/part/extrusion_helper.h"
#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/element_map_producer_trace_snapshot.h"
#include "element_map_producer_trace_snapshot_internal.h"
#include "topo_shape_producer_trace_internal.h"
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
#include <cctype>
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

void recordElementMapEntry(NamedShape& namedShape,
                           const std::string& mappedName,
                           const std::string& currentElement,
                           bool preserveEncoding)
{
    if (mappedName.empty() || currentElement.empty()) {
        return;
    }
    std::vector<cad_core::app::StringId> refs;
    if (const auto provenance = namedShape.mappedNameProvenance.find(mappedName);
        provenance != namedShape.mappedNameProvenance.end()) {
        refs = provenance->second.elementIdRefs;
    }
    auto& entries = namedShape.elementMapEntries[currentElement];
    const auto existing = std::find_if(
        entries.begin(),
        entries.end(),
        [&](const ElementMapEntry& entry) { return entry.mappedName == mappedName; }
    );
    const bool appended = existing == entries.end();
    if (appended) {
        auto insertion = entries.end();
        if (preserveEncoding && !entries.empty()) {
            // ElementMap::setElementName() retains the head NameKey and links subsequent
            // preserved aliases immediately behind it. Repeated splitter aliases are therefore
            // LIFO after the compact head, while the pre-existing tail remains stable.
            insertion = std::next(entries.begin());
        }
        entries.insert(insertion, ElementMapEntry {mappedName, std::move(refs)});
    }
    else {
        existing->elementIdRefs = std::move(refs);
    }
    if (namedShape.stringHasher && namedShape.stringHasher->producerTrace() != nullptr) {
        app::ElementMapProducerTrace* trace = namedShape.stringHasher->producerTrace();
        const auto writtenEntry = std::find_if(
            entries.begin(), entries.end(), [&](const ElementMapEntry& entry) {
                return entry.mappedName == mappedName;
            }
        );
        const ElementMapEntry& written = *writtenEntry;
        std::vector<app::StringId> displayRefs = written.elementIdRefs;
        const auto provenance = namedShape.mappedNameProvenance.find(mappedName);
        if (!preserveEncoding && provenance != namedShape.mappedNameProvenance.end()) {
            const std::size_t postfix = provenance->second.rawMappedName.find(';');
            if (const auto primary = app::parseStringId(
                    provenance->second.rawMappedName.substr(0U, postfix)
                )) {
                const auto primaryRef = std::find_if(
                    displayRefs.begin(), displayRefs.end(), [&](const app::StringId& ref) {
                        return ref.value == primary->value && ref.index == primary->index;
                    }
                );
                if (primaryRef != displayRefs.end() && primaryRef != displayRefs.begin()) {
                    std::rotate(displayRefs.begin(), primaryRef, std::next(primaryRef));
                }
            }
        }
        std::string entryRefs;
        for (const app::StringId& ref : displayRefs) {
            if (!entryRefs.empty()) {
                entryRefs += ",";
            }
            entryRefs += ref.toString();
        }
        const std::string before = provenance != namedShape.mappedNameProvenance.end()
                && !provenance->second.encodeInputMappedName.empty()
            ? provenance->second.encodeInputMappedName
            : (provenance != namedShape.mappedNameProvenance.end()
                   ? provenance->second.sourceElement + provenance->second.operationPostfix
                   : mappedName);
        const std::string hashed = displayRefs.empty()
            ? before
            : displayRefs.front().toString();
        if (preserveEncoding) {
            trace->record({
                "element_map.encode", "preserved", "same_or_empty_tag",
                {{"before", provenance != namedShape.mappedNameProvenance.end()
                                ? provenance->second.rawMappedName
                                : mappedName}},
            });
        }
        else {
            trace->record({
                "element_map.encode", "hashed", "string_hasher",
                {{"before", before}, {"after", hashed}, {"entryLocalRefs", entryRefs}},
            });
            trace->record({
                "element_map.encode", "encoded", "normal",
                {{"before", before},
                 {"after", provenance != namedShape.mappedNameProvenance.end()
                               ? provenance->second.rawMappedName
                               : mappedName},
                 {"elementType", currentElement.empty() ? "" : currentElement.substr(0U, 1U)},
                 {"entryLocalRefs", entryRefs},
                 {"inputTag", std::to_string(
                      provenance != namedShape.mappedNameProvenance.end()
                          ? provenance->second.sourceTag.value_or(0L)
                          : 0L)},
                 {"masterTag", std::to_string(namedShape.producerTag.value_or(0L))}},
            });
        }
        trace->record({
            "element_map.write",
            "begin",
            "set_element_name",
            {{"entryLocalRefs", entryRefs},
             {"masterTag", std::to_string(namedShape.producerTag.value_or(0L))},
             {"raw", provenance != namedShape.mappedNameProvenance.end()
                         ? provenance->second.rawMappedName
                         : mappedName},
             {"target", currentElement}},
        });
        checkpointNamedShapeLedger(
            namedShape,
            namedShape.owner + ":write",
            "element_map.write_checkpoint"
        );
    }
}

void producer_trace_detail::publishDuplicateCandidateSuppressed(
    app::ElementMapProducerTrace& trace,
    const std::string& slice,
    const std::string& reason,
    const DuplicateCandidateEvidence& evidence
)
{
    const auto candidateJson = [](const DuplicateCandidateValue& candidate) {
        nlohmann::json refs = nlohmann::json::array();
        for (const app::StringId& ref : candidate.entryLocalRefs) {
            refs.push_back({{"value", ref.value}, {"index", ref.index}});
        }
        return nlohmann::json {
            {"parent", candidate.parent},
            {"child", candidate.child},
            {"ordinal", candidate.ordinal},
            {"entryLocalRefs", std::move(refs)},
        };
    };
    trace.record({
        slice,
        "rejected",
        reason,
        {{"rawMappedName", evidence.rawMappedName},
         {"keptCandidate", candidateJson(evidence.kept)},
         {"suppressedCandidate", candidateJson(evidence.suppressed)}},
    });
}

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
        childSource.childElementMapPostfix = source.childElementMapPostfix;
        childSource.expandCompoundForBoolean = source.expandCompoundForBoolean;
        childSource.fuseCompoundForCut = source.fuseCompoundForCut;
        childSource.producerTag = source.producerTag;
        expandCompoundSource(childSource, expanded);
    }
}

void appendBooleanSource(const NamedShapeSource& source, std::vector<NamedShapeSource>& sources)
{
    if (source.expandCompoundForBoolean || source.shape.ShapeType() == TopAbs_COMPOUND) {
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
    // TopoShapeExpansion.cpp::TopoShape::makeElementBoolean() expands every Fuse compound and
    // every Cut tool compound before SetArguments()/SetTools(); only a Cut argument remains a
    // compound. That source order is later required by mapSubElement(shapes) child maps.
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

std::array<TopAbs_ShapeEnum, 3> makerMappedKinds()
{
    // TopoShape::makeShapeWithElementMap() collects MapperMaker candidates in the order that
    // controls NameKey and document StringHasher allocation: Vertex, Edge, Face.
    return {TopAbs_VERTEX, TopAbs_EDGE, TopAbs_FACE};
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/MappedElement.cpp
// ::Data::ElementNameComparator.  makeShapeWithElementMap() uses this ordering for the
// NameKey winner and for the reverse/forward U/L passes.  Lexical std::string ordering is not
// equivalent: it sorts "#16" before "#f", while FreeCAD first compares the count and value of
// the hexadecimal prefix so that later StringHasher entries do not become accidental producers.
int compareFreeCadMappedNames(const std::string& left, const std::string& right)
{
    const auto compareText = [](const std::string& lhs, const std::string& rhs, std::size_t start) {
        const std::size_t limit = std::min(lhs.size(), rhs.size());
        for (std::size_t index = start; index < limit; ++index) {
            if (lhs[index] != rhs[index]) {
                return lhs[index] < rhs[index] ? -1 : 1;
            }
        }
        if (lhs.size() == rhs.size()) {
            return 0;
        }
        return lhs.size() < rhs.size() ? -1 : 1;
    };
    const auto hexLength = [](const std::string& value) {
        if (value.empty() || value.front() != '#') {
            return std::size_t {0};
        }
        std::size_t index = 1;
        while (index < value.size()
               && std::isxdigit(static_cast<unsigned char>(value[index])) != 0) {
            ++index;
        }
        return index - 1U;
    };
    const auto hexValue = [](char value) {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        return value - 'A' + 10;
    };
    const auto compareIdentifiers = [&compareText](const std::string& lhs,
                                                    const std::string& rhs) {
        const std::size_t limit = std::min(lhs.size(), rhs.size());
        for (std::size_t index = 0; index < limit; ++index) {
            const bool leftDigit = std::isdigit(static_cast<unsigned char>(lhs[index])) != 0;
            const bool rightDigit = std::isdigit(static_cast<unsigned char>(rhs[index])) != 0;
            if (!leftDigit && !rightDigit) {
                if (lhs[index] != rhs[index]) {
                    return lhs[index] < rhs[index] ? -1 : 1;
                }
                continue;
            }
            if (leftDigit && rightDigit) {
                std::size_t leftEnd = index;
                while (leftEnd < lhs.size()
                       && std::isdigit(static_cast<unsigned char>(lhs[leftEnd])) != 0) {
                    ++leftEnd;
                }
                std::size_t rightEnd = index;
                while (rightEnd < rhs.size()
                       && std::isdigit(static_cast<unsigned char>(rhs[rightEnd])) != 0) {
                    ++rightEnd;
                }
                const std::size_t leftLength = leftEnd - index;
                const std::size_t rightLength = rightEnd - index;
                if (leftLength != rightLength) {
                    return leftLength < rightLength ? -1 : 1;
                }
                for (std::size_t digit = index; digit < leftEnd; ++digit) {
                    if (lhs[digit] != rhs[digit]) {
                        return lhs[digit] < rhs[digit] ? -1 : 1;
                    }
                }
                return compareText(lhs, rhs, leftEnd);
            }
            return leftDigit ? -1 : 1;
        }
        if (lhs.size() == rhs.size()) {
            return 0;
        }
        return lhs.size() < rhs.size() ? -1 : 1;
    };

    if (left.empty() || right.empty()) {
        return left.size() == right.size() ? 0 : (left.empty() ? -1 : 1);
    }
    if (left.front() == '#' || right.front() == '#') {
        const std::size_t leftLength = hexLength(left);
        const std::size_t rightLength = hexLength(right);
        if (leftLength == 0U || rightLength == 0U || leftLength != rightLength) {
            return leftLength < rightLength ? -1 : 1;
        }
        for (std::size_t index = 1; index <= leftLength; ++index) {
            const int leftValue = hexValue(left[index]);
            const int rightValue = hexValue(right[index]);
            if (leftValue != rightValue) {
                return leftValue < rightValue ? -1 : 1;
            }
        }
        return compareText(left, right, leftLength + 1U);
    }
    return compareIdentifiers(left, right);
}

struct FreeCadMappedNameLess
{
    bool operator()(const std::string& left, const std::string& right) const
    {
        return compareFreeCadMappedNames(left, right) < 0;
    }
};

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
    mixStableChildMapHash(hash, childMap.tag);
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
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() increments
    // `newShapeCounter` while it consumes Mapper::modified()/generated().  The ordinal is part
    // of the producer name (`:M2` / `:G2`) when one source yields several terminal targets; a
    // set of target names alone would overwrite those independent ElementMap entries.
    std::map<std::string, int> historyOrdinals;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() writes the immediate
    // `incomingShape.Tag` into NameKey. This is distinct from the terminal mapped-name Tag
    // used by ElementMap::encodeElementName() when an incoming Tag is zero. Keeping both is
    // required after a Boolean: base and tool aliases carry different terminal tags but are
    // candidates from the same Tag=0 Boolean result during the following Refine.
    std::optional<long> nameKeyTag;
    std::optional<long> sourceTag;
    std::optional<MappedNameProvenance> inheritedMappedName;
    std::optional<TopAbs_ShapeEnum> sourceKind;
    // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()
    // calls mapSubElement(shapes) before mapper history, after constructing ShapeInfo in
    // Vertex -> Edge -> Face order. The document StringHasher observes that order, so a map key
    // sorted by raw `#SID` text is not an equivalent traversal.
    std::size_t mapSubElementOrder = std::numeric_limits<std::size_t>::max();
    std::string preservedOperationPostfix;
    std::string sourceElement;
    // A nested child ElementMap is resolved by FreeCAD's addChildElements() before its parent
    // serializes the final mapped name.  Keep that lifecycle fact beside the source evidence so
    // the inherited raw name is retagged at the Part boundary rather than synthesized by runtime.
    bool composeInheritedChildMapTag = false;
    // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::mapSubElement() reaches
    // copyElementMap() only for a partner incoming TopoShape. A Fuse can retain raw ElementMap
    // refs for one partner input while other inputs still require the later M/G producer pass.
    bool partnerShape = false;
    bool rehashPreservedMappedName = false;
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

int makerSourceKindOrder(const std::optional<TopAbs_ShapeEnum>& kind)
{
    if (!kind) {
        return 3;
    }
    switch (*kind) {
        case TopAbs_VERTEX:
            return 0;
        case TopAbs_EDGE:
            return 1;
        case TopAbs_FACE:
            return 2;
        default:
            return 3;
    }
}

int producerStringIdIndex(const cad_core::app::StringHasher& hasher,
                          const std::string& mappedName,
                          const std::string& operationPostfix,
                          bool promoteBareSourceIdForGenerated)
{
    const std::size_t postfix = mappedName.find(';');
    const auto id = cad_core::app::parseStringId(mappedName.substr(0U, postfix));
    if (!id) {
        return 0;
    }
    // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()
    // gets both `first_name` and that one NameInfo's `sids` from
    // incomingShape.getMappedName(..., true, &sids).  StringHasher::getID() then observes a
    // `#id:index` MappedName only when that same entry owns a matching StringIDRef.  Do not
    // infer an index from the numeric SID or from another entry's relation.
    // Once StringHasher has recorded PrefixID for this producer SID, FreeCAD's next maker
    // carries the root prefix ID, not the current entry's rendered index.  This is how the
    // sequence `#f:2 -> #28:8` retains its Sketch source identity.
    if (const int prefixIndex = hasher.producerIndexForMappedNameId(*id); prefixIndex != 0) {
        return prefixIndex;
    }
    if (id->index != 0) {
        return id->index;
    }
    if (promoteBareSourceIdForGenerated && operationPostfix.rfind(";:G", 0U) == 0U) {
        return static_cast<int>(id->value);
    }
    // FreeCAD StringHasher only returns a non-zero StringIDRef index when the MappedName data
    // itself is indexed. `sids` are related producer evidence; a bare `#id` must not borrow an
    // endpoint index from that list. The entry-local refs still participate in hashElementName()
    // below, where PrefixID records their relation without rewriting the visible MappedName.
    return 0;
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
    (void)shape;
    // FreeCAD: TopoShape::Tag is assigned by TopoShape(long Tag, ...) or
    // PropertyPartShape::setValue(DocumentObject::getID()). A newly constructed maker result has
    // Tag==0. Geometry has no authority to invent that document identity: a fingerprint here
    // changes ElementMap::encodeElementName()'s early-return and StringHasher lifecycle.
    return std::nullopt;
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
    const std::string& producerOperation,
    int ordinal = 1
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
    // FreeCAD: TopoShapeExpansion.cpp::makeShapeWithElementMap() marks generated parallel
    // high-level expansion with `index == INT_MIN`, which encodes as `:G0`. CAD Core represents
    // that request-local high-level fallback with ordinal 0 instead of leaking an OCCT Solid or
    // Shell as a fake direct Face history source.
    if (ordinal == 0 && kind == ElementHistoryKind::Generated) {
        relationPostfix += "0";
    }
    else if (ordinal > 1) {
        relationPostfix += std::to_string(ordinal);
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
    if (!targets.nameKeyTag) {
        targets.nameKeyTag = requestLocalProducerTagForShapeImpl(sourceShape);
    }
    if (targets.preservedOperationPostfix.empty() && !preservedOperationPostfix.empty()) {
        targets.preservedOperationPostfix = preservedOperationPostfix;
    }
}

std::optional<MappedNameProvenance> resolveChildMappedNameProvenance(
    const NamedShape& namedShape,
    const std::string& localElementName,
    const std::string& rawMappedName,
    int depth = 0
)
{
    // FreeCAD: src/App/ElementMap.cpp::ElementMap::findAll() first reads direct refs and then
    // recurses through MappedChildElements, appending `child.postfix` without allocating a new
    // StringIDRef list. This resolver is the Part-side equivalent used by the next maker's
    // NameKey construction; it never derives a raw name from the response or from display text.
    if (depth > 16 || localElementName.empty() || rawMappedName.empty()) {
        return std::nullopt;
    }
    const auto direct = [&](const std::string& mappedName) -> std::optional<MappedNameProvenance> {
        const auto provenance = namedShape.mappedNameProvenance.find(mappedName);
        if (provenance == namedShape.mappedNameProvenance.end()
            || provenance->second.status != MappedNameProvenanceStatus::SourceBacked
            || provenance->second.rawMappedName != rawMappedName) {
            return std::nullopt;
        }
        return provenance->second;
    };
    if (const auto entries = namedShape.elementMapEntries.find(localElementName);
        entries != namedShape.elementMapEntries.end()) {
        for (const ElementMapEntry& entry : entries->second) {
            if (const auto resolved = direct(entry.mappedName)) {
                return resolved;
            }
        }
    }
    for (const auto& [mappedName, currentName] : namedShape.elementMap) {
        if (currentName != localElementName) {
            continue;
        }
        if (const auto resolved = direct(mappedName)) {
            return resolved;
        }
    }

    const auto sourceSubshape = parseSubshapeName(localElementName);
    if (!sourceSubshape) {
        return std::nullopt;
    }
    for (const NamedShapeChildMap& child : namedShape.childElementMaps) {
        if (child.recursiveExpansion || child.kind != subshapeKindName(sourceSubshape->kind)
            || child.sourceNamedShape == nullptr || child.count <= 0
            || sourceSubshape->index <= child.offset
            || sourceSubshape->index > child.offset + child.count
            || (!child.postfix.empty()
                && (rawMappedName.size() < child.postfix.size()
                    || rawMappedName.compare(rawMappedName.size() - child.postfix.size(),
                                             child.postfix.size(),
                                             child.postfix)
                        != 0))) {
            continue;
        }
        const auto childBase = parseSubshapeName(child.indexedName);
        if (!childBase || childBase->kind != sourceSubshape->kind) {
            continue;
        }
        const std::string childLocalName = prefixForKind(childBase->kind)
            + std::to_string(childBase->index + sourceSubshape->index - child.offset - 1);
        const std::string inheritedRaw = child.postfix.empty()
            ? rawMappedName
            : rawMappedName.substr(0U, rawMappedName.size() - child.postfix.size());
        auto inherited = resolveChildMappedNameProvenance(
            *child.sourceNamedShape, childLocalName, inheritedRaw, depth + 1
        );
        if (!inherited) {
            continue;
        }
        inherited->rawMappedName = rawMappedName;
        inherited->canonicalMappedName = cad_core::topo::canonicalizeFreeCadMappedName(rawMappedName);
        if (!child.postfix.empty()) {
            inherited->operationPostfix = child.postfix;
        }
        return inherited;
    }
    return std::nullopt;
}

std::optional<MappedNameProvenance> firstMappedNameProvenanceForElement(
    const NamedShape& namedShape,
    const std::string& localElementName,
    int depth = 0
)
{
    // FreeCAD: src/App/ElementMap.cpp::ElementMap::findAll() returns the direct
    // MappedNameRef list in insertion order, then follows a typed MappedChildElements range.
    // Link needs that producer-owned query when Body's PropertyPartShape retag has deliberately
    // removed its direct ElementMap.  This returns the first source-backed ref and its own
    // ElementIDRefs; it never attempts to recover a raw name from a response subname.
    if (depth > 16 || localElementName.empty()) {
        return std::nullopt;
    }
    const auto direct = [&](const std::string& mappedName) -> std::optional<MappedNameProvenance> {
        const auto provenance = namedShape.mappedNameProvenance.find(mappedName);
        if (provenance == namedShape.mappedNameProvenance.end()
            || provenance->second.status != MappedNameProvenanceStatus::SourceBacked
            || provenance->second.rawMappedName.empty()) {
            return std::nullopt;
        }
        return provenance->second;
    };
    if (const auto entries = namedShape.elementMapEntries.find(localElementName);
        entries != namedShape.elementMapEntries.end()) {
        for (const ElementMapEntry& entry : entries->second) {
            if (const auto provenance = direct(entry.mappedName)) {
                return provenance;
            }
        }
    }
    for (const auto& [mappedName, currentName] : namedShape.elementMap) {
        if (currentName != localElementName) {
            continue;
        }
        if (const auto provenance = direct(mappedName)) {
            return provenance;
        }
    }

    const auto sourceSubshape = parseSubshapeName(localElementName);
    if (!sourceSubshape) {
        return std::nullopt;
    }
    for (const NamedShapeChildMap& child : namedShape.childElementMaps) {
        if (child.recursiveExpansion || child.kind != subshapeKindName(sourceSubshape->kind)
            || child.sourceNamedShape == nullptr || child.count <= 0
            || sourceSubshape->index <= child.offset
            || sourceSubshape->index > child.offset + child.count) {
            continue;
        }
        const auto childBase = parseSubshapeName(child.indexedName);
        if (!childBase || childBase->kind != sourceSubshape->kind) {
            continue;
        }
        const std::string childLocalName = prefixForKind(childBase->kind)
            + std::to_string(childBase->index + sourceSubshape->index - child.offset - 1);
        auto provenance = firstMappedNameProvenanceForElement(
            *child.sourceNamedShape, childLocalName, depth + 1
        );
        if (!provenance) {
            continue;
        }
        if (!child.postfix.empty()) {
            provenance->rawMappedName += child.postfix;
            provenance->canonicalMappedName =
                cad_core::topo::canonicalizeFreeCadMappedName(provenance->rawMappedName);
            provenance->operationPostfix = child.postfix;
        }
        return provenance;
    }
    return std::nullopt;
}

void rememberSourceTargetEvidence(
    SourceTargets& targets,
    const NamedShapeSource& source,
    const std::string& sourceElement,
    const std::string& preservedOperationPostfix = {}
)
{
    rememberSourceTargetEvidence(targets, source.shape, sourceElement, preservedOperationPostfix);
    // FreeCAD: TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() writes
    // `key.tag = incomingShape.Tag` for every Modified/Generated candidate. This immediate
    // producer Tag is deliberately distinct from a recursively decoded mapped-name tag.
    if (source.producerTag) {
        targets.nameKeyTag = source.producerTag;
        if (*source.producerTag != 0L) {
            targets.sourceTag = source.producerTag;
        }
    }
    const bool hasPersistedProducerTag = !source.producerTag && source.namedShape != nullptr
        && source.namedShape->producerTag.has_value();
    if (hasPersistedProducerTag) {
        // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap()
        // stores incomingShape.Tag in NameKey. That current property tag is separate from the
        // terminal :H tag encoded in the source ElementMap.
        targets.nameKeyTag = source.namedShape->producerTag;
        if (*source.namedShape->producerTag != 0L) {
            targets.sourceTag = source.namedShape->producerTag;
        }
    }
    if (source.namedShape == nullptr) {
        return;
    }
    auto provenanceIt = source.namedShape->mappedNameProvenance.find(sourceElement);
    if (provenanceIt == source.namedShape->mappedNameProvenance.end()) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::mapSubElement() looks up the source's existing
        // ElementMap by local IndexedName before it records the next maker relation.  The
        // owner-qualified `Pad.EdgeN` is a resolver alias here; the raw StringID must come from
        // the already source-backed producer entry whether that producer is a Sketch, Pad, or
        // an earlier DressUp.
        auto inheritedSourceIt = source.namedShape->mappedNameProvenance.end();
        const std::string ownerPrefix = source.owner.empty() ? std::string {} : source.owner + ".";
        if (!ownerPrefix.empty() && sourceElement.rfind(ownerPrefix, 0U) == 0U) {
            inheritedSourceIt = source.namedShape->mappedNameProvenance.find(
                sourceElement.substr(ownerPrefix.size())
            );
        }
        if (inheritedSourceIt == source.namedShape->mappedNameProvenance.end()) {
            inheritedSourceIt =
                source.namedShape->mappedNameProvenance.find(localElementName(sourceElement));
        }
        if (inheritedSourceIt == source.namedShape->mappedNameProvenance.end()) {
            const std::string sourceLocalName = localElementName(sourceElement);
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
            // ::ElementMap::find(const IndexedName&, ElementIDRefs*) returns the mapped name
            // stored for the current IndexedName.  CAD Core keeps the inverse table as
            // `elementMap[mappedName] = currentElement`; recover the first source-backed entry
            // for this local element before a subsequent maker assigns M/G/U history.  Reading
            // only `Pad.EdgeN` as a key loses the actual `#id[:index]` source token.
            for (const auto& [mappedName, currentElement] : source.namedShape->elementMap) {
                if (currentElement != sourceLocalName) {
                    continue;
                }
                const auto candidate = source.namedShape->mappedNameProvenance.find(mappedName);
                if (candidate != source.namedShape->mappedNameProvenance.end()
                    && candidate->second.status == MappedNameProvenanceStatus::SourceBacked) {
                    inheritedSourceIt = candidate;
                    break;
                }
            }
        }
        if (inheritedSourceIt != source.namedShape->mappedNameProvenance.end()
            && inheritedSourceIt->second.status == MappedNameProvenanceStatus::SourceBacked) {
            provenanceIt = inheritedSourceIt;
        }
    }
    std::optional<MappedNameProvenance> childInherited;
    if (provenanceIt == source.namedShape->mappedNameProvenance.end()) {
        std::string rawSourceName = sourceElement;
        const std::string ownerPrefix = source.owner.empty() ? std::string {} : source.owner + ".";
        if (!ownerPrefix.empty() && rawSourceName.rfind(ownerPrefix, 0U) == 0U) {
            rawSourceName.erase(0U, ownerPrefix.size());
        }
        // sourceElementNames() returns ElementMap's raw MappedName (optionally owner-qualified),
        // not the IndexedName used to query findAll(). Locate that typed IndexedName from the
        // request-local source ledger before following a child range; do not parse a display
        // subname or invent a geometry correspondence here.
        for (const auto& [indexedName, element] : source.namedShape->elements) {
            (void)element;
            childInherited = resolveChildMappedNameProvenance(
                *source.namedShape, indexedName, rawSourceName
            );
            if (childInherited) {
                break;
            }
        }
        if (!childInherited) {
            return;
        }
    }
    const MappedNameProvenance& inherited = childInherited ? *childInherited : provenanceIt->second;
    if (inherited.status != MappedNameProvenanceStatus::SourceBacked) {
        return;
    }
    if (!inherited.sourceElement.empty()) {
        targets.sourceElement = inherited.sourceElement;
    }
    const bool hasNonzeroIncomingTag = (source.producerTag && *source.producerTag != 0L)
        || (hasPersistedProducerTag && *source.namedShape->producerTag != 0L);
    if (!hasNonzeroIncomingTag && inherited.sourceTag) {
        // A regular source already carries the producer-side ElementMap tag encoded with its
        // mapped name.  Keep that document StringHasher evidence intact; only an explicitly
        // local source (FeatureExtrude's negative prism Tag) replaces it for this maker call.
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
    provenance.producerTag = namedShape.producerTag;
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
        || provenance.rawMappedName.empty() || provenance.canonicalMappedName.empty()) {
        return false;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::mapSubElement() copies every incoming ElementMap before
    // MapperMaker contributes generated/modified relations.  Keep that producer-written raw
    // evidence across preserved/refine passes while retargeting only the current result element;
    // rebuilding from `Body.EdgeN` or another display alias loses the StringID provenance.
    provenance.entryKey = entryKey;
    provenance.currentElement = currentElement;
    provenance.elementType = mappedNameElementType(currentElement);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap() consumes the Sketch producer map at this Part
    // maker boundary.  The resulting owner map is public evidence; only the raw Sketch
    // producer itself remains ProducerOnly.
    provenance.publicationScope = MappedNamePublicationScope::Public;
    if (targets.composeInheritedChildMapTag && !provenance.elementType.empty()) {
        const std::optional<long> parentTag = namedShape.producerTag;
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
    recordElementMapEntry(namedShape, entryKey, currentElement, true);
    return true;
}

std::optional<long> terminalMappedNameTag(const std::string& mappedName)
{
    const std::size_t marker = mappedName.rfind(";:H");
    if (marker == std::string::npos) {
        return std::nullopt;
    }
    std::size_t cursor = marker + 3U;
    bool negative = false;
    if (cursor < mappedName.size() && mappedName[cursor] == '-') {
        negative = true;
        ++cursor;
    }
    const std::size_t begin = cursor;
    while (cursor < mappedName.size()
           && std::isxdigit(static_cast<unsigned char>(mappedName[cursor])) != 0) {
        ++cursor;
    }
    if (cursor == begin) {
        return std::nullopt;
    }
    long tag = 0;
    std::istringstream input(mappedName.substr(begin, cursor - begin));
    input >> std::hex >> tag;
    if (!input || tag == 0) {
        return std::nullopt;
    }
    return negative ? -tag : tag;
}

std::string appendMappedNameTag(const std::string& mappedName,
                                long tag,
                                const std::string& elementType)
{
    if (mappedName.empty() || tag == 0 || elementType.empty()) {
        return mappedName;
    }
    if (const std::optional<long> terminal = terminalMappedNameTag(mappedName); terminal
        && *terminal == tag) {
        return mappedName;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::encodeElementName() takes the preserved mapSubElement path with no maker
    // postfix.  It keeps the incoming raw MappedName and appends only POSTFIX_TAG when the
    // incoming TopoShape Tag differs from the name's terminal tag.  Do not rehash that raw
    // name: hashElementName() belongs to the later M/G relation path.
    std::ostringstream output;
    output << mappedName << ";:H" << std::hex;
    if (tag < 0) {
        output << '-' << -tag;
    }
    else {
        output << tag;
    }
    output << ',' << elementType.front();
    return output.str();
}

std::string appendPropertyRetagMappedName(const std::string& mappedName,
                                          long propertyTag,
                                          const std::string& elementType)
{
    if (mappedName.empty() || propertyTag == 0 || elementType.empty()) {
        return mappedName;
    }
    if (const std::optional<long> terminal = terminalMappedNameTag(mappedName); terminal
        && *terminal == propertyTag) {
        return mappedName;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::encodeElementName() records the prior mapped postfix length when a
    // property retag appends its own `;:H<tag>:<len>,<type>` segment.  The length is derived
    // from the source-backed terminal tag segment, never from a display/stable subname.
    const std::size_t previousTag = mappedName.rfind(";:H");
    const std::size_t previousPostfixLength = previousTag == std::string::npos
        ? 0U
        : mappedName.size() - previousTag;
    std::ostringstream output;
    output << mappedName << ";:H" << std::hex;
    if (propertyTag < 0) {
        output << '-' << -propertyTag;
    }
    else {
        output << propertyTag;
    }
    if (previousPostfixLength != 0U) {
        output << ':' << previousPostfixLength;
    }
    output << ',' << elementType.front();
    return output.str();
}

std::string mappedNameWithoutTerminalTag(const std::string& mappedName)
{
    const std::size_t tag = mappedName.rfind(";:H");
    if (tag == std::string::npos) {
        return mappedName;
    }
    const std::size_t type = mappedName.rfind(',');
    if (type == std::string::npos || type <= tag + 3U || type + 1U >= mappedName.size()) {
        return mappedName;
    }
    return mappedName.substr(0U, tag);
}

bool recordPreservedMappedNameProvenance(NamedShape& namedShape,
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
        || provenance.rawMappedName.empty()) {
        return false;
    }
    provenance.currentElement = currentElement;
    provenance.elementType = mappedNameElementType(currentElement);
    provenance.publicationScope = MappedNamePublicationScope::Public;
    if (targets.sourceTag) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::mapSubElement() takes the partner/copyElementMap
        // path before the mapper M/G pass. A retained ElementMap entry keeps its original
        // producer chain and gains the receiving TopoShape Tag; it is not rebuilt from the
        // current EdgeN/FaceN or replaced by a response alias.
        provenance.rawMappedName = appendMappedNameTag(
            provenance.rawMappedName, *targets.sourceTag, provenance.elementType
        );
        provenance.sourceTag = targets.sourceTag;
    }
    provenance.canonicalMappedName =
        cad_core::topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
    const std::string producedKey = provenance.canonicalMappedName.empty()
        ? provenance.rawMappedName
        : provenance.canonicalMappedName;
    if (producedKey.empty()) {
        return false;
    }
    provenance.entryKey = producedKey;
    if (namedShape.stringHasher) {
        const std::size_t postfix = provenance.rawMappedName.find(';');
        const auto primary = cad_core::app::parseStringId(
            provenance.rawMappedName.substr(0U, postfix)
        );
        namedShape.stringHasher->rememberMappedName(
            provenance.rawMappedName,
            primary.value_or(cad_core::app::StringId {}),
            provenance.elementIdRefs
        );
    }
    namedShape.elementMap[producedKey] = currentElement;
    namedShape.mappedNameProvenance[producedKey] = std::move(provenance);
    recordElementMapEntry(namedShape, producedKey, currentElement);
    return true;
}

const MappedNameProvenance* selectedSourceBackedMappedNameProvenance(
    const NamedShape& namedShape,
    const std::string& elementName
);

bool recordProducerMappedNameProvenance(NamedShape& namedShape,
                                        const std::string& entryKey,
                                        const std::string& currentElement,
                                        const SourceTargets& targets,
                                        const std::string& operationPostfix,
                                        const std::vector<cad_core::app::StringId>& relatedRefs = {},
                                        bool promoteBareSourceIdForGenerated = false,
                                        bool delayedHighLevel = false)
{
    if (!targets.inheritedMappedName || entryKey.empty() || currentElement.empty()
        || namedShape.elements.count(currentElement) == 0U) {
        return false;
    }
    const MappedNameProvenance& originalInherited = *targets.inheritedMappedName;
    if (originalInherited.status != MappedNameProvenanceStatus::SourceBacked
        || originalInherited.rawMappedName.empty()) {
        return false;
    }

    const MappedNameProvenance* inherited = &originalInherited;

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() first reads the incoming
    // ElementMap name and ElementIDRefs, then encodes the result with this producer's op code.
    // A preserved/refine pass may retain the full source name, but a maker result must start from
    // the source StringID token and add its own M/G/U relation. Reusing Pad's full `;XTR` name
    // here would claim that the Chamfer or Fillet was still produced by Pad.
    const std::string& sourceMappedName = inherited->rawMappedName;
    const std::size_t postfix = sourceMappedName.find(';');
    std::string sourceId = sourceMappedName.substr(0U, postfix);
    std::vector<cad_core::app::StringId> sourceRefs;
    if (sourceId.empty()) {
        return false;
    }
    if (namedShape.stringHasher) {
        sourceRefs = inherited->elementIdRefs;
        if (sourceRefs.empty()) {
            sourceRefs = namedShape.stringHasher->relatedIdsForMappedName(inherited->rawMappedName);
        }
        // FreeCAD: TopoShapeExpansion.cpp::makeShapeWithElementMap() initializes `sids` from
        // the selected NameInfo, appends every additional NameInfo's ElementIDRefs, then adds
        // the StringID for the parenthesized multi-source tuple before encodeElementName().
        // These refs must be present before hashElementName() creates the selected producer SID;
        // attaching them afterwards changes the next maker's StringHasher relation graph.
        for (const cad_core::app::StringId& related : relatedRefs) {
            if (related) {
                sourceRefs.push_back(related);
            }
        }
        if (sourceMappedName.front() == '#' && postfix != std::string::npos) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
            // ::ElementMap::encodeElementName() invokes hashElementName() before it appends the
            // next producer tag whenever the incoming mapped name already has an ElementMap
            // postfix.  The input is the complete prior producer name plus its ElementIDRefs;
            // using only its leading `#id` loses the `#id:;postfix` StringHasher entry and makes
            // Chamfer/Fillet reuse Pad's producer identity.
            const cad_core::app::HashedMappedName hashed = namedShape.stringHasher->hashMappedName(
                sourceMappedName.substr(0U, postfix),
                producerStringIdIndex(
                    *namedShape.stringHasher,
                    sourceMappedName,
                    operationPostfix,
                    promoteBareSourceIdForGenerated
                ),
                sourceMappedName.substr(postfix),
                sourceRefs
            );
            sourceId = hashed.id.toString();
            sourceRefs = hashed.elementRefs;
        }
        else if (const auto materialized = namedShape.stringHasher->mappedNameId(
                     sourceMappedName
                 )) {
            // Sketch keeps raw g<ID>;SKT provenance publicly, but its ElementMap has already
            // materialized this private StringIDRef in SketchObject::buildShape(). A first Pad
            // consumption begins from that ID, not from a newly parsed display token.
            sourceId = materialized->toString();
        }
    }
    if (!cad_core::app::parseStringId(sourceId) && namedShape.stringHasher) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp
        // ::StringHasher::getID(const Data::MappedName&, const QVector<StringIDRef>&) interns a
        // source name before a Part producer appends its own operation. Sketch g<ID> tokens are
        // source evidence, not the final Part raw base; promote them through the document table
        // at this producer boundary instead of emitting an unhashable g<ID>;XTR name.
        std::string idData = sourceId;
        int idIndex = 0;
        int displayedIndex = 0;
        if (sourceId.size() > 1U && sourceId.front() == 'g') {
            const std::string geometryIndex = sourceId.substr(1U);
            if (!geometryIndex.empty()
                && std::all_of(
                    geometryIndex.begin(), geometryIndex.end(), [](unsigned char value) {
                        return std::isdigit(value) != 0;
                    }
                )) {
                try {
                    // FreeCAD StringHasher recognises an IndexedName and stores its common
                    // prefix once. All g1..gN sketch geometry tokens therefore share one ID
                    // with individual StringIDRef indices rather than receiving N unrelated
                    // document slots.
                    idData = "g";
                    idIndex = std::stoi(geometryIndex);
                }
                catch (const std::exception&) {
                    idData = sourceId;
                    idIndex = 0;
                }
            }
            else {
                // Sketch endpoint names are g<geometryId>v<point>. FreeCAD keeps the complete
                // endpoint token in the StringID data but returns the point ordinal through the
                // StringIDRef index (for example g1v1;SKT -> #2:1). This is distinct from the
                // g<geometryId> indexed-name relation above.
                const std::size_t vertexMarker = sourceId.rfind('v');
                if (vertexMarker != std::string::npos && vertexMarker > 1U
                    && vertexMarker + 1U < sourceId.size()) {
                    const std::string pointIndex = sourceId.substr(vertexMarker + 1U);
                    const std::string geometryPart = sourceId.substr(1U, vertexMarker - 1U);
                    if (!geometryPart.empty() && !pointIndex.empty()
                        && std::all_of(
                            geometryPart.begin(), geometryPart.end(), [](unsigned char value) {
                                return std::isdigit(value) != 0;
                            }
                        )
                        && std::all_of(
                            pointIndex.begin(), pointIndex.end(), [](unsigned char value) {
                                return std::isdigit(value) != 0;
                            }
                        )) {
                        try {
                            displayedIndex = std::stoi(pointIndex);
                        }
                        catch (const std::exception&) {
                            displayedIndex = 0;
                        }
                    }
                }
            }
        }
        cad_core::app::StringId sourceStringId = namedShape.stringHasher->getMappedNameId(
            idData,
            idIndex,
            postfix == std::string::npos ? std::string {} : sourceMappedName.substr(postfix)
        );
        if (displayedIndex != 0) {
            sourceStringId.index = displayedIndex;
        }
        sourceId = sourceStringId.toString();
        if (sourceRefs.empty()) {
            sourceRefs.push_back(sourceStringId);
        }
    }
    MappedNameProvenance provenance;
    provenance.entryKey = entryKey;
    provenance.currentElement = currentElement;
    provenance.sourceElement = sourceId;
    provenance.encodeInputMappedName = sourceMappedName;
    provenance.elementType = mappedNameElementType(currentElement);
    provenance.producerTag = namedShape.producerTag;
    provenance.masterTag = provenance.producerTag;
    // FreeCAD: src/App/ElementMap.cpp::ElementMap::encodeElementName() receives the current
    // result TopoShape Tag as `masterTag`, but the final `:H...` segment is its `tag` argument
    // from the incoming source shape.  Keeping those two values distinct is what lets a
    // FaceMaker map and Pad's later mapper lookup reuse the same StringID entry.
    provenance.sourceTag = targets.sourceTag ? targets.sourceTag : provenance.producerTag;
    provenance.operationPostfix = operationPostfix;
    provenance.delayedHighLevel = delayedHighLevel;
    provenance.status = MappedNameProvenanceStatus::IndexedOnly;
    provenance = cad_core::topo::encodedMappedNameProvenance(std::move(provenance));
    if (provenance.status != MappedNameProvenanceStatus::SourceBacked) {
        return false;
    }
    // ElementMap::setElementName() stores the encoded mapped name, not the source IndexedName,
    // as its key. A source can be both preserved and generate/modify several distinct targets;
    // using `SketchPad.g1` for each write silently overwrote the preserved entry with the final
    // generated one. The final canonical mapped name is the request-local key used by resolver
    // and publisher, while sourceElement below remains the provenance link to the original map.
    const std::string producedKey = provenance.canonicalMappedName.empty()
        ? provenance.rawMappedName
        : provenance.canonicalMappedName;
    if (producedKey.empty()) {
        return false;
    }
    provenance.entryKey = producedKey;
    provenance.elementIdRefs = sourceRefs;
    if (namedShape.stringHasher) {
        cad_core::app::StringId primaryId;
        if (const auto source = cad_core::app::parseStringId(sourceId)) {
            primaryId = *source;
        }
        if (sourceRefs.empty() && primaryId) {
            sourceRefs.push_back(primaryId);
        }
        namedShape.stringHasher->rememberMappedName(
            provenance.rawMappedName,
            primaryId,
            std::move(sourceRefs)
        );
    }
    namedShape.elementMap[producedKey] = currentElement;
    namedShape.mappedNameProvenance[producedKey] = std::move(provenance);
    recordElementMapEntry(namedShape, producedKey, currentElement);
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
    TopAbs_ShapeEnum kind,
    bool allowGeometricVertexFallback = false,
    bool honorLogicalBindings = true
)
{
    const std::string prefix = prefixForKind(kind);
    if (prefix.empty()) {
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap() queries the producer ElementMap's IndexedName,
    // rather than re-enumerating the OCCT result.  A producer can therefore intentionally bind
    // its logical FaceN/EdgeN order to a different physical BRep index.  Honor that binding while
    // consuming maker history; the raw TopExp enumeration below remains a fallback for shapes
    // that have not published an ElementMap binding yet.
    if (honorLogicalBindings) {
        for (const auto& [elementName, element] : namedShape.elements) {
            const auto parsed = parseSubshapeName(localElementName(elementName));
            if (!parsed || parsed->kind != kind) {
                continue;
            }
            const auto physicalShape = subshapeByName(namedShape.shape, element.subshape);
            if (physicalShape && physicalShape->IsSame(shape)) {
                return localElementName(elementName);
            }
        }
    }

    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(namedShape.shape, kind, shapes);
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return prefix + std::to_string(index);
        }
    }

    if (allowGeometricVertexFallback && kind == TopAbs_VERTEX && shape.ShapeType() == TopAbs_VERTEX) {
        // BRepPrim can replace a source vertex with an equivalent result vertex even while its
        // incident source edge remains IsSame. FreeCAD's mapSubElement still carries that
        // source name; recover only a unique geometric vertex match, keeping ambiguous coincident
        // points unnamed rather than inventing an ownership mapping from display order.
        const gp_Pnt sourcePoint = BRep_Tool::Pnt(TopoDS::Vertex(shape));
        std::optional<std::string> geometricMatch;
        for (int index = 1; index <= shapes.Extent(); ++index) {
            if (shapes(index).ShapeType() != TopAbs_VERTEX) {
                continue;
            }
            const gp_Pnt candidatePoint = BRep_Tool::Pnt(TopoDS::Vertex(shapes(index)));
            if (sourcePoint.SquareDistance(candidatePoint) > Precision::SquareConfusion()) {
                continue;
            }
            const std::string candidate = prefix + std::to_string(index);
            if (geometricMatch && *geometricMatch != candidate) {
                return std::nullopt;
            }
            geometricMatch = candidate;
        }
        return geometricMatch;
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
    std::map<std::string, SourceTargets>& sourceTargets,
    int ordinal = 1
)
{
    bool applied = false;
    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const auto elementName = findElementName(namedShape, historyShape, kind, true);
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
        sourceTargets[sourceName].historyOrdinals[*elementName] = ordinal;
        applied = true;
    }
    if (applied) {
        return true;
    }

    const auto sourceTargetsIt = sourceTargets.find(sourceName);
    const std::optional<TopAbs_ShapeEnum> sourceKind = sourceTargetsIt == sourceTargets.end()
        ? std::optional<TopAbs_ShapeEnum> {}
        : sourceTargetsIt->second.sourceKind;
    if (!sourceKind || historyShape.ShapeType() == *sourceKind) {
        return false;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() expands a generated Shell or
    // Solid through `checkForParallelOrCoplanar()` when the maker reports a higher-level shape.
    // Only expand to the source level (Face -> Face, Edge -> Edge, Vertex -> Vertex); flattening
    // every descendant would manufacture unrelated source relations.
    TopTools_IndexedMapOfShape expanded;
    TopExp::MapShapes(historyShape, *sourceKind, expanded);
    bool expandedApplied = false;
    for (int index = 1; index <= expanded.Extent(); ++index) {
        expandedApplied = applyHistoryShape(
                              namedShape,
                              sourceName,
                              expanded(index),
                              historyKind,
                              sourceTargets,
                              historyKind == ElementHistoryKind::Generated ? 0 : ordinal
                          )
            || expandedApplied;
    }
    return expandedApplied;
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
    int ordinal = 0;
    for (TopTools_ListIteratorOfListOfShape it(historyShapes); it.More(); it.Next()) {
        applied = applyHistoryShape(
                      namedShape,
                      sourceName,
                      it.Value(),
                      historyKind,
                      sourceTargets,
                      ++ordinal
                  )
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

enum class SourceElementMapLookup
{
    All,
    First,
};

std::vector<std::string> sourceElementNames(
    const NamedShapeSource& source,
    const std::string& localElementName,
    SourceElementMapLookup lookup = SourceElementMapLookup::All,
    bool recordTrace = true,
    bool publishFirstRefs = true
)
{
    app::ElementMapProducerTrace* trace = source.namedShape != nullptr
            && source.namedShape->stringHasher
        ? source.namedShape->stringHasher->producerTrace()
        : nullptr;
    const auto recordLookup = [&](const std::vector<std::string>& values,
                                  const std::string& reason,
                                  bool childFallback) {
        if (trace == nullptr || !recordTrace) {
            return;
        }
        std::string orderedEntries;
        std::string firstRaw;
        std::string firstRefs;
        if (source.namedShape != nullptr) {
            const auto found = source.namedShape->elementMapEntries.find(localElementName);
            if (found != source.namedShape->elementMapEntries.end()) {
                for (const ElementMapEntry& entry : found->second) {
                    const auto provenance = source.namedShape->mappedNameProvenance.find(
                        entry.mappedName
                    );
                    if (!orderedEntries.empty()) {
                        orderedEntries += '|';
                    }
                    orderedEntries += provenance != source.namedShape->mappedNameProvenance.end()
                        ? provenance->second.rawMappedName
                        : entry.mappedName;
                    if (firstRaw.empty()) {
                        firstRaw = provenance != source.namedShape->mappedNameProvenance.end()
                            ? provenance->second.rawMappedName
                            : entry.mappedName;
                    }
                    orderedEntries += '[';
                    bool firstRef = true;
                    for (const app::StringId& ref : entry.elementIdRefs) {
                        if (!firstRef) {
                            orderedEntries += ',';
                        }
                        firstRef = false;
                        orderedEntries += ref.toString();
                        if (firstRaw == (provenance != source.namedShape->mappedNameProvenance.end()
                                            ? provenance->second.rawMappedName
                                            : entry.mappedName)) {
                            if (!firstRefs.empty()) {
                                firstRefs += ',';
                            }
                            firstRefs += ref.toString();
                        }
                    }
                    orderedEntries += ']';
                }
            }
        }
        if (lookup == SourceElementMapLookup::First) {
            if (firstRaw.empty() && childFallback && source.namedShape != nullptr) {
                if (const auto provenance = firstMappedNameProvenanceForElement(
                        *source.namedShape, localElementName
                    )) {
                    firstRaw = provenance->rawMappedName;
                    for (const app::StringId& ref : provenance->elementIdRefs) {
                        if (!firstRefs.empty()) firstRefs += ',';
                        firstRefs += ref.toString();
                    }
                }
            }
            if (!publishFirstRefs) {
                firstRefs.clear();
            }
            trace->record({
                "element_map.find",
                values.empty() ? "miss" : "hit",
                values.empty() ? reason : (childFallback ? "child_map_fallback" : "first_entry"),
                {{"indexed", localElementName},
                 {"raw", firstRaw},
                 {"entryLocalRefs", firstRefs}},
            });
        }
        else {
            if (orderedEntries.empty() && childFallback) {
                const auto inherited = source.namedShape != nullptr
                    ? firstMappedNameProvenanceForElement(
                          *source.namedShape, localElementName
                      )
                    : std::optional<MappedNameProvenance> {};
                for (std::string raw : values) {
                    const std::string ownerPrefix = source.owner.empty()
                        ? std::string {}
                        : source.owner + ".";
                    if (!ownerPrefix.empty() && raw.rfind(ownerPrefix, 0U) == 0U) {
                        raw.erase(0U, ownerPrefix.size());
                    }
                    if (!orderedEntries.empty()) orderedEntries += '|';
                    orderedEntries += raw + '[';
                    if (inherited && inherited->rawMappedName == raw) {
                        bool first = true;
                        for (const app::StringId& ref : inherited->elementIdRefs) {
                            if (!first) orderedEntries += ',';
                            first = false;
                            orderedEntries += ref.toString();
                        }
                    }
                    orderedEntries += ']';
                }
            }
            trace->record({
                "element_map.find_all",
                values.empty() ? "miss" : "hit",
                orderedEntries.empty() ? reason : "entry_local_refs",
                {{"indexed", localElementName}, {"orderedEntries", orderedEntries}},
            });
        }
        (void)childFallback;
    };
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
        recordLookup(names, "indexed_owner_fallback", false);
        return names;
    }

    std::string sourceLocalName = localElementName;
    if (const auto parsed = parseSubshapeName(localElementName)) {
        for (const auto& [logicalName, element] : source.namedShape->elements) {
            if (element.subshape.kind == parsed->kind && element.subshape.index == parsed->index) {
                sourceLocalName = logicalName;
                break;
            }
        }
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::findAll() returns mapped producer names for an IndexedName; it does not
    // append an object-local FaceN/EdgeN fallback once the source already owns a mapping.  The
    // fallback is only for source geometry that has never entered an ElementMap.  Keeping it
    // beside a real producer name makes a later maker manufacture two unrelated lineages for
    // the same source target.
    std::vector<std::string> mappedNames;
    const auto addMappedName = [&](const std::string& mappedName) {
        if (mappedName.empty()) {
            return;
        }
        const auto mapped = source.namedShape->elementMap.find(mappedName);
        if (mapped == source.namedShape->elementMap.end() || mapped->second != sourceLocalName
            || mappedName == sourceLocalName) {
            return;
        }
        const auto provenance = source.namedShape->mappedNameProvenance.find(mappedName);
        if (provenance == source.namedShape->mappedNameProvenance.end()
            || provenance->second.status != MappedNameProvenanceStatus::SourceBacked) {
            return;
        }
        addDistinctString(mappedNames, mappedName);
    };

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::findAll() walks the
    // MappedNameRef linked list in insertion order.  That list is the preserve/mapSubElement
    // input; it cannot be reconstructed by sorting the inverse mapped-name lookup index.
    const auto entries = source.namedShape->elementMapEntries.find(sourceLocalName);
    if (entries != source.namedShape->elementMapEntries.end()) {
        for (const ElementMapEntry& entry : entries->second) {
            addMappedName(entry.mappedName);
        }
    }
    // Some older producer paths may not yet have emitted an entry ledger. Retain their existing
    // source-backed map rather than silently dropping an input. New Part writers always enter
    // through the ordered path above.
    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (currentName != sourceLocalName || stableName == sourceLocalName) {
            continue;
        }
        addMappedName(stableName);
    }

    if (mappedNames.empty()) {
        // FreeCAD: src/App/ElementMap.cpp::ElementMap::findAll(), when the direct ref list is
        // empty, resolves MappedChildElements by its typed range and appends child.postfix to
        // every returned MappedName. copyElementMap() deliberately leaves the receiver with
        // those ranges rather than flattening a transformed source map.
        const auto sourceSubshape = parseSubshapeName(sourceLocalName);
        if (sourceSubshape) {
            for (const NamedShapeChildMap& child : source.namedShape->childElementMaps) {
                if (child.recursiveExpansion || child.kind != subshapeKindName(sourceSubshape->kind)
                    || child.sourceNamedShape == nullptr || child.count <= 0
                    || sourceSubshape->index <= child.offset
                    || sourceSubshape->index > child.offset + child.count) {
                    continue;
                }
                const auto childBase = parseSubshapeName(child.indexedName);
                if (!childBase || childBase->kind != sourceSubshape->kind) {
                    continue;
                }
                const std::string childLocalName = prefixForKind(childBase->kind)
                    + std::to_string(childBase->index + sourceSubshape->index - child.offset - 1);
                for (const std::string& childName : sourceElementNames(
                         NamedShapeSource {child.sourceOwner,
                                           child.sourceNamedShape->shape,
                                           child.sourceNamedShape},
                         childLocalName,
                         lookup,
                         true,
                         publishFirstRefs
                     )) {
                    std::string rawChildName = childName;
                    const std::string childOwnerPrefix = child.sourceOwner.empty()
                        ? std::string {}
                        : child.sourceOwner + ".";
                    if (!childOwnerPrefix.empty()
                        && rawChildName.rfind(childOwnerPrefix, 0U) == 0U) {
                        rawChildName.erase(0U, childOwnerPrefix.size());
                    }
                    // sourceElementNames() may use an owner-qualified resolver key internally
                    // (for example `SketchPad.#8`) to distinguish NameKeys with matching
                    // canonical forms. ElementMap::findAll(), however, returns the producer's
                    // MappedName bytes. Recover those bytes from the entry-local provenance
                    // before this child range becomes the input to another Part maker.
                    if (const auto provenance = child.sourceNamedShape->mappedNameProvenance.find(
                                 rawChildName
                             ); provenance != child.sourceNamedShape->mappedNameProvenance.end()
                             && provenance->second.status == MappedNameProvenanceStatus::SourceBacked
                             && !provenance->second.rawMappedName.empty()) {
                        rawChildName = provenance->second.rawMappedName;
                    }
                    else if (const auto provenance = firstMappedNameProvenanceForElement(
                                 *child.sourceNamedShape, childLocalName
                             )) {
                        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                        // ::ElementMap::findAll() recursively returns the nested producer's raw
                        // MappedName. A canonical resolver key containing `:H*` is never raw
                        // child-map payload and must be resolved through the typed child range.
                        rawChildName = provenance->rawMappedName;
                    }
                    addDistinctString(mappedNames, rawChildName + child.postfix);
                    if (lookup == SourceElementMapLookup::First && !mappedNames.empty()) {
                        break;
                    }
                }
                if (lookup == SourceElementMapLookup::First && !mappedNames.empty()) {
                    break;
                }
            }
        }
    }

    if (mappedNames.empty()) {
        names.push_back(source.owner + "." + sourceLocalName);
        for (const std::string& aliasOwner : source.ownerAliases) {
            if (!aliasOwner.empty()) {
                addDistinctString(names, aliasOwner + "." + sourceLocalName);
            }
        }
    }

    for (const std::string& stableName : mappedNames) {
        std::string sourceName = stableName;
        if (!source.owner.empty()) {
            const std::string ownerPrefix = source.owner + ".";
            if (sourceName.rfind(ownerPrefix, 0U) != 0U) {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                // TopoShapeExpansion.cpp::NameKey stores both the mapped name and its source
                // TopoShape Tag.  The public canonical form intentionally normalizes `:H<tag>`
                // to `:H*`, so Pad and a Pocket tool can otherwise share a canonical #SID while
                // being distinct NameKey sources. Keep the owner only in CAD Core's request-local
                // source-target key; recordProducerMappedNameProvenance() still publishes the
                // raw name carried by the producer ledger, never this resolver key.
                sourceName = ownerPrefix + sourceName;
            }
        }
        if (std::find(names.begin(), names.end(), sourceName) == names.end()) {
            names.push_back(std::move(sourceName));
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
            // ::ElementMap::findAll() preserves the complete MappedNameRef chain. Shared Sketch
            // vertices therefore carry both endpoint aliases through FaceMaker; only the
            // explicit find-first API is allowed to stop at the first entry.
            if (lookup == SourceElementMapLookup::First) {
                break;
            }
        }
    }
    recordLookup(
        names,
        mappedNames.empty() ? "indexed_owner_fallback" : "ordered_entry_chain",
        entries == source.namedShape->elementMapEntries.end() && !source.namedShape->childElementMaps.empty()
    );
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
    // FreeCAD: TopoShapeExpansion.cpp::TopoShape::mapSubElement() calls
    // `shapeMap.find(_Shape, otherMap.find(other._Shape, k))`; it is an ancestry/TopoDS identity
    // lookup, not a geometric-point recovery. A unique coincident vertex may help decode a
    // maker history, but treating it as preserved here hides the later Modified/Generated map.
    const auto elementName = findElementName(namedShape, sourceElement, kind, false, false);
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

bool directCompoundChildrenPartnerSources(
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
)
{
    if (resultShape.IsNull()
        || (resultShape.ShapeType() != TopAbs_COMPOUND && resultShape.ShapeType() != TopAbs_COMPSOLID)
        || sources.empty()) {
        return false;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::countSubShapes(TopAbs_SHAPE) counts direct children with `TopoDS_Iterator`;
    // TopoShapeExpansion.cpp::TopoShape::mapSubElement(vector) then compares
    // `getSubShape(TopAbs_SHAPE, ++count, true).IsPartner(s._Shape)` before publishing child maps.
    TopoDS_Iterator child(resultShape);
    for (const NamedShapeSource& source : sources) {
        if (source.shape.IsNull() || !child.More()) {
            return false;
        }
        if (!child.Value().IsPartner(source.shape)) {
            return false;
        }
        child.Next();
    }
    return !child.More();
}

TopoDS_Shape sourceOrderedPartnerCompound(const TopoDS_Shape& resultShape,
                                          const std::vector<NamedShapeSource>& sources)
{
    if (resultShape.IsNull()
        || (resultShape.ShapeType() != TopAbs_COMPOUND && resultShape.ShapeType() != TopAbs_COMPSOLID)
        || sources.empty()
        || directCompoundChildrenPartnerSources(resultShape, sources)) {
        return resultShape;
    }

    std::vector<TopoDS_Shape> resultChildren;
    for (TopoDS_Iterator childIt(resultShape); childIt.More(); childIt.Next()) {
        resultChildren.push_back(childIt.Value());
    }
    if (resultChildren.size() != sources.size()) {
        return resultShape;
    }

    std::vector<bool> consumed(resultChildren.size(), false);
    std::vector<TopoDS_Shape> ordered;
    ordered.reserve(sources.size());
    for (const NamedShapeSource& source : sources) {
        if (source.shape.IsNull()) {
            return resultShape;
        }
        const auto match = std::find_if(
            resultChildren.begin(), resultChildren.end(), [&](const TopoDS_Shape& child) {
                const std::size_t index = static_cast<std::size_t>(&child - resultChildren.data());
                return !consumed.at(index) && child.IsPartner(source.shape);
            }
        );
        if (match == resultChildren.end()) {
            return resultShape;
        }
        const std::size_t index = static_cast<std::size_t>(match - resultChildren.begin());
        consumed.at(index) = true;
        ordered.push_back(*match);
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::mapSubElement(const std::vector<TopoShape>&) accepts a compound child map
    // only when `getSubShape(..., ++count).IsPartner(s._Shape)` in source order. An OCCT Fuse
    // with unchanged disjoint children may return the same partners in a different order; rebuild
    // just that wrapper so the source-order ElementMap contract survives without reusing geometry.
    BRep_Builder builder;
    if (resultShape.ShapeType() == TopAbs_COMPSOLID) {
        TopoDS_CompSolid orderedCompSolid;
        builder.MakeCompSolid(orderedCompSolid);
        for (const TopoDS_Shape& child : ordered) {
            builder.Add(orderedCompSolid, child);
        }
        return orderedCompSolid;
    }
    TopoDS_Compound orderedCompound;
    builder.MakeCompound(orderedCompound);
    for (const TopoDS_Shape& child : ordered) {
        builder.Add(orderedCompound, child);
    }
    return orderedCompound;
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
    std::map<const NamedShape*, std::shared_ptr<NamedShape>> sharedSourceLedgers;
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
            // FreeCAD: TopoShapeExpansion.cpp::TopoShape::mapSubElement(
            // const std::vector<TopoShape>&) creates compound child maps with `child.tag =
            // s.Tag`. This is deliberately not inferred from a raw mapped name.
            childMap.tag = source.producerTag
                ? *source.producerTag
                : (source.namedShape && source.namedShape->producerTag
                       ? *source.namedShape->producerTag
                       : 0L);
            childMap.postfix = source.childElementMapPostfix;
            const long masterTag = namedShape.producerTag.value_or(0L);
            if (childMap.tag != 0L && childMap.tag != masterTag) {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                // ::ElementMap::addChildElements() calls encodeElementName(..., masterTag,
                // child.postfix, child.tag, true), then assigns `insertedChild.postfix = tmp`.
                // findAll() therefore appends the encoded child tag, not merely the input op.
                std::ostringstream encodedPostfix;
                encodedPostfix << childMap.postfix << ";:H";
                if (childMap.tag < 0L) encodedPostfix << '-';
                encodedPostfix << std::hex << std::abs(childMap.tag) << ',' << prefix.front();
                childMap.postfix = encodedPostfix.str();
            }
            if (source.namedShape != nullptr) {
                auto [ledger, inserted] = sharedSourceLedgers.emplace(source.namedShape, nullptr);
                if (inserted) {
                    ledger->second = std::make_shared<NamedShape>(*source.namedShape);
                }
                childMap.sourceLedger = ledger->second;
                childMap.sourceNamedShape = childMap.sourceLedger.get();
            }
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
                    if (!recursiveChildMap.sourceLedger && sourceChildMap.sourceNamedShape != nullptr) {
                        recursiveChildMap.sourceLedger =
                            std::make_shared<NamedShape>(*sourceChildMap.sourceNamedShape);
                    }
                    recursiveChildMap.sourceNamedShape = recursiveChildMap.sourceLedger
                        ? recursiveChildMap.sourceLedger.get()
                        : sourceChildMap.sourceNamedShape;
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

int canonicalCollisionShapeKindOrder(const std::string& shapeKind)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::createChildMap() records each direct child's `offset` and `count` per
    // TopAbs kind, while /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::findAll() appends every `MappedNameRef` target to `res`.  This is only a
    // CAD Core serialization tie-breaker over those ledger endpoints; it does not claim that
    // FreeCAD itself creates a canonical-collision event or selects a collision owner.
    if (shapeKind == "face") {
        return 0;
    }
    if (shapeKind == "edge") {
        return 1;
    }
    if (shapeKind == "vertex") {
        return 2;
    }
    return 3;
}

int canonicalCollisionTopologicalOrdinal(const std::string& subname)
{
    std::size_t digitStart = subname.size();
    while (digitStart > 0U
           && std::isdigit(static_cast<unsigned char>(subname.at(digitStart - 1U))) != 0) {
        --digitStart;
    }
    if (digitStart == subname.size()) {
        return std::numeric_limits<int>::max();
    }
    try {
        return std::stoi(subname.substr(digitStart));
    }
    catch (const std::exception&) {
        return std::numeric_limits<int>::max();
    }
}

bool canonicalCollisionCandidateLess(const MapperHistoryCollisionCandidate& left,
                                     const MapperHistoryCollisionCandidate& right)
{
    const int leftKind = canonicalCollisionShapeKindOrder(left.shapeKind);
    const int rightKind = canonicalCollisionShapeKindOrder(right.shapeKind);
    if (leftKind != rightKind) {
        return leftKind < rightKind;
    }
    const int leftOrdinal = canonicalCollisionTopologicalOrdinal(left.target.subname);
    const int rightOrdinal = canonicalCollisionTopologicalOrdinal(right.target.subname);
    if (leftOrdinal != rightOrdinal) {
        return leftOrdinal < rightOrdinal;
    }
    if (left.target.object != right.target.object) {
        return left.target.object < right.target.object;
    }
    if (left.target.subname != right.target.subname) {
        return left.target.subname < right.target.subname;
    }
    if (left.source.object != right.source.object) {
        return left.source.object < right.source.object;
    }
    return left.source.subname < right.source.subname;
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
        const auto [indexIt, inserted]
            = groupIndex.emplace(candidate.canonicalMappedName, groups.size());
        if (inserted) {
            groups.push_back(CollisionGroup {candidate.canonicalMappedName, {}});
        }
        groups.at(indexIt->second).candidates.push_back(candidate);
    }

    std::sort(
        groups.begin(),
        groups.end(),
        [](const CollisionGroup& left, const CollisionGroup& right) {
            return left.canonical < right.canonical;
        }
    );
    for (CollisionGroup& group : groups) {
        std::sort(
            group.candidates.begin(),
            group.candidates.end(),
            canonicalCollisionCandidateLess
        );
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

bool localizeProtocolChildMapProvenance(MappedNameProvenance& provenance,
                                        const std::string& childPrefix)
{
    if (provenance.status != MappedNameProvenanceStatus::SourceBacked
        || provenance.rawMappedName.empty()
        || provenance.sourceElement.rfind(childPrefix, 0U) != 0U
        || provenance.rawMappedName.rfind(childPrefix, 0U) != 0U) {
        return false;
    }
    provenance.sourceElement = provenance.sourceElement.substr(childPrefix.size());
    provenance.rawMappedName = provenance.rawMappedName.substr(childPrefix.size());
    provenance.canonicalMappedName = topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
    return !provenance.sourceElement.empty() && !provenance.canonicalMappedName.empty();
}

void appendProtocolChildMapCanonicalCollisionHistoryImpl(NamedShape& namedShape)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements() first resolves a direct child's map and only then applies
    // its rebased range; ::ElementMap::findAll() returns every current target for a mapped name.
    // FreeCAD's raw MappedName remains one-to-one.  CAD Core's public canonical codec can fold
    // distinct raw child names, so record that ambiguity here in the Part ledger rather than
    // allowing runtime response publication to infer it from StableSubList or response DTOs.
    std::vector<MapperHistoryCollisionCandidate> candidates;
    for (const NamedShapeChildMap& childMap : namedShape.childElementMaps) {
        if (childMap.recursiveExpansion || childMap.indexedName.rfind("Child", 0U) != 0U
            || childMap.sourceOwner.empty() || childMap.count <= 0) {
            continue;
        }
        const std::string childPrefix = childMap.sourceOwner + ".";
        for (const auto& [stableName, currentElement] : namedShape.elementMap) {
            if (stableName.rfind(childPrefix, 0U) != 0U) {
                continue;
            }
            const auto elementIt = namedShape.elements.find(currentElement);
            const auto provenanceIt = namedShape.mappedNameProvenance.find(stableName);
            if (elementIt == namedShape.elements.end()
                || provenanceIt == namedShape.mappedNameProvenance.end()
                || subshapeKindName(elementIt->second.subshape.kind) != childMap.kind
                || elementIt->second.subshape.index <= childMap.offset
                || elementIt->second.subshape.index > childMap.offset + childMap.count) {
                continue;
            }

            const std::string localSourceElement = prefixForKind(elementIt->second.subshape.kind)
                + std::to_string(elementIt->second.subshape.index - childMap.offset);
            MappedNameProvenance provenance = provenanceIt->second;
            if (!localizeProtocolChildMapProvenance(provenance, childPrefix)
                || provenance.sourceElement != localSourceElement) {
                continue;
            }
            candidates.push_back(MapperHistoryCollisionCandidate {
                {childMap.sourceOwner, provenance.sourceElement},
                {namedShape.owner, currentElement},
                childMap.kind,
                provenance.rawMappedName,
                provenance.canonicalMappedName,
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
        // A maker is a new producer boundary even for a preserved element. Only history helpers
        // without an operation (notably refine/copy propagation) may retain the complete raw
        // source mapping unchanged.
        const bool recorded = producerOperation.empty()
            ? recordInheritedMappedNameProvenance(namedShape, sourceName, target, targets)
            : recordProducerMappedNameProvenance(
                  namedShape, sourceName, target, targets, operationPostfix
              );
        if (!recorded) {
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
        const auto ordinalIt = targets.historyOrdinals.find(target);
        return operationPostfixForHistoryKind(
            historyKindIt->second,
            producerOperation,
            ordinalIt == targets.historyOrdinals.end() ? 1 : ordinalIt->second
        );
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
                applyAlias(
                    sourceName,
                    targets,
                    target,
                    historyOperationPostfix(targets, target)
                );
                continue;
            }
            if (sameKindHistory.size() > 1U) {
                applySplit(sourceName, sameKindHistory);
                continue;
            }
        }

        if (targets.history.size() == 1U) {
            const std::string target = *targets.history.begin();
            applyAlias(
                sourceName,
                targets,
                target,
                historyOperationPostfix(targets, target)
            );
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

void applyMakerHistoryElementMap(NamedShape& namedShape,
                                 const std::map<std::string, SourceTargets>& sourceTargets,
                                 const std::string& producerOperation,
                                 bool recordUnmappedSourceDeletions,
                                 bool promoteBareSourceIdForGenerated = false)
{
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    bool multiSourceTupleObserved = false;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() accumulates all
    // MapperMaker candidates per result IndexedName in `newNames`, then selects one NameKey in
    // vertex/edge/face order. The ledger is target-oriented at this boundary: applying aliases in
    // source-map iteration order lets a later Face overwrite a better Vertex/Edge candidate.
    struct Candidate
    {
        std::string sourceName;
        const SourceTargets* targets = nullptr;
        std::string target;
        std::string operationPostfix;
        bool preserved = false;
    };

    const auto candidateLess = [](const Candidate& left, const Candidate& right) {
        if (left.preserved != right.preserved) {
            return left.preserved;
        }
        const int leftKind = makerSourceKindOrder(left.targets->sourceKind);
        const int rightKind = makerSourceKindOrder(right.targets->sourceKind);
        if (leftKind != rightKind) {
            return leftKind < rightKind;
        }
        const long leftTag = left.targets->nameKeyTag.value_or(0L);
        const long rightTag = right.targets->nameKeyTag.value_or(0L);
        if (leftTag != rightTag) {
            return leftTag < rightTag;
        }
        // FreeCAD NameKey compares the incoming mapped name, not a CAD Core owner-qualified
        // resolver alias. The latter is only a local lookup handle and would make object names
        // influence the chosen Vertex/Edge/Face producer candidate.
        const std::string& leftName = left.targets->inheritedMappedName
                && !left.targets->inheritedMappedName->rawMappedName.empty()
            ? left.targets->inheritedMappedName->rawMappedName
            : left.sourceName;
        const std::string& rightName = right.targets->inheritedMappedName
                && !right.targets->inheritedMappedName->rawMappedName.empty()
            ? right.targets->inheritedMappedName->rawMappedName
            : right.sourceName;
        return compareFreeCadMappedNames(leftName, rightName) < 0;
    };

    std::map<std::string, std::vector<Candidate>> candidatesByTarget;
    const auto consider = [&](Candidate candidate) {
        if (candidate.target.empty() || namedShape.elements.count(candidate.target) == 0U) {
            if (trace != nullptr) {
                trace->record({
                    "maker.candidate.reject",
                    "rejected",
                    candidate.target.empty() ? "target_empty" : "target_not_in_output",
                    {{"source", candidate.sourceName}, {"target", candidate.target}},
                });
            }
            return;
        }
        if (!candidate.preserved
            && selectedSourceBackedMappedNameProvenance(namedShape, candidate.target) != nullptr) {
            // mapSubElement(shapes) has already consumed this terminal target. FreeCAD's later
            // generated/modified scan has the same `getMappedName(element)` guard, so it must
            // not replace a preserved source alias with a maker relation.
            return;
        }
        auto& candidates = candidatesByTarget[candidate.target];
        const auto sameSource = std::find_if(
            candidates.begin(),
            candidates.end(),
            [&](const Candidate& existing) { return existing.sourceName == candidate.sourceName; }
        );
        if (sameSource == candidates.end()) {
            candidates.push_back(std::move(candidate));
        }
        else if (candidateLess(candidate, *sameSource)) {
            if (trace != nullptr) {
                trace->record({
                    "maker.candidate.reject",
                    "replaced",
                    "better_duplicate_source_candidate",
                    {{"source", candidate.sourceName}, {"target", candidate.target}},
                });
            }
            *sameSource = std::move(candidate);
        }
        else if (trace != nullptr) {
            trace->record({
                "maker.candidate.reject",
                "rejected",
                "duplicate_source_candidate",
                {{"source", candidate.sourceName}, {"target", candidate.target}},
            });
        }
    };

    for (const auto& [sourceName, targets] : sourceTargets) {
        // mapSubElement(shapes) has already written the one-to-one preserved mapping in the
        // first phase.  Do not feed it back through the producer encoder here: FreeCAD's
        // getMappedName(element) guard prevents the later Modified/Generated pass from
        // re-encoding an already named target.  Doing so creates a duplicate `#SID;:H...`
        // alongside the preserved combo and corrupts the following producer's candidate set.
        if (targets.preserved.size() > 1U) {
            for (const std::string& target : targets.preserved) {
                auto elementIt = namedShape.elements.find(target);
                if (elementIt == namedShape.elements.end()) {
                    continue;
                }
                elementIt->second.status = ElementHistoryKind::Split;
                addTerminalHistory(
                    namedShape, ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
                );
            }
        }

        if (targets.history.empty()) {
            // mapSubElement(shapes) has already written this source's preserved relation in the
            // first phase. FreeCAD does not turn that successful preserved lookup into a deleted
            // MapperHistory event merely because MapperMaker reports no later M/G relation.
            if (targets.preserved.empty() && recordUnmappedSourceDeletions) {
                addTerminalHistory(
                    namedShape,
                    ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
                );
            }
            continue;
        }
        for (const std::string& target : targets.history) {
            const auto relation = targets.historyKinds.find(target);
            // FreeCAD: TopoShapeExpansion.cpp::makeShapeWithElementMap() rejects a
            // Mapper::modified() result when `newInfo.type != newShape.ShapeType()`. Generated
            // history may legitimately cross Vertex/Edge/Face through its higher-shape expansion,
            // but a cross-type Modified candidate must not join NameKey's multi-source tuple.
            if (relation != targets.historyKinds.end()
                && relation->second == ElementHistoryKind::Modified && targets.sourceKind
                && elementKindFromName(target) != *targets.sourceKind) {
                if (trace != nullptr) {
                    trace->record({
                        "maker.candidate.reject",
                        "rejected",
                        "modified_source_target_type_mismatch",
                        {{"source", sourceName},
                         {"target", target},
                         {"reportedType",
                          subshapeKindName(elementKindFromName(target).value_or(TopAbs_SHAPE))},
                         {"effectiveSourceType", subshapeKindName(*targets.sourceKind)}},
                    });
                }
                continue;
            }
            const auto ordinal = targets.historyOrdinals.find(target);
            const std::string postfix = relation == targets.historyKinds.end()
                ? std::string {}
                : operationPostfixForHistoryKind(
                      relation->second,
                      producerOperation,
                      ordinal == targets.historyOrdinals.end() ? 1 : ordinal->second
                  );
            consider(Candidate {sourceName, &targets, target, postfix, false});
        }
    }

    const auto sourceMappedName = [](const Candidate& candidate) -> std::string {
        if (candidate.targets != nullptr && candidate.targets->inheritedMappedName
            && !candidate.targets->inheritedMappedName->rawMappedName.empty()) {
            return candidate.targets->inheritedMappedName->rawMappedName;
        }
        return candidate.sourceName;
    };
    const auto combinedPostfix = [&](const Candidate& selected,
                                     const std::vector<Candidate>& candidates,
                                     std::vector<cad_core::app::StringId>& relatedRefs) {
        std::string postfix = selected.operationPostfix;
        if (candidates.size() <= 1U || !namedShape.stringHasher) {
            return postfix;
        }

        if (trace != nullptr) {
            multiSourceTupleObserved = true;
            trace->record({
                "maker.multi_source",
                "begin",
                "name_key_tuple_required",
                {{"target", selected.target},
                 {"candidateCount", candidates.size()},
                 {"postfix", postfix}},
            });
        }

        // `sids` in makeShapeWithElementMap starts with the selected NameInfo's refs. Before
        // it hashes the parenthesized tuple, FreeCAD runs encodeElementName() on every *other*
        // candidate. That step is observable even though those temporary K names are not
        // published: a Pad source with a Sketch terminal tag is re-encoded under the Pad's
        // incoming Tag, consuming document StringIDs before the tuple is interned.
        //
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), the `names.size()>1`
        // block calls `encodeElementName(... other_name, ss2, &sids, Tag, nullptr,
        // other_key.tag)` and only then `Hasher->getID(ss.str())` for the tuple.
        std::vector<cad_core::app::StringId> tupleRefs;
        if (selected.targets != nullptr && selected.targets->inheritedMappedName) {
            const MappedNameProvenance& provenance = *selected.targets->inheritedMappedName;
            tupleRefs = provenance.elementIdRefs.empty()
                ? namedShape.stringHasher->relatedIdsForMappedName(provenance.rawMappedName)
                : provenance.elementIdRefs;
        }

        const auto appendCandidateRefs = [&](const Candidate& candidate) {
            if (candidate.targets == nullptr || !candidate.targets->inheritedMappedName) {
                return;
            }
            const MappedNameProvenance& provenance = *candidate.targets->inheritedMappedName;
            const std::vector<cad_core::app::StringId> refs = provenance.elementIdRefs.empty()
                ? namedShape.stringHasher->relatedIdsForMappedName(provenance.rawMappedName)
                : provenance.elementIdRefs;
            tupleRefs.insert(tupleRefs.end(), refs.begin(), refs.end());
        };

        const auto encodedAdditionalName = [&](const Candidate& candidate) {
            std::string name = sourceMappedName(candidate);
            if (name.empty() || candidate.targets == nullptr) {
                return name;
            }

            const auto relation = candidate.targets->historyKinds.find(candidate.target);
            const auto ordinal = candidate.targets->historyOrdinals.find(candidate.target);
            int mapperIndex = 1;
            if (relation != candidate.targets->historyKinds.end()) {
                const int count = ordinal == candidate.targets->historyOrdinals.end()
                    ? 1
                    : ordinal->second;
                mapperIndex = relation->second == ElementHistoryKind::Generated ? -count : count;
            }
            std::string extraPostfix;
            if (mapperIndex != 1) {
                extraPostfix = ";K" + std::to_string(mapperIndex);
            }

            const long masterTag = namedShape.producerTag.value_or(0L);
            long incomingTag = candidate.targets->nameKeyTag.value_or(0L);
            const std::optional<long> terminalTag = terminalMappedNameTag(name);
            const bool sameUnpostfixedTag = extraPostfix.empty()
                && (incomingTag == 0L || incomingTag == masterTag
                    || (terminalTag && *terminalTag == incomingTag));
            if (sameUnpostfixedTag) {
                appendCandidateRefs(candidate);
                return name;
            }

            const std::size_t separator = name.find(';');
            const cad_core::app::HashedMappedName hashed = namedShape.stringHasher->hashMappedName(
                name.substr(0U, separator),
                0,
                separator == std::string::npos ? std::string {} : name.substr(separator),
                tupleRefs
            );
            if (!hashed.id) {
                return name;
            }
            tupleRefs = hashed.elementRefs;
            name = hashed.id.toString();

            // encodeElementName() reuses the terminal tag when the incoming TopoShape Tag is
            // zero (or is the result master tag) and an auxiliary `;K` postfix forced encoding.
            if (incomingTag == 0L || incomingTag == masterTag) {
                incomingTag = terminalTag.value_or(0L);
            }
            name += extraPostfix;
            if (incomingTag != 0L) {
                const std::string type = candidate.targets->sourceKind
                    ? prefixForKind(*candidate.targets->sourceKind)
                    : std::string {};
                if (!type.empty()) {
                    std::ostringstream tag;
                    tag << ";:H" << std::hex;
                    if (incomingTag < 0L) {
                        tag << '-' << -incomingTag;
                    }
                    else {
                        tag << incomingTag;
                    }
                    if (!extraPostfix.empty()) {
                        tag << ':' << std::hex << extraPostfix.size();
                    }
                    tag << ',' << type.front();
                    name += tag.str();
                }
            }
            appendCandidateRefs(candidate);
            return name;
        };

        std::string tuple = "(";
        bool first = true;
        for (std::size_t index = 1U; index < candidates.size() && index <= 4U; ++index) {
            const Candidate& other = candidates.at(index);
            const std::string otherName = encodedAdditionalName(other);
            if (otherName.empty()) {
                continue;
            }
            if (!first) {
                tuple += '|';
            }
            first = false;
            tuple += otherName;
        }
        tuple += ')';
        if (first) {
            return postfix;
        }
        const cad_core::app::StringId tupleId = namedShape.stringHasher->getId(tuple);
        tupleRefs.push_back(tupleId);
        relatedRefs = std::move(tupleRefs);

        // makeShapeWithElementMap() appends the tuple StringID after :M/:G but before the
        // producer op supplied to encodeElementName(), yielding e.g. :M#44;RFI.
        const std::string producerSuffix = normalizedProducerOperation(producerOperation);
        const std::size_t suffix = producerSuffix.empty() ? std::string::npos
                                                           : postfix.rfind(producerSuffix);
        if (suffix == std::string::npos) {
            postfix += tupleId.toString();
        }
        else {
            postfix.insert(suffix, tupleId.toString());
        }
        if (trace != nullptr) {
            trace->record({
                "maker.multi_source",
                "encoded",
                "k_tuple_sid_allocated",
                {{"target", selected.target},
                 {"tuple", tuple},
                 {"tupleId", {{"value", tupleId.value}, {"index", tupleId.index}}},
                 {"combinedPostfix", postfix},
                 {"variant", candidates.size() > 2U ? "K00" : "K0"}},
            });
        }
        return postfix;
    };

    // FreeCAD collects mapper candidates through ShapeInfo in Vertex -> Edge -> Face order, but
    // stores them in `std::map<Data::IndexedName, ...> newNames`. IndexedName compares its text
    // lexicographically before its numeric index (src/App/IndexedName.h::IndexedName::compare),
    // so the later encode pass is Edge -> Face -> Vertex. Keeping collection and consumption as
    // two separate orders matters: a Boolean can create all three kinds, and its tuple/StringID
    // allocations are observable by the following Refine.
    constexpr std::array<TopAbs_ShapeEnum, 3> encodedKinds {
        TopAbs_EDGE,
        TopAbs_FACE,
        TopAbs_VERTEX,
    };
    for (const TopAbs_ShapeEnum kind : encodedKinds) {
        const std::string prefix = prefixForKind(kind);
        TopTools_IndexedMapOfShape outputElements;
        TopExp::MapShapes(namedShape.shape, kind, outputElements);
        for (int index = 1; index <= outputElements.Extent(); ++index) {
            const std::string target = prefix + std::to_string(index);
            const auto targetIt = candidatesByTarget.find(target);
            if (targetIt == candidatesByTarget.end() || targetIt->second.empty()) {
                continue;
            }
            std::vector<Candidate> candidates = targetIt->second;
            const std::size_t observedCandidateCount = candidates.size();
            const auto isHighLevelGenerated = [&](const Candidate& candidate) {
                if (candidate.targets == nullptr) {
                    return false;
                }
                const auto relation = candidate.targets->historyKinds.find(target);
                const auto ordinal = candidate.targets->historyOrdinals.find(target);
                return relation != candidate.targets->historyKinds.end()
                    && relation->second == ElementHistoryKind::Generated
                    && ordinal != candidate.targets->historyOrdinals.end() && ordinal->second == 0;
            };
            // FreeCAD delays NameKeys expanded from a Shell/Solid (`shapeOffset = 3`) while a
            // direct mapper candidate exists.  The high-level candidate is used only when no
            // lower-level source names the target; combining both creates a false `:G#...` SID.
            const bool hasDirectCandidate = std::any_of(
                candidates.begin(), candidates.end(),
                [&](const Candidate& candidate) { return !isHighLevelGenerated(candidate); }
            );
            if (hasDirectCandidate) {
                candidates.erase(
                    std::remove_if(candidates.begin(), candidates.end(), isHighLevelGenerated),
                    candidates.end()
                );
            }
            if (candidates.empty()) {
                continue;
            }
            std::sort(candidates.begin(), candidates.end(), candidateLess);
            const Candidate& candidate = candidates.front();
            if (trace != nullptr) {
                trace->record({
                    "element_map.find", "miss", "no_entry", {{"indexed", target}},
                });
            }
            std::vector<cad_core::app::StringId> relatedRefs;
            const std::string postfix = combinedPostfix(candidate, candidates, relatedRefs);
        if (!recordProducerMappedNameProvenance(
                namedShape,
                candidate.sourceName,
                target,
                *candidate.targets,
                postfix,
                relatedRefs,
                promoteBareSourceIdForGenerated,
                isHighLevelGenerated(candidate)
            )) {
            recordMappedNameProvenance(
                namedShape,
                candidate.sourceName,
                target,
                candidate.targets->sourceElement.empty() ? candidate.sourceName
                                                         : candidate.targets->sourceElement,
                candidate.targets->sourceTag,
                postfix
            );
            namedShape.elementMap[candidate.sourceName] = target;
            if (trace != nullptr) {
                trace->record({
                    "element_map.encode",
                    "fallback",
                    "producer_encoding_unavailable",
                    {{"source", candidate.sourceName},
                     {"target", target},
                     {"postfix", postfix}},
                });
            }
        }
        if (trace != nullptr) {
            const MappedNameProvenance* selected = selectedSourceBackedMappedNameProvenance(
                namedShape, target
            );
            std::string refs;
            if (selected != nullptr) {
                std::vector<app::StringId> displayRefs = selected->elementIdRefs;
                const std::size_t postfix = selected->rawMappedName.find(';');
                if (const auto primary = app::parseStringId(
                        selected->rawMappedName.substr(0U, postfix)
                    )) {
                    const auto primaryRef = std::find_if(
                        displayRefs.begin(), displayRefs.end(), [&](const app::StringId& ref) {
                            return ref.value == primary->value && ref.index == primary->index;
                        }
                    );
                    if (primaryRef != displayRefs.end() && primaryRef != displayRefs.begin()) {
                        std::rotate(displayRefs.begin(), primaryRef, std::next(primaryRef));
                    }
                }
                for (const app::StringId& ref : displayRefs) {
                    if (!refs.empty()) refs += ',';
                    refs += ref.toString();
                }
            }
            trace->record({
                "maker.select", "selected", "sorted_name_key",
                {{"source", sourceMappedName(candidate)},
                 {"target", target},
                 {"raw", selected != nullptr ? selected->rawMappedName : std::string {}},
                 {"entryLocalRefs", refs},
                 {"candidateCount", std::to_string(observedCandidateCount)},
                 {"delayed", "false"}},
            });
        }
        }
    }
    (void)multiSourceTupleObserved;
}

void applyMakerPreservedElementMap(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets,
    const std::string& producerOperation = {},
    bool rehashPreservedMappedName = false,
    bool preserveRawMappedName = false
)
{
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() calls `mapSubElement(shapes)`
    // before it asks Mapper::modified()/generated(). This phase has no maker operation postfix:
    // ElementMap::encodeElementName() either preserves an already matching MappedName or writes
    // only the child/source tag. Applying `producerOperation` here turns a preserved Pocket
    // endpoint into a synthetic FUS/CUT producer before FreeCAD has queried Mapper history.
    (void)producerOperation;
    (void)rehashPreservedMappedName;
    std::vector<std::pair<const std::string*, const SourceTargets*>> orderedSources;
    orderedSources.reserve(sourceTargets.size());
    for (const auto& [sourceName, targets] : sourceTargets) {
        orderedSources.emplace_back(&sourceName, &targets);
    }
    std::sort(
        orderedSources.begin(),
        orderedSources.end(),
        [](const auto& left, const auto& right) {
            const int leftKind = makerSourceKindOrder(left.second->sourceKind);
            const int rightKind = makerSourceKindOrder(right.second->sourceKind);
            if (leftKind != rightKind) {
                return leftKind < rightKind;
            }
            if (left.second->mapSubElementOrder != right.second->mapSubElementOrder) {
                return left.second->mapSubElementOrder < right.second->mapSubElementOrder;
            }
            return compareFreeCadMappedNames(*left.first, *right.first) < 0;
        }
    );
    for (const auto& [sourceNameRef, targetsRef] : orderedSources) {
        const std::string& sourceName = *sourceNameRef;
        const SourceTargets& targets = *targetsRef;
        if (targets.preserved.size() == 1U) {
            const std::string& target = *targets.preserved.begin();
            if (namedShape.elements.count(target) == 0U) {
                if (trace != nullptr) {
                    trace->record({
                        "maker.candidate.reject",
                        "rejected",
                        "preserved_target_not_in_output",
                        {{"source", sourceName}, {"target", target}},
                    });
                }
                continue;
            }
            bool recorded = false;
            bool directMapSubElementName = false;
            if (preserveRawMappedName && targets.partnerShape) {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                // TopoShapeExpansion.cpp::TopoShape::mapSubElement(), if the target is a
                // partner of its incoming shape, takes copyElementMap() and retains that
                // ElementMap ref list before makeShapeWithElementMap() asks Mapper::modified()
                // or generated(). A Boolean-preserved source therefore remains a map entry
                // with its own StringIDRefs; it must not be promoted into a new M/G producer.
                recorded = recordPreservedMappedNameProvenance(
                    namedShape, sourceName, target, targets
                );
            }
            else {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                // ::ElementMap::encodeElementName(), with no postfix from mapSubElement(),
                // returns immediately when `tag == masterTag` or when the incoming name already
                // terminates in that tag. Preserve the whole entry-local raw/ref list in those
                // cases; only the non-matching branch hashes and appends the incoming tag.
                const long masterTag = namedShape.producerTag.value_or(0L);
                // FreeCAD NameKey uses incomingShape.Tag for the current mapSubElement call.
                // The inherited provenance sourceTag may describe an older nested producer and
                // must not force re-encoding when the current incoming TopoShape has Tag zero.
                const long sourceTag = targets.nameKeyTag.value_or(
                    targets.sourceTag.value_or(0L)
                );
                const std::optional<long> terminal = targets.inheritedMappedName
                    ? terminalMappedNameTag(targets.inheritedMappedName->rawMappedName)
                    : std::optional<long> {};
                directMapSubElementName = sourceTag == 0L || sourceTag == masterTag
                    || (terminal && *terminal == sourceTag);
                recorded = directMapSubElementName
                    ? recordInheritedMappedNameProvenance(namedShape, sourceName, target, targets)
                    : recordProducerMappedNameProvenance(namedShape, sourceName, target, targets, {});
            }
            if (!recorded) {
                recordMappedNameProvenance(
                    namedShape,
                    sourceName,
                    target,
                    targets.sourceElement.empty() ? sourceName : targets.sourceElement,
                    targets.sourceTag,
                    {}
                );
                namedShape.elementMap[sourceName] = target;
            }
            else if (directMapSubElementName) {
                // mapSubElement() wrote this entry before the Mapper M/G scan. Keep its
                // source-backed key in the lookup index so the later getMappedName(element)
                // guard observes the same terminal mapping and does not replace it.
                namedShape.elementMap[sourceName] = target;
            }
            continue;
        }
        if (targets.preserved.size() > 1U) {
            if (trace != nullptr) {
                trace->record({
                    "toposhape.map_sub_element",
                    "split",
                    "preserve_one_to_many",
                    {{"source", sourceName}, {"targets", targets.preserved}},
                });
            }
            for (const std::string& target : targets.preserved) {
                const auto element = namedShape.elements.find(target);
                if (element == namedShape.elements.end()) {
                    continue;
                }
                element->second.status = ElementHistoryKind::Split;
                addTerminalHistory(
                    namedShape,
                    ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
                );
            }
        }
    }
}

std::optional<std::string> logicalElementForShape(const NamedShape& namedShape,
                                                  const TopoDS_Shape& shape,
                                                  TopAbs_ShapeEnum kind)
{
    if (shape.IsNull()) {
        return std::nullopt;
    }
    for (const auto& [elementName, element] : namedShape.elements) {
        if (element.subshape.kind != kind) {
            continue;
        }
        const auto currentShape = subshapeByName(namedShape, elementName);
        if (currentShape && currentShape->IsSame(shape)) {
            return elementName;
        }
    }
    return std::nullopt;
}

bool hasSourceBackedMapForElement(const NamedShape& namedShape, const std::string& elementName)
{
    return selectedSourceBackedMappedNameProvenance(namedShape, elementName) != nullptr;
}

void addDerivedMakerAlias(NamedShape& namedShape,
                          const std::string& target,
                          const MappedNameProvenance& source,
                          const std::string& operationPostfix)
{
    if (target.empty() || namedShape.elements.count(target) == 0U || source.rawMappedName.empty()) {
        return;
    }
    std::string sourceId;
    std::vector<cad_core::app::StringId> sourceRefs;
    const std::size_t sourcePostfix = source.rawMappedName.find(';');
    if (namedShape.stringHasher) {
        sourceRefs = source.elementIdRefs;
        if (sourceRefs.empty()) {
            sourceRefs = namedShape.stringHasher->relatedIdsForMappedName(source.rawMappedName);
        }
        if (source.rawMappedName.front() == '#' && sourcePostfix != std::string::npos) {
            const cad_core::app::HashedMappedName hashed = namedShape.stringHasher->hashMappedName(
                source.rawMappedName.substr(0U, sourcePostfix),
                producerStringIdIndex(
                    *namedShape.stringHasher,
                    source.rawMappedName,
                    operationPostfix,
                    false
                ),
                source.rawMappedName.substr(sourcePostfix),
                sourceRefs
            );
            sourceId = hashed.id.toString();
            sourceRefs = hashed.elementRefs;
        }
        else if (const auto materialized = namedShape.stringHasher->mappedNameId(
                     source.rawMappedName
                 )) {
            sourceId = materialized->toString();
        }
    }
    if (sourceId.empty()) {
        sourceId = source.rawMappedName.substr(0U, sourcePostfix);
    }
    if (sourceId.empty()) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() reverse pass applies
    // POSTFIX_UPPER from a named Face to unnamed Edges and from a named Edge to unnamed Vertices.
    // The new StringID is related to the already mapped parent; it is not derived from FaceN or
    // another display index.
    MappedNameProvenance provenance;
    provenance.entryKey = sourceId + operationPostfix + "->" + target;
    provenance.currentElement = target;
    provenance.sourceElement = sourceId;
    provenance.encodeInputMappedName = source.rawMappedName;
    provenance.elementType = mappedNameElementType(target);
    provenance.producerTag = namedShape.producerTag;
    provenance.masterTag = provenance.producerTag;
    provenance.sourceTag = source.sourceTag ? source.sourceTag : provenance.producerTag;
    provenance.operationPostfix = operationPostfix;
    provenance.status = MappedNameProvenanceStatus::IndexedOnly;
    provenance = cad_core::topo::encodedMappedNameProvenance(std::move(provenance));
    if (provenance.status != MappedNameProvenanceStatus::SourceBacked) {
        return;
    }
    provenance.elementIdRefs = sourceRefs;
    if (namedShape.stringHasher) {
        cad_core::app::StringId primaryId;
        if (const auto sourceIdRef = cad_core::app::parseStringId(sourceId)) {
            primaryId = *sourceIdRef;
        }
        if (sourceRefs.empty() && primaryId) {
            sourceRefs.push_back(primaryId);
        }
        namedShape.stringHasher->rememberMappedName(
            provenance.rawMappedName,
            primaryId,
            std::move(sourceRefs)
        );
    }
    // ElementMap::setElementName() is keyed by the encoded mapped name. The source->target text
    // above is only an internal construction discriminator; publishing it as the map key turns
    // a valid `#id;:U...`/FaceMaker combo into a resolver-only alias that downstream makers can
    // no longer consume through ElementMap::findAll().
    const std::string entryKey = provenance.canonicalMappedName.empty()
        ? provenance.rawMappedName
        : provenance.canonicalMappedName;
    if (entryKey.empty()) {
        return;
    }
    provenance.entryKey = entryKey;
    namedShape.elementMap[entryKey] = target;
    namedShape.mappedNameProvenance[entryKey] = std::move(provenance);
    recordElementMapEntry(namedShape, entryKey, target);
}

void addMakerReverseAliases(NamedShape& namedShape, const std::string& producerOperation)
{
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    const auto rejectUpper = [](std::string reason, nlohmann::json fields) {
        (void)reason;
        (void)fields;
    };
    if (producerOperation.empty()) {
        rejectUpper("upper_producer_operation_missing", nlohmann::json::object());
        return;
    }
    const auto addReverse = [&](TopAbs_ShapeEnum parentKind, TopAbs_ShapeEnum childKind) {
        struct Candidate
        {
            MappedNameProvenance source;
            bool hasSource = false;
            int ordinal = 1;
            std::string parent;
            std::string child;
        };
        // ShapeInfo in FreeCAD walks IndexedName 1..count, not the lexical key order of an
        // ElementMap.  Keep candidate collection in that topology order: `Edge10` must not be
        // considered before `Edge2`, otherwise U aliases receive different StringIDRefs.
        using CandidateMap = std::map<std::string, Candidate, FreeCadMappedNameLess>;
        std::map<int, std::pair<std::string, CandidateMap>> candidatesByChild;
        TopTools_IndexedMapOfShape parents;
        TopExp::MapShapes(namedShape.shape, parentKind, parents);
        const auto resolveMapped = [&](const std::string& indexed,
                                       bool publishDirectRefs,
                                       bool publishInheritedRefs)
            -> std::optional<MappedNameProvenance> {
            if (const MappedNameProvenance* direct =
                    selectedSourceBackedMappedNameProvenance(namedShape, indexed)) {
                if (publishDirectRefs && direct->delayedHighLevel) {
                    return *direct;
                }
                if (trace != nullptr) {
                    std::string refs;
                    if (publishDirectRefs) {
                        for (const app::StringId& ref : direct->elementIdRefs) {
                            if (!refs.empty()) refs += ',';
                            refs += ref.toString();
                        }
                    }
                    trace->record({
                        "element_map.find", "hit", "first_entry",
                        {{"indexed", indexed},
                         {"raw", direct->rawMappedName},
                         {"entryLocalRefs", refs}},
                    });
                }
                return *direct;
            }
            auto inherited = firstMappedNameProvenanceForElement(namedShape, indexed);
            if (publishDirectRefs && inherited && inherited->delayedHighLevel) {
                return inherited;
            }
            if (inherited && trace != nullptr) {
                (void)sourceElementNames(
                    NamedShapeSource {namedShape.owner, namedShape.shape, &namedShape},
                    indexed,
                    SourceElementMapLookup::First,
                    true,
                    publishInheritedRefs
                );
            }
            else if (!inherited && trace != nullptr) {
                trace->record({
                    "element_map.find", "miss", "no_entry", {{"indexed", indexed}},
                });
            }
            return inherited;
        };
        for (int parentIndex = 1; parentIndex <= parents.Extent(); ++parentIndex) {
            const auto parentName = logicalElementForShape(namedShape, parents(parentIndex), parentKind);
            if (!parentName) {
                rejectUpper(
                    "upper_parent_has_no_indexed_name",
                    {{"parentKind", subshapeKindName(parentKind)},
                     {"parentIndex", parentIndex},
                     {"childKind", subshapeKindName(childKind)}}
                );
                continue;
            }
            const auto parentIt = namedShape.elements.find(*parentName);
            if (parentIt == namedShape.elements.end()) {
                rejectUpper(
                    "upper_parent_missing_from_element_ledger",
                    {{"parent", *parentName}, {"childKind", subshapeKindName(childKind)}}
                );
                continue;
            }
            const auto source = resolveMapped(*parentName, true, true);
            if (!source) {
                rejectUpper(
                    "upper_parent_has_no_source_backed_name",
                    {{"parent", *parentName}, {"childKind", subshapeKindName(childKind)}}
                );
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() leaves
            // `newNames.count(element) != 0` entries (shapeOffset=3) out of the first reverse
            // pass. They are considered only when `delayed=true` after precise names are spent.
            if (source->delayedHighLevel) {
                continue;
            }
            const auto parentShape = subshapeByName(namedShape, *parentName);
            if (!parentShape) {
                rejectUpper(
                    "upper_parent_shape_lookup_failed",
                    {{"parent", *parentName}, {"childKind", subshapeKindName(childKind)}}
                );
                continue;
            }
            TopTools_IndexedMapOfShape children;
            TopExp::MapShapes(*parentShape, childKind, children);
            int unnamedChildOrdinal = 1;
            for (int index = 1; index <= children.Extent(); ++index) {
                const auto childName = logicalElementForShape(namedShape, children(index), childKind);
                if (!childName) {
                    rejectUpper(
                        "upper_child_has_no_indexed_name",
                        {{"parent", *parentName},
                         {"childKind", subshapeKindName(childKind)},
                         {"childIndex", index}}
                    );
                    continue;
                }
                const auto childSource = resolveMapped(*childName, false, false);
                if (childSource) {
                    rejectUpper(
                        "upper_child_already_named",
                        {{"parent", *parentName}, {"child", *childName}}
                    );
                    continue;
                }
                const auto parsedChild = parseSubshapeName(*childName);
                if (!parsedChild || parsedChild->kind != childKind) {
                    rejectUpper(
                        "upper_child_indexed_name_kind_mismatch",
                        {{"parent", *parentName},
                         {"child", *childName},
                         {"expectedKind", subshapeKindName(childKind)}}
                    );
                    continue;
                }
                // FreeCAD makeShapeWithElementMap() gathers every parent candidate first in
                // `names[indexedName][mapped]`, then selects the first sorted mapped name. Do
                // not assign while traversing a Face: that makes Face1's incidental edge order
                // override a lexically earlier source from another Face and fabricates U2/U3/U4.
                auto& childCandidates = candidatesByChild[parsedChild->index];
                if (childCandidates.first.empty()) {
                    childCandidates.first = *childName;
                }
                const int candidateOrdinal = unnamedChildOrdinal++;
                const auto [candidateIt, inserted] = childCandidates.second.try_emplace(
                    source->rawMappedName,
                    Candidate {*source, true, candidateOrdinal, *parentName, *childName}
                );
                if (!inserted && trace != nullptr) {
                    producer_trace_detail::publishDuplicateCandidateSuppressed(
                        *trace,
                        "maker.upper",
                        "upper_duplicate_candidate_suppressed",
                        {source->rawMappedName,
                         {candidateIt->second.parent,
                          candidateIt->second.child,
                          static_cast<std::size_t>(candidateIt->second.ordinal),
                          candidateIt->second.source.elementIdRefs},
                         {*parentName,
                          *childName,
                          static_cast<std::size_t>(candidateOrdinal),
                          source->elementIdRefs}}
                    );
                }
            }
        }
        for (const auto& [childIndex, childCandidateEntry] : candidatesByChild) {
            (void)childIndex;
            const std::string& childName = childCandidateEntry.first;
            const auto& candidates = childCandidateEntry.second;
            if (candidates.empty()) {
                rejectUpper(
                    "upper_child_has_no_candidates",
                    {{"child", childName}, {"childIndex", childIndex}}
                );
                continue;
            }
            const Candidate& candidate = candidates.begin()->second;
            if (!candidate.hasSource) {
                if (trace != nullptr) {
                    trace->record({
                        "maker.upper",
                        "rejected",
                        "upper_candidate_has_no_source_provenance",
                        {{"child", childName}},
                    });
                }
                continue;
            }
            // FreeCAD: TopoShapeExpansion.cpp::makeShapeWithElementMap() writes
            // `upperPostfix()` followed by `NameInfo::index` only when the same parent has more
            // than one unnamed child. The existing source StringIDRef (for example `#d:4`) stays
            // intact; rewriting that index to the child ordinal turns `#16:4;:U` into a distinct
            // and incorrect producer lineage.
            std::string postfix = ";:U";
            if (candidate.ordinal > 1) {
                postfix += std::to_string(candidate.ordinal);
            }
            postfix += normalizedProducerOperation(producerOperation);
            addDerivedMakerAlias(namedShape, childName, candidate.source, postfix);
            if (trace != nullptr) {
                const MappedNameProvenance* derived =
                    selectedSourceBackedMappedNameProvenance(namedShape, childName);
                std::string refs;
                if (derived != nullptr) {
                    std::vector<app::StringId> displayRefs = derived->elementIdRefs;
                    const std::size_t rawPostfix = derived->rawMappedName.find(';');
                    if (const auto primary = app::parseStringId(
                            derived->rawMappedName.substr(0U, rawPostfix)
                        )) {
                        const auto primaryRef = std::find_if(
                            displayRefs.begin(), displayRefs.end(), [&](const app::StringId& ref) {
                                return ref.value == primary->value && ref.index == primary->index;
                            }
                        );
                        if (primaryRef != displayRefs.end() && primaryRef != displayRefs.begin()) {
                            std::rotate(displayRefs.begin(), primaryRef, std::next(primaryRef));
                        }
                    }
                    for (const app::StringId& ref : displayRefs) {
                        if (!refs.empty()) refs += ',';
                        refs += ref.toString();
                    }
                }
                trace->record({
                    "maker.upper",
                    "selected",
                    "face_edge_vertex_adjacency",
                    {{"target", childName},
                     {"raw", derived != nullptr ? derived->rawMappedName : std::string {}},
                     {"entryLocalRefs", refs}},
                });
            }
        }
    };
    addReverse(TopAbs_FACE, TopAbs_EDGE);
    addReverse(TopAbs_EDGE, TopAbs_VERTEX);

}

void addMakerForwardAliases(NamedShape& namedShape, const std::string& producerOperation)
{
    app::ElementMapProducerTrace* trace = !producerOperation.empty() && namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    const auto addForward = [&](TopAbs_ShapeEnum parentKind, TopAbs_ShapeEnum childKind) {
        TopTools_IndexedMapOfShape parents;
        TopExp::MapShapes(namedShape.shape, parentKind, parents);
        for (int parentIndex = 1; parentIndex <= parents.Extent(); ++parentIndex) {
            const auto parentName = logicalElementForShape(
                namedShape, parents(parentIndex), parentKind
            );
            if (!parentName) {
                continue;
            }
            const MappedNameProvenance* direct =
                selectedSourceBackedMappedNameProvenance(namedShape, *parentName);
            auto inherited = direct == nullptr
                ? firstMappedNameProvenanceForElement(namedShape, *parentName)
                : std::optional<MappedNameProvenance> {};
            if (trace != nullptr) {
                if (direct != nullptr) {
                    trace->record({
                        "element_map.find", "hit", "first_entry",
                        {{"indexed", *parentName},
                         {"raw", direct->rawMappedName},
                         {"entryLocalRefs", ""}},
                    });
                }
                else if (inherited) {
                    (void)sourceElementNames(
                        NamedShapeSource {namedShape.owner, namedShape.shape, &namedShape},
                        *parentName,
                        SourceElementMapLookup::First,
                        true,
                        false
                    );
                }
                else {
                    trace->record({
                        "element_map.find", "miss", "no_entry", {{"indexed", *parentName}},
                    });
                }
            }
            if (direct != nullptr || inherited) {
                continue;
            }
            const auto parentShape = subshapeByName(namedShape, *parentName);
            if (!parentShape) {
                continue;
            }

            std::vector<const MappedNameProvenance*> childNames;
            if (parentKind == TopAbs_FACE && childKind == TopAbs_EDGE) {
                const TopoDS_Wire outerWire = BRepTools::OuterWire(TopoDS::Face(*parentShape));
                for (TopExp_Explorer explorer(outerWire, childKind); explorer.More(); explorer.Next()) {
                    const auto childName = logicalElementForShape(namedShape, explorer.Current(), childKind);
                    const MappedNameProvenance* child = childName
                        ? selectedSourceBackedMappedNameProvenance(namedShape, *childName)
                        : nullptr;
                    if (child == nullptr) {
                        childNames.clear();
                        break;
                    }
                    childNames.push_back(child);
                }
            }
            else {
                TopExp_Explorer explorer(*parentShape, childKind);
                for (; explorer.More(); explorer.Next()) {
                    const auto childName = logicalElementForShape(namedShape, explorer.Current(), childKind);
                    const MappedNameProvenance* child = childName
                        ? selectedSourceBackedMappedNameProvenance(namedShape, *childName)
                        : nullptr;
                    if (child == nullptr) {
                        childNames.clear();
                        break;
                    }
                    childNames.push_back(child);
                }
            }
            if (childNames.empty()) {
                continue;
            }

            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), "The forward pass"
            // gets every lower mapped name, orders them with ElementNameComparator, uses the
            // first one as the base, and records `lowerPostfix()` (`:L`) plus a related StringID
            // for the remaining lower names.  These are producer ledger names; no response
            // subname or expected artifact is consulted.
            std::sort(
                childNames.begin(),
                childNames.end(),
                [](const MappedNameProvenance* left, const MappedNameProvenance* right) {
                    return compareFreeCadMappedNames(left->rawMappedName, right->rawMappedName)
                        < 0;
                }
            );
            childNames.erase(
                std::unique(
                    childNames.begin(),
                    childNames.end(),
                    [](const MappedNameProvenance* left, const MappedNameProvenance* right) {
                        return left->rawMappedName == right->rawMappedName;
                    }
                ),
                childNames.end()
            );

            std::string postfix = ";:L";
            if (childNames.size() > 1U && namedShape.stringHasher) {
                std::string related = "(";
                for (std::size_t index = 1U; index < childNames.size() && index <= 4U; ++index) {
                    if (index != 1U) {
                        related += '|';
                    }
                    related += childNames.at(index)->rawMappedName;
                }
                related += ')';
                postfix += namedShape.stringHasher->getId(related).toString();
            }
            postfix += normalizedProducerOperation(producerOperation);
            addDerivedMakerAlias(namedShape, *parentName, *childNames.front(), postfix);
        }
    };

    addForward(TopAbs_EDGE, TopAbs_VERTEX);
    addForward(TopAbs_FACE, TopAbs_EDGE);
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
            const auto ordinal = targets.historyOrdinals.find(candidate.currentElement);
            operationPostfix = operationPostfixForHistoryKind(
                historyKindIt->second,
                producerOperation,
                ordinal == targets.historyOrdinals.end() ? 1 : ordinal->second
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

bool copyLinkedMappedNameProvenance(
    NamedShape& namedShape,
    const NamedShapeSource& source,
    const std::string& sourceElement,
    const std::string& targetElement,
    const std::optional<long>& propertyTag
)
{
    if (source.namedShape == nullptr || sourceElement.empty() || targetElement.empty()
        || namedShape.elements.count(targetElement) == 0U) {
        return false;
    }

    SourceTargets targets;
    std::string sourceLocalName = targetElement;
    if (const auto targetShape = subshapeByName(namedShape, targetElement)) {
        if (const auto targetSubshape = parseSubshapeName(targetElement)) {
            if (const auto matchedSource = sourceLocalElementName(
                    source, targetSubshape->kind, *targetShape
                )) {
                sourceLocalName = *matchedSource;
            }
        }
    }
    targets.inheritedMappedName = firstMappedNameProvenanceForElement(
        *source.namedShape, sourceLocalName
    );
    if (!targets.inheritedMappedName) {
        rememberSourceTargetEvidence(targets, source, sourceElement);
    }
    if (!targets.inheritedMappedName) {
        std::string rawMappedName = sourceElement;
        const std::string ownerPrefix = source.owner.empty() ? std::string {} : source.owner + ".";
        if (!ownerPrefix.empty() && rawMappedName.rfind(ownerPrefix, 0U) == 0U) {
            rawMappedName.erase(0U, ownerPrefix.size());
        }
        targets.inheritedMappedName = resolveChildMappedNameProvenance(
            *source.namedShape, sourceLocalName, rawMappedName
        );
    }
    if (!targets.inheritedMappedName
        || targets.inheritedMappedName->status != MappedNameProvenanceStatus::SourceBacked
        || targets.inheritedMappedName->rawMappedName.empty()) {
        return false;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
    // LinkBaseExtension::checkGeoElementMap() delegates the resolved ComplexGeoData through
    // reTagElementMap(obj->getID(), ...).  The App boundary decides whether this materialized
    // Link Shape owns a property tag; Part only appends that tag to the already source-backed
    // ElementMap bytes and preserves their entry-local StringIDRefs.
    MappedNameProvenance provenance = *targets.inheritedMappedName;
    provenance.currentElement = targetElement;
    provenance.elementType = mappedNameElementType(targetElement);
    provenance.publicationScope = MappedNamePublicationScope::Public;
    if (propertyTag && !provenance.elementType.empty()) {
        provenance.rawMappedName = appendPropertyRetagMappedName(
            provenance.rawMappedName, *propertyTag, provenance.elementType
        );
        provenance.producerTag = propertyTag;
        provenance.masterTag = propertyTag;
        provenance.sourceTag = propertyTag;
    }
    provenance.canonicalMappedName =
        cad_core::topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
    const std::string entryKey = provenance.canonicalMappedName.empty()
        ? provenance.rawMappedName
        : provenance.canonicalMappedName;
    if (entryKey.empty()) {
        return false;
    }
    provenance.entryKey = entryKey;
    namedShape.elementMap[entryKey] = targetElement;
    namedShape.mappedNameProvenance[entryKey] = std::move(provenance);
    recordElementMapEntry(namedShape, entryKey, targetElement);
    return true;
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
        {"tag", childMap.tag},
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

void appendProtocolChildMapCanonicalCollisionHistory(NamedShape& namedShape)
{
    appendProtocolChildMapCanonicalCollisionHistoryImpl(namedShape);
}

std::optional<long> requestLocalProducerTagForShape(const TopoDS_Shape& shape)
{
    return requestLocalProducerTagForShapeImpl(shape);
}

std::string mappedNamePublicEvidenceSource(const MappedNameProvenance& provenance)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::findAll() returns the stored `MappedName` before any child-range postfix is
    // appended. This compatibility class is CAD Core public-DTO policy over that Part provenance,
    // not a claim that FreeCAD produces an `evidence.source` field or a collision event.
    const std::size_t postfix = provenance.rawMappedName.find(';');
    const bool producerLocal = provenance.sourceElement.find('.') == std::string::npos
        && postfix != std::string::npos
        && provenance.rawMappedName.substr(0U, postfix) == provenance.sourceElement
        && provenance.operationPostfix.rfind(";:M;", 0U) == 0U;
    // This is a legacy public DTO evidence class for native producer-local MappedName tokens.
    // The classification is based solely on the Part-owned provenance above, not a runtime
    // collector or response-level reconstruction.
    return producerLocal
            || (!provenance.rawMappedName.empty() && provenance.rawMappedName.front() == '#')
        ? "freecad_expected_collector"
        : "element_map";
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
    std::map<std::string, std::string> internalEdgeMappedNames,
    const NamedShape* rawNamedShape,
    std::shared_ptr<cad_core::app::StringHasher> stringHasher
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

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::buildShape() first produces Shape's g<ID>;SKT ElementMap, then ::buildInternals() calls
    // result.makeElementFace(edges.getSubTopoShapes(TopAbs_WIRE), "", "Part::FaceMakerBuildFace",
    // ...). The FaceMaker result therefore receives the already materialized raw Sketch map;
    // retain its raw ledger and consume it here in Vertex -> Edge order before forwarding Edge
    // evidence to the bounded InternalFace. This is producer state, not a response-time rewrite.
    namedShape.stringHasher = stringHasher;
    if (!namedShape.stringHasher && rawNamedShape != nullptr) {
        namedShape.stringHasher = rawNamedShape->stringHasher;
    }

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
    if (rawNamedShape != nullptr && namedShape.stringHasher) {
        const NamedShapeSource source {rawNamedShape->owner, rawShape, rawNamedShape};
        const std::array<std::pair<TopAbs_ShapeEnum, const char*>, 2> kinds {
            std::pair {TopAbs_VERTEX, "InternalVertex"},
            std::pair {TopAbs_EDGE, "InternalEdge"},
        };
        for (const auto& [kind, internalPrefix] : kinds) {
            TopTools_IndexedMapOfShape internalElements;
            TopExp::MapShapes(internalShape, kind, internalElements);
            for (int index = 1; index <= internalElements.Extent(); ++index) {
                const std::string internalName = std::string(internalPrefix) + std::to_string(index);
                const auto rawElement = internalMap.find(internalName);
                if (rawElement == internalMap.end() || !rawElement->is_string()) {
                    continue;
                }
                const std::string rawIndexed = rawElement->get<std::string>();
                for (const auto& [sourceName, currentElement] : rawNamedShape->elementMap) {
                    if (currentElement != rawIndexed) {
                        continue;
                    }
                    SourceTargets targets;
                    rememberSourceTargetEvidence(targets, source, sourceName);
                    if (!targets.inheritedMappedName) {
                        continue;
                    }
                    targets.sourceKind = kind;
                    targets.preserved.insert(internalName);
                    applyMakerPreservedElementMap(
                        namedShape,
                        std::map<std::string, SourceTargets> {{sourceName, std::move(targets)}}
                    );
                }
            }
        }
        // FaceMaker's no-op producer still runs makeShapeWithElementMap()'s forward V->E->F
        // completion. This creates its source-backed Face map from the preserved edge map; it
        // must happen before Pad reads the profile face and collects OCCT history.
        addMakerForwardAliases(namedShape, {});
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

NamedShape namedShapeForSketchProfileShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& profileShape,
    const NamedShape& rawNamedShape,
    std::shared_ptr<cad_core::app::StringHasher> stringHasher,
    std::function<void(const std::string&)> beforeSourceEntry
)
{
    // ProfileBased::getTopoShapeVerifiedFace() does not replace Sketch's raw wire ElementMap
    // with InternalShape's display map. FaceMakerCheese keeps the raw vertex/edge producer ledger
    // but its TopoShape is the bounded profile face.  Keep the map entries from the raw ledger
    // while indexing their current targets against the actual profile BRep; reusing rawShape's
    // ElementRecord indices here changes mapSubElement()'s Vertex -> Edge traversal and the
    // document StringHasher allocation order.
    NamedShape namedShape = indexedNamedShapeForObject(owner, profileShape);
    namedShape.stringHasher = stringHasher ? std::move(stringHasher) : rawNamedShape.stringHasher;
    if (!namedShape.stringHasher || profileShape.IsNull()) {
        return namedShape;
    }
    const nlohmann::json profileMap = app::internalElementMapForSketch(rawShape, profileShape);
    if (!profileMap.is_object()) {
        return namedShape;
    }

    // SketchObject::getInternalElementMap() maps each profile vertex/edge back to the raw Shape.
    // FreeCAD builds the profile's Part ElementMap by consuming that raw g<ID>;SKT producer
    // through mapSubElement()/encodeElementName(), which materializes the entry-local StringID
    // before later Pad M/G history reads it. Merely rebinding raw aliases skips that producer
    // lifecycle and makes Pad borrow result-side entries to recover the missing #ID chain.
    const NamedShapeSource rawSource {rawNamedShape.owner, rawShape, &rawNamedShape};
    app::ElementMapProducerTrace* profileTrace = namedShape.stringHasher->producerTrace();
    app::ElementMapProducerTrace::Scope sourceMapScope;
    if (profileTrace != nullptr) {
        sourceMapScope = profileTrace->scope(
            {"mapSubElement", "", 0, "Part::TopoShape",
             {{"operation", "FaceMaker.source_map"}, {"requiresFinalCheckpoint", true}}}
        );
        profileTrace->record({
            "toposhape.map_sub_element",
            "begin",
            "map_sub_element",
            {{"operation", ""},
             {"resultTag", "0"},
             {"sourceTag", std::to_string(rawNamedShape.producerTag.value_or(0L))}},
        });
        profileTrace->record({
            "toposhape.can_map",
            "accepted",
            "cache_ready",
            {{"resultTag", "0"},
             {"sourceTag", std::to_string(rawNamedShape.producerTag.value_or(0L))}},
        });
    }
    const std::array<std::pair<TopAbs_ShapeEnum, const char*>, 2> mappedKinds {
        std::pair {TopAbs_VERTEX, "Vertex"},
        std::pair {TopAbs_EDGE, "Edge"},
    };
    std::vector<std::pair<std::string, std::string>> splitterWrites;
    for (const auto& [kind, prefix] : mappedKinds) {
        TopTools_IndexedMapOfShape rawElements;
        TopExp::MapShapes(rawShape, kind, rawElements);
        for (int rawIndex = 1; rawIndex <= rawElements.Extent(); ++rawIndex) {
            const std::string rawIndexed = std::string(prefix) + std::to_string(rawIndex);
            std::string profileIndexed;
            for (const auto& [internalIndexed, mapped] : profileMap.items()) {
                if (mapped.is_string() && mapped.get<std::string>() == rawIndexed
                    && internalIndexed.rfind("Internal" + std::string(prefix), 0U) == 0U) {
                    profileIndexed = internalIndexed.substr(std::string("Internal").size());
                    break;
                }
            }
            if (profileIndexed.empty()) {
                continue;
            }
            // FreeCAD: SketchObject::getInternalElementMap() records one raw endpoint for the
            // profile-side IndexedName. Its Shape ElementMap may still retain a second endpoint
            // alias at a shared vertex, but FeatureExtrude receives getElementMappedName()'s
            // first ref at this Profile boundary, not a findAll expansion of both aliases.
            // FaceMaker::postBuild() iterates the incoming ElementMap directly while mapping
            // source subshapes. Native findAll events begin only when the completed map is read
            // for outer-wire combo naming, so suppress lookup instrumentation at this write path.
            for (const std::string& sourceName : sourceElementNames(
                     rawSource, rawIndexed, SourceElementMapLookup::All, false
                 )) {
                if (beforeSourceEntry) {
                    beforeSourceEntry(localElementName(sourceName));
                }
                const auto provenance = rawNamedShape.mappedNameProvenance.find(
                    localElementName(sourceName)
                );
                if (provenance == rawNamedShape.mappedNameProvenance.end()
                    || provenance->second.status != MappedNameProvenanceStatus::SourceBacked) {
                    continue;
                }
                SourceTargets targets;
                rememberSourceTargetEvidence(targets, rawSource, sourceName);
                if (!targets.inheritedMappedName) {
                    continue;
                }
                targets.sourceKind = kind;
                targets.preserved.insert(profileIndexed);
                applyMakerPreservedElementMap(
                    namedShape,
                    std::map<std::string, SourceTargets> {{sourceName, std::move(targets)}}
                );
                splitterWrites.emplace_back(sourceName, profileIndexed);
            }
        }
    }
    if (profileTrace != nullptr) {
        checkpointNamedShapeLedger(
            namedShape, owner, "toposhape.map_sub_element_checkpoint"
        );
    }
    sourceMapScope.success();
    sourceMapScope = {};
    if (profileTrace != nullptr) {
        checkpointNamedShapeLedger(
            namedShape, owner, "toposhape.map_sub_element_checkpoint"
        );
        profileTrace->record({
            "face_maker.lifecycle",
            "splitter",
            "splitter_history_available",
            nlohmann::json::object()
        });
    }
    // FaceMaker::postBuild() applies splitter history to the already source-mapped myTopoShape;
    // the second map augments that ledger instead of starting from an empty scratch ElementMap.
    NamedShape splitterMapped = namedShape;
    app::ElementMapProducerTrace::Scope splitMapScope;
    if (profileTrace != nullptr) {
        splitMapScope = profileTrace->scope(
            {"mapSubElement", "", 0, "Part::TopoShape",
             {{"operation", "FaceMaker.splitter_map"}, {"requiresFinalCheckpoint", true}}}
        );
        profileTrace->record({
            "toposhape.map_sub_element", "begin", "map_sub_element",
            {{"operation", ""}, {"resultTag", "0"}, {"sourceTag", "0"}},
        });
        profileTrace->record({
            "toposhape.can_map", "accepted", "cache_ready",
            {{"resultTag", "0"}, {"sourceTag", "0"}},
        });
    }
    for (const auto& [sourceName, target] : splitterWrites) {
        const auto rawProvenance = rawNamedShape.mappedNameProvenance.find(
            localElementName(sourceName)
        );
        if (rawProvenance == rawNamedShape.mappedNameProvenance.end()) {
            continue;
        }
        MappedNameProvenance inherited = rawProvenance->second;
        inherited.encodeInputMappedName = inherited.rawMappedName;
        inherited.rawMappedName = appendMappedNameTag(
            inherited.rawMappedName,
            rawNamedShape.producerTag.value_or(0L),
            mappedNameElementType(target)
        );
        inherited.canonicalMappedName = cad_core::topo::canonicalizeFreeCadMappedName(
            inherited.rawMappedName
        );
        inherited.elementIdRefs.clear();
        SourceTargets targets;
        targets.inheritedMappedName = std::move(inherited);
        targets.sourceElement = target;
        targets.sourceTag = 0L;
        targets.sourceKind = parseSubshapeName(target)->kind;
        targets.preserved.insert(target);
        applyMakerPreservedElementMap(
            splitterMapped,
            std::map<std::string, SourceTargets> {{sourceName, std::move(targets)}}
        );
    }
    if (profileTrace != nullptr) {
        checkpointNamedShapeLedger(
            splitterMapped, owner + ".FaceMaker", "toposhape.map_sub_element_checkpoint"
        );
    }
    splitMapScope.success();
    splitMapScope = {};
    namedShape = splitterMapped;
    // Source evidence below is selected from the outer-wire edges in the same producer layer as
    // FaceMaker::postBuild(), not from subshape display order or fixture output.
    const NamedShape& indexedProfile = namedShape;
    TopTools_IndexedMapOfShape profileEdges;
    TopExp::MapShapes(profileShape, TopAbs_EDGE, profileEdges);
    for (const auto& [faceName, faceElement] : indexedProfile.elements) {
        if (faceElement.subshape.kind != TopAbs_FACE) {
            continue;
        }
        const auto faceShape = subshapeByName(indexedProfile, faceName);
        if (!faceShape) {
            continue;
        }
        const TopoDS_Wire outerWire = BRepTools::OuterWire(TopoDS::Face(*faceShape));
        std::vector<std::string> rawEdgeCandidates;
        for (TopExp_Explorer explorer(outerWire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            const int profileEdgeIndex = profileEdges.FindIndex(explorer.Current());
            if (profileEdgeIndex <= 0) {
                continue;
            }
            for (const auto& [sourceName, currentElement] : namedShape.elementMap) {
                if (currentElement != "Edge" + std::to_string(profileEdgeIndex)) {
                    continue;
                }
                const auto provenance = namedShape.mappedNameProvenance.find(sourceName);
                if (provenance == namedShape.mappedNameProvenance.end()
                    || provenance->second.status != MappedNameProvenanceStatus::SourceBacked) {
                    continue;
                }
                addDistinctString(rawEdgeCandidates, sourceName);
            }
        }
        if (rawEdgeCandidates.empty()) {
            continue;
        }
        std::sort(
            rawEdgeCandidates.begin(),
            rawEdgeCandidates.end(),
            [](const std::string& left, const std::string& right) {
                return compareFreeCadMappedNames(left, right) < 0;
            }
        );

        // FaceMaker::postBuild() first maps the selected source edge into its own TopoShape,
        // then setElementComboName(FaceN, names, marker=";", op="") hashes that mapped edge
        // and writes the Face producer alias. Reuse the same Part helpers in a request-local
        // scratch ledger and retain only the resulting Face evidence on the profile ledger.
        const std::string& sourceName = rawEdgeCandidates.front();
        const auto sourceProvenance = namedShape.mappedNameProvenance.find(sourceName);
        if (sourceProvenance == namedShape.mappedNameProvenance.end()) {
            continue;
        }
        int profileEdgeIndex = 0;
        TopExp_Explorer explorer(outerWire, TopAbs_EDGE);
        if (explorer.More()) {
            profileEdgeIndex = profileEdges.FindIndex(explorer.Current());
        }
        if (profileEdgeIndex <= 0) {
            continue;
        }
        app::ElementMapProducerTrace::Scope wireMapScope;
        NamedShape outerWireMapped = indexedNamedShapeForObject(
            owner + ".OuterWire", outerWire
        );
        outerWireMapped.stringHasher = namedShape.stringHasher;
        if (profileTrace != nullptr) {
            wireMapScope = profileTrace->scope(
                {"mapSubElement", "", 0, "Part::TopoShape",
                 {{"operation", "FaceMaker.outer_wire_map"},
                  {"requiresFinalCheckpoint", true}}}
            );
            profileTrace->record({
                "toposhape.map_sub_element", "begin", "map_sub_element",
                {{"operation", ""}, {"resultTag", "0"}, {"sourceTag", "0"}},
            });
            profileTrace->record({
                "toposhape.can_map", "accepted", "cache_ready",
                {{"resultTag", "0"}, {"sourceTag", "0"}},
            });
        }
        const NamedShapeSource splitterSource {
            splitterMapped.owner, splitterMapped.shape, &splitterMapped
        };
        for (const TopAbs_ShapeEnum kind : {TopAbs_VERTEX, TopAbs_EDGE}) {
            TopTools_IndexedMapOfShape outerElements;
            TopTools_IndexedMapOfShape profileElements;
            TopExp::MapShapes(outerWire, kind, outerElements);
            TopExp::MapShapes(profileShape, kind, profileElements);
            for (int outerIndex = 1; outerIndex <= outerElements.Extent(); ++outerIndex) {
                const int sourceIndex = profileElements.FindIndex(outerElements(outerIndex));
                if (sourceIndex <= 0) {
                    continue;
                }
                const std::string prefix = kind == TopAbs_VERTEX ? "Vertex" : "Edge";
                const std::string sourceIndexed = prefix + std::to_string(sourceIndex);
                const std::string targetIndexed = prefix + std::to_string(outerIndex);
                for (const std::string& inheritedName : sourceElementNames(
                         splitterSource, sourceIndexed
                     )) {
                    std::string inheritedKey = inheritedName;
                    const std::string ownerPrefix = splitterSource.owner + ".";
                    if (inheritedKey.rfind(ownerPrefix, 0U) == 0U) {
                        inheritedKey.erase(0U, ownerPrefix.size());
                    }
                    const auto inherited = splitterMapped.mappedNameProvenance.find(inheritedKey);
                    if (inherited == splitterMapped.mappedNameProvenance.end()) {
                        continue;
                    }
                    SourceTargets targets;
                    targets.inheritedMappedName = inherited->second;
                    targets.sourceElement = sourceIndexed;
                    targets.sourceTag = 0L;
                    targets.sourceKind = kind;
                    targets.preserved.insert(targetIndexed);
                    applyMakerPreservedElementMap(
                        outerWireMapped,
                        std::map<std::string, SourceTargets> {
                            {inheritedKey, std::move(targets)}
                        }
                    );
                }
            }
        }
        if (profileTrace != nullptr) {
            checkpointNamedShapeLedger(
                outerWireMapped, owner + ".OuterWire", "toposhape.map_sub_element_checkpoint"
            );
        }
        wireMapScope.success();
        wireMapScope = {};
        const NamedShapeSource outerWireSource {
            outerWireMapped.owner, outerWireMapped.shape, &outerWireMapped
        };
        const MappedNameProvenance* mappedEdge = nullptr;
        for (int edgeIndex = 1; edgeIndex <= profileEdges.Extent(); ++edgeIndex) {
            const std::string indexed = "Edge" + std::to_string(edgeIndex);
            const std::vector<std::string> first = sourceElementNames(
                outerWireSource, indexed, SourceElementMapLookup::First
            );
            if (first.empty()) {
                continue;
            }
            std::string key = first.front();
            const std::string prefix = outerWireSource.owner + ".";
            if (key.rfind(prefix, 0U) == 0U) {
                key.erase(0U, prefix.size());
            }
            const auto candidate = outerWireMapped.mappedNameProvenance.find(key);
            if (candidate != outerWireMapped.mappedNameProvenance.end()) {
                mappedEdge = &candidate->second;
            }
        }
        if (mappedEdge == nullptr) {
            continue;
        }
        addDerivedMakerAlias(namedShape, faceName, *mappedEdge, ";");
        if (profileTrace != nullptr) {
            checkpointNamedShapeLedger(namedShape, owner, "face_maker.final_checkpoint");
        }
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
    namedShape.producerTag = options.producerTag;
    namedShape.stringHasher = options.stringHasher;
    if (!namedShape.stringHasher) {
        for (const NamedShapeSource& source : sources) {
            if (source.namedShape != nullptr && source.namedShape->stringHasher) {
                namedShape.stringHasher = source.namedShape->stringHasher;
                break;
            }
        }
    }
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    app::ElementMapProducerTrace::Scope makerScope;
    const long primarySourceTag = sources.size() > 1U && !sources.empty()
        ? sources.front().producerTag.value_or(
              sources.front().namedShape && sources.front().namedShape->producerTag
                  ? *sources.front().namedShape->producerTag
                  : 0L
          )
        : 0L;
    if (trace != nullptr && options.emitMakerScopes) {
        makerScope = trace->scope(
            {"makeShapeWithElementMap",
             "",
             0,
             // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
             // ::makeShapeWithElementMap() is the producer; opcodes such as "XTR" describe the
             // mapping operation and must not replace the Part::TopoShape producer identity.
             "Part::TopoShape",
             {{"operation", options.producerOperation},
              {"inputCount", sources.size()},
              {"policy", "map"},
              {"requiresFinalCheckpoint", true}}}
        );
        trace->record({
            "maker.begin",
            "begin",
            "make_shape_with_element_map",
            {{"outputTag", std::to_string(options.producerTag.value_or(0L))},
             {"operation", options.producerOperation},
             {"inputCount", std::to_string(sources.size())},
             {"elementMapPolicy", "preserve"}},
        });
        trace->record({
            "toposhape.set_shape", "begin", "reset_requested",
            {{"incomingNull", resultShape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", std::to_string(options.producerTag.value_or(0L))}},
        });
        checkpointNamedShapeLedger(
            namedShape, owner + ":reset", "toposhape.set_shape_checkpoint"
        );
        trace->record({
            "toposhape.can_map", "accepted", "cache_ready",
            {{"resultTag", std::to_string(options.producerTag.value_or(0L))},
             {"sourceTag", std::to_string(primarySourceTag)}},
        });
        if (sources.size() > 1U) {
            for (std::size_t index = 1U; index < sources.size(); ++index) {
                const NamedShapeSource& source = sources[index];
                const long sourceTag = source.producerTag.value_or(
                    source.namedShape && source.namedShape->producerTag
                        ? *source.namedShape->producerTag
                        : 0L
                );
                trace->record({
                    "toposhape.can_map",
                    "accepted",
                    "cache_ready",
                    {{"resultTag", std::to_string(options.producerTag.value_or(0L))},
                     {"sourceTag", std::to_string(sourceTag)}},
                });
            }
        }
    }
    const RawMakerHistoryCapture rawMakerHistory =
        captureRawMakerHistory(sources, resultShape, maker);
    if (trace != nullptr) {
        try {
            const nlohmann::json rawMapper = inspectRawMakerMapper(rawMakerHistory);
            (void)trace->checkpoint(
                {"mapper",
                 {{"raw", rawMapper},
                  {"rawCanonicalSha256",
                   app::ElementMapProducerTrace::canonicalSha256(rawMapper)}},
                 {},
                 {},
                 {},
                 "mapper.snapshot"}
            );
        }
        catch (const Standard_Failure& failure) {
            trace->record({
                "mapper.snapshot",
                "exception",
                "raw_mapper_inspection_failed",
                {{"message", failure.GetMessageString() != nullptr
                                  ? failure.GetMessageString()
                                  : "Standard_Failure"}},
            });
            makerScope.exception("raw_mapper_inspection_failed");
        }
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() starts with
    // mapSubElement(shapes).  It consumes an incoming ElementMap entry's own StringIDRefs only
    // when that entry reaches a preserved or mapper-history encode site; it does not pre-intern
    // every source name.  Pre-interning allocates document-wide IDs for source elements that
    // never participate in this producer and shifts later mapped-name IDs.
    std::map<std::string, SourceTargets> sourceTargets;
    std::size_t mapSubElementOrder = 0U;
    const std::string& producerOperation = options.producerOperation;
    const bool compoundPartnerChildren = directCompoundChildrenPartnerSources(resultShape, sources);
    if (compoundPartnerChildren && !sources.empty()) {
        namedShape.producerTag = sources.front().producerTag
            ? sources.front().producerTag
            : (sources.front().namedShape != nullptr
                   ? sources.front().namedShape->producerTag
                   : options.producerTag);
    }
    // First phase of FreeCAD makeShapeWithElementMap(): mapSubElement(shapes) walks the input
    // ledger in Vertex -> Edge -> Face order and writes every preserved alias before mapper
    // history is queried. Keep this as a separate phase: merely priming IDs is insufficient,
    // because ElementMap::setElementName() also fixes the producer StringIDRef relationship.
    for (const auto& source : sources) {
        const long sourceTraceTag = source.producerTag.value_or(
            source.namedShape && source.namedShape->producerTag
                ? *source.namedShape->producerTag
                : 0L
        );
        std::map<std::string, std::set<std::string>> resultPreservedTargets;
        std::map<std::string, SourceTargets> sourceMapTargets;
        app::ElementMapProducerTrace::Scope mapSubElementScope;
        if (trace != nullptr && options.emitMakerScopes) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::mapSubElement(const std::vector<TopoShape>&) calls mapSubElement(shape) once per
            // ordered source, so a two-input Boolean publishes two sibling producer scopes.
            mapSubElementScope = trace->scope(
                {"mapSubElement", "", compoundPartnerChildren ? sourceTraceTag : 0, "Part::TopoShape",
                 {{"operation", producerOperation},
                  {"sourceOwner", source.owner},
                  {"requiresFinalCheckpoint", true}}}
            );
            trace->record({
                "toposhape.map_sub_element", "begin", "map_sub_element",
                {{"operation", ""},
                 {"resultTag", std::to_string(compoundPartnerChildren
                                                   ? sourceTraceTag
                                                   : options.producerTag.value_or(0L))},
                 {"sourceTag", std::to_string(sourceTraceTag)}},
            });
        }
        const bool sourceIsPartner = namedShape.shape.IsPartner(source.shape);
        if (trace != nullptr && options.emitMakerScopes) {
            trace->record({
                "toposhape.can_map",
                source.shape.IsNull() || resultShape.IsNull() ? "rejected" : "accepted",
                source.shape.IsNull() || resultShape.IsNull() ? "shape_missing" : "cache_ready",
                {{"sourceTag", std::to_string(sourceTraceTag)},
                 {"resultTag", std::to_string(compoundPartnerChildren
                                                   ? sourceTraceTag
                                                   : options.producerTag.value_or(0L))}},
            });
        }
        const bool mapsReceiverCopy = sources.size() > 2U && options.producerTag
            && sourceTraceTag == *options.producerTag;
        if (trace != nullptr && options.emitMakerScopes && source.namedShape != nullptr
            && mapsReceiverCopy) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::mapSubElement() recursively maps each receiver
            // copy in the multi-source vector produced by FeatureTransformed before the parent
            // writes its resolved entries. Preserve that nested lifecycle for this receiver-copy
            // form; an ordinary two-input Fuse/Cut keeps sibling mapSubElement scopes.
            auto childMapScope = trace->scope(
                {"toposhape.map_sub_element",
                 "",
                 sourceTraceTag,
                 "Part::TopoShape",
                 nlohmann::json::object()}
            );
            trace->record({
                "toposhape.map_sub_element", "begin", "map_sub_element",
                {{"operation", ""},
                 {"resultTag", std::to_string(sourceTraceTag)},
                 {"sourceTag", std::to_string(sourceTraceTag)}},
            });
            trace->record({
                "toposhape.can_map", "accepted", "cache_ready",
                {{"resultTag", std::to_string(sourceTraceTag)},
                 {"sourceTag", std::to_string(sourceTraceTag)}},
            });
            (void)checkpointNamedShapeLedger(
                *source.namedShape,
                owner + ":mapSubElement-child:" + source.owner,
                "toposhape.map_sub_element_checkpoint"
            );
        }
        for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                // TopoShapeExpansion.cpp::TopoShape::mapSubElement(), first computes `idx` with
                // `shapeMap.find(...)` and `continue`s when it is zero; only then does it call
                // `other.getElementMappedNames(...)`. A CUT tool with no preserved subshape must
                // therefore publish no findAll events or source-map writes.
                const auto preservedElement =
                    findElementName(namedShape, sourceElement, kind, false, false);
                if (!preservedElement) {
                    continue;
                }
                const std::vector<std::string> allSourceNames = sourceElementNames(
                    source,
                    localElementName,
                    SourceElementMapLookup::All,
                    sources.size() > 1U
                );
                for (std::size_t sourceNameIndex = 0;
                     sourceNameIndex < allSourceNames.size(); ++sourceNameIndex) {
                    const std::string& sourceName = allSourceNames[sourceNameIndex];
                    SourceTargets& targets = sourceTargets[sourceName];
                    if (targets.mapSubElementOrder == std::numeric_limits<std::size_t>::max()) {
                        targets.mapSubElementOrder = mapSubElementOrder;
                    }
                    ++mapSubElementOrder;
                    if (!targets.sourceKind) {
                        targets.sourceKind = kind;
                    }
                    rememberSourceTargetEvidence(
                        targets,
                        source,
                        sourceName
                    );
                    targets.preserved.insert(*preservedElement);
                    if (sourceNameIndex != 0U) {
                        continue;
                    }
                    resultPreservedTargets[sourceName] = targets.preserved;
                    SourceTargets sourceMapTarget = targets;
                    if (sources.size() == 1U) {
                        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                        // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() performs
                        // `mapSubElement(shapes)` before Mapper M/G consumption. For one incoming
                        // shape the source-index write is published first; the ancestry target in
                        // `targets` is replayed by the completed-map pass below. Multi-input
                        // Booleans apply each source ancestry result directly and must not inject
                        // a same-text EdgeN write ahead of the next source scope.
                        sourceMapTarget.preserved.clear();
                        if (namedShape.elements.count(localElementName) != 0U) {
                            sourceMapTarget.preserved.insert(localElementName);
                        }
                    }
                    sourceMapTargets[sourceName] = std::move(sourceMapTarget);
                    if (sources.size() > 1U && !producerOperation.empty()) {
                        applyMakerPreservedElementMap(
                            namedShape,
                            std::map<std::string, SourceTargets> {
                                {sourceName, sourceMapTargets.at(sourceName)}
                            },
                            producerOperation,
                            options.rehashPreservedMappedName,
                            options.preserveRawMappedName
                        );
                    }
                    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                    // TopoShapeExpansion.cpp::TopoShape::mapSubElement(const TopoShape&, ...)
                    // takes `copyElementMap(other, op)` only when the *receiver Shape* is a
                    // partner of the incoming Shape and has no existing ElementMap. A matching
                    // Edge/Face in mapSubElementTypeForShape() is not that condition: it still
                    // calls encodeElementName() with the entry-local refs. Do not promote a
                    // per-element IsSame match into copyElementMap-style raw retention here.
                    targets.partnerShape = targets.partnerShape || sourceIsPartner;
                    targets.rehashPreservedMappedName = targets.rehashPreservedMappedName
                        || (options.preserveRawMappedName && !sourceIsPartner);
                }
            }
        }
        if (sources.size() == 1U && !producerOperation.empty() && !compoundPartnerChildren) {
            applyMakerPreservedElementMap(
                namedShape,
                sourceMapTargets,
                producerOperation,
                options.rehashPreservedMappedName,
                options.preserveRawMappedName
            );
        }
        if (trace != nullptr) {
            checkpointNamedShapeLedger(
                namedShape,
                owner + ":mapSubElement:" + source.owner,
                "toposhape.map_sub_element_checkpoint"
            );
        }
        if (sources.size() == 1U) {
            // ElementMap::copyElementMap() immediately replays the completed single-source map
            // through findAll(). A multi-input Boolean instead accumulates each mapSubElement()
            // call on the same receiver and must not reset/replay between sources.
            NamedShape sourceMappedLedger = namedShape;
            namedShape = indexedNamedShapeForObject(owner, resultShape);
            namedShape.stringHasher = sourceMappedLedger.stringHasher;
            namedShape.producerTag = options.producerTag;
            const NamedShapeSource replaySource {
                sourceMappedLedger.owner, sourceMappedLedger.shape, &sourceMappedLedger
            };
            for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
                const std::string prefix = prefixForKind(kind);
                TopTools_IndexedMapOfShape replayElements;
                TopExp::MapShapes(source.shape, kind, replayElements);
                for (int index = 1; index <= replayElements.Extent(); ++index) {
                    const std::string indexed = prefix + std::to_string(index);
                    const std::vector<std::string> replayNames = sourceElementNames(
                        replaySource, indexed, SourceElementMapLookup::All
                    );
                    if (replayNames.empty()) {
                        continue;
                    }
                    std::string replayKey = replayNames.front();
                    const std::string ownerPrefix = replaySource.owner + ".";
                    if (replayKey.rfind(ownerPrefix, 0U) == 0U) {
                        replayKey.erase(0U, ownerPrefix.size());
                    }
                    const auto provenance = sourceMappedLedger.mappedNameProvenance.find(replayKey);
                    if (provenance == sourceMappedLedger.mappedNameProvenance.end()) {
                        continue;
                    }
                    SourceTargets replayTargets;
                    replayTargets.inheritedMappedName = provenance->second;
                    replayTargets.sourceElement = indexed;
                    replayTargets.sourceTag = 0L;
                    replayTargets.sourceKind = kind;
                    const auto mappedTargets = resultPreservedTargets.find(replayKey);
                    if (mappedTargets != resultPreservedTargets.end()) {
                        replayTargets.preserved = mappedTargets->second;
                    }
                    if (replayTargets.preserved.empty()) {
                        continue;
                    }
                    applyMakerPreservedElementMap(
                        namedShape,
                        std::map<std::string, SourceTargets> {
                            {replayKey, std::move(replayTargets)}
                        }
                    );
                }
            }
            if (trace != nullptr) {
                checkpointNamedShapeLedger(
                    namedShape,
                    owner + ":mapSubElement-replay:" + source.owner,
                    "toposhape.map_sub_element_checkpoint"
                );
            }
        }
        mapSubElementScope.success();
        mapSubElementScope = {};
        if (trace != nullptr && (sources.size() == 1U || &source == &sources.back())) {
            checkpointNamedShapeLedger(
                namedShape,
                owner + ":mapSubElement-complete:" + source.owner,
                "toposhape.map_sub_element_checkpoint"
            );
        }
        if (compoundPartnerChildren) {
            break;
        }
    }
    if (compoundPartnerChildren) {
        // FreeCAD producer trace around TopoShapeExpansion.cpp::mapSubElement(vector) exposes
        // the first child's normal mapSubElement call before the direct-child range publication.
        // `setMappedChildElements(children)` replaces that temporary flat receiver map: the
        // compound parent remains indexed-only and all source entries live in nested ledgers.
        const auto compoundHasher = namedShape.stringHasher;
        namedShape = indexedNamedShapeForObject(owner, resultShape);
        namedShape.stringHasher = compoundHasher;
        namedShape.producerTag = options.producerTag;
        std::size_t childRanges = 0U;
        for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
            for (const NamedShapeSource& source : sources) {
                if (subshapeCount(source.shape, kind) > 0) {
                    ++childRanges;
                }
            }
        }
        if (trace != nullptr) {
            trace->record({
                "child_map", "begin", "add_child_elements",
                {{"count", std::to_string(childRanges)},
                 {"masterTag", std::to_string(options.producerTag.value_or(0L))}},
            });
            for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
                for (const NamedShapeSource& source : sources) {
                    if (subshapeCount(source.shape, kind) == 0) {
                        continue;
                    }
                    const long childTag = source.producerTag.value_or(
                        source.namedShape && source.namedShape->producerTag
                            ? *source.namedShape->producerTag
                            : 0L
                    );
                    const char elementType = prefixForKind(kind).front();
                    std::ostringstream encoded;
                    encoded << ";:H";
                    if (childTag < 0) {
                        encoded << '-';
                    }
                    encoded << std::hex << std::abs(childTag) << ',' << elementType;
                    trace->record({
                        "element_map.encode", "encoded", "forced_tag",
                        {{"before", ""},
                         {"after", encoded.str()},
                         {"elementType", std::string(1U, elementType)},
                         {"entryLocalRefs", ""},
                         {"inputTag", std::to_string(childTag)},
                         {"masterTag", std::to_string(options.producerTag.value_or(0L))}},
                    });
                }
            }
        }
        collectChildElementMaps(namedShape, resultShape, sources);
        if (trace != nullptr) {
            checkpointNamedShapeLedger(namedShape, owner + ":child-map", "child_map_checkpoint");
            trace->record({
                "toposhape.map_sub_element", "preserved", "compound_partner_child_map",
                {{"operation", ""},
                 {"sourceCount", std::to_string(sources.size())},
                 {"childRanges", std::to_string(childRanges)}},
            });
            checkpointNamedShapeLedger(
                namedShape, owner + ":compound-map", "toposhape.map_sub_element_checkpoint"
            );
            checkpointNamedShapeLedger(
                namedShape, owner + ":compound-map-preserve", "maker.preserve.checkpoint"
            );
        }
    }
    if (!producerOperation.empty() && !compoundPartnerChildren) {
        if (trace != nullptr) {
            checkpointNamedShapeLedger(
                namedShape,
                owner + ":preserve",
                "maker.preserve.checkpoint"
            );
        }
    }
    // Second phase: FreeCAD gathers OCCT Modified and Generated candidates only after the
    // preserved map is present. makeShapeWithElementMap() owns ShapeInfo in Vertex -> Edge ->
    // Face order and, for each kind, visits every incoming TopoShape. Do not invert these loops:
    // a two-input Boolean uses this order to decide NameKey winners and StringIDRef lifetimes.
    for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
        for (std::size_t sourceOrdinal = 0; sourceOrdinal < sources.size(); ++sourceOrdinal) {
            const auto& source = sources[sourceOrdinal];
            if (trace != nullptr) {
                const long sourceTraceTag = source.producerTag.value_or(
                    source.namedShape && source.namedShape->producerTag
                        ? *source.namedShape->producerTag
                        : 0L
                );
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), inside each
                // Vertex/Edge/Face pass, calls canMapElement(incomingShape) for every ordered
                // input. The observable tag is the incoming TopoShape tag, not a synthetic zero.
                trace->record({
                    "toposhape.can_map", "accepted", "cache_ready",
                    {{"resultTag", std::to_string(options.producerTag.value_or(0L))},
                     {"sourceTag", std::to_string(sourceTraceTag)}},
                });
            }
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(
                         source, localElementName, SourceElementMapLookup::First
                     )) {
                    // The preserved pass intentionally skips getElementMappedNames() when the
                    // source TShape is absent from the result. Mapper::Modified/Generated still
                    // consumes that source later, so initialize its producer evidence here.
                    rememberSourceTargetEvidence(sourceTargets[sourceName], source, sourceName);
                    try {
                        const RawMakerHistoryEntry* rawHistory = findRawMakerHistoryEntry(
                            rawMakerHistory,
                            sourceOrdinal,
                            kind,
                            index
                        );
                        if (rawHistory == nullptr || !rawHistory->modifiedError.empty()) {
                            continue;
                        }
                        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                        // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() consumes
                        // Mapper::modified() before Mapper::generated().  When OCCT reports the
                        // same source/target through both lists, that ordering controls the final
                        // NameInfo relation and the document StringID lifecycle.
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            rawHistory->modified,
                            ElementHistoryKind::Modified,
                            sourceTargets
                        );
                        if (!rawHistory->generatedError.empty()) {
                            continue;
                        }
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            rawHistory->generated,
                            ElementHistoryKind::Generated,
                            sourceTargets
                        );
                        if (trace != nullptr) {
                            const auto emitCandidates = [&](const TopTools_ListOfShape& values,
                                                            const char* slice,
                                                            const char* reason) {
                                std::size_t ordinal = 0U;
                                for (TopTools_ListIteratorOfListOfShape iterator(values);
                                     iterator.More(); iterator.Next()) {
                                    std::vector<std::string> targetNames;
                                    const auto directTarget = logicalElementForShape(
                                        namedShape,
                                        iterator.Value(),
                                        iterator.Value().ShapeType()
                                    );
                                    if (directTarget) {
                                        targetNames.push_back(*directTarget);
                                    }
                                    else {
                                        TopTools_IndexedMapOfShape expanded;
                                        TopExp::MapShapes(iterator.Value(), kind, expanded);
                                        for (int member = 1; member <= expanded.Extent(); ++member) {
                                            const auto target = logicalElementForShape(
                                                namedShape, expanded(member), kind
                                            );
                                            if (target && std::find(
                                                    targetNames.begin(), targetNames.end(), *target
                                                ) == targetNames.end()) {
                                                targetNames.push_back(*target);
                                            }
                                        }
                                    }
                                    const auto sourceEvidence = sourceTargets.find(sourceName);
                                    std::string raw;
                                    std::string refs;
                                    if (sourceEvidence != sourceTargets.end()
                                        && sourceEvidence->second.inheritedMappedName) {
                                        raw = sourceEvidence->second.inheritedMappedName
                                                  ->rawMappedName;
                                        for (const app::StringId& ref : sourceEvidence->second
                                                 .inheritedMappedName->elementIdRefs) {
                                            if (!refs.empty()) refs += ',';
                                            refs += ref.toString();
                                        }
                                    }
                                    if (std::string(slice) == "maker.generated"
                                        && kind == TopAbs_FACE && !targetNames.empty()
                                        && !directTarget) {
                                        trace->record({
                                            "maker.parallel_coplanar",
                                            "examined",
                                            "high_level_generated_shape",
                                            {{"source", raw},
                                             {"shapeOffset", "3"},
                                             {"parallelOrdinal", "6"},
                                             {"coplanarOrdinal", "5"}},
                                        });
                                    }
                                    for (const std::string& target : targetNames) {
                                        ++ordinal;
                                    auto inheritedExisting = firstMappedNameProvenanceForElement(
                                        namedShape, target
                                    );
                                    const MappedNameProvenance* existing =
                                        selectedSourceBackedMappedNameProvenance(namedShape, target);
                                    if (existing == nullptr && inheritedExisting) {
                                        existing = &*inheritedExisting;
                                    }
                                    std::string existingRefs;
                                    trace->record({
                                        "element_map.find",
                                        existing == nullptr ? "miss" : "hit",
                                        existing == nullptr ? "no_entry" : "first_entry",
                                        existing == nullptr
                                            ? nlohmann::json{{"indexed", target}}
                                            : nlohmann::json{{"indexed", target},
                                                             {"raw", existing->rawMappedName},
                                                             {"entryLocalRefs", existingRefs}},
                                    });
                                    if (existing != nullptr) {
                                        trace->record({
                                            "maker.candidate.reject",
                                            "rejected",
                                            "target_already_named",
                                            {{"source", raw}, {"target", target}},
                                        });
                                    }
                                    else {
                                        trace->record({
                                            slice, "candidate_collected", reason,
                                            {{"source", raw},
                                             {"target", target},
                                             {"ordinal", std::to_string(ordinal)},
                                             {"entryLocalRefs", refs}},
                                        });
                                    }
                                    }
                                }
                            };
                            emitCandidates(
                                rawHistory->modified,
                                "maker.modified",
                                "mapper_modified_target_in_output"
                            );
                            emitCandidates(
                                rawHistory->generated,
                                "maker.generated",
                                "mapper_generated_target_in_output"
                            );
                        }
                    }
                    catch (const Standard_Failure&) {
                        continue;
                    }
                }
            }
        }
    }
    if (producerOperation.empty()) {
        applyHistoryElementMap(
            namedShape,
            sourceTargets,
            producerOperation,
            options.recordUnmappedSourceDeletions
        );
    }
    else {
        applyMakerHistoryElementMap(
            namedShape,
            sourceTargets,
            producerOperation,
            options.recordUnmappedSourceDeletions,
            options.promoteBareSourceIdForGenerated
        );
        addMakerReverseAliases(namedShape, producerOperation);
        addMakerForwardAliases(namedShape, producerOperation);
        if (trace != nullptr) {
            trace->record({
                "maker.end",
                "success",
                "naming_complete",
                {{"operation",
                  normalizedProducerOperation(producerOperation).empty()
                      ? std::string {}
                      : normalizedProducerOperation(producerOperation).substr(1U)}},
            });
        }
    }
    if (options.addProducerLocalAliases) {
        addUnambiguousProducerLocalHistoryAliases(
            namedShape,
            sourceTargets,
            producerOperation
        );
    }
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    appendPartCanonicalCollisionHistory(namedShape, sources);

    if (trace != nullptr) {
        (void)checkpointNamedShapeLedger(
            namedShape,
            owner + ":final",
            "maker.final_checkpoint"
        );
    }

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
    for (const NamedShapeSource& source : sources) {
        if (source.namedShape != nullptr && source.namedShape->stringHasher) {
            namedShape.stringHasher = source.namedShape->stringHasher;
            break;
        }
    }
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
    const RawMakerHistoryCapture& rawMakerHistory
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MyRefineMaker::populate(), "mapper.populate(MappingStatus::Modified, it.Key(),
    // it.Value())"; ::TopoShape::makeElementRefine() then calls "mapper.init(shape,
    // mkRefine.Shape())" before makeShapeWithElementMap().
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    if (source.namedShape != nullptr) {
        namedShape.stringHasher = source.namedShape->stringHasher;
    }
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    app::ElementMapProducerTrace::Scope makerScope;
    if (trace != nullptr) {
        makerScope = trace->scope(
            {"makeShapeWithElementMap", "", 0, "Part::TopoShape",
             {{"operation", "RFI"}, {"inputCount", 1}, {"requiresFinalCheckpoint", true}}}
        );
        trace->record({
            "maker.begin",
            "begin",
            "make_shape_with_element_map",
            {{"elementMapPolicy", "preserve"},
             {"inputCount", "1"},
             {"operation", "RFI"},
             {"outputTag", "0"}},
        });
        trace->record({
            "toposhape.set_shape",
            "begin",
            "reset_requested",
            {{"incomingNull", resultShape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", "0"}},
        });
        checkpointNamedShapeLedger(
            namedShape, owner + ":refine-reset", "toposhape.set_shape_checkpoint"
        );
        trace->record({
            "toposhape.can_map",
            "accepted",
            "cache_ready",
            {{"resultTag", "0"}, {"sourceTag", "0"}},
        });
        const nlohmann::json rawMapper = inspectRawMakerMapper(rawMakerHistory);
        (void)trace->checkpoint(
            {"mapper",
             {{"raw", rawMapper},
              {"rawCanonicalSha256", app::ElementMapProducerTrace::canonicalSha256(rawMapper)}},
             {},
             {},
             {},
             "mapper.snapshot"}
        );
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementRefine() calls
    // makeShapeWithElementMap(mkRefine.Shape(), mapper, {shape}, OpCodes::Refine).
    // Refine is consequently a normal producer boundary: mapSubElement() preserves the
    // incoming names first, then Modified/Generated candidates are encoded with RFI.  Treating
    // this as an operation-free inherited map leaks the preceding Boolean's FUS identity into
    // the completed PartDesign feature.
    constexpr const char* refineOperation = "RFI";
    std::map<std::string, SourceTargets> sourceTargets;
    std::size_t mapSubElementOrder = 0U;
    app::ElementMapProducerTrace::Scope mapScope;
    if (trace != nullptr) {
        mapScope = trace->scope(
            {"toposhape.map_sub_element", "", 0, "Part::TopoShape",
             {{"operation", "RFI"}, {"requiresFinalCheckpoint", true}}}
        );
        trace->record({
            "toposhape.map_sub_element",
            "begin",
            "map_sub_element",
            {{"operation", ""}, {"resultTag", "0"}, {"sourceTag", "0"}},
        });
        trace->record({
            "toposhape.can_map",
            "accepted",
            "cache_ready",
            {{"resultTag", "0"}, {"sourceTag", "0"}},
        });
    }

    // This is the mapSubElement({shape}) phase of makeShapeWithElementMap().  It must precede
    // history collection and keep Vertex -> Edge -> Face order, because the associated
    // StringIDRefs are also the source of later RFI Modified/Generated names.
    for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
        const std::string prefix = prefixForKind(kind);
        TopTools_IndexedMapOfShape sourceElements;
        TopExp::MapShapes(source.shape, kind, sourceElements);
        for (int index = 1; index <= sourceElements.Extent(); ++index) {
            const TopoDS_Shape& sourceElement = sourceElements(index);
            const std::string localElementName = prefix + std::to_string(index);
            for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                SourceTargets& targets = sourceTargets[sourceName];
                if (targets.mapSubElementOrder == std::numeric_limits<std::size_t>::max()) {
                    targets.mapSubElementOrder = mapSubElementOrder;
                }
                ++mapSubElementOrder;
                if (!targets.sourceKind) {
                    targets.sourceKind = kind;
                }
                rememberSourceTargetEvidence(targets, source, sourceName);
                collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                // FreeCAD mapSubElement() consumes each ElementMap::findAll() result
                // immediately: find_all -> encode -> setElementName, then advances to the next
                // IndexedName. Deferring all writes until after collection changes both the
                // observable producer order and the ledger snapshot chain.
                applyMakerPreservedElementMap(
                    namedShape,
                    std::map<std::string, SourceTargets> {{sourceName, targets}},
                    refineOperation
                );
            }
        }
    }
    std::string mapSubElementSnapshot;
    if (trace != nullptr) {
        mapSubElementSnapshot = checkpointNamedShapeLedger(
            namedShape,
            owner + ":refine-map",
            "toposhape.map_sub_element_checkpoint"
        );
    }
    mapScope.success();
    mapScope = {};
    // A shapeOffset=3 marker belongs to the producer that first reported the high-level
    // generated candidate. Once Refine's mapSubElement() preserves that entry into its fresh
    // ElementMap, it is an established name and participates in this producer's reverse pass.
    for (auto& [name, provenance] : namedShape.mappedNameProvenance) {
        (void)name;
        provenance.delayedHighLevel = false;
    }
    if (trace != nullptr) {
        trace->record({
            "toposhape.map_sub_element_checkpoint",
            "published",
            "",
            {{"snapshot", mapSubElementSnapshot}},
            mapSubElementSnapshot,
            mapSubElementSnapshot,
        });
        trace->record({
            "maker.preserve.checkpoint",
            "published",
            "",
            {{"snapshot", mapSubElementSnapshot}},
            mapSubElementSnapshot,
            mapSubElementSnapshot,
        });
        trace->record({
            "toposhape.can_map",
            "accepted",
            "cache_ready",
            {{"resultTag", "0"}, {"sourceTag", "0"}},
        });
    }

    // MyRefineMaker::populate() passes its BRepBuilderAPI_RefineModel Modified map into the
    // same GenericShapeMapper consumed by makeShapeWithElementMap().  Collect it with the
    // regular maker traversal so same-target candidates, U/L completion and deletion handling
    // follow every other producer lifecycle.
    bool firstRefineHistoryKind = true;
    for (const TopAbs_ShapeEnum kind : makerMappedKinds()) {
        if (trace != nullptr && !firstRefineHistoryKind) {
            trace->record({
                "toposhape.can_map",
                "accepted",
                "cache_ready",
                {{"resultTag", "0"}, {"sourceTag", "0"}},
            });
        }
        firstRefineHistoryKind = false;
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        TopTools_IndexedMapOfShape sourceElements;
        TopExp::MapShapes(source.shape, kind, sourceElements);
        for (int index = 1; index <= sourceElements.Extent(); ++index) {
            const TopoDS_Shape& sourceElement = sourceElements(index);
            const std::string localElementName = prefix + std::to_string(index);
            for (const std::string& sourceName : sourceElementNames(
                     source, localElementName, SourceElementMapLookup::First
                )) {
                try {
                    const RawMakerHistoryEntry* rawHistory = findRawMakerHistoryEntry(
                        rawMakerHistory,
                        0U,
                        kind,
                        index
                    );
                    if (rawHistory == nullptr || !rawHistory->modifiedError.empty()) {
                        continue;
                    }
                    applyHistoryList(
                        namedShape,
                        sourceName,
                        rawHistory->modified,
                        ElementHistoryKind::Modified,
                        sourceTargets
                    );
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.h
                    // ::BRepBuilderAPI_RefineModel exposes "IsDeleted(const TopoDS_Shape& S)";
                    // TopoShapeExpansion.cpp::makeElementRefine() routes that mapper into
                    // makeShapeWithElementMap(), so refined-away source elements remain terminal
                    // deleted history for later updateElementReference().
                    if (rawHistory->deletedError.empty() && rawHistory->deleted
                        && sourceTargets[sourceName].preserved.empty()
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
    applyMakerHistoryElementMap(namedShape, sourceTargets, refineOperation, true);
    addMakerReverseAliases(namedShape, refineOperation);
    addMakerForwardAliases(namedShape, refineOperation);
    if (trace != nullptr) {
        trace->record({
            "maker.end", "success", "naming_complete", {{"operation", "RFI"}},
        });
    }
    // `makeElementRefine()` creates a fresh ElementMap through MyRefineMaker and does not carry
    // the previous maker's terminal MapperHistory as a second public ledger.  Its current
    // ElementMap already encodes whichever preserved/Modified/Generated source survived the
    // refine.  Forwarding the Boolean's old deleted entries here manufactures stale history on
    // the completed Pad (and therefore on a Body that merely inherits its Tip).
    addMergeHistory(namedShape);
    appendPartCanonicalCollisionHistory(namedShape, {source});

    if (trace != nullptr) {
        checkpointNamedShapeLedger(namedShape, owner + ":refine-maker", "maker.final_checkpoint");
    }

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
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);
    appendPartCanonicalCollisionHistory(namedShape, sources);

    return namedShape;
}

NamedShape namedShapeForPropertyShapeValue(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const NamedShape& source,
    long propertyTag,
    bool emitReferenceUpdate,
    bool emitBodyTipLifecycle,
    const std::vector<std::string>& referencedSubnames
)
{
    NamedShape stored = source;
    stored.owner = owner;
    stored.shape = shape;
    app::ElementMapProducerTrace* trace = stored.stringHasher
        ? stored.stringHasher->producerTrace()
        : nullptr;
    std::string beforeSnapshot;
    std::string beforeIdentity;
    const bool retagging = source.producerTag && *source.producerTag != 0L
        && *source.producerTag != propertyTag;
    if (trace != nullptr && emitBodyTipLifecycle) {
        NamedShape reset = indexedNamedShapeForObject(owner, shape);
        reset.producerTag = source.producerTag;
        reset.stringHasher = stored.stringHasher;
        trace->record({
            "toposhape.set_shape", "begin", "reset_requested",
            {{"incomingNull", shape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", std::to_string(source.producerTag.value_or(0L))}},
        });
        (void)checkpointNamedShapeLedger(
            reset, owner + ":body-tip-reset", "toposhape.set_shape_checkpoint"
        );
        trace->record({
            "property_shape.set_value", "begin", "property_part_shape",
            {{"inputTag", std::to_string(source.producerTag.value_or(0L))},
             {"objectTag", std::to_string(propertyTag)},
             {"owner", owner}},
        });
        trace->record({
            "property_shape.retag", "begin", "reset_and_copy_element_map",
            {{"newTag", std::to_string(propertyTag)}, {"postfix", ""}},
        });
    }
    else if (trace != nullptr && !retagging) {
        NamedShape reset;
        reset.owner = owner;
        reset.shape = shape;
        reset.producerTag = 0L;
        reset.stringHasher = stored.stringHasher;
        trace->record({
            "toposhape.set_shape",
            "begin",
            "reset_requested",
            {{"incomingNull", shape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", "0"}},
        });
        (void)checkpointNamedShapeLedger(
            reset, owner + ":property-reset", "toposhape.set_shape_checkpoint"
        );
        trace->record({
            "property_shape.set_value",
            "begin",
            "property_part_shape",
            {{"inputTag", std::to_string(source.producerTag.value_or(0L))},
             {"objectTag", std::to_string(propertyTag)},
             {"owner", owner}},
        });
    }
    else if (trace != nullptr && !emitBodyTipLifecycle) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
        // ::PropertyPartShape::setValue() mutates the owning feature's Shape property inside
        // its current producer scope; it does not open a separate TopoShape maker lifecycle.
        const std::string sourceIdentity = trace->firstSeenIdentity(
            "ledger",
            source.owner + ":producer",
            "create"
        );
        beforeIdentity = trace->firstSeenIdentity(
            "ledger",
            owner + ":property-before",
            "copy",
            sourceIdentity
        );
        beforeSnapshot = checkpointNamedShapeLedger(
            stored,
            owner + ":property-before",
            "property_shape.before_checkpoint",
            "copy",
            sourceIdentity
        );
        trace->record({
            "property_shape.set_value",
            "begin",
            "property_shape_handoff_started",
            {{"sourceOwner", source.owner},
             {"inputTag", source.producerTag.value_or(0L)},
             {"outputTag", propertyTag},
             {"elementMapSize", source.elementMapEntries.size()}},
            beforeSnapshot,
            beforeSnapshot,
        });
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
    // ::PropertyPartShape::setValue(), when `_Shape.Tag && tag != _Shape.Tag`, calls
    // `reTagElementMap(tag, hasher)`. TopoShapeExpansion.cpp::TopoShape::reTagElementMap()
    // resets the receiver then calls `copyElementMap(tmp)`, which installs one direct
    // MappedChildElements range per Vertex/Edge/Face kind. This is a property copy, not another
    // maker pass: raw MappedName values and their entry-local StringIDRefs remain owned by the
    // incoming Shape and are reached through the new child ranges.
    if (retagging) {
        // reTagElementMap() starts with resetElementMap(), then copyElementMap(tmp). The
        // receiver must not retain direct raw entries beside its child ranges: ElementMap::findAll
        // would choose those direct refs and skip the child postfix/tag lifecycle.
        stored.elementMap.clear();
        stored.mappedNameProvenance.clear();
        stored.elementMapEntries.clear();
        stored.childElementMaps.clear();
        if (trace != nullptr && !emitBodyTipLifecycle) {
            trace->record({
                "element_map.reset",
                "reset",
                "property_retag_resets_direct_entries",
                {{"sourceTag", *source.producerTag},
                 {"propertyTag", propertyTag},
                 {"removedEntryGroups", source.elementMapEntries.size()},
                 {"removedChildMaps", source.childElementMaps.size()}},
            });
            trace->record({
                "element_map.erase",
                "erased",
                "retag_reset_removed_previous_map",
                {{"removedEntryGroups", source.elementMapEntries.size()},
                 {"removedChildMaps", source.childElementMaps.size()}},
            });
            trace->record({
                "property_shape.retag",
                "retag",
                "source_tag_differs_from_property_tag",
                {{"sourceTag", *source.producerTag}, {"propertyTag", propertyTag}},
            });
        }
        std::shared_ptr<NamedShape> sharedSourceLedger = std::make_shared<NamedShape>(source);
        if (trace != nullptr && emitBodyTipLifecycle) {
            trace->record({
                "child_map", "begin", "add_child_elements",
                {{"count", "3"}, {"masterTag", std::to_string(propertyTag)}},
            });
        }
        for (const TopAbs_ShapeEnum kind : childMapKinds()) {
            const int sourceCount = subshapeCount(source.shape, kind);
            const int targetCount = subshapeCount(shape, kind);
            const int count = std::min(sourceCount, targetCount);
            if (count <= 0) {
                continue;
            }

            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            NamedShapeChildMap childMap;
            childMap.sourceOwner = source.owner;
            childMap.kind = subshapeKindName(kind);
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::setupChild(), assigns
            // `IndexedName::fromConst(TopoShape::shapeName(elementType).c_str(), 1)`.  The
            // child-map owner identifies the incoming ledger, but its query key is the typed
            // Vertex1/Edge1/Face1 range.  Storing the owner here makes findAll() unable to
            // resolve this PropertyPartShape-retagged map in a later Link or Part maker.
            childMap.indexedName = prefix + "1";
            // FreeCAD: PropertyPartShape receives exactly one Tip Shape.  Its public child-map
            // projection groups the three setupChild() typed ranges under that Tip owner, while
            // the internal ElementMap still queries the typed IndexedName above.
            childMap.protocolPathPrefix = source.owner;
            childMap.offset = 0;
            childMap.count = count;
            childMap.targetStart = prefix + "1";
            childMap.targetEnd = childMapTargetName(prefix, 0, count);
            // FreeCAD: TopoShapeExpansion.cpp::TopoShape::setupChild() writes the source Tag
            // only when reTagElementMap() changed the receiving PropertyPartShape Tag.
            childMap.tag = source.producerTag && *source.producerTag != propertyTag
                ? *source.producerTag
                : 0L;
            if (childMap.tag != 0L) {
                std::ostringstream encoded;
                encoded << ";:H";
                if (childMap.tag < 0L) encoded << '-';
                encoded << std::hex << std::abs(childMap.tag) << ',' << prefix.front();
                childMap.postfix = encoded.str();
            }
            childMap.sourceLedger = sharedSourceLedger;
            childMap.sourceNamedShape = childMap.sourceLedger.get();
            childMap.hasSourceElementMap = !source.elementMap.empty();
            childMap.sourceElementMapSize = source.elementMap.size();
            childMap.sourceChildMapCount = source.childElementMaps.size();
            if (shouldEncodeChildMapKey(childMap)) {
                childMap.encodedChildMapKey = encodedChildMapKey(childMap);
            }
            stored.childElementMaps.push_back(std::move(childMap));
            if (trace != nullptr && emitBodyTipLifecycle) {
                const NamedShapeChildMap& published = stored.childElementMaps.back();
                trace->record({
                    "element_map.encode", "encoded", "forced_tag",
                    {{"before", ""},
                     {"after", published.postfix},
                     {"elementType", std::string(1U, prefix.front())},
                     {"entryLocalRefs", ""},
                     {"inputTag", std::to_string(published.tag)},
                     {"masterTag", std::to_string(propertyTag)}},
                });
            }
            else if (trace != nullptr) {
                const NamedShapeChildMap& published = stored.childElementMaps.back();
                trace->record({
                    "child_map",
                    "write",
                    published.encodedChildMapKey.empty() ? "direct_typed_range"
                                                         : "encoded_typed_range",
                    {{"sourceOwner", published.sourceOwner},
                     {"kind", published.kind},
                     {"indexedName", published.indexedName},
                     {"offset", published.offset},
                     {"count", published.count},
                     {"targetStart", published.targetStart},
                     {"targetEnd", published.targetEnd},
                     {"tag", published.tag},
                     {"postfix", published.postfix},
                     {"encodedChildMapKey", published.encodedChildMapKey}},
                });
            }
        }
        addDistinctString(stored.elementHistoryStatus, "element_map_property:retag_copy");
        if (trace != nullptr && emitBodyTipLifecycle) {
            (void)checkpointNamedShapeLedger(
                stored, owner + ":body-tip-child-map", "child_map_checkpoint"
            );
            trace->record({
                "toposhape.copy_map", "copied", "partner_shape_child_map",
                {{"childRanges", std::to_string(stored.childElementMaps.size())},
                 {"operation", ""},
                 {"resultTag", std::to_string(propertyTag)},
                 {"sourceTag", std::to_string(source.producerTag.value_or(0L))}},
            });
            (void)checkpointNamedShapeLedger(
                stored, owner + ":body-tip-copy-map", "toposhape.copy_map_checkpoint"
            );
            (void)checkpointNamedShapeLedger(
                stored, owner + ":body-tip-retag", "property_shape.retag_checkpoint"
            );
            trace->record({
                "shape_slot.assign", "retagged", "property_owner_tag", {{"owner", owner}},
            });
        }
    }
    // The `else` arm in setValue assigns only `_Shape.Tag = obj->getID()`. In particular, a
    // Tag==0 maker result must not be needlessly copied or rehashed while it is first persisted.
    stored.producerTag = propertyTag;
    if (trace != nullptr && emitBodyTipLifecycle) {
        if (emitReferenceUpdate) {
            trace->record({
                "reference.update", "begin", "geometry_property_changed", {{"object", owner}},
            });
            trace->record({
                "reference.update", "updated", "element_map_version", {{"reset", "false"}},
            });
        }
        (void)checkpointNamedShapeLedger(
            stored, owner + ":body-tip-property", "property_shape.set_value_checkpoint"
        );
    }
    else if (trace != nullptr && !retagging) {
        if (emitReferenceUpdate) {
            trace->record({
                "reference.update",
                "begin",
                "geometry_property_changed",
                {{"object", owner}},
            });
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp
            // ::GeoFeature::updateElementReferences() resolves each downstream indexed LinkSub
            // through the new ElementMap before publishing the map-version notification.
            for (const std::string& subname : referencedSubnames) {
                const auto entries = source.elementMapEntries.find(subname);
                if (entries == source.elementMapEntries.end() || entries->second.empty()) {
                    continue;
                }
                const ElementMapEntry& entry = entries->second.front();
                const auto provenance = source.mappedNameProvenance.find(entry.mappedName);
                const std::string raw = provenance != source.mappedNameProvenance.end()
                    ? provenance->second.rawMappedName
                    : entry.mappedName;
                trace->record({
                    "element_map.find", "hit", "first_entry",
                    {{"indexed", subname}, {"raw", raw}, {"entryLocalRefs", ""}},
                });
                trace->record({
                    "reference.resolve", "resolved", "geofeature_element",
                    {{"object", owner},
                     {"old", subname},
                     {"new", ";" + raw + "." + subname}},
                });
                trace->record({
                    "reference.update", "updated", "resolved_reference",
                    {{"object", owner}, {"subname", subname}},
                });
            }
            trace->record({
                "reference.update",
                "updated",
                "element_map_version",
                {{"reset", "false"}},
            });
        }
        (void)checkpointNamedShapeLedger(
            stored,
            owner + ":property-value",
            "property_shape.set_value_checkpoint"
        );
    }
    else if (trace != nullptr && !emitBodyTipLifecycle) {
        const std::string afterSnapshot = checkpointNamedShapeLedger(
            stored,
            owner + ":property-after",
            "property_shape.after_checkpoint",
            retagging ? "reset" : "share",
            beforeIdentity
        );
        trace->record({
            "property_shape.set_value",
            "success",
            source.producerTag && *source.producerTag != 0L && *source.producerTag != propertyTag
                ? "retag_child_ranges_published"
                : "tag_assigned_without_retag",
            {{"inputTag", source.producerTag.value_or(0L)},
             {"outputTag", propertyTag},
             {"childMapCount", stored.childElementMaps.size()}},
            beforeSnapshot,
            afterSnapshot,
        });
        trace->record({
            "toposhape.set_shape",
            "assigned",
            "property_shape_value_installed",
            {{"shapeRole", owner + ".Shape"},
             {"tag", propertyTag},
             {"resetElementMap",
              source.producerTag && *source.producerTag != 0L
                  && *source.producerTag != propertyTag},
             {"beforeSnapshot", beforeSnapshot},
             {"afterSnapshot", afterSnapshot}},
            beforeSnapshot,
            afterSnapshot,
        });
    }
    return stored;
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
    const NamedShapeSource& source,
    std::optional<long> propertyTag
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

    if (!propertyTag && resultShape.IsPartner(source.shape)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::checkGeoElementMap() returns before reTagElementMap() for a
        // same-document Link with no postfix. The linked TopoShape consequently retains the
        // source ElementMap itself, including a Body Tip's typed child ranges; do not flatten
        // those ranges into Link-owned direct aliases.
        namedShape.producerTag = source.namedShape->producerTag;
        namedShape.stringHasher = source.namedShape->stringHasher;
        for (const TopAbs_ShapeEnum kind : childMapKinds()) {
            const int sourceCount = subshapeCount(source.shape, kind);
            const int targetCount = subshapeCount(resultShape, kind);
            const int count = std::min(sourceCount, targetCount);
            const std::string prefix = prefixForKind(kind);
            if (count <= 0 || prefix.empty()) {
                continue;
            }
            NamedShapeChildMap childMap;
            childMap.sourceOwner = source.owner;
            childMap.kind = subshapeKindName(kind);
            childMap.indexedName = prefix + "1";
            childMap.protocolPathPrefix = source.owner;
            childMap.offset = 0;
            childMap.count = count;
            childMap.targetStart = prefix + "1";
            childMap.targetEnd = childMapTargetName(prefix, 0, count);
            childMap.sourceLedger = std::make_shared<NamedShape>(*source.namedShape);
            childMap.sourceNamedShape = childMap.sourceLedger.get();
            childMap.hasSourceElementMap = !source.namedShape->elementMap.empty();
            childMap.sourceElementMapSize = source.namedShape->elementMap.size();
            childMap.sourceChildMapCount = source.namedShape->childElementMaps.size();
            if (shouldEncodeChildMapKey(childMap)) {
                childMap.encodedChildMapKey = encodedChildMapKey(childMap);
            }
            namedShape.childElementMaps.push_back(std::move(childMap));
        }
        addDistinctString(namedShape.elementHistoryStatus, "element_map_link:same_document_copy");
        return namedShape;
    }

    for (const auto& [targetName, element] : namedShape.elements) {
        (void)element;
        bool copiedSourceBackedEntry = false;
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::findAll() is the only source for Link's copied producer refs.  This
        // covers both a direct Body ElementMap and a PropertyPartShape-created child range;
        // never use the flattened response's stable/display subname as a fallback raw name.
        for (const std::string& sourceName : sourceElementNames(source, targetName)) {
            copiedSourceBackedEntry =
                copyLinkedMappedNameProvenance(
                    namedShape, source, sourceName, targetName, propertyTag
                )
                || copiedSourceBackedEntry;
        }
        if (copiedSourceBackedEntry) {
            continue;
        }

        // A source with no producer ElementMap still needs the old request-local Link alias
        // so LinkSub resolution has an IndexedName to target.  That alias remains IndexedOnly
        // and is deliberately not published as native raw mapped-name evidence.
        addLinkRetagAlias(namedShape, source, targetName, targetName);
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
    const NamedShapeSource& source,
    std::optional<std::string> postfix,
    bool emitDirectMapLifecycle
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementTransform(), after transforming/copying the shape, calls
    // "copyElementMap(tmp, op)" instead of deriving ownership from result geometry.
    if (!postfix) {
        // Callers which only use a BRep transform have not requested the TopoShape operation
        // path. Preserve their existing linked-shape ledger; FeatureTransformed always passes
        // an engaged value, including the first empty Data::indexSuffix(1).
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

    // TopoShape::makeElementTransform() starts from a TopoShape constructed with the source
    // Tag/Hasher. When op is non-null it resets the direct ElementMap and installs exactly one
    // MappedChildElements range per Vertex/Edge/Face type through copyElementMap(tmp, op).
    // Keep that request-local lazy map rather than emitting owner aliases for transformed BRep
    // output. The next Part maker consumes it through ElementMap::findAll().
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    if (source.namedShape == nullptr) {
        return namedShape;
    }
    namedShape.producerTag = source.producerTag ? source.producerTag
                                                : source.namedShape->producerTag;
    namedShape.stringHasher = source.namedShape->stringHasher;
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    app::ElementMapProducerTrace::Scope mapScope;
    const long traceTag = namedShape.producerTag.value_or(0L);
    const bool emitsDirectTransformMap = trace != nullptr && postfix->empty()
        && emitDirectMapLifecycle;
    if (emitsDirectTransformMap) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeElementTransform() finishes with
        // "copyElementMap(tmp, op)"; that copy is a direct mapSubElement lifecycle, not a
        // makeShapeWithElementMap maker nested beneath the transformed feature.
        mapScope = trace->scope(
            {"toposhape.map_sub_element",
             "",
             traceTag,
             "Part::TopoShape",
             nlohmann::json::object()}
        );
        trace->record({
            "toposhape.map_sub_element", "begin", "map_sub_element",
            {{"operation", *postfix},
             {"resultTag", std::to_string(traceTag)},
             {"sourceTag", std::to_string(traceTag)}},
        });
        trace->record({
            "toposhape.can_map", "accepted", "cache_ready",
            {{"resultTag", std::to_string(traceTag)}, {"sourceTag", std::to_string(traceTag)}},
        });
    }
    for (const TopAbs_ShapeEnum kind : childMapKinds()) {
        const int sourceCount = subshapeCount(source.shape, kind);
        const int targetCount = subshapeCount(resultShape, kind);
        const int count = std::min(sourceCount, targetCount);
        if (count <= 0) {
            continue;
        }
        const std::string prefix = prefixForKind(kind);
        if (prefix.empty()) {
            continue;
        }
        NamedShapeChildMap childMap;
        childMap.sourceOwner = source.owner;
        childMap.kind = subshapeKindName(kind);
        childMap.indexedName = prefix + "1";
        childMap.offset = 0;
        childMap.count = count;
        childMap.targetStart = prefix + "1";
        childMap.targetEnd = childMapTargetName(prefix, 0, count);
        // makeElementTransform() copies a TopoShape initialized from its source, so setupChild()
        // observes the same receiver/source Tag and leaves the child range tagless.
        childMap.tag = 0;
        childMap.postfix = *postfix;
        childMap.sourceLedger = std::make_shared<NamedShape>(*source.namedShape);
        childMap.sourceNamedShape = childMap.sourceLedger.get();
        childMap.hasSourceElementMap = !source.namedShape->elementMap.empty();
        childMap.sourceElementMapSize = source.namedShape->elementMap.size();
        childMap.sourceChildMapCount = source.namedShape->childElementMaps.size();
        if (shouldEncodeChildMapKey(childMap)) {
            childMap.encodedChildMapKey = encodedChildMapKey(childMap);
        }
        namedShape.childElementMaps.push_back(std::move(childMap));
    }
    addDistinctString(namedShape.elementHistoryStatus, "element_map_transform:copy");
    if (emitsDirectTransformMap) {
        (void)checkpointNamedShapeLedger(
            namedShape, owner + ":transform", "toposhape.map_sub_element_checkpoint"
        );
    }
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
    std::optional<double> tolerance,
    std::optional<long> producerTag
)
{
    app::ElementMapProducerTrace* trace = nullptr;
    long traceTag = producerTag.value_or(0L);
    for (const NamedShapeSource& source : sources) {
        if (source.namedShape != nullptr && source.namedShape->stringHasher
            && source.namedShape->stringHasher->producerTrace() != nullptr) {
            trace = source.namedShape->stringHasher->producerTrace();
            break;
        }
    }
    app::ElementMapProducerTrace::Scope booleanScope;
    if (trace != nullptr) {
        booleanScope = trace->scope(
            {"makeElementBoolean",
             "",
             0,
             // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
             // ::makeElementBoolean() remains a Part::TopoShape producer; FUS/CUT names the op.
             "Part::TopoShape",
             {{"operation", booleanOperationName(operation)},
              {"outputTag", traceTag},
              // FreeCAD makeElementBoolean delegates its ledger closure to the nested
              // makeShapeWithElementMap scope; the outer Boolean scope has no independent final.
              {"requiresFinalCheckpoint", false}}}
        );
        std::ostringstream fuzzy;
        fuzzy << std::fixed << std::setprecision(6) << tolerance.value_or(0.0);
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementBoolean() dispatches Part::OpCodes::Fuse/::Cut. Producer Trace
        // records those op codes ("FUS"/"CUT"), not the display names "fuse"/"cut".
        const std::string traceOperation = booleanOperationCode(operation);
        trace->record({
            "boolean.lifecycle",
            "begin",
            "boolean_dispatch",
            {{"operation", traceOperation},
             {"inputCount", std::to_string(sources.size())},
             {"fuzzy", fuzzy.str()}},
        });
    }
    if (sources.empty()) {
        if (trace != nullptr) {
            trace->record({"boolean.lifecycle", "rejected", "no_sources", nlohmann::json::object()});
            booleanScope.abort("no_sources");
        }
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            if (trace != nullptr) {
                trace->record({
                    "boolean.lifecycle",
                    "rejected",
                    "null_input_shape",
                    {{"sourceOwner", source.owner}},
                });
                booleanScope.abort("null_input_shape");
            }
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
        if (trace != nullptr) {
            trace->record({
                "boolean.lifecycle",
                "fallback",
                "single_source_preserved",
                {{"sourceOwner", sources.front().owner}},
            });
        }
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
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // FCBRepAlgoAPI_BooleanOperation.cpp::FCBRepAlgoAPI_BooleanOperation() sets
    // SetRunParallel(Standard_True) and SetNonDestructive(Standard_True).  FeatureExtrude
    // constructs that wrapper in TopoShape::makeElementBoolean(), so retain its maker identity
    // semantics before mapSubElement() consumes preserved ElementMap entries.
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
        if (trace != nullptr) {
            trace->record({
                "boolean.lifecycle",
                "failed",
                "occt_boolean_build_failed",
                {{"operation", booleanOperationName(operation)}, {"partialWrite", false}},
            });
            booleanScope.abort("occt_boolean_build_failed");
        }
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "OCCT could not " + booleanOperationName(operation) + " boolean sources"
        };
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementBoolean() passes the maker result directly
    // to makeShapeWithElementMap(). Rewrapping a Fuse compound in source order changes the
    // compound-child identity observed by mapSubElement() and lets CAD Core manufacture a
    // preserved Base/Tool relation that FreeCAD's maker never exposed.
    const TopoDS_Shape resultShape = maker->Shape();
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap() uses the new result's Tag for its master map and
    // every input TopoShape's Tag in NameKey. Keep those tags explicitly in the Part ledger.
    // FreeCAD: src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap() first calls
    MakerHistoryOptions options {
        booleanOperationCode(operation), true, false, nullptr, true
    };
    // FreeCAD: TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() invokes
    // mapSubElement(shapes) before Mapper::modified()/generated() for every Boolean operation.
    // Whether an individual input ref survives is decided later by the per-subshape IsSame map;
    // Cut/Common must not preemptively discard an unchanged Pad/Body producer chain.
    options.preserveRawMappedName = true;
    // FreeCAD FeatureExtrude constructs `TopoShape result(0, hasher)` for the Boolean result.
    // Preserve that explicit Tag=0 in the request-local ledger so a subsequent Refine compares
    // all aliases through the same NameKey Tag; terminal mapped-name tags remain separate.
    options.producerTag = producerTag ? producerTag : std::optional<long> {0L};
    if (trace != nullptr && directCompoundChildrenPartnerSources(resultShape, booleanSources)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeElementBoolean() preserves the first argument's
        // TopoShape wrapper while assigning the direct-child compound result; its trace is
        // `setShape(..., resetElementMap=false)` before makeShapeWithElementMap resets the result.
        NamedShape preserved = indexedNamedShapeForObject(owner, resultShape);
        if (booleanSources.front().namedShape != nullptr) {
            preserved.stringHasher = booleanSources.front().namedShape->stringHasher;
        }
        preserved.producerTag = booleanSources.front().producerTag
            ? booleanSources.front().producerTag
            : (booleanSources.front().namedShape != nullptr
                   ? booleanSources.front().namedShape->producerTag
                   : std::optional<long> {});
        trace->record({
            "toposhape.set_shape", "begin", "preserve_requested",
            {{"incomingNull", resultShape.IsNull() ? "true" : "false"},
             {"resetElementMap", "false"},
             {"tag", std::to_string(preserved.producerTag.value_or(0L))}},
        });
        checkpointNamedShapeLedger(
            preserved, owner + ":boolean-preserve", "toposhape.set_shape_checkpoint"
        );
    }
    NamedShape mapped = namedShapeForMakerHistory(
        owner,
        resultShape,
        booleanSources,
        *maker,
        std::move(options)
    );
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementBoolean(), returns makeShapeWithElementMap() directly. That inner
    // maker owns the sole final checkpoint; Boolean adds no second success/checkpoint event.
    return NamedShapeBuild {resultShape, std::move(mapped), {}};
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
    app::ElementMapProducerTrace* trace = source.namedShape != nullptr
            && source.namedShape->stringHasher
        ? source.namedShape->stringHasher->producerTrace()
        : nullptr;
    app::ElementMapProducerTrace::Scope refineScope;
    if (trace != nullptr) {
        refineScope = trace->scope(
            {"makeElementRefine",
             "",
             0,
             // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
             // ::makeElementRefine() is a Part::TopoShape producer scope; "Refine" is the
             // operation, not a separate producer type.
             "Part::TopoShape",
             {{"sourceOwner", source.owner},
              {"outputTag",
               source.producerTag.value_or(source.namedShape && source.namedShape->producerTag
                                               ? *source.namedShape->producerTag
                                               : 0L)},
              {"requiresFinalCheckpoint", true}}}
        );
        trace->record({
            "refine.lifecycle",
            "begin",
            "refine",
            {{"inputTag",
              std::to_string(
                  source.producerTag.value_or(source.namedShape && source.namedShape->producerTag
                                                  ? *source.namedShape->producerTag
                                                  : 0L)
              )},
             {"operation", ""}},
        });
    }
    if (source.shape.IsNull()) {
        if (trace != nullptr) {
            trace->record({"refine.lifecycle", "rejected", "null_input_shape", nlohmann::json::object()});
            refineScope.abort("null_input_shape");
        }
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for refine operation"};
    }

    try {
        part::BRepBuilderAPI_RefineModel maker(source.shape);
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            if (trace != nullptr) {
                trace->record({
                    "refine.lifecycle",
                    "failed",
                    "refine_result_null",
                    {{"partialWrite", false}},
                });
                refineScope.abort("refine_result_null");
            }
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Refine produced a null shape"};
        }
        const RawMakerHistoryCapture rawMakerHistory =
            captureRawMakerHistory({source}, resultShape, maker);
        NamedShape mapped = namedShapeForRefineHistory(
            owner,
            resultShape,
            source,
            rawMakerHistory
        );
        if (trace != nullptr) {
            trace->record({
                "refine.lifecycle",
                "success",
                "refine_preserved_closed_state",
                nlohmann::json::object(),
            });
            const std::string snapshot = trace->currentSnapshotId();
            trace->record({
                "refine.final_checkpoint",
                "published",
                "",
                {{"snapshot", snapshot}},
                snapshot,
                snapshot,
            });
        }
        return NamedShapeBuild {resultShape, std::move(mapped), {}};
    }
    catch (const Standard_Failure& failure) {
        if (trace != nullptr) {
            trace->record({
                "refine.lifecycle",
                "exception",
                "occt_refine_failure",
                {{"message", failure.GetMessageString() != nullptr
                                  ? failure.GetMessageString()
                                  : "Refine operation failed"},
                 {"partialWrite", false}},
            });
            refineScope.exception("occt_refine_failure");
        }
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
        if (source.namedShape != nullptr && source.namedShape->stringHasher) {
            namedShape.stringHasher = source.namedShape->stringHasher;
            break;
        }
    }
    app::ElementMapProducerTrace* trace = namedShape.stringHasher
        ? namedShape.stringHasher->producerTrace()
        : nullptr;
    if (trace != nullptr) {
        std::string relatedIdentity;
        if (!sources.empty()) {
            relatedIdentity = trace->firstSeenIdentity(
                "elementMap",
                sources.front().owner + ":producer",
                "create"
            );
        }
        trace->firstSeenIdentity(
            "elementMap",
            owner + ":drop",
            "drop",
            relatedIdentity
        );
        trace->firstSeenIdentity(
            "ledger",
            owner + ":drop",
            "drop",
            relatedIdentity
        );
        trace->firstSeenIdentity(
            "shapeRole",
            owner + ":drop",
            "drop",
            relatedIdentity
        );
        trace->record({
            "element_map.drop",
            "drop",
            "element_map_policy_drop",
            {{"owner", owner},
             {"sourceCount", sources.size()},
             {"outputInventory", inspectShapeInventory(resultShape)}},
        });
    }

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
    if (trace != nullptr) {
        checkpointNamedShapeLedger(
            namedShape,
            owner + ":drop",
            "maker.final_checkpoint"
        );
    }
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
