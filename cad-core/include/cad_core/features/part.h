#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features
{

void executePart(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartVertex(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartLine(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartPlane(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartBox(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartCylinder(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartPrism(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartRegularPolygon(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartSphere(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartEllipsoid(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartCone(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartTorus(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartWedge(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartEllipse(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartHelix(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartSpiral(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartExtrusion(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartOffset(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportBrep(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportStep(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportIges(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
