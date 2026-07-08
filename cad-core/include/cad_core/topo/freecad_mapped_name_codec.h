#pragma once

#include "cad_core/part/topo_shape.h"

#include <string>

namespace cad_core::topo {

enum class FreeCadMappedNameCodecStatus {
    Encoded,
    MissingSourceElement,
    MissingTag,
    MissingOperation,
    MissingElementType,
    Blocked,
};

struct FreeCadMappedNameCodecResult {
    FreeCadMappedNameCodecStatus status = FreeCadMappedNameCodecStatus::Blocked;
    std::string rawMappedName;
    std::string canonicalMappedName;
    std::string message;
};

std::string freeCadMappedNameCodecStatusName(FreeCadMappedNameCodecStatus status);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
// ::ElementMap::encodeElementName(... masterTag ... postfix ... tag ...) appends the operation
// postfix and "POSTFIX_TAG" tag/type segment. /Users/li/Chili3DProject/FreeCAD/src/App/
// MappedName.cpp::MappedName::findTagInElementName() parses ";:H<tag>:<len>,<type>".
// /Users/li/Chili3DProject/FreeCAD/src/App/ElementNamingUtils.h defines POSTFIX_TAG,
// POSTFIX_GEN, POSTFIX_MOD, and POSTFIX_DUPLICATE. This helper is intentionally source-backed:
// it refuses to synthesize raw mapped names from stable/display names when producer evidence is
// missing.
FreeCadMappedNameCodecResult encodeFreeCadMappedName(
    const part::MappedNameProvenance& provenance
);

// Matches cad-core/tools/collect_freecad_expected.py::canonical_freecad_mapped_name(): tag
// hashes become :H* or :H*:*, and duplicate postfixes become ;D*. Full FreeCAD SID
// hash/dehash fidelity remains bounded by producer evidence and future child-map work.
std::string canonicalizeFreeCadMappedName(const std::string& rawMappedName);

part::MappedNameProvenance encodedMappedNameProvenance(
    part::MappedNameProvenance provenance
);

}  // namespace cad_core::topo
