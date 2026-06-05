#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::app {
struct ReferenceShadow;
}

namespace cad_core::part {

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

struct ReferenceShadowRecoveryResult {
    ReferenceMatchStatus status = ReferenceMatchStatus::Missing;
    std::string subname;
    std::optional<TopoDS_Shape> shape;
    std::string reason;
    std::string diagnosticCode;
    bool usedBrep = false;
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
                                                           const std::string& sha256,
                                                           const std::string& expectedShapeType,
                                                           std::string& error);

ReferenceMatchResult findUniqueSubshapeByReferenceBrepSnapshot(const TopoDS_Shape& currentShape,
                                                               const std::string& subnamePrefix,
                                                               const std::string& format,
                                                               const std::string& data,
                                                               long long byteLength,
                                                               const std::string& sha256,
                                                               const std::string& expectedShapeType,
                                                               std::string& error);

// FreeCAD basis: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp
// ::Feature::onBeforeChange() stores the old referenced subshape in ElementCache; cad-core's
// ReferenceShadow is the approved stateless single-subshape evidence channel. Keep fingerprint /
// BREP matching centralized here so feature executors do not grow their own recovery policy.
ReferenceShadowRecoveryResult recoverReferenceShadowSubshape(const TopoDS_Shape& currentShape,
                                                             const std::string& subnamePrefix,
                                                             const app::ReferenceShadow& shadow);

bool referenceShadowMatchesCurrentSubshape(const TopoDS_Shape& currentShape,
                                           const std::string& subnamePrefix,
                                           const std::string& currentSubname,
                                           const TopoDS_Shape& currentSubshape,
                                           const app::ReferenceShadow& shadow);

}  // namespace cad_core::part
