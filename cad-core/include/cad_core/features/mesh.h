#pragma once

// Compatibility facade: Mesh::Import now lives under the Mesh module path,
// aligned with FreeCAD src/Mod/Mesh/App/FeatureMeshImport.cpp.
// New internal code should include cad_core/mesh/feature_mesh_import.h.
#include "cad_core/mesh/feature_mesh_import.h"
