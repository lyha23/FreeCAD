#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design
{

// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.h
// declares "class ShapeBinder : public PartDesign::Feature" and
// /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
// ::ShapeBinder::execute() writes Shape from "updatedShape()".
void executeShapeBinder(const app::DocumentObject& object, runtime::ComputeContext& context);

// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.h
// declares "class SubShapeBinder : public PartDesign::Feature" with Support, MakeFace,
// Fuse, Refine, Offset, Relative, BindMode and BindCopyOnChange request properties.
void executeSubShapeBinder(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
