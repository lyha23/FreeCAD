#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part
{

void executePart(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartVertex(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartLine(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartPlane(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartBox(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartCylinder(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartPrism(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartRegularPolygon(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartSphere(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartEllipsoid(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartCone(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartTorus(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartWedge(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartEllipse(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartHelix(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartSpiral(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartCompound(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartExtrusion(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartRuledSurface(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartOffset(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartOffset2D(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartThickness(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportBrep(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportStep(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportIges(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part
