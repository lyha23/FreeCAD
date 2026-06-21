#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <TopAbs_ShapeEnum.hxx>
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
    std::optional<FilledFaceSource> initialSurface;
    std::vector<FilledFaceSupportSource> supports;
    std::vector<FilledFaceOrderSource> orders;
};

void addFillingDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {},
    const std::string& subname = {}
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
        target,
        subname
    );
    context.objects[object.name] = {
        {"status", "error"},
        {"feature", "part_filled_face"},
        {"helper", "Part.makeFilledFace"},
    };
}

const nlohmann::json* rawPropertyPayload(
    const app::DocumentObject& object,
    const std::string& property
)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::vector<nlohmann::json> rawLinkSubListItems(
    const app::DocumentObject& object,
    const std::string& property
)
{
    const nlohmann::json* payload = rawPropertyPayload(object, property);
    if (payload == nullptr || !payload->is_object()) {
        return {};
    }
    const auto it = payload->find("SubSet");
    if (it == payload->end() || !it->is_array()) {
        return {};
    }
    std::vector<nlohmann::json> result;
    for (const auto& item : *it) {
        result.push_back(item);
    }
    return result;
}

std::string firstDeferredTarget(const app::DocumentObject& object, const std::string& property)
{
    if (const auto link = app::readLink(object, property)) {
        if (!link->object.empty()) {
            return link->object;
        }
    }
    const std::vector<app::Link> links = app::readLinks(object, property);
    if (!links.empty() && !links.front().object.empty()) {
        return links.front().object;
    }
    return object.name;
}

std::string firstDeferredSubname(const app::DocumentObject& object, const std::string& property)
{
    if (const auto link = app::readLink(object, property)) {
        if (!link->stableSubnames.empty() && !link->stableSubnames.front().empty()) {
            return link->stableSubnames.front();
        }
        if (!link->subnames.empty()) {
            return link->subnames.front();
        }
    }
    const std::vector<app::Link> links = app::readLinks(object, property);
    if (!links.empty()) {
        if (!links.front().stableSubnames.empty() && !links.front().stableSubnames.front().empty()) {
            return links.front().stableSubnames.front();
        }
        if (!links.front().subnames.empty()) {
            return links.front().subnames.front();
        }
    }
    return property;
}

std::string stableSubnameForLink(const app::Link& link, std::size_t index)
{
    if (index < link.stableSubnames.size() && !link.stableSubnames[index].empty()) {
        return link.stableSubnames[index];
    }
    if (index < link.subnames.size()) {
        return link.subnames[index];
    }
    return {};
}

bool addInvalidFillingParameterDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& expected
)
{
    addFillingDiagnostic(
        object,
        context,
        "invalid_parameter",
        "Part.makeFilledFace " + property + " must be " + expected,
        property,
        object.name,
        property
    );
    return false;
}

bool readPositiveIntParam(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    unsigned int& output
)
{
    if (app::propertyValue(object, property) == nullptr) {
        return true;
    }
    const auto value = app::readNumber(object, property);
    if (!value || !std::isfinite(*value) || *value <= 0.0
        || std::abs(*value - std::round(*value)) > 1e-9) {
        return addInvalidFillingParameterDiagnostic(object, context, property, "a positive integer");
    }
    output = static_cast<unsigned int>(std::llround(*value));
    return true;
}

bool readPositiveNumberParam(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    double& output
)
{
    if (app::propertyValue(object, property) == nullptr) {
        return true;
    }
    const auto value = app::readNumber(object, property);
    if (!value || !std::isfinite(*value) || *value <= 0.0) {
        return addInvalidFillingParameterDiagnostic(object, context, property, "a positive number");
    }
    output = *value;
    return true;
}

bool readBoolParam(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    bool& output
)
{
    if (app::propertyValue(object, property) == nullptr) {
        return true;
    }
    const auto value = app::readBool(object, property);
    if (!value) {
        return addInvalidFillingParameterDiagnostic(object, context, property, "a boolean");
    }
    output = *value;
    return true;
}

std::optional<FilledFaceParams> readFillingParams(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeFilledFace() forwards these fields as one BRepFillingParams batch; cad-core keeps
    // the same batch boundary before calling makeElementFilledFaceFromSources().
    FilledFaceParams params;
    if (!readPositiveIntParam(object, context, "Degree", params.degree)
        || !readPositiveIntParam(object, context, "PtsOnCurve", params.pointsOnCurve)
        || !readPositiveIntParam(object, context, "NumIter", params.iterations)
        || !readBoolParam(object, context, "Anisotropy", params.anisotropy)
        || !readPositiveNumberParam(object, context, "Tol2d", params.tolerance2d)
        || !readPositiveNumberParam(object, context, "Tol3d", params.tolerance3d)
        || !readPositiveNumberParam(object, context, "TolG1", params.toleranceG1)
        || !readPositiveNumberParam(object, context, "TolG2", params.toleranceG2)
        || !readPositiveIntParam(object, context, "MaxDegree", params.maxDegree)
        || !readPositiveIntParam(object, context, "MaxSegments", params.maxSegments)) {
        return std::nullopt;
    }
    return params;
}

nlohmann::json fillingParamsJson(const FilledFaceParams& params)
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

nlohmann::json boundaryEvidenceJson(const std::optional<FilledFaceBoundaryEvidence>& evidence)
{
    if (!evidence) {
        return nullptr;
    }
    return {
        {"object", evidence->objectName},
        {"subname", evidence->subname},
        {"stable_subname", evidence->stableSubname},
        {"shape_kind", evidence->shapeKind},
    };
}

nlohmann::json supportOrderEvidenceJson(
    const std::vector<FilledFaceSupportOrderEvidence>& evidence
)
{
    nlohmann::json result = nlohmann::json::array();
    for (const FilledFaceSupportOrderEvidence& item : evidence) {
        nlohmann::json payload = {
            {"target_object", item.targetObject},
            {"target_subname", item.targetSubname},
            {"target_stable_subname", item.targetStableSubname},
            {"target_shape_kind", item.targetShapeKind},
            {"is_boundary", item.isBoundary},
            {"builder_call", item.builderCall},
            {"has_support", item.hasSupport},
            {"has_order", item.hasOrder},
        };
        if (item.hasSupport) {
            payload["support_object"] = item.supportObject;
            payload["support_subname"] = item.supportSubname;
            payload["support_stable_subname"] = item.supportStableSubname;
        }
        if (item.hasOrder) {
            payload["order"] = item.order;
        }
        result.push_back(std::move(payload));
    }
    return result;
}

nlohmann::json constraintEvidenceJson(const std::vector<FilledFaceConstraintEvidence>& evidence)
{
    nlohmann::json result = nlohmann::json::array();
    for (const FilledFaceConstraintEvidence& item : evidence) {
        result.push_back({
            {"object", item.objectName},
            {"subname", item.subname},
            {"stable_subname", item.stableSubname},
            {"shape_kind", item.shapeKind},
            {"builder_call", item.builderCall},
            {"is_boundary", item.isBoundary},
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

std::string shapeKindLabel(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "face";
        case TopAbs_EDGE:
            return "edge";
        case TopAbs_WIRE:
            return "wire";
        case TopAbs_VERTEX:
            return "vertex";
        default:
            return "shape";
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

std::optional<FilledFaceSource> resolveFillingLinkedSource(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const app::Link& link,
    std::size_t index,
    TopAbs_ShapeEnum expectedKind,
    const std::string& diagnosticCode,
    const std::string& role
)
{
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addFillingDiagnostic(
            object,
            context,
            "missing_link_target",
            property + " target " + link.object + " did not produce a shape",
            property,
            link.object,
            stableSubnameForLink(link, index)
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link.object);
    const NamedShape* namedShape = namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second
                                                                             : nullptr;
    const auto selected = resolveFillingSubshape(shapeIt->second.shape, namedShape, link, index);
    if (!selected || selected->IsNull()) {
        addFillingDiagnostic(
            object,
            context,
            diagnosticCode,
            "Part.makeFilledFace " + role + " " + stableSubnameForLink(link, index)
                + " did not resolve",
            property,
            link.object,
            stableSubnameForLink(link, index)
        );
        return std::nullopt;
    }
    if (selected->ShapeType() != expectedKind) {
        addFillingDiagnostic(
            object,
            context,
            diagnosticCode,
            "Part.makeFilledFace " + role + " must resolve to a "
                + shapeKindLabel(expectedKind),
            property,
            link.object,
            stableSubnameForLink(link, index)
        );
        return std::nullopt;
    }

    const std::string stable = stableSubnameForLink(link, index);
    const std::string current = index < link.subnames.size() ? link.subnames[index] : stable;
    return FilledFaceSource {
        link.object,
        *selected,
        namedShape,
        current,
        stable,
    };
}

std::optional<app::Link> readNestedFillingLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const nlohmann::json& item,
    const std::string& property,
    const std::string& field,
    const app::Link& target,
    std::size_t index
)
{
    const auto it = item.find(field);
    if (it == item.end()) {
        addFillingDiagnostic(
            object,
            context,
            "invalid_support_source",
            "Part.makeFilledFace " + property + " item requires " + field + " face link",
            property,
            target.object,
            stableSubnameForLink(target, index)
        );
        return std::nullopt;
    }
    auto link = app::readLink(*it);
    if (!link) {
        addFillingDiagnostic(
            object,
            context,
            "invalid_support_source",
            "Part.makeFilledFace " + property + "." + field
                + " must be an App::PropertyLinkSub payload",
            property,
            target.object,
            stableSubnameForLink(target, index)
        );
        return std::nullopt;
    }
    return link;
}

std::string continuityName(GeomAbs_Shape order)
{
    switch (order) {
        case GeomAbs_C0:
            return "C0";
        case GeomAbs_G1:
            return "G1";
        case GeomAbs_C1:
            return "C1";
        case GeomAbs_G2:
            return "G2";
        case GeomAbs_C2:
            return "C2";
        case GeomAbs_C3:
            return "C3";
        case GeomAbs_CN:
            return "CN";
        default:
            return "C0";
    }
}

std::optional<GeomAbs_Shape> continuityFromString(const std::string& value)
{
    if (value == "C0") {
        return GeomAbs_C0;
    }
    if (value == "G1") {
        return GeomAbs_G1;
    }
    if (value == "C1") {
        return GeomAbs_C1;
    }
    if (value == "G2") {
        return GeomAbs_G2;
    }
    if (value == "C2") {
        return GeomAbs_C2;
    }
    if (value == "C3") {
        return GeomAbs_C3;
    }
    if (value == "CN") {
        return GeomAbs_CN;
    }
    return std::nullopt;
}

std::optional<GeomAbs_Shape> continuityFromInteger(long value)
{
    switch (value) {
        case 0:
            return GeomAbs_C0;
        case 1:
            return GeomAbs_G1;
        case 2:
            return GeomAbs_C1;
        case 3:
            return GeomAbs_G2;
        case 4:
            return GeomAbs_C2;
        case 5:
            return GeomAbs_C3;
        case 6:
            return GeomAbs_CN;
        default:
            return std::nullopt;
    }
}

std::optional<FilledFaceOrderSource> readFillingOrderSource(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const nlohmann::json& raw,
    const app::Link& link,
    std::size_t index
)
{
    const auto orderIt = raw.find("Order") != raw.end() ? raw.find("Order") : raw.find("Continuity");
    if (orderIt == raw.end()) {
        addFillingDiagnostic(
            object,
            context,
            "invalid_order_source",
            "Part.makeFilledFace Orders item requires Order or Continuity",
            "Orders",
            link.object,
            stableSubnameForLink(link, index)
        );
        return std::nullopt;
    }

    std::optional<GeomAbs_Shape> order;
    if (orderIt->is_string()) {
        order = continuityFromString(orderIt->get<std::string>());
    }
    else if (orderIt->is_number_integer()) {
        order = continuityFromInteger(orderIt->get<long>());
    }
    else if (orderIt->is_number_float()) {
        const double rawNumber = orderIt->get<double>();
        if (std::isfinite(rawNumber) && std::abs(rawNumber - std::round(rawNumber)) <= 1e-9) {
            order = continuityFromInteger(static_cast<long>(std::llround(rawNumber)));
        }
    }
    if (!order) {
        addFillingDiagnostic(
            object,
            context,
            "invalid_order_source",
            "Part.makeFilledFace Orders order must be C0/G1/C1/G2/C2/C3/CN or matching enum index",
            "Orders",
            link.object,
            stableSubnameForLink(link, index)
        );
        return std::nullopt;
    }

    const auto target = resolveFillingLinkedSource(
        object,
        context,
        "Orders",
        link,
        index,
        TopAbs_EDGE,
        "invalid_order_target",
        "order target"
    );
    if (!target) {
        return std::nullopt;
    }
    return FilledFaceOrderSource {*target, *order, continuityName(*order)};
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
                link.object,
                stableSubnameForLink(link, 0U)
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
            const std::string stable = index < link.stableSubnames.size()
                    && !link.stableSubnames[index].empty()
                ? link.stableSubnames[index]
                : link.subnames[index];
            const auto selected = resolveFillingSubshape(shapeIt->second.shape, namedShape, link, index);
            if (!selected || selected->IsNull()) {
                addFillingDiagnostic(
                    object,
                    context,
                    "invalid_subshape",
                    "Boundary subshape " + link.subnames[index] + " did not resolve",
                    "Boundary",
                    link.object,
                    stable
                );
                return std::nullopt;
            }
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

bool resolveFillingSurfaceSupportOrderSources(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    ResolvedFillingSources& resolved
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeFilledFace(), parses "surface", "supports" and "orders" into
    // TopoShape::BRepFillingParams; /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace() then consumes them through
    // "LoadInitSurface", "getSupport()", "getOrder()" and
    // "maker.Add(edge, support, order, IsBound=true)" for boundary edges.
    if (app::propertyValue(object, "Surface") != nullptr) {
        const auto link = app::readLink(object, "Surface");
        if (!link) {
            addFillingDiagnostic(
                object,
                context,
                "invalid_surface_source",
                "Part.makeFilledFace Surface must reference one face",
                "Surface",
                firstDeferredTarget(object, "Surface"),
                firstDeferredSubname(object, "Surface")
            );
            return false;
        }
        if (link->subnames.size() > 1U) {
            addFillingDiagnostic(
                object,
                context,
                "invalid_surface_source",
                "Part.makeFilledFace Surface must reference exactly one face",
                "Surface",
                link->object,
                stableSubnameForLink(*link, 0U)
            );
            return false;
        }
        const auto surface = resolveFillingLinkedSource(
            object,
            context,
            "Surface",
            *link,
            0U,
            TopAbs_FACE,
            "invalid_surface_source",
            "initial surface"
        );
        if (!surface) {
            return false;
        }
        resolved.initialSurface = *surface;
        addHistorySource(resolved.historySources, surface->objectName, surface->shape, surface->namedShape);
    }

    const std::vector<app::Link> supportTargets = app::readLinks(object, "Supports");
    const std::vector<nlohmann::json> supportItems = rawLinkSubListItems(object, "Supports");
    for (std::size_t linkIndex = 0; linkIndex < supportTargets.size(); ++linkIndex) {
        const app::Link& targetLink = supportTargets[linkIndex];
        const nlohmann::json raw = linkIndex < supportItems.size() && supportItems[linkIndex].is_object()
            ? supportItems[linkIndex]
            : nlohmann::json::object();
        const std::size_t count = targetLink.subnames.empty() ? 1U : targetLink.subnames.size();
        for (std::size_t subIndex = 0; subIndex < count; ++subIndex) {
            const auto target = resolveFillingLinkedSource(
                object,
                context,
                "Supports",
                targetLink,
                subIndex,
                TopAbs_EDGE,
                "invalid_support_target",
                "support target"
            );
            if (!target) {
                return false;
            }
            const auto supportLink = readNestedFillingLink(
                object,
                context,
                raw,
                "Supports",
                "Support",
                targetLink,
                subIndex
            );
            if (!supportLink) {
                return false;
            }
            if (supportLink->subnames.size() > 1U) {
                addFillingDiagnostic(
                    object,
                    context,
                    "invalid_support_source",
                    "Part.makeFilledFace Supports.Support must reference exactly one face",
                    "Supports",
                    supportLink->object,
                    stableSubnameForLink(*supportLink, 0U)
                );
                return false;
            }
            const auto supportFace = resolveFillingLinkedSource(
                object,
                context,
                "Supports",
                *supportLink,
                0U,
                TopAbs_FACE,
                "invalid_support_source",
                "support face"
            );
            if (!supportFace) {
                return false;
            }
            resolved.supports.push_back(FilledFaceSupportSource {*target, *supportFace});
            addHistorySource(resolved.historySources, target->objectName, target->shape, target->namedShape);
            addHistorySource(
                resolved.historySources,
                supportFace->objectName,
                supportFace->shape,
                supportFace->namedShape
            );
        }
    }

    const std::vector<app::Link> orderTargets = app::readLinks(object, "Orders");
    const std::vector<nlohmann::json> orderItems = rawLinkSubListItems(object, "Orders");
    for (std::size_t linkIndex = 0; linkIndex < orderTargets.size(); ++linkIndex) {
        const app::Link& targetLink = orderTargets[linkIndex];
        const nlohmann::json raw = linkIndex < orderItems.size() && orderItems[linkIndex].is_object()
            ? orderItems[linkIndex]
            : nlohmann::json::object();
        const std::size_t count = targetLink.subnames.empty() ? 1U : targetLink.subnames.size();
        for (std::size_t subIndex = 0; subIndex < count; ++subIndex) {
            const auto order = readFillingOrderSource(object, context, raw, targetLink, subIndex);
            if (!order) {
                return false;
            }
            resolved.orders.push_back(*order);
            addHistorySource(
                resolved.historySources,
                order->target.objectName,
                order->target.shape,
                order->target.namedShape
            );
        }
    }

    return true;
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
             "MaxSegments",
             "Surface",
             "Supports",
             "Orders"}
        )) {
        context.objects[object.name] = {
            {"status", "error"},
            {"feature", "part_filled_face"},
            {"helper", "Part.makeFilledFace"},
        };
        return;
    }

    const auto params = readFillingParams(object, context);
    if (!params) {
        return;
    }
    const auto sources = resolveFillingBoundarySources(object, context);
    if (!sources) {
        return;
    }
    ResolvedFillingSources resolved = *sources;
    if (!resolveFillingSurfaceSupportOrderSources(object, context, resolved)) {
        return;
    }

    const FilledFaceBuild build = makeElementFilledFaceFromSources(
        object.name,
        resolved.boundarySources,
        resolved.historySources,
        *params,
        resolved.initialSurface,
        resolved.supports,
        resolved.orders
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addFillingDiagnostic(
            object,
            context,
            build.diagnosticCode.empty() ? "execution_failed" : build.diagnosticCode,
            build.error.empty() ? "Part.makeFilledFace failed" : build.error,
            build.diagnosticProperty.empty() ? "Boundary" : build.diagnosticProperty,
            build.diagnosticTarget,
            build.diagnosticSubname
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
            {"initial_surface_source_evidence", boundaryEvidenceJson(build.initialSurfaceSource)},
            {"support_order_source_evidence", supportOrderEvidenceJson(build.supportOrderSources)},
            {"non_boundary_constraint_count", build.nonBoundaryConstraintCount},
            {"non_boundary_constraint_source_evidence", constraintEvidenceJson(build.nonBoundarySources)},
            {"support_face_count", build.supportFaceCount},
            {"order_count", build.orderCount},
            {"surface_support_order_status", "source_backed_native_helper_oracle_known_gap"},
            {"non_boundary_constraints_status", "source_backed_native_helper_oracle_known_gap"},
            {"default_params", fillingParamsJson(FilledFaceParams {})},
            {"params", fillingParamsJson(*params)},
            {"params_source", "Part.makeFilledFace constructor kwargs"},
            {"topo_naming_history", "maker_history:filling"},
        },
        build.namedShape
    );
}

}  // namespace cad_core::part
