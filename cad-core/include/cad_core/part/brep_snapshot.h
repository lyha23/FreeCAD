#pragma once

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>

// Part/App ReferenceShadow.brep transport helper.
namespace cad_core::part {

struct BrepTextSnapshot {
    std::string format;
    long long byteLength = 0;
    std::string sha256;
    std::string data;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeature.cpp
// ::Feature::onBeforeChange() stores the old referenced subshape in ElementCache::shape before
// Shape changes. cad-core serializes only that single subshape as request-carried recovery
// evidence; it is not a modeling input or a topoNamingState geometry payload.
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

}  // namespace cad_core::part
