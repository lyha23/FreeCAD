#pragma once

// Compatibility facade: SketchObject::buildInternals orchestration now lives under
// cad_core/sketcher, aligned with FreeCAD src/Mod/Sketcher/App/SketchObject.cpp.
// New internal code should include cad_core/sketcher/sketch_internal_builder.h.
#include "cad_core/sketcher/sketch_internal_builder.h"
