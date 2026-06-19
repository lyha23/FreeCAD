#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct ResolvedFillingSources
{
    std::vector<FilledFaceSource> boundarySources;
    std::vector<NamedShapeSource> historySources;
};

void addFillingDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {}
)
{
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        code,
        message,
        object.name,
        property,
        "runtime",
        target
    );
    context.objects[object.name] = {
        {"status", "error"},
        {"feature", "part_filled_face"},
        {"helper", "Part.makeFilledFace"},
    };
}

bool sameDefault(double actual, double expected)
{
    return std::abs(actual - expected) <= 1e-12;
}

bool rejectNonDefaultNumber(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    double expected
)
{
    const auto value = app::readNumber(object, property);
    if (!value || sameDefault(*value, expected)) {
        return false;
    }
    addFillingDiagnostic(
        object,
        context,
        "unsupported_property",
        "Part.makeFilledFace non-default " + property + " is deferred until helper kwargs are expected-backed",
        property
    );
    return true;
}

bool rejectNonDefaultBool(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    bool expected
)
{
    const auto value = app::readBool(object, property);
    if (!value || *value == expected) {
        return false;
    }
    addFillingDiagnostic(
        object,
        context,
        "unsupported_property",
        "Part.makeFilledFace non-default " + property + " is deferred until helper kwargs are expected-backed",
        property
    );
    return true;
}

std::optional<FilledFaceDefaultParams> readDefaultFillingParams(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    FilledFaceDefaultParams params;
    if (rejectNonDefaultNumber(object, context, "Degree", params.degree)
        || rejectNonDefaultNumber(object, context, "PtsOnCurve", params.pointsOnCurve)
        || rejectNonDefaultNumber(object, context, "NumIter", params.iterations)
        || rejectNonDefaultBool(object, context, "Anisotropy", params.anisotropy)
        || rejectNonDefaultNumber(object, context, "Tol2d", params.tolerance2d)
        || rejectNonDefaultNumber(object, context, "Tol3d", params.tolerance3d)
        || rejectNonDefaultNumber(object, context, "TolG1", params.toleranceG1)
        || rejectNonDefaultNumber(object, context, "TolG2", params.toleranceG2)
        || rejectNonDefaultNumber(object, context, "MaxDegree", params.maxDegree)
        || rejectNonDefaultNumber(object, context, "MaxSegments", params.maxSegments)) {
        return std::nullopt;
    }
    return params;
}

nlohmann::json fillingParamsJson(const FilledFaceDefaultParams& params)
{
    return {
        {"degree", params.degree},
        {"points_on_curve", params.pointsOnCurve},
        {"iterations", params.iterations},
        {"anisotropy", params.anisotropy},
        {"tolerance_2d", params.tolerance2d},
        {"tolerance_3d", params.tolerance3d},
        {"tolerance_g1", params.toleranceG1},
        {"tolerance_g2", params.toleranceG2},
        {"max_degree", params.maxDegree},
        {"max_segments", params.maxSegments},
    };
}

nlohmann::json boundaryEvidenceJson(const std::vector<FilledFaceBoundaryEvidence>& evidence)
{
    nlohmann::json result = nlohmann::json::array();
    for (const FilledFaceBoundaryEvidence& item : evidence) {
        result.push_back({
            {"object", item.objectName},
            {"subname", item.subname},
            {"stable_subname", item.stableSubname},
            {"shape_kind", item.shapeKind},
        });
    }
    return result;
}

void addHistorySource(
    std::vector<NamedShapeSource>& sources,
    const std::string& objectName,
    const TopoDS_Shape& shape,
    const NamedShape* namedShape
)
{
    const auto duplicate = std::find_if(
        sources.begin(),
        sources.end(),
        [&](const NamedShapeSource& current) { return current.owner == objectName; }
    );
    if (duplicate == sources.end()) {
        sources.push_back(NamedShapeSource {objectName, shape, namedShape});
    }
}

std::optional<TopoDS_Shape> resolveFillingSubshape(
    const TopoDS_Shape& sourceShape,
    const NamedShape* namedShape,
    const app::Link& link,
    std::size_t index
)
{
    if (index >= link.subnames.size() || link.subnames[index].empty()) {
        return sourceShape;
    }
    const std::string current = link.subnames[index];
    const std::string stable = index < link.stableSubnames.size() && !link.stableSubnames[index].empty()
        ? link.stableSubnames[index]
        : current;
    if (namedShape != nullptr) {
        if (const auto shape = part::subshapeByName(*namedShape, current, stable)) {
            return shape;
        }
    }
    if (const auto shape = part::subshapeByName(sourceShape, current)) {
        return shape;
    }
    if (stable != current) {
        return part::subshapeByName(sourceShape, stable);
    }
    return std::nullopt;
}

std::optional<ResolvedFillingSources> resolveFillingBoundarySources(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeFilledFace(), parses "shapes" through getPyShapes() and rejects empty input with
    // "No input shape"; cad-core models those shapes as source-backed Boundary LinkSubList items.
    if (app::propertyValue(object, "Boundary") == nullptr) {
        addFillingDiagnostic(object, context, "missing_property", "No input shape", "Boundary");
        return std::nullopt;
    }
    const std::vector<app::Link> links = app::readLinks(object, "Boundary");
    if (links.empty()) {
        addFillingDiagnostic(object, context, "missing_property", "No input shape", "Boundary");
        return std::nullopt;
    }

    ResolvedFillingSources resolved;
    for (const app::Link& link : links) {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            addFillingDiagnostic(
                object,
                context,
                "missing_link_target",
                "Boundary target " + link.object + " did not produce a shape",
                "Boundary",
                link.object
            );
            return std::nullopt;
        }

        const auto namedShapeIt = context.namedShapes.find(link.object);
        const NamedShape* namedShape = namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second
                                                                                 : nullptr;
        addHistorySource(resolved.historySources, link.object, shapeIt->second.shape, namedShape);

        if (link.subnames.empty()) {
            resolved.boundarySources.push_back(FilledFaceSource {
                link.object,
                shapeIt->second.shape,
                namedShape,
                {},
                {},
            });
            continue;
        }

        for (std::size_t index = 0; index < link.subnames.size(); ++index) {
            const auto selected = resolveFillingSubshape(shapeIt->second.shape, namedShape, link, index);
            if (!selected || selected->IsNull()) {
                addFillingDiagnostic(
                    object,
                    context,
                    "invalid_subshape",
                    "Boundary subshape " + link.subnames[index] + " did not resolve",
                    "Boundary",
                    link.object
                );
                return std::nullopt;
            }
            const std::string stable = index < link.stableSubnames.size()
                    && !link.stableSubnames[index].empty()
                ? link.stableSubnames[index]
                : link.subnames[index];
            resolved.boundarySources.push_back(FilledFaceSource {
                link.object,
                *selected,
                namedShape,
                link.subnames[index],
                stable,
            });
        }
    }
    return resolved;
}

}  // namespace

void executePartFilledFace(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeFilledFace(), exposes a Python helper rather than a native Part::FilledFace object;
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementFilledFace() runs the BRepOffsetAPI_MakeFilling path. This executor
    // is a cad-core source-backed helper request object and must not be described as a native
    // FreeCAD DocumentObject.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Boundary",
             "Degree",
             "PtsOnCurve",
             "NumIter",
             "Anisotropy",
             "Tol2d",
             "Tol3d",
             "TolG1",
             "TolG2",
             "MaxDegree",
             "MaxSegments"}
        )) {
        context.objects[object.name] = {
            {"status", "error"},
            {"feature", "part_filled_face"},
            {"helper", "Part.makeFilledFace"},
        };
        return;
    }

    const auto params = readDefaultFillingParams(object, context);
    if (!params) {
        return;
    }
    const auto sources = resolveFillingBoundarySources(object, context);
    if (!sources) {
        return;
    }

    const FilledFaceBuild build = makeElementFilledFaceFromSources(
        object.name,
        sources->boundarySources,
        sources->historySources,
        *params
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addFillingDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part.makeFilledFace failed" : build.error,
            "Boundary"
        );
        return;
    }

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        {
            {"feature", "part_filled_face"},
            {"helper", "Part.makeFilledFace"},
            {"source_backed_helper", true},
            {"freecad_native_document_object", false},
            {"boundary_mode", build.boundaryMode},
            {"boundary_edge_count", build.boundaryEdgeCount},
            {"boundary_source_evidence", boundaryEvidenceJson(build.boundarySources)},
            {"default_params", fillingParamsJson(*params)},
            {"topo_naming_history", "maker_history:filling"},
        },
        build.namedShape
    );
}

}  // namespace cad_core::part
