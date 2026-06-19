#include "cad_core/part/part_geometry_curve.h"

#include "cad_core/app/document.h"
#include "cad_core/app/document_object.h"
#include "cad_core/part/part_feature.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include "part_feature_support.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepGProp.hxx>
#include <GC_MakeArcOfHyperbola.hxx>
#include <GC_MakeArcOfParabola.hxx>
#include <GC_MakeHyperbola.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <gce_MakeParab.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

constexpr const char* kPayloadKey = "partGeometryCurve";
constexpr const char* kConsumerPayloadKey = "partGeometryCurveConsumers";
constexpr const char* kDefaultObjectName = "PartConicCurve";

enum class CurveKind
{
    Hyperbola,
    Parabola,
};

struct PartConicCurveDTO
{
    std::string objectName;
    CurveKind kind = CurveKind::Hyperbola;
    std::array<double, 3> center {{0.0, 0.0, 0.0}};
    std::array<double, 3> normal {{0.0, 0.0, 1.0}};
    double angleXU = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double focal = 0.0;
};

std::string lowerAscii(std::string value)
{
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

void rejectDto(
    runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& code,
    const std::string& message,
    const std::string& property
)
{
    runtime::addDiagnostic(context.diagnostics, "error", code, message, objectName, property, "parse");
    context.objects[objectName] = {{"status", "error"}, {"feature", "part_geometry_curve"}};
}

std::optional<double> readFiniteNumber(
    const nlohmann::json& payload,
    runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& field
)
{
    const auto it = payload.find(field);
    const std::string property = std::string(kPayloadKey) + "." + field;
    if (it == payload.end()) {
        rejectDto(context, objectName, "missing_property", "PartConicCurveDTO requires " + field, property);
        return std::nullopt;
    }
    if (!it->is_number()) {
        rejectDto(
            context,
            objectName,
            "invalid_part_conic_number",
            "PartConicCurveDTO " + field + " must be a finite number",
            property
        );
        return std::nullopt;
    }
    const double value = it->get<double>();
    if (!std::isfinite(value)) {
        rejectDto(
            context,
            objectName,
            "invalid_part_conic_number",
            "PartConicCurveDTO " + field + " must be finite",
            property
        );
        return std::nullopt;
    }
    return value;
}

std::optional<std::array<double, 3>> readVector3(
    const nlohmann::json& payload,
    runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& field
)
{
    const auto it = payload.find(field);
    const std::string property = std::string(kPayloadKey) + "." + field;
    if (it == payload.end()) {
        rejectDto(context, objectName, "missing_property", "PartConicCurveDTO requires " + field, property);
        return std::nullopt;
    }
    if (!it->is_array() || it->size() != 3U) {
        rejectDto(
            context,
            objectName,
            "invalid_part_conic_number",
            "PartConicCurveDTO " + field + " must be a three-number vector",
            property
        );
        return std::nullopt;
    }

    std::array<double, 3> result {};
    for (std::size_t index = 0; index < 3U; ++index) {
        const auto& item = it->at(index);
        if (!item.is_number()) {
            rejectDto(
                context,
                objectName,
                "invalid_part_conic_number",
                "PartConicCurveDTO " + field + " must contain finite numbers",
                property
            );
            return std::nullopt;
        }
        result[index] = item.get<double>();
        if (!std::isfinite(result[index])) {
            rejectDto(
                context,
                objectName,
                "invalid_part_conic_number",
                "PartConicCurveDTO " + field + " must contain finite numbers",
                property
            );
            return std::nullopt;
        }
    }
    return result;
}

std::optional<CurveKind> readCurveKind(
    const nlohmann::json& payload,
    runtime::ComputeContext& context,
    const std::string& objectName
)
{
    const auto it = payload.find("curveKind");
    constexpr const char* property = "partGeometryCurve.curveKind";
    if (it == payload.end() || !it->is_string()) {
        rejectDto(context, objectName, "missing_property", "PartConicCurveDTO requires curveKind", property);
        return std::nullopt;
    }

    const std::string value = lowerAscii(it->get<std::string>());
    if (value == "hyperbola") {
        return CurveKind::Hyperbola;
    }
    if (value == "parabola") {
        return CurveKind::Parabola;
    }
    rejectDto(
        context,
        objectName,
        "invalid_part_conic_curve_kind",
        "PartConicCurveDTO curveKind must be hyperbola or parabola",
        property
    );
    return std::nullopt;
}

std::optional<PartConicCurveDTO> parsePartConicCurveDTO(
    const nlohmann::json& payload,
    runtime::ComputeContext& context
)
{
    std::string objectName = kDefaultObjectName;
    if (!payload.is_object()) {
        rejectDto(context, objectName, "parse_error", "partGeometryCurve entries must be JSON objects", kPayloadKey);
        return std::nullopt;
    }
    const auto nameIt = payload.find("name");
    if (nameIt != payload.end() && nameIt->is_string() && !nameIt->get<std::string>().empty()) {
        objectName = nameIt->get<std::string>();
    }

    auto kind = readCurveKind(payload, context, objectName);
    if (!kind) {
        return std::nullopt;
    }
    auto center = readVector3(payload, context, objectName, "center");
    if (!center) {
        return std::nullopt;
    }
    auto normal = readVector3(payload, context, objectName, "normal");
    if (!normal) {
        return std::nullopt;
    }
    const double normalMagnitude = std::sqrt(
        normal->at(0) * normal->at(0) + normal->at(1) * normal->at(1)
        + normal->at(2) * normal->at(2)
    );
    if (normalMagnitude <= Precision::Confusion()) {
        rejectDto(
            context,
            objectName,
            "invalid_part_conic_axis",
            "PartConicCurveDTO normal must be non-zero",
            "partGeometryCurve.normal"
        );
        return std::nullopt;
    }

    auto angleXU = readFiniteNumber(payload, context, objectName, "angleXU");
    if (!angleXU) {
        return std::nullopt;
    }
    auto startAngle = readFiniteNumber(payload, context, objectName, "startAngle");
    if (!startAngle) {
        return std::nullopt;
    }
    auto endAngle = readFiniteNumber(payload, context, objectName, "endAngle");
    if (!endAngle) {
        return std::nullopt;
    }
    if (std::abs(*endAngle - *startAngle) <= Precision::PConfusion()) {
        rejectDto(
            context,
            objectName,
            "invalid_part_conic_trim",
            "PartConicCurveDTO startAngle and endAngle must define a finite edge",
            "partGeometryCurve.startAngle"
        );
        return std::nullopt;
    }

    PartConicCurveDTO dto;
    dto.objectName = objectName;
    dto.kind = *kind;
    dto.center = *center;
    dto.normal = *normal;
    dto.angleXU = *angleXU;
    dto.startAngle = *startAngle;
    dto.endAngle = *endAngle;

    if (dto.kind == CurveKind::Hyperbola) {
        auto majorRadius = readFiniteNumber(payload, context, objectName, "majorRadius");
        if (!majorRadius) {
            return std::nullopt;
        }
        auto minorRadius = readFiniteNumber(payload, context, objectName, "minorRadius");
        if (!minorRadius) {
            return std::nullopt;
        }
        if (*majorRadius <= Precision::Confusion()) {
            rejectDto(
                context,
                objectName,
                "invalid_part_conic_radius",
                "PartConicCurveDTO hyperbola majorRadius must be positive",
                "partGeometryCurve.majorRadius"
            );
            return std::nullopt;
        }
        if (*minorRadius <= Precision::Confusion()) {
            rejectDto(
                context,
                objectName,
                "invalid_part_conic_radius",
                "PartConicCurveDTO hyperbola minorRadius must be positive",
                "partGeometryCurve.minorRadius"
            );
            return std::nullopt;
        }
        dto.majorRadius = *majorRadius;
        dto.minorRadius = *minorRadius;
    }
    else {
        auto focal = readFiniteNumber(payload, context, objectName, "focal");
        if (!focal) {
            return std::nullopt;
        }
        if (*focal <= Precision::Confusion()) {
            rejectDto(
                context,
                objectName,
                "invalid_part_conic_focal",
                "PartConicCurveDTO parabola focal must be positive",
                "partGeometryCurve.focal"
            );
            return std::nullopt;
        }
        dto.focal = *focal;
    }

    return dto;
}

double linearLengthForShape(const TopoDS_Shape& shape)
{
    GProp_GProps properties;
    BRepGProp::LinearProperties(shape, properties);
    return properties.Mass();
}

const char* curveKindName(CurveKind kind)
{
    return kind == CurveKind::Hyperbola ? "hyperbola" : "parabola";
}

const char* partGeometryTypeName(CurveKind kind)
{
    return kind == CurveKind::Hyperbola ? "Part.Hyperbola" : "Part.Parabola";
}

std::string geomAbsCurveTypeName(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    switch (curve.GetType()) {
        case GeomAbs_Hyperbola:
            return "GeomAbs_Hyperbola";
        case GeomAbs_Parabola:
            return "GeomAbs_Parabola";
        case GeomAbs_Line:
            return "GeomAbs_Line";
        case GeomAbs_Circle:
            return "GeomAbs_Circle";
        case GeomAbs_Ellipse:
            return "GeomAbs_Ellipse";
        case GeomAbs_BSplineCurve:
            return "GeomAbs_BSplineCurve";
        case GeomAbs_BezierCurve:
            return "GeomAbs_BezierCurve";
        case GeomAbs_OffsetCurve:
            return "GeomAbs_OffsetCurve";
        case GeomAbs_OtherCurve:
            return "GeomAbs_OtherCurve";
    }
    return "GeomAbs_OtherCurve";
}

std::optional<TopoDS_Edge> makePartConicEdge(
    const PartConicCurveDTO& dto,
    runtime::ComputeContext& context
)
{
    const gp_Pnt center(dto.center[0], dto.center[1], dto.center[2]);
    const gp_Dir normal(dto.normal[0], dto.normal[1], dto.normal[2]);
    const gp_Ax1 normalAxis(center, normal);
    gp_Ax2 axes(center, normal);
    axes.Rotate(normalAxis, dto.angleXU);

    try {
        if (dto.kind == CurveKind::Hyperbola) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
            // ::GeomHyperbola::Save/Restore() persists "CenterX/Y/Z", "NormalX/Y/Z",
            // "MajorRadius", "MinorRadius" and "AngleXU"; ::GeomArcOfHyperbola::Save/Restore()
            // adds "StartAngle"/"EndAngle" and restores with
            // "GC_MakeArcOfHyperbola(..., Standard_True)".
            GC_MakeHyperbola conic(axes, dto.majorRadius, dto.minorRadius);
            if (!conic.IsDone()) {
                throw Standard_Failure("Failed to create hyperbola");
            }
            GC_MakeArcOfHyperbola arc(conic.Value()->Hypr(), dto.startAngle, dto.endAngle, Standard_True);
            if (!arc.IsDone()) {
                throw Standard_Failure("Failed to create hyperbola arc");
            }
            BRepBuilderAPI_MakeEdge edgeBuilder(arc.Value());
            if (!edgeBuilder.IsDone()) {
                throw Standard_Failure("Failed to create hyperbola edge");
            }
            return edgeBuilder.Edge();
        }

        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomParabola::Save/Restore() persists "CenterX/Y/Z", "NormalX/Y/Z", "Focal"
        // and "AngleXU"; ::GeomArcOfParabola::Save/Restore() adds "StartAngle"/"EndAngle"
        // and restores with "GC_MakeArcOfParabola(..., Standard_True)".
        gce_MakeParab conic(axes, dto.focal);
        if (!conic.IsDone()) {
            throw Standard_Failure("Failed to create parabola");
        }
        GC_MakeArcOfParabola arc(conic.Value(), dto.startAngle, dto.endAngle, Standard_True);
        if (!arc.IsDone()) {
            throw Standard_Failure("Failed to create parabola arc");
        }
        BRepBuilderAPI_MakeEdge edgeBuilder(arc.Value());
        if (!edgeBuilder.IsDone()) {
            throw Standard_Failure("Failed to create parabola edge");
        }
        return edgeBuilder.Edge();
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            dto.objectName,
            kPayloadKey,
            "runtime"
        );
        context.objects[dto.objectName] = {{"status", "error"}, {"feature", "part_geometry_curve"}};
        return std::nullopt;
    }
}

nlohmann::json metadataForDTO(const PartConicCurveDTO& dto, const TopoDS_Edge& edge)
{
    nlohmann::json metadata = {
        {"feature", "part_geometry_curve"},
        {"dto", "PartConicCurveDTO"},
        {"curve_kind", curveKindName(dto.kind)},
        {"curve_type", geomAbsCurveTypeName(edge)},
        {"part_geometry_type", partGeometryTypeName(dto.kind)},
        {"center", dto.center},
        {"normal", dto.normal},
        {"angle_xu", dto.angleXU},
        {"start_angle", dto.startAngle},
        {"end_angle", dto.endAngle},
        {"length", linearLengthForShape(edge)},
    };
    if (dto.kind == CurveKind::Hyperbola) {
        metadata["major_radius"] = dto.majorRadius;
        metadata["minor_radius"] = dto.minorRadius;
    }
    else {
        metadata["focal"] = dto.focal;
    }
    return metadata;
}

void executePartConicCurveDTO(const nlohmann::json& payload, runtime::ComputeContext& context)
{
    const auto dto = parsePartConicCurveDTO(payload, context);
    if (!dto) {
        return;
    }
    const auto edge = makePartConicEdge(*dto, context);
    if (!edge) {
        return;
    }

    app::DocumentObject object;
    object.name = dto->objectName;
    object.typeId = "PartConicCurveDTO";
    part_feature_detail::publishPartShape(object, context, *edge, metadataForDTO(*dto, *edge));
}

std::vector<nlohmann::json> requestItems(const nlohmann::json& raw)
{
    const auto it = raw.find(kPayloadKey);
    if (it == raw.end()) {
        return {};
    }
    if (it->is_array()) {
        return it->get<std::vector<nlohmann::json>>();
    }
    return {*it};
}

std::vector<nlohmann::json> requestConsumerItems(
    const nlohmann::json& raw,
    runtime::ComputeContext& context
)
{
    const auto it = raw.find(kConsumerPayloadKey);
    if (it == raw.end()) {
        return {};
    }
    if (it->is_array()) {
        return it->get<std::vector<nlohmann::json>>();
    }
    if (it->is_object()) {
        return {*it};
    }
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "parse_error",
        "partGeometryCurveConsumers must be an object or a list of objects",
        {},
        kConsumerPayloadKey,
        "parse"
    );
    return {};
}

void executePartGeometryCurveConsumers(const nlohmann::json& raw, runtime::ComputeContext& context)
{
    const auto consumerItems = requestConsumerItems(raw, context);
    if (consumerItems.empty()) {
        return;
    }

    const nlohmann::json consumerDocument = {{"Objects", consumerItems}};
    auto [document, diagnostics] = app::parseDocument(consumerDocument);
    context.diagnostics.insert(context.diagnostics.end(), diagnostics.begin(), diagnostics.end());

    for (const auto& object : document.objects) {
        if (object.typeId != "Part::Extrusion") {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "partGeometryCurveConsumers currently supports Part::Extrusion",
                object.name,
                "TypeId",
                "parse"
            );
            context.objects[object.name] = {{"status", "error"}};
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
        // ::Extrusion::extrudeShape(), the regular consumer path calls
        // "result.makeElementPrism(myShape, vec)". This request-local bridge seeds typed conic
        // edges first, then invokes the existing Part::Extrusion executor instead of registering
        // fake Part::Hyperbola or Part::Parabola DocumentObjects.
        executePartExtrusion(object, context);
    }
}

nlohmann::json meshForObject(const runtime::ComputeContext& context, const std::string& objectName)
{
    const auto it = context.mesh.find(objectName);
    return it == context.mesh.end() ? nlohmann::json(nullptr) : it->second;
}

nlohmann::json subshapesForObject(const runtime::ComputeContext& context, const std::string& objectName)
{
    const auto it = context.subshapes.find(objectName);
    return it == context.subshapes.end() ? nlohmann::json::object() : it->second;
}

}  // namespace

bool isPartGeometryCurveRequest(const nlohmann::json& raw)
{
    return raw.is_object() && raw.contains(kPayloadKey);
}

runtime::ComputeContext computePartGeometryCurveRequest(const nlohmann::json& raw)
{
    runtime::ComputeContext context;
    if (!isPartGeometryCurveRequest(raw)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "parse_error",
            "Document root must contain partGeometryCurve",
            {},
            kPayloadKey,
            "parse"
        );
        return context;
    }

    const auto items = requestItems(raw);
    if (items.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "parse_error",
            "partGeometryCurve must be an object or non-empty array",
            {},
            kPayloadKey,
            "parse"
        );
        return context;
    }
    for (const auto& item : items) {
        executePartConicCurveDTO(item, context);
    }
    executePartGeometryCurveConsumers(raw, context);
    return context;
}

nlohmann::json partGeometryCurveResultJson(const runtime::ComputeContext& context)
{
    nlohmann::json results = nlohmann::json::array();
    for (const auto& [objectName, object] : context.objects) {
        (void)object;
        results.push_back({
            {"object", objectName},
            {"mesh", meshForObject(context, objectName)},
            {"subshapes", subshapesForObject(context, objectName)},
        });
    }

    return {
        {"results", std::move(results)},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", runtime::diagnosticsToJson(context.diagnostics)},
    };
}

nlohmann::json partGeometryCurveLegacyResultJson(const runtime::ComputeContext& context)
{
    return {
        {"objects", context.objects},
        {"mesh", context.mesh},
        {"subshapes", context.subshapes},
        {"named_shapes", part::namedShapesToJson(context.namedShapes)},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", runtime::diagnosticsToJson(context.diagnostics)},
    };
}

}  // namespace cad_core::part
