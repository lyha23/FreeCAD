#pragma once

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>

namespace cad_core::geometry {

struct BrepTextSnapshot {
    std::string format;
    long long byteLength = 0;
    std::string sha256;
    std::string data;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp
// ::Feature::onBeforeChange() stores old referenced subshape geometry in ElementCache before
// shape changes; cad-core serializes only that single subshape as request-carried recovery
// evidence and verifies the payload before topo matching consumes it.
std::optional<BrepTextSnapshot> brepTextSnapshotForShape(const TopoDS_Shape& shape);

std::optional<TopoDS_Shape> readBrepTextSnapshot(const std::string& brepText,
                                                 long long byteLength,
                                                 const std::string& sha256,
                                                 std::string& error);

std::optional<TopoDS_Shape> readBrepSnapshot(const std::string& format,
                                             const std::string& data,
                                             long long byteLength,
                                             const std::string& sha256,
                                             std::string& error);

std::string sha256Hex(const std::string& data);

}  // namespace cad_core::geometry
