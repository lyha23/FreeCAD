from __future__ import annotations

import json
import os
from typing import Any

import FreeCAD  # type: ignore
import Part  # type: ignore


PAYLOAD_PREFIX = "C12M2_PROBE_PAYLOAD="


def version_string(version: Any) -> str:
    if isinstance(version, (list, tuple)) and len(version) >= 4:
        return f"{version[0]}.{version[1]}.{version[2]} revision {str(version[3]).split()[0]}"
    if isinstance(version, (list, tuple)):
        return " ".join(str(item) for item in version if item)
    return str(version)


cfg = FreeCAD.ConfigDump()
version = FreeCAD.Version()
payload = {
    "freecad_version": version,
    "freecad_version_string": version_string(version),
    "build_revision": cfg.get("BuildRevision", ""),
    "build_revision_date": cfg.get("BuildRevisionDate", ""),
    "occt_version": getattr(Part, "OCC_VERSION", ""),
    "config_occt_version": cfg.get("OCC_VERSION", ""),
    "libpack": cfg.get("LibPack", ""),
    "libpack_version": cfg.get("LibPackVersion", ""),
    "freecad_libs": cfg.get("FreeCADLibs", ""),
    "app_home_path": cfg.get("AppHomePath", ""),
    "run_mode": cfg.get("RunMode", ""),
    "python_version": cfg.get("PYTHON_VERSION", ""),
    "env_FREECAD_LIBPACK": os.environ.get("FREECAD_LIBPACK"),
}
print(PAYLOAD_PREFIX + json.dumps(payload, ensure_ascii=False, sort_keys=True))
