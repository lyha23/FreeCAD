#include "cad_core/features/sketch_object.h"

#include "cad_core/features/feature_executor.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <optional>

namespace cad_core::features {

namespace {

double readNumber2(const nlohmann::json& value, std::size_t index, bool& ok)
{
    if (!value.is_array() || value.size() != 2 || !value.at(index).is_number()) {
        ok = false;
        return 0.0;
    }
    return value.at(index).get<double>();
}

bool samePoint(const gp_Pnt& left, const gp_Pnt& right)
{
    constexpr double eps = 1e-9;
    return std::abs(left.X() - right.X()) < eps && std::abs(left.Y() - right.Y()) < eps;
}

}  // namespace

void executeSketchObject(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/Sketcher/App/SketchObject.cpp
    if (!rejectUnsupportedProperties(object, context, {"Geometry", "Constraints"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Geometry") || !object.properties.at("Geometry").is_array()) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Sketch Geometry must be a list", object.name, "Geometry");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto constraintsIt = object.properties.find("Constraints");
    if (constraintsIt != object.properties.end() && constraintsIt->is_array() && !constraintsIt->empty()) {
        runtime::addDiagnostic(context.diagnostics, "error", "unsupported_property", "Sketch constraints are not solved in the MVP", object.name, "Constraints");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto& geometry = object.properties.at("Geometry");
    for (const auto& item : geometry) {
        if (!item.is_object() || !item.contains("kind") || item.at("kind") != "LineSegment") {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_geometry",
                                   "Sketch Geometry supports only LineSegment in the MVP",
                                   object.name,
                                   "Geometry");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
    }
    if (geometry.size() < 3) {
        runtime::addDiagnostic(context.diagnostics, "error", "open_profile", "Sketch Geometry must be a closed profile", object.name, "Geometry");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeWire wireBuilder;
    std::optional<gp_Pnt> firstStart;
    std::optional<gp_Pnt> expectedStart;
    std::optional<gp_Pnt> lastEnd;

    for (std::size_t index = 0; index < geometry.size(); ++index) {
        const auto& item = geometry.at(index);
        bool ok = true;
        const double sx = readNumber2(item.at("start"), 0, ok);
        const double sy = readNumber2(item.at("start"), 1, ok);
        const double ex = readNumber2(item.at("end"), 0, ok);
        const double ey = readNumber2(item.at("end"), 1, ok);
        if (!ok) {
            runtime::addDiagnostic(context.diagnostics, "error", "unsupported_geometry", "LineSegment start/end must be two numbers", object.name, "Geometry");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        gp_Pnt start(sx, sy, 0.0);
        gp_Pnt end(ex, ey, 0.0);
        if (index == 0) {
            firstStart = start;
        }
        else if (!expectedStart || !samePoint(start, *expectedStart)) {
            runtime::addDiagnostic(context.diagnostics, "error", "open_profile", "Sketch LineSegments must connect in order", object.name, "Geometry");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        wireBuilder.Add(BRepBuilderAPI_MakeEdge(start, end).Edge());
        expectedStart = end;
        lastEnd = end;
    }

    if (!firstStart || !lastEnd || !samePoint(*firstStart, *lastEnd) || !wireBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "open_profile", "Sketch Geometry must be closed", object.name, "Geometry");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeFace faceBuilder(wireBuilder.Wire());
    if (!faceBuilder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "OCCT could not build a face from Sketch wire", object.name, "Geometry");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Profile, faceBuilder.Face()};
    context.objects[object.name] = {
        {"status", "ok"},
        {"profile", "occt_face"},
        {"edge_count", geometry.size()},
    };
}

}  // namespace cad_core::features
