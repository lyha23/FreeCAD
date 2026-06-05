#pragma once

// Compatibility facade: Part/App TopoShape export, mesh summary, bbox and kernel metadata
// now live in cad_core/part/shape_exporter.h, aligned with FreeCAD
// src/Mod/Part/App/TopoShape.cpp export and tessellation responsibilities.
// New internal code should include cad_core/part/shape_exporter.h.
#include "cad_core/part/shape_exporter.h"
