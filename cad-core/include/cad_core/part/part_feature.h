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
void executePartExtrusion(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartOffset(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportBrep(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportStep(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartImportIges(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part

namespace cad_core::features {

using part::executePart;
using part::executePartBox;
using part::executePartCone;
using part::executePartCylinder;
using part::executePartEllipse;
using part::executePartEllipsoid;
using part::executePartExtrusion;
using part::executePartHelix;
using part::executePartImportBrep;
using part::executePartImportIges;
using part::executePartImportStep;
using part::executePartLine;
using part::executePartOffset;
using part::executePartPlane;
using part::executePartPrism;
using part::executePartRegularPolygon;
using part::executePartSphere;
using part::executePartSpiral;
using part::executePartTorus;
using part::executePartVertex;
using part::executePartWedge;

}  // namespace cad_core::features
