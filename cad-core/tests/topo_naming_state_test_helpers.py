from __future__ import annotations

import re
from typing import Any


FREECAD_MAPPED_NAME_HASH_RE = re.compile(r":H(?!\*)-?[0-9A-Fa-f]+(?::[0-9A-Fa-f]+)?")
FREECAD_MAPPED_NAME_COLON_HASH_RE = re.compile(r":H:(?!\*)-?[0-9A-Fa-f]+")
FREECAD_MAPPED_NAME_DELETE_RE = re.compile(r";D(?!\*)[0-9A-Fa-f]+")
TOPO_INDEX_NAME_RE = re.compile(
    r"^(InternalFace|InternalEdge|InternalVertex|Face|Edge|Vertex|Wire|Shell|Solid|Compound)\d+$"
)
RESPONSE_SUBSHAPE_IDENTITY_FIELDS = (
    "kind",
    "indexed",
    "subname",
    "stableSubname",
    "identityStatus",
    "fullSubname",
    "rawFreecadMappedName",
    "canonicalFreecadMappedName",
    "resolvedIndexed",
    "sourceStableSubname",
    "fragmentStableSubname",
    "sourceGeometryKind",
)


def canonical_freecad_mapped_name(mapped_name: str) -> str:
    def replace_hash(match: re.Match[str]) -> str:
        return ":H*:*" if match.group(0).count(":") > 1 else ":H*"

    normalized = FREECAD_MAPPED_NAME_HASH_RE.sub(replace_hash, mapped_name)
    normalized = FREECAD_MAPPED_NAME_COLON_HASH_RE.sub(":H*", normalized)
    return FREECAD_MAPPED_NAME_DELETE_RE.sub(";D*", normalized)


def canonicalize_freecad_mapped_names_and_keys(value: Any) -> Any:
    if isinstance(value, str):
        canonical = canonical_freecad_mapped_name(value)
        return TOPO_INDEX_NAME_RE.sub(lambda match: f"{match.group(1)}*", canonical)
    if isinstance(value, list):
        return [canonicalize_freecad_mapped_names_and_keys(item) for item in value]
    if isinstance(value, dict):
        return {
            canonical_freecad_mapped_name(str(key)): canonicalize_freecad_mapped_names_and_keys(item)
            for key, item in value.items()
        }
    return value


def response_subshape_identity_index(response: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for object_result in response.get("results", []):
        if not isinstance(object_result, dict):
            continue
        object_name = object_result.get("object")
        if not isinstance(object_name, str) or not object_name:
            continue
        for subshape in object_result.get("subshapes", []):
            if not isinstance(subshape, dict):
                continue
            indexed = subshape.get("indexed")
            if not isinstance(indexed, str) or not indexed:
                continue
            identity = {
                field: subshape[field]
                for field in RESPONSE_SUBSHAPE_IDENTITY_FIELDS
                if field in subshape
            }
            result[f"{object_name}:{indexed}"] = identity
    return canonicalize_freecad_mapped_names_and_keys(result)
