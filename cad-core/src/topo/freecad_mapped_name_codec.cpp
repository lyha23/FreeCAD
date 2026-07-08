#include "cad_core/topo/freecad_mapped_name_codec.h"

#include <cctype>
#include <sstream>

namespace cad_core::topo {

namespace {

constexpr const char* elementMapPrefix = ";";
constexpr const char* postfixTag = ";:H";
constexpr const char* postfixDuplicate = ";D";

bool isHexDigit(char value)
{
    return std::isxdigit(static_cast<unsigned char>(value)) != 0;
}

std::string hexLong(long value)
{
    std::ostringstream stream;
    stream << std::hex;
    if (value < 0) {
        stream << '-' << -value;
    }
    else {
        stream << value;
    }
    return stream.str();
}

std::string normalizedOperationPostfix(const std::string& operationPostfix)
{
    if (operationPostfix.empty()) {
        return {};
    }
    if (operationPostfix.front() == elementMapPrefix[0]) {
        return operationPostfix;
    }
    return std::string {elementMapPrefix} + operationPostfix;
}

std::string elementTypeCode(const std::string& elementType)
{
    if (elementType.empty()) {
        return {};
    }
    switch (elementType.front()) {
        case 'F':
        case 'E':
        case 'V':
            return std::string {elementType.front()};
        default:
            return {};
    }
}

part::MappedNameProvenanceStatus provenanceStatusForCodecStatus(
    FreeCadMappedNameCodecStatus status
)
{
    switch (status) {
        case FreeCadMappedNameCodecStatus::Encoded:
            return part::MappedNameProvenanceStatus::SourceBacked;
        case FreeCadMappedNameCodecStatus::MissingTag:
            return part::MappedNameProvenanceStatus::MissingTag;
        case FreeCadMappedNameCodecStatus::MissingOperation:
            return part::MappedNameProvenanceStatus::MissingOperation;
        case FreeCadMappedNameCodecStatus::MissingSourceElement:
        case FreeCadMappedNameCodecStatus::MissingElementType:
        case FreeCadMappedNameCodecStatus::Blocked:
            return part::MappedNameProvenanceStatus::Blocked;
    }
    return part::MappedNameProvenanceStatus::Blocked;
}

bool startsWith(const std::string& value, std::size_t offset, const char* prefix)
{
    const std::string prefixValue {prefix};
    return offset + prefixValue.size() <= value.size()
        && value.compare(offset, prefixValue.size(), prefixValue) == 0;
}

}  // namespace

std::string freeCadMappedNameCodecStatusName(FreeCadMappedNameCodecStatus status)
{
    switch (status) {
        case FreeCadMappedNameCodecStatus::Encoded:
            return "encoded";
        case FreeCadMappedNameCodecStatus::MissingSourceElement:
            return "missing_source_element";
        case FreeCadMappedNameCodecStatus::MissingTag:
            return "missing_tag";
        case FreeCadMappedNameCodecStatus::MissingOperation:
            return "missing_operation";
        case FreeCadMappedNameCodecStatus::MissingElementType:
            return "missing_element_type";
        case FreeCadMappedNameCodecStatus::Blocked:
            return "blocked";
    }
    return "blocked";
}

FreeCadMappedNameCodecResult encodeFreeCadMappedName(
    const part::MappedNameProvenance& provenance
)
{
    if (provenance.status == part::MappedNameProvenanceStatus::Blocked) {
        return {FreeCadMappedNameCodecStatus::Blocked, {}, {}, "provenance is blocked"};
    }
    if (provenance.sourceElement.empty()) {
        return {FreeCadMappedNameCodecStatus::MissingSourceElement,
                {},
                {},
                "source element evidence is required"};
    }
    if (!provenance.sourceTag) {
        return {FreeCadMappedNameCodecStatus::MissingTag, {}, {}, "source tag evidence is required"};
    }
    const std::string operationPostfix = normalizedOperationPostfix(provenance.operationPostfix);
    if (operationPostfix.empty()) {
        return {FreeCadMappedNameCodecStatus::MissingOperation,
                {},
                {},
                "operation postfix evidence is required"};
    }
    const std::string typeCode = elementTypeCode(provenance.elementType);
    if (typeCode.empty()) {
        return {FreeCadMappedNameCodecStatus::MissingElementType,
                {},
                {},
                "element type evidence must be F, E, or V"};
    }

    std::string raw = provenance.sourceElement + operationPostfix;
    raw += postfixTag;
    raw += hexLong(*provenance.sourceTag);
    raw += ':';
    raw += hexLong(static_cast<long>(operationPostfix.size()));
    raw += ',';
    raw += typeCode;

    return {FreeCadMappedNameCodecStatus::Encoded, raw, canonicalizeFreeCadMappedName(raw), {}};
}

std::string canonicalizeFreeCadMappedName(const std::string& rawMappedName)
{
    std::string canonical;
    canonical.reserve(rawMappedName.size());

    for (std::size_t index = 0; index < rawMappedName.size();) {
        if (startsWith(rawMappedName, index, ":H")) {
            std::size_t cursor = index + 2;
            if (cursor < rawMappedName.size() && rawMappedName[cursor] == '*') {
                canonical.push_back(rawMappedName[index]);
                ++index;
                continue;
            }
            if (cursor < rawMappedName.size() && rawMappedName[cursor] == '-') {
                ++cursor;
            }
            const std::size_t tagStart = cursor;
            while (cursor < rawMappedName.size() && isHexDigit(rawMappedName[cursor])) {
                ++cursor;
            }
            if (cursor != tagStart) {
                bool hasLength = false;
                std::size_t lengthEnd = cursor;
                if (cursor < rawMappedName.size() && rawMappedName[cursor] == ':') {
                    std::size_t lengthCursor = cursor + 1;
                    const std::size_t lengthStart = lengthCursor;
                    while (lengthCursor < rawMappedName.size()
                           && isHexDigit(rawMappedName[lengthCursor])) {
                        ++lengthCursor;
                    }
                    if (lengthCursor != lengthStart) {
                        hasLength = true;
                        lengthEnd = lengthCursor;
                    }
                }
                canonical += hasLength ? ":H*:*" : ":H*";
                index = lengthEnd;
                continue;
            }
        }

        if (startsWith(rawMappedName, index, postfixDuplicate)) {
            std::size_t cursor = index + 2;
            if (cursor < rawMappedName.size() && rawMappedName[cursor] == '*') {
                canonical.push_back(rawMappedName[index]);
                ++index;
                continue;
            }
            const std::size_t duplicateStart = cursor;
            while (cursor < rawMappedName.size() && isHexDigit(rawMappedName[cursor])) {
                ++cursor;
            }
            if (cursor != duplicateStart) {
                canonical += ";D*";
                index = cursor;
                continue;
            }
        }

        canonical.push_back(rawMappedName[index]);
        ++index;
    }

    return canonical;
}

part::MappedNameProvenance encodedMappedNameProvenance(
    part::MappedNameProvenance provenance
)
{
    const FreeCadMappedNameCodecResult result = encodeFreeCadMappedName(provenance);
    provenance.status = provenanceStatusForCodecStatus(result.status);
    provenance.rawMappedName = result.rawMappedName;
    provenance.canonicalMappedName = result.canonicalMappedName;
    return provenance;
}

}  // namespace cad_core::topo
