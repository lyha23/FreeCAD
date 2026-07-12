#include "cad_core/app/string_hasher.h"

#include "cad_core/app/element_map_producer_trace.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>

namespace cad_core::app
{
namespace
{

std::string hexadecimal(long value)
{
    std::ostringstream stream;
    stream << std::hex << value;
    return stream.str();
}

std::string encodedPostfix(const std::string& postfix, StringHasher& hasher)
{
    if (postfix.empty() || postfix.find('#') != std::string::npos) {
        return postfix;
    }
    return hasher.getId(postfix).toString();
}

bool sameId(const StringId& left, const StringId& right)
{
    return left.value == right.value && left.index == right.index;
}

bool containsId(const std::vector<StringId>& values, const StringId& value)
{
    return std::any_of(values.begin(), values.end(), [&value](const StringId& candidate) {
        return sameId(candidate, value);
    });
}

nlohmann::json stringIdsJson(const std::vector<StringId>& values)
{
    nlohmann::json result = nlohmann::json::array();
    for (const StringId& value : values) {
        result.push_back({{"value", value.value}, {"index", value.index}});
    }
    return result;
}

std::pair<std::string, std::string> producerTraceKeyParts(const std::string& key)
{
    if (key.rfind("data:", 0) == 0U) {
        return {key.substr(5U), {}};
    }
    if (key.rfind("mapped:", 0) == 0U) {
        const std::size_t separator = key.find('\x1f', 7U);
        return {key.substr(7U, separator == std::string::npos ? std::string::npos
                                                              : separator - 7U),
                separator == std::string::npos ? std::string {} : key.substr(separator + 1U)};
    }
    return {key, {}};
}

std::optional<StringId> encodedPrefixId(const std::string& data)
{
    if (data.size() < 2U || data.front() != '#') {
        return std::nullopt;
    }
    const std::size_t separator = data.find(':');
    const std::size_t idEnd = separator == std::string::npos ? data.size() : separator;
    if (idEnd == 1U || (separator != std::string::npos && separator + 1U == data.size())) {
        return std::nullopt;
    }
    const std::string idText = data.substr(1U, idEnd - 1U);
    const std::string indexText = separator == std::string::npos ? std::string {}
                                                                  : data.substr(separator + 1U);
    if (!std::all_of(
            idText.begin(),
            idText.end(),
            [](unsigned char value) { return std::isxdigit(value) != 0; }
        )
        || (!indexText.empty() && !std::all_of(indexText.begin(), indexText.end(), [](unsigned char value) {
               return std::isxdigit(value) != 0;
           }))) {
        return std::nullopt;
    }
    long value = 0;
    int index = 0;
    const auto [valueEnd, valueError]
        = std::from_chars(idText.data(), idText.data() + idText.size(), value, 16);
    std::from_chars_result indexResult {indexText.data(), std::errc {}};
    if (!indexText.empty()) {
        indexResult = std::from_chars(
            indexText.data(), indexText.data() + indexText.size(), index, 16
        );
    }
    const auto [indexEnd, indexError] = indexResult;
    if (valueError != std::errc {} || valueEnd != idText.data() + idText.size()
        || indexError != std::errc {} || indexEnd != indexText.data() + indexText.size()
        || value <= 0 || index <= 0) {
        if (valueError != std::errc {} || valueEnd != idText.data() + idText.size() || value <= 0
            || !indexText.empty()) {
            return std::nullopt;
        }
    }
    return StringId {value, index};
}

}  // namespace

std::string StringId::toString() const
{
    if (!*this) {
        return {};
    }
    std::string result = "#" + hexadecimal(value);
    if (index != 0) {
        result += ":" + hexadecimal(index);
    }
    return result;
}

StringId StringHasher::allocate(const std::string& key, int index)
{
    const long lastIdBefore = nextId_;
    const auto found = ids_.find(key);
    if (found != ids_.end()) {
        if (producerTrace_ != nullptr) {
            const auto parts = producerTraceKeyParts(key);
            producerTrace_->record({"hasher.insert",
                                    "hit",
                                    "string_id_lookup_hit",
                                    {{"lastIdBefore", lastIdBefore},
                                     {"lastIdAfter", nextId_},
                                     {"data", parts.first},
                                     {"postfix", parts.second},
                                     {"flags", key.rfind("mapped:", 0) == 0U ? "mapped" : "data"},
                                     {"result", {{"value", found->second}, {"index", index}}}}});
        }
        return {found->second, index};
    }
    const long id = ++nextId_;
    ids_.emplace(key, id);
    const auto parts = producerTraceKeyParts(key);
    producerTraceEntries_.push_back(
        {id,
         parts.first,
         parts.second,
         key.rfind("mapped:", 0) == 0U ? "mapped" : "data",
         {},
         0,
         0}
    );
    if (producerTrace_ != nullptr) {
        producerTrace_->record({"hasher.insert",
                                "allocation",
                                "string_id_allocated",
                                {{"lastIdBefore", lastIdBefore},
                                 {"lastIdAfter", nextId_},
                                 {"data", parts.first},
                                 {"postfix", parts.second},
                                 {"flags", producerTraceEntries_.back().flags},
                                 {"related", nlohmann::json::array()},
                                 {"result", {{"value", id}, {"index", index}}}}});
    }
    return {id, index};
}

StringId StringHasher::getId(const std::string& data)
{
    return allocate("data:" + data);
}

StringId StringHasher::getMappedNameId(
    const std::string& data,
    int index,
    const std::string& postfix,
    const std::vector<StringId>& relatedIds
)
{
    const long lastIdBefore = nextId_;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp
    // ::StringHasher::getID(const Data::MappedName&, const QVector<StringIDRef>&).  A lookup is
    // performed before PrefixIDIndex handling; only a newly produced StringID can turn a leading
    // `#<id>:<index>` into a related prefix SID.  Doing that transformation before lookup changes
    // which producer owns the document entry and drifts the later U/L StringIDRefs.
    const std::string encoded = encodedPostfix(postfix, *this);
    const std::string uncollapsedKey = "mapped:" + data + "\x1f" + encoded;
    const std::optional<StringId> prefixId = encodedPrefixId(data);
    if (producerTrace_ != nullptr) {
        producerTrace_->record({
            "mapped_name.parse",
            prefixId || data.empty() || data.front() != '#' ? "parsed" : "rejected",
            prefixId ? "prefix_id_parsed"
                : data.empty() || data.front() != '#' ? "plain_mapped_name_data"
                                                      : "invalid_prefix_id",
            {{"rawData", data},
             {"rawPostfix", postfix},
             {"purpose", "StringHasher::getMappedNameId"},
             {"prefixId", prefixId ? prefixId->value : 0L},
             {"prefixIdIndex", prefixId ? prefixId->index : 0},
             {"failurePosition", prefixId || data.empty() || data.front() != '#' ? -1 : 0}},
        });
    }
    // A producer receives `getMappedName(..., &sids)` as an entry-local pair.  For a leading
    // `#id`, the matching StringIDRef can carry the rendered index even though the compact
    // `MappedName` data itself remains `#id`.  This is a prefix reference, not an IndexedName
    // such as `Edge2`, so it must not allocate a separate data SID for `#id`.
    const bool indexedPrefixRef = index != 0 && prefixId.has_value();
    int returnedIndex = index;
    if (prefixId) {
        if (prefixId->index != 0) {
            returnedIndex = prefixId->index;
        }
    }
    const auto uncollapsed = ids_.find(uncollapsedKey);
    if (uncollapsed != ids_.end()) {
        const StringId result {uncollapsed->second, returnedIndex};
        emitMappedNameTrace(
            data,
            index,
            postfix,
            relatedIds,
            result,
            lastIdBefore,
            "hit",
            "uncollapsed_mapped_name_hit"
        );
        return result;
    }

    std::string storedData = data;
    if (prefixId && prefixId->index != 0) {
        // The final stored data has the trailing colon, while the reference itself keeps the
        // incoming index.  The source SID is moved to the first non-postfix related position
        // below, matching StringHasher's PrefixIDIndex sidecar.
        storedData = data.substr(0U, data.find(':') + 1U);
        returnedIndex = prefixId->index;
    }
    if (index != 0 && !indexedPrefixRef) {
        // StringHasher::getID(Data::MappedName, ...) has already split an IndexedName into its
        // alphabetic data part before it creates the StringIDRef.  It always interns that data
        // part as the index relation; looking for a digit here skipped the relation when callers
        // correctly supplied the split form (for example data="g", index=1).
        (void)getId(storedData);
    }
    const std::string key = "mapped:" + storedData + "\x1f" + encoded;
    const auto existing = ids_.find(key);
    if (existing != ids_.end()) {
        // A prior PrefixIDIndex insertion has the compact stored key.  Like StringHasher::insert,
        // preserve that existing entry's related SID list and return the caller's index only.
        const StringId result {existing->second, returnedIndex};
        emitMappedNameTrace(
            data,
            index,
            postfix,
            relatedIds,
            result,
            lastIdBefore,
            "hit",
            "collapsed_mapped_name_hit"
        );
        return result;
    }
    StringId result = allocate(key, returnedIndex);
    std::vector<StringId> newRelated;
    if (!postfix.empty() && postfix.find('#') == std::string::npos) {
        newRelated.push_back(getId(postfix));
    }
    if (index != 0 && !indexedPrefixRef) {
        newRelated.push_back(getId(storedData));
    }
    for (const StringId& related : relatedIds) {
        if (related) {
            newRelated.push_back(related);
        }
    }
    const std::size_t inserted = (!postfix.empty() && postfix.find('#') == std::string::npos ? 1U : 0U)
        + (index != 0 && !indexedPrefixRef ? 1U : 0U);
    if (prefixId) {
        const auto prefix = std::find_if(
            newRelated.begin() + static_cast<std::ptrdiff_t>(inserted),
            newRelated.end(),
            [&prefixId](const StringId& related) { return related.value == prefixId->value; }
        );
        if (prefix != newRelated.end()
            && prefix != newRelated.begin() + static_cast<std::ptrdiff_t>(inserted)) {
            std::iter_swap(newRelated.begin() + static_cast<std::ptrdiff_t>(inserted), prefix);
        }
        if (prefix != newRelated.end()) {
            prefixSourceIds_[result.value] = prefixId->value;
        }
    }
    if (newRelated.size() > 10U) {
        std::sort(
            newRelated.begin() + static_cast<std::ptrdiff_t>(inserted),
            newRelated.end(),
            [](const StringId& left, const StringId& right) {
                return left.value == right.value ? left.index < right.index
                                                 : left.value < right.value;
            }
        );
        newRelated.erase(
            std::unique(
                newRelated.begin() + static_cast<std::ptrdiff_t>(inserted),
                newRelated.end(),
                [](const StringId& left, const StringId& right) { return sameId(left, right); }
            ),
            newRelated.end()
        );
    }
    relatedIds_[result.value] = std::move(newRelated);
    refreshProducerTraceEntry(result.value);
    emitMappedNameTrace(
        data,
        index,
        postfix,
        relatedIds,
        result,
        lastIdBefore,
        "allocation",
        "mapped_name_allocated"
    );
    return result;
}

void StringHasher::emitMappedNameTrace(const std::string& data,
                                       int index,
                                       const std::string& postfix,
                                       const std::vector<StringId>& relatedIds,
                                       StringId result,
                                       long lastIdBefore,
                                       const std::string& decision,
                                       const std::string& reason) const
{
    if (producerTrace_ == nullptr) {
        return;
    }
    const std::optional<StringId> prefix = encodedPrefixId(data);
    const auto storedRelated = relatedIds_.find(result.value);
    producerTrace_->record({
        "hasher.mapped_name",
        decision,
        reason,
        {{"lastIdBefore", lastIdBefore},
         {"lastIdAfter", nextId_},
         {"data", data},
         {"postfix", postfix},
         {"index", index},
         {"inputRelated", stringIdsJson(relatedIds)},
         {"orderedRelated",
          storedRelated == relatedIds_.end() ? nlohmann::json::array()
                                             : stringIdsJson(storedRelated->second)},
         {"prefixId", prefix ? prefix->value : 0},
         {"prefixIdIndex", prefix ? prefix->index : 0},
         {"result", {{"value", result.value}, {"index", result.index}}}},
    });
}

void StringHasher::refreshProducerTraceEntry(long value)
{
    if (value <= 0 || static_cast<std::size_t>(value) > producerTraceEntries_.size()) {
        return;
    }
    ProducerTraceEntry& entry = producerTraceEntries_.at(static_cast<std::size_t>(value - 1));
    const auto related = relatedIds_.find(value);
    entry.related = related == relatedIds_.end() ? std::vector<StringId> {} : related->second;
    const auto prefix = prefixSourceIds_.find(value);
    entry.prefixId = prefix == prefixSourceIds_.end() ? 0 : prefix->second;
    entry.prefixIdIndex = entry.prefixId == 0 ? 0 : producerIndexForMappedNameId({value, 0});
}

HashedMappedName StringHasher::hashMappedName(
    const std::string& data,
    int index,
    const std::string& postfix,
    const std::vector<StringId>& inputRefs
)
{
    StringId id = getMappedNameId(data, index, postfix, inputRefs);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp
    // ::StringHasher::getID(const Data::MappedName&, ...), after parsing a postfixed `#id`,
    // moves the matching input SID to the PrefixID position in `_sids`.  It returns an index
    // only when the data itself contains `#id:<index>` (or is an IndexedName); a bare `#id`
    // never borrows an endpoint index from that related SID.
    const auto relatedIt = relatedIds_.find(id.value);
    const std::vector<StringId> related = relatedIt == relatedIds_.end() ? std::vector<StringId> {}
                                                                         : relatedIt->second;
    std::vector<StringId> elementRefs {id};
    if (related != inputRefs) {
        for (const StringId& input : inputRefs) {
            if (input && !containsId(related, input)) {
                elementRefs.push_back(input);
            }
        }
    }
    return HashedMappedName {id, std::move(elementRefs)};
}

void StringHasher::rememberRelatedMappedName(
    const std::string& rawMappedName,
    std::vector<StringId> relatedIds
)
{
    if (rawMappedName.empty()) {
        return;
    }
    relatedIds.erase(
        std::remove_if(relatedIds.begin(), relatedIds.end(), [](const StringId& id) { return !id; }),
        relatedIds.end()
    );
    // StringHasher::getID() preserves source ElementIDRefs insertion order and only de-duplicates
    // the tail once more than ten IDs are related. Sorting every entry here changes the next
    // producer's document-local SID lifecycle.
    mappedNameRelations_[rawMappedName] = std::move(relatedIds);
}

void StringHasher::rememberMappedName(
    const std::string& rawMappedName,
    StringId primaryId,
    std::vector<StringId> elementRefs
)
{
    rememberRelatedMappedName(rawMappedName, std::move(elementRefs));
    if (primaryId) {
        mappedNameIds_[rawMappedName] = primaryId;
        // FreeCAD: src/App/StringHasher.cpp::StringHasher::getID(const Data::MappedName&,
        // const QVector<StringIDRef>&) first looks up the full (data, postfix) StringID before
        // it allocates. ElementMap::setElementName() can hand a later maker an already hashed
        // `#id[:index];postfix` plus entry-local refs; that lookup must resolve back to the
        // original #id rather than allocate a second producer SID. This is request-local cache
        // registration from a Part producer write, never recovery from a response token.
        const std::size_t separator = rawMappedName.find(';');
        const std::string data = rawMappedName.substr(0U, separator);
        const std::string postfix = separator == std::string::npos
            ? std::string {}
            : rawMappedName.substr(separator);
        if (!data.empty() && !postfix.empty()) {
            ids_.emplace("mapped:" + data + "\x1f" + postfix, primaryId.value);
            // getID() encodes a textual postfix through getId(postfix) before it looks up the
            // StringID. A producer write that already performed that encoding therefore has an
            // existing data entry; register the same compact key without allocating a new ID.
            if (postfix.find('#') == std::string::npos) {
                const auto encoded = ids_.find("data:" + postfix);
                if (encoded != ids_.end()) {
                    ids_.emplace(
                        "mapped:" + data + "\x1f" + StringId {encoded->second}.toString(),
                        primaryId.value
                    );
                }
            }
        }
    }
}

std::vector<StringId> StringHasher::relatedIdsForMappedName(const std::string& rawMappedName) const
{
    const auto found = mappedNameRelations_.find(rawMappedName);
    return found == mappedNameRelations_.end() ? std::vector<StringId> {} : found->second;
}

std::optional<StringId> StringHasher::mappedNameId(const std::string& rawMappedName) const
{
    const auto found = mappedNameIds_.find(rawMappedName);
    return found == mappedNameIds_.end() ? std::optional<StringId> {} : found->second;
}

int StringHasher::producerIndexForMappedNameId(StringId id) const
{
    if (!id) {
        return 0;
    }
    // FreeCAD: src/App/StringHasher.cpp::StringHasher::getID(Data::MappedName, ...), stores
    // PrefixID for a postfixed `#<id>` only after matching that ID in the entry-local related
    // StringIDRefs. A later ElementMap producer carries the root PrefixID as the StringIDRef
    // index (for example #8 -> #3f:8 and #1f -> #28:8); this follows StringID metadata rather
    // than inspecting response names or borrowing an arbitrary related ref.
    const auto firstParent = prefixSourceIds_.find(id.value);
    if (firstParent == prefixSourceIds_.end() || firstParent->second <= 0
        || firstParent->second == id.value) {
        return id.index;
    }
    long root = firstParent->second;
    std::size_t depth = 0U;
    while (depth++ < prefixSourceIds_.size()) {
        const auto parent = prefixSourceIds_.find(root);
        if (parent == prefixSourceIds_.end() || parent->second <= 0 || parent->second == root) {
            break;
        }
        root = parent->second;
    }
    return static_cast<int>(root);
}

nlohmann::json StringHasher::inspectProducerTraceState() const
{
    nlohmann::json entries = nlohmann::json::array();
    std::map<long, std::vector<std::string>> lookupKeys;
    for (const auto& item : ids_) {
        lookupKeys[item.second].push_back(item.first);
    }
    for (auto& item : lookupKeys) {
        std::sort(item.second.begin(), item.second.end());
    }
    for (const ProducerTraceEntry& entry : producerTraceEntries_) {
        entries.push_back({
            {"value", entry.value},
            {"data", entry.data},
            {"postfix", entry.postfix},
            {"flags", entry.flags},
            {"related", stringIdsJson(entry.related)},
            {"prefixId", entry.prefixId},
            {"prefixIdIndex", entry.prefixIdIndex},
            {"lookupKeys", lookupKeys[entry.value]},
        });
    }

    nlohmann::json mappedLookups = nlohmann::json::array();
    std::vector<std::string> rawNames;
    rawNames.reserve(mappedNameRelations_.size());
    for (const auto& item : mappedNameRelations_) {
        rawNames.push_back(item.first);
    }
    std::sort(rawNames.begin(), rawNames.end());
    for (const std::string& rawName : rawNames) {
        const auto primary = mappedNameIds_.find(rawName);
        mappedLookups.push_back({
            {"rawMappedName", rawName},
            {"primaryId",
             primary == mappedNameIds_.end()
                 ? nlohmann::json(nullptr)
                 : nlohmann::json{{"value", primary->second.value},
                                  {"index", primary->second.index}}},
            {"orderedRelated", stringIdsJson(mappedNameRelations_.at(rawName))},
        });
    }
    return {{"lastId", nextId_}, {"entries", entries}, {"mappedNameLookup", mappedLookups}};
}

void StringHasher::clear()
{
    nextId_ = 0;
    ids_.clear();
    relatedIds_.clear();
    prefixSourceIds_.clear();
    mappedNameRelations_.clear();
    mappedNameIds_.clear();
    producerTraceEntries_.clear();
}

std::optional<StringId> parseStringId(const std::string& value)
{
    if (value.empty() || value.front() != '#') {
        return std::nullopt;
    }
    const std::size_t separator = value.find(':');
    const std::string idText
        = value.substr(1, separator == std::string::npos ? std::string::npos : separator - 1);
    long id = 0;
    const auto [idEnd, idError] = std::from_chars(idText.data(), idText.data() + idText.size(), id, 16);
    if (idError != std::errc {} || idEnd != idText.data() + idText.size() || id <= 0) {
        return std::nullopt;
    }
    int index = 0;
    if (separator != std::string::npos) {
        const std::string indexText = value.substr(separator + 1);
        const auto [indexEnd, indexError]
            = std::from_chars(indexText.data(), indexText.data() + indexText.size(), index, 16);
        if (indexError != std::errc {} || indexEnd != indexText.data() + indexText.size()) {
            return std::nullopt;
        }
    }
    return StringId {id, index};
}

}  // namespace cad_core::app
