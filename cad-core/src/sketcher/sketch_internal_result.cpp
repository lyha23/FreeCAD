#include "cad_core/sketcher/sketch_internal_result.h"

#include "cad_core/app/element_map.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include "sketch_object_operations.h"

namespace cad_core::sketcher
{

SketchInternalResult buildSketchInternalResult(const SketchInternalResultInput& input)
{
    SketchInternalResult result {
        runtime::ShapeValue {runtime::ShapeValue::Kind::Sketch, input.rawShape}
    };
    result.shapeValue.profileShape = input.profileShape;
    result.shapeValue.profileNormal = input.profileNormal;
    result.shapeValue.internalShape = input.internalShape;
    result.shapeValue.profileRequiresSubshapeSelection = input.profileRequiresSubshapeSelection;

    const bool hasNonEmptyInternalShape = input.internalShape && !input.internalShape->IsNull();
    if (hasNonEmptyInternalShape) {
        result.shapeValue.internalNamedShape = part::namedShapeForSketchInternalShape(
            input.objectName,
            input.rawShape,
            *input.internalShape,
            input.historyLedger
        );
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), writes auxiliary "InternalShape"; the web response
        // renders that request-local shape with InternalFace ids matching subshapes.
        result.mesh
            = part::meshForShape(*input.internalShape, "InternalFace", "InternalEdge", "InternalVertex");
    }

    const nlohmann::json internalSubshapes = hasNonEmptyInternalShape
        ? part::subshapeMapForShape(*input.internalShape, "Internal")
        : nlohmann::json::object();
    if (!input.rawShape.IsNull()) {
        result.subshapes = part::subshapeMapForShape(input.rawShape);
        if (hasNonEmptyInternalShape) {
            for (const auto& item : internalSubshapes.items()) {
                result.subshapes[item.key()] = item.value();
            }
        }
    }

    const nlohmann::json internalElementMap = hasNonEmptyInternalShape
        ? app::internalElementMapForSketch(input.rawShape, *input.internalShape)
        : nlohmann::json::object();

    result.objectFields = {
        {"profile", profileShapeLabel(input.profileShape)},
        {"profile_ready", input.profileShape.has_value()},
        {"internal_shape",
         input.internalShape ? (input.internalShape->IsNull() ? "empty" : "occt_internal_shape")
                             : "none"},
        {"internal_face_count", countSubshapesOfKind(internalSubshapes, "face")},
        {"internal_edge_count", countSubshapesOfKind(internalSubshapes, "edge")},
        {"internal_vertex_count", countSubshapesOfKind(internalSubshapes, "vertex")},
        {"internal_element_map", internalElementMap},
    };
    if (input.historyLedger) {
        result.objectFields["internal_shape_history_diagnostics"] =
            input.historyLedger->diagnosticsJson();
    }

    return result;
}

}  // namespace cad_core::sketcher
