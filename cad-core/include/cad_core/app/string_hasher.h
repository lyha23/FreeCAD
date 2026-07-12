#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cad_core::app
{

class ElementMapProducerTrace;

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.h and
// /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp::StringHasher::getID().  A document
// owns one StringHasher and ElementMap stores compact #<hex> references into that table.  This
// request-local counterpart deliberately preserves insertion order; it is not a hash of a shape
// or a display subname.
struct StringId
{
    long value = 0;
    int index = 0;

    explicit operator bool() const { return value > 0; }
    std::string toString() const;

    bool operator==(const StringId& other) const
    {
        return value == other.value && index == other.index;
    }
    bool operator!=(const StringId& other) const { return !(*this == other); }
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
// ::ElementMap::hashElementName() first asks the document StringHasher to encode a MappedName,
// then retains the new StringID plus only source IDs that are not already related to it.  The
// element-map sidecar is private producer state: callers use it to carry provenance into the
// next maker, never as a response identity or geometry input.
struct HashedMappedName
{
    StringId id;
    std::vector<StringId> elementRefs;
};

class StringHasher
{
public:
    void attachProducerTrace(ElementMapProducerTrace* trace) noexcept { producerTrace_ = trace; }
    ElementMapProducerTrace* producerTrace() const noexcept { return producerTrace_; }

    StringId getId(const std::string& data);
    StringId getMappedNameId(
        const std::string& data,
        int index,
        const std::string& postfix,
        const std::vector<StringId>& relatedIds = {}
    );

    HashedMappedName hashMappedName(const std::string& data,
                                    int index,
                                    const std::string& postfix,
                                    const std::vector<StringId>& inputRefs);

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp
    // ::StringHasher::getID(const Data::MappedName&, const QVector<StringIDRef>&) carries the
    // source ElementIDRefs into a producer's newly encoded name. CAD Core records the observed
    // raw name and its already-resolved related IDs without inventing an ID from a display name.
    void rememberRelatedMappedName(const std::string& rawMappedName,
                                   std::vector<StringId> relatedIds);
    void rememberMappedName(const std::string& rawMappedName,
                            StringId primaryId,
                            std::vector<StringId> elementRefs);
    std::vector<StringId> relatedIdsForMappedName(const std::string& rawMappedName) const;
    std::optional<StringId> mappedNameId(const std::string& rawMappedName) const;
    int producerIndexForMappedNameId(StringId id) const;

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD2/src/App/StringHasher.cpp
    // ::StringHasher::inspectProducerTraceState() reads the monotonic allocation table without
    // calling getID(), marking entries, compacting relations, or changing PrefixID metadata.
    nlohmann::json inspectProducerTraceState() const;

    long lastId() const { return nextId_; }
    void clear();

private:
    struct ProducerTraceEntry
    {
        long value = 0;
        std::string data;
        std::string postfix;
        std::string flags;
        std::vector<StringId> related;
        long prefixId = 0;
        int prefixIdIndex = 0;
    };

    StringId allocate(const std::string& key, int index = 0);
    void emitMappedNameTrace(const std::string& data,
                             int index,
                             const std::string& postfix,
                             const std::vector<StringId>& relatedIds,
                             StringId result,
                             long lastIdBefore,
                             const std::string& decision,
                             const std::string& reason) const;
    void refreshProducerTraceEntry(long value);

    long nextId_ = 0;
    std::unordered_map<std::string, long> ids_;
    std::unordered_map<long, std::vector<StringId>> relatedIds_;
    // FreeCAD: src/App/StringHasher.cpp::StringHasher::getID(Data::MappedName, ...), records
    // PrefixID/PrefixIDIndex after matching a `#<id>[:index]` data prefix against the entry's
    // own related StringIDRefs. This is StringID metadata, not a relation reconstructed from a
    // published mapped-name string; later ElementMap writes need it to retain the prefix ID as
    // their StringIDRef index.
    std::unordered_map<long, long> prefixSourceIds_;
    std::unordered_map<std::string, std::vector<StringId>> mappedNameRelations_;
    std::unordered_map<std::string, StringId> mappedNameIds_;
    std::vector<ProducerTraceEntry> producerTraceEntries_;
    ElementMapProducerTrace* producerTrace_ = nullptr;
};

std::optional<StringId> parseStringId(const std::string& value);

}  // namespace cad_core::app
