#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::topo {

enum class ReferenceMatchStatus {
    Missing,
    Ambiguous,
    Deleted,
    Split,
    Unique,
};

struct ReferenceMatchResult {
    ReferenceMatchStatus status = ReferenceMatchStatus::Missing;
    std::string subname;
    std::optional<TopoDS_Shape> shape;
};

// FreeCAD basis: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::findSubShapesWithSharedVertex() compares old and current subshape geometry.
// cad-core uses this lightweight fingerprint check before any future BREP matcher fallback.
nlohmann::json referenceFingerprintForShape(const TopoDS_Shape& shape);

std::optional<std::string> referenceFingerprintDriftReason(const TopoDS_Shape& currentShape,
                                                           const nlohmann::json& expectedFingerprint,
                                                           const std::string& expectedShapeType);

ReferenceMatchResult findUniqueSubshapeByReferenceFingerprint(const TopoDS_Shape& currentShape,
                                                              const std::string& subnamePrefix,
                                                              const nlohmann::json& expectedFingerprint,
                                                              const std::string& expectedShapeType);

ReferenceMatchResult findUniqueSubshapeByReferenceBrepText(const TopoDS_Shape& currentShape,
                                                           const std::string& subnamePrefix,
                                                           const std::string& brepText,
                                                           long long byteLength,
                                                           const std::string& expectedShapeType,
                                                           std::string& error);

}  // namespace cad_core::topo
