#include "cad_core/part/brep_snapshot.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <iostream>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(5.0, 5.0, 1.0).Shape();
    TopExp_Explorer explorer(box, TopAbs_FACE);
    if (!explorer.More()) {
        return fail("probe source box has no face");
    }

    const auto snapshot = cad_core::part::brepTextSnapshotForShape(explorer.Current());
    if (!snapshot) {
        return fail("cannot create a BREP snapshot for the source face");
    }

    std::string error;
    const auto restored = cad_core::part::readBrepTextSnapshot(
        snapshot->data,
        snapshot->byteLength,
        snapshot->sha256,
        error
    );
    if (!restored || restored->IsNull() || restored->ShapeType() != TopAbs_FACE) {
        return fail("valid single-face BREP did not restore: " + error);
    }

    const auto requireRejected = [](const std::string& text, const std::string& label) {
        std::string rejection;
        const auto rejected = cad_core::part::readBrepTextSnapshot(
            text,
            static_cast<long long>(text.size()),
            cad_core::part::sha256Hex(text),
            rejection
        );
        return !rejected && !rejection.empty() ? 0 : fail(label + " was not rejected with a diagnostic");
    };

    const std::string truncated = snapshot->data.substr(0, snapshot->data.size() / 2U);
    if (requireRejected(truncated, "truncated BREP") != 0) {
        return 1;
    }

    std::string incompatibleVersion = snapshot->data;
    const std::string versionMarker = "CASCADE Topology V1";
    const std::size_t versionOffset = incompatibleVersion.find(versionMarker);
    if (versionOffset == std::string::npos) {
        return fail("probe BREP has no topology version marker");
    }
    incompatibleVersion.replace(versionOffset, versionMarker.size(), "CASCADE Topology V999");
    if (requireRejected(incompatibleVersion, "version-incompatible BREP") != 0) {
        return 1;
    }

    return 0;
}
