#pragma once

#include "cad_core/app/document.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepLProp_SLProps.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRep_Tool.hxx>
#include <CSLib.hxx>
#include <CSLib_NormalStatus.hxx>
#include <GeomAdaptor.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_IntSS.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Curve.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Plane.hxx>
#include <Precision.hxx>
#include <Standard_DomainError.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopLoc_Location.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Hypr.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Parab.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design::detail {

enum class DatumAttachmentEngine
{
    Plane,
    Line,
    Point,
    CoordinateSystem,
};

struct DatumAttachmentPlacement
{
    bool attached = false;
    gp_Trsf placement;
    std::string mapMode;
    std::string aliasSourceMode;
};

struct SupportResolution
{
    app::Link link;
    std::string subname;
    TopoDS_Shape shape;
    gp_Trsf placement;
    bool recovered = false;
    std::string shadowOldName;
};

inline const nlohmann::json& propertyPayload(const app::PropertyValue& property)
{
    if (property.raw.is_object() && property.raw.contains("value")) {
        return property.raw.at("value");
    }
    return property.raw;
}

inline bool isDefaultDatumMapMode(const app::DocumentObject& object)
{
    const auto* property = app::propertyValue(object, "MapMode");
    if (property == nullptr) {
        return true;
    }

    const nlohmann::json& payload = propertyPayload(*property);
    if (payload.is_string()) {
        const std::string value = payload.get<std::string>();
        return value.empty() || value == "Deactivated";
    }
    if (payload.is_number_integer()) {
        return payload.get<int>() == 0;
    }
    return true;
}

inline std::string datumMapModeLabel(const app::DocumentObject& object)
{
    const auto* property = app::propertyValue(object, "MapMode");
    if (property == nullptr) {
        return "Deactivated";
    }

    const nlohmann::json& payload = propertyPayload(*property);
    if (payload.is_string()) {
        return payload.get<std::string>();
    }
    if (payload.is_number_integer()) {
        static const std::vector<std::string> names = {
            "Deactivated",
            "Translate",
            "ObjectXY",
            "ObjectXZ",
            "ObjectYZ",
            "FlatFace",
            "TangentPlane",
            "NormalToEdge",
            "FrenetNB",
            "FrenetTN",
            "FrenetTB",
            "Concentric",
            "SectionOfRevolution",
            "ThreePointsPlane",
            "ThreePointsNormal",
            "Folding",
            "ObjectX",
            "ObjectY",
            "ObjectZ",
            "AxisOfCurvature",
            "Directrix1",
            "Directrix2",
            "Asymptote1",
            "Asymptote2",
            "Tangent",
            "Normal",
            "Binormal",
            "TangentU",
            "TangentV",
            "TwoPointLine",
            "IntersectionLine",
            "ProximityLine",
            "ObjectOrigin",
            "Focus1",
            "Focus2",
            "OnEdge",
            "CenterOfCurvature",
            "CenterOfMass",
            "IntersectionPoint",
            "Vertex",
            "ProximityPoint1",
            "ProximityPoint2",
        };
        const int index = payload.get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < names.size()) {
            return names.at(static_cast<std::size_t>(index));
        }
        return std::to_string(index);
    }
    return {};
}

inline bool hasNonDefaultPlacement(const app::DocumentObject& object, const std::string& propertyName)
{
    const auto placement = app::readPlacement(object, propertyName);
    if (!placement) {
        return app::propertyValue(object, propertyName) != nullptr;
    }

    constexpr double tolerance = 1.0e-12;
    const auto near = [](double lhs, double rhs) {
        return std::abs(lhs - rhs) <= tolerance;
    };
    return !(near(placement->base[0], 0.0) && near(placement->base[1], 0.0)
             && near(placement->base[2], 0.0) && near(placement->rotation[0], 0.0)
             && near(placement->rotation[1], 0.0) && near(placement->rotation[2], 0.0)
             && near(placement->rotation[3], 1.0));
}

inline bool hasActiveNumberProperty(const app::DocumentObject& object, const std::string& propertyName)
{
    const auto value = app::readNumber(object, propertyName);
    if (!value) {
        return false;
    }
    return std::abs(*value) > 1.0e-12;
}

inline std::string firstSupportSubname(const app::Link& support)
{
    if (!support.subnames.empty()) {
        return support.subnames.front();
    }
    if (!support.stableSubnames.empty()) {
        return support.stableSubnames.front();
    }
    if (!support.shadowSubs.empty()) {
        if (!support.shadowSubs.front().oldName.empty()) {
            return support.shadowSubs.front().oldName;
        }
        return support.shadowSubs.front().newName;
    }
    return {};
}

inline bool hasReferenceStabilityEvidence(const app::Link& support)
{
    return support.stableSubnamesExplicit || !support.shadowSubs.empty() || !support.referenceShadows.empty()
        || support.fullSubnamesExplicit;
}

inline void addDatumAttachmentDiagnostic(runtime::ComputeContext& context,
                                         const app::DocumentObject& object,
                                         const app::Link& support,
                                         const std::string& property,
                                         const std::string& message,
                                         const std::string& code = "unsupported_property")
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           code,
                           message,
                           object.name,
                           property,
                           "runtime",
                           support.object,
                           firstSupportSubname(support));
}

inline gp_Trsf placementForObject(const std::string& objectName, const runtime::ComputeContext& context)
{
    const auto placementIt = context.globalPlacements.find(objectName);
    if (placementIt != context.globalPlacements.end()) {
        return placementIt->second;
    }
    return gp_Trsf();
}

inline gp_Trsf attachmentOffsetPlacement(const app::DocumentObject& object)
{
    const auto offset = app::readPlacement(object, "AttachmentOffset");
    if (!offset) {
        return gp_Trsf();
    }
    return base::placementFromComponents(offset->base, offset->rotation);
}

inline std::optional<double> datumPathParameter(const app::DocumentObject& object)
{
    if (const auto value = app::readNumber(object, "MapPathParameter")) {
        return *value;
    }
    if (const auto value = app::readNumber(object, "Parameter")) {
        return *value;
    }
    return std::nullopt;
}

inline gp_Pnt transformOrigin(const gp_Trsf& placement)
{
    gp_Pnt origin(0.0, 0.0, 0.0);
    origin.Transform(placement);
    return origin;
}

inline gp_Dir transformDirection(const gp_Trsf& placement, const gp_Dir& direction)
{
    gp_Dir transformed = direction;
    transformed.Transform(placement);
    return transformed;
}

inline gp_Vec transformVector(const gp_Trsf& placement, const gp_Vec& vector)
{
    gp_Vec transformed = vector;
    transformed.Transform(placement);
    return transformed;
}

inline gp_Trsf placementFromAxes(const gp_Pnt& origin,
                                 gp_Dir zAxis,
                                 gp_Vec xAxis,
                                 bool mapReverse,
                                 bool makeYVertical = false)
{
    if (mapReverse) {
        zAxis.Reverse();
        xAxis.Reverse();
    }
    if (xAxis.Magnitude() < Precision::Confusion() || makeYVertical) {
        gp_Vec yAxis(0.0, 0.0, 1.0);
        xAxis = yAxis.Crossed(gp_Vec(zAxis));
        if (xAxis.Magnitude() < Precision::Confusion()) {
            xAxis = (gp_Vec(1.0, 0.0, 0.0) * zAxis.Z()).Normalized();
        }
    }

    gp_Ax3 axes(origin, zAxis, xAxis);
    gp_Trsf placement;
    placement.SetTransformation(axes);
    placement.Invert();
    placement.SetScaleFactor(1.0);
    return placement;
}

inline gp_Trsf placementFromObjectMode(const std::string& mode,
                                       const gp_Trsf& supportPlacement,
                                       bool mapReverse)
{
    const gp_Pnt origin = transformOrigin(supportPlacement);
    const gp_Dir xDir = transformDirection(supportPlacement, gp_Dir(1.0, 0.0, 0.0));
    const gp_Dir yDir = transformDirection(supportPlacement, gp_Dir(0.0, 1.0, 0.0));
    const gp_Dir zDir = transformDirection(supportPlacement, gp_Dir(0.0, 0.0, 1.0));

    if (mode == "ObjectXY" || mode == "ObjectZ" || mode == "ObjectOrigin") {
        return placementFromAxes(origin, zDir, gp_Vec(xDir), mapReverse);
    }
    if (mode == "ObjectXZ" || mode == "ObjectY") {
        return placementFromAxes(origin, yDir.Reversed(), gp_Vec(xDir), mapReverse);
    }
    if (mode == "ObjectYZ" || mode == "ObjectX") {
        return placementFromAxes(origin, xDir, gp_Vec(yDir), mapReverse);
    }
    return gp_Trsf();
}

inline gp_Trsf pointDatumPlacement(const gp_Pnt& point)
{
    gp_Trsf placement;
    placement.SetTranslation(gp_Vec(gp_Pnt(0.0, 0.0, 0.0), point));
    return placement;
}

inline bool simpleSelectedSubshapeWholeShape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    return shape.ShapeType() == TopAbs_VERTEX || shape.ShapeType() == TopAbs_EDGE
        || shape.ShapeType() == TopAbs_FACE;
}

inline std::optional<std::string> currentSubnameFromStable(const app::Link& link,
                                                           std::size_t index,
                                                           const part::NamedShape* namedShape)
{
    if (namedShape == nullptr || index >= link.stableSubnames.size()) {
        return std::nullopt;
    }
    const std::string& stable = link.stableSubnames.at(index);
    if (stable.empty()) {
        return std::nullopt;
    }
    const auto resolved = part::resolveElementName(*namedShape,
                                                   index < link.subnames.size() ? link.subnames.at(index)
                                                                                : std::string{},
                                                   stable);
    if (!resolved || resolved->empty()) {
        return std::nullopt;
    }
    return *resolved;
}

inline std::optional<SupportResolution> resolveAttachmentSupport(const app::DocumentObject& object,
                                                                 runtime::ComputeContext& context,
                                                                 const app::Link& support,
                                                                 bool requireSubshape)
{
    const auto shapeIt = context.shapes.find(support.object);
    if (shapeIt == context.shapes.end()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support,
                                     "AttachmentSupport",
                                     "Datum AttachmentSupport target " + support.object + " has no computed shape",
                                     "missing_link_target");
        return std::nullopt;
    }

    SupportResolution resolution;
    resolution.link = support;
    resolution.placement = placementForObject(support.object, context);

    const part::NamedShape* namedShape = nullptr;
    const auto namedShapeIt = context.namedShapes.find(support.object);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = &namedShapeIt->second;
    }

    std::vector<std::string> candidates;
    if (!support.subnames.empty() && !support.subnames.front().empty()) {
        candidates.push_back(support.subnames.front());
    }
    if (const auto stableResolved = currentSubnameFromStable(support, 0U, namedShape)) {
        candidates.push_back(*stableResolved);
    }
    if (!support.stableSubnames.empty() && !support.stableSubnames.front().empty()) {
        candidates.push_back(support.stableSubnames.front());
    }
    if (!support.shadowSubs.empty()) {
        if (!support.shadowSubs.front().newName.empty()) {
            candidates.push_back(support.shadowSubs.front().newName);
        }
        if (!support.shadowSubs.front().oldName.empty()) {
            candidates.push_back(support.shadowSubs.front().oldName);
        }
    }
    for (const auto& shadow : support.referenceShadows) {
        if (!shadow.subname.empty()) {
            candidates.push_back(shadow.subname);
        }
        if (!shadow.stableSubname.empty()) {
            candidates.push_back(shadow.stableSubname);
        }
    }

    candidates.erase(std::remove_if(candidates.begin(),
                                    candidates.end(),
                                    [](const std::string& item) { return item.empty(); }),
                     candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    if (!requireSubshape && candidates.empty()) {
        resolution.shape = shapeIt->second.shape;
        return resolution;
    }

    if (requireSubshape && candidates.empty() && simpleSelectedSubshapeWholeShape(shapeIt->second.shape)) {
        resolution.shape = shapeIt->second.shape;
        return resolution;
    }

    for (const auto& candidate : candidates) {
        std::optional<TopoDS_Shape> subshape;
        if (namedShape != nullptr) {
            subshape = part::subshapeByName(*namedShape, candidate);
        }
        if (!subshape) {
            subshape = part::subshapeByName(shapeIt->second.shape, candidate);
        }
        if (!subshape || subshape->IsNull()) {
            continue;
        }
        resolution.subname = candidate;
        resolution.shape = *subshape;
        const bool differsFromInput = support.subnames.empty() || support.subnames.front() != candidate;
        resolution.recovered = differsFromInput;
        if (!support.shadowSubs.empty() && !support.shadowSubs.front().oldName.empty()) {
            resolution.shadowOldName = support.shadowSubs.front().oldName;
        }
        else if (!support.subnames.empty()) {
            resolution.shadowOldName = support.subnames.front();
        }
        return resolution;
    }

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support,
                                 "AttachmentSupport",
                                 "Datum AttachmentSupport subname "
                                     + (candidates.empty() ? std::string{"<empty>"} : candidates.front())
                                     + " could not be resolved from request-local support shape evidence",
                                 "subname_resolve_failed");
    return std::nullopt;
}

inline void appendAttachmentSupportWriteback(const app::DocumentObject& object,
                                             const SupportResolution& support,
                                             runtime::ComputeContext& context)
{
    if (support.subname.empty() || support.link.subnames.empty()
        || support.link.subnames.front() == support.subname) {
        return;
    }

    nlohmann::json property = {
        {"PropertyType", "App::PropertyLinkSub"},
        {"value", support.link.object},
        {"SubList", {support.subname}},
        {"StableSubList", {support.subname}},
    };
    if (!support.shadowOldName.empty() && support.shadowOldName != support.subname) {
        property["ShadowSub"] = {{{"oldName", support.shadowOldName}, {"newName", support.subname}}};
    }
    if (!support.link.referenceShadows.empty()) {
        property["ReferenceShadow"] = nlohmann::json::array();
        for (const auto& shadow : support.link.referenceShadows) {
            nlohmann::json item = {
                {"target", support.link.object},
                {"targetId", shadow.targetId},
                {"property", shadow.property},
                {"shapeType", shadow.shapeType},
                {"indexed", support.subname},
                {"subname", support.subname},
                {"stableSubname", support.subname},
                {"fingerprint", shadow.fingerprint},
            };
            if (shadow.brep) {
                item["brep"] = {
                    {"format", shadow.brep->format},
                    {"byteLength", shadow.brep->byteLength},
                    {"sha256", shadow.brep->sha256},
                    {"data", shadow.brep->data},
                };
            }
            property["ReferenceShadow"].push_back(std::move(item));
        }
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::positionBySupport(), after "calculateAttachedPlacement(..., &subChanged)",
    // calls "AttachmentSupport.setValues(AttachmentSupport.getValues(), _props.attacher->getSubValues())".
    // cad-core is stateless, so it returns a request-local documentObjectUpdates suggestion instead
    // of mutating the frontend graph.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "attachment_support_subname_recovered"},
        {"object", object.name},
        {"properties", {{"AttachmentSupport", std::move(property)}}},
    });
}

inline bool needsAttachmentSupportWriteback(const SupportResolution& support)
{
    return !support.subname.empty() && !support.link.subnames.empty()
        && support.link.subnames.front() != support.subname && hasReferenceStabilityEvidence(support.link);
}

inline nlohmann::json referenceShadowsJson(const app::Link& link, const std::string& subname)
{
    nlohmann::json referenceShadows = nlohmann::json::array();
    for (const auto& shadow : link.referenceShadows) {
        nlohmann::json item = {
            {"target", link.object},
            {"targetId", shadow.targetId},
            {"property", shadow.property},
            {"shapeType", shadow.shapeType},
            {"indexed", subname.empty() ? shadow.indexed : subname},
            {"subname", subname.empty() ? shadow.subname : subname},
            {"stableSubname", subname.empty() ? shadow.stableSubname : subname},
            {"fingerprint", shadow.fingerprint},
        };
        if (shadow.brep) {
            item["brep"] = {
                {"format", shadow.brep->format},
                {"byteLength", shadow.brep->byteLength},
                {"sha256", shadow.brep->sha256},
                {"data", shadow.brep->data},
            };
        }
        referenceShadows.push_back(std::move(item));
    }
    return referenceShadows;
}

inline nlohmann::json attachmentSupportListItemJson(const SupportResolution& support, bool recovered)
{
    nlohmann::json item = {
        {"value", support.link.object},
        {"SubList", recovered ? std::vector<std::string>{support.subname} : support.link.subnames},
    };
    if (recovered) {
        item["StableSubList"] = {support.subname};
        if (!support.shadowOldName.empty() && support.shadowOldName != support.subname) {
            item["ShadowSub"] = {{{"oldName", support.shadowOldName}, {"newName", support.subname}}};
        }
    }
    else {
        if (support.link.stableSubnamesExplicit || !support.link.stableSubnames.empty()) {
            item["StableSubList"] = support.link.stableSubnames;
        }
        if (!support.link.fullSubnames.empty() || support.link.fullSubnamesExplicit) {
            item["FullSubList"] = support.link.fullSubnames;
        }
        if (!support.link.shadowSubs.empty()) {
            item["ShadowSub"] = nlohmann::json::array();
            for (const auto& shadowSub : support.link.shadowSubs) {
                item["ShadowSub"].push_back({{"oldName", shadowSub.oldName}, {"newName", shadowSub.newName}});
            }
        }
    }
    if (!support.link.referenceShadows.empty()) {
        item["ReferenceShadow"] = referenceShadowsJson(support.link, recovered ? support.subname : std::string{});
    }
    return item;
}

inline void appendAttachmentSupportsWriteback(const app::DocumentObject& object,
                                              const std::vector<SupportResolution>& supports,
                                              runtime::ComputeContext& context)
{
    bool hasRecoveredSupport = false;
    nlohmann::json subSet = nlohmann::json::array();
    for (const auto& support : supports) {
        const bool recovered = needsAttachmentSupportWriteback(support);
        hasRecoveredSupport = hasRecoveredSupport || recovered;
        subSet.push_back(attachmentSupportListItemJson(support, recovered));
    }
    if (!hasRecoveredSupport) {
        return;
    }

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::positionBySupport(), after "calculateAttachedPlacement(..., &subChanged)",
    // writes AttachmentSupport sub values back into the document. cad-core has no backend
    // attachment session, so multi-support modes publish one request-local PropertyLinkSubList
    // documentObjectUpdates suggestion instead.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "attachment_support_subname_recovered"},
        {"object", object.name},
        {"properties",
         {{"AttachmentSupport",
           {
               {"PropertyType", "App::PropertyLinkSubList"},
               {"SubSet", std::move(subSet)},
           }}}},
    });
}

inline std::optional<gp_Trsf> flatFacePlacement(const SupportResolution& support,
                                                bool mapReverse,
                                                runtime::ComputeContext& context,
                                                const app::DocumentObject& object)
{
    TopoDS_Face face;
    gp_Pln plane;
    bool reversed = false;
    try {
        face = TopoDS::Face(support.shape);
    }
    catch (const Standard_Failure&) {
    }
    if (face.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "FlatFace requires a planar Face support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    BRepAdaptor_Surface adapt(face);
    if (adapt.GetType() == GeomAbs_Plane) {
        plane = adapt.Plane();
    }
    else {
        TopLoc_Location location;
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face, location);
        GeomLib_IsPlanarSurface check(surface, Precision::Confusion());
        if (!check.IsPlanar()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "FlatFace requires a planar Face support",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        plane = check.Plan();
    }
    if (face.Orientation() == TopAbs_REVERSED) {
        reversed = true;
    }
    if (!plane.Direct()) {
        plane.UReverse();
        reversed = !reversed;
    }

    gp_Ax1 normal = plane.Axis();
    if (reversed) {
        normal.Reverse();
    }
    Handle(Geom_Plane) planeSurface = new Geom_Plane(plane);
    GeomAPI_ProjectPointOnSurf projector(transformOrigin(support.placement), planeSurface);
    if (projector.NbPoints() == 0) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "FlatFace could not project support origin to planar face",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }
    return placementFromAxes(projector.NearestPoint(),
                             normal.Direction(),
                             transformVector(support.placement, gp_Vec(1.0, 0.0, 0.0)),
                             mapReverse,
                             true);
}

inline std::optional<gp_Trsf> normalToEdgePlacement(const SupportResolution& support,
                                                    bool mapReverse,
                                                    double parameter,
                                                    runtime::ComputeContext& context,
                                                    const app::DocumentObject& object)
{
    TopoDS_Edge edge;
    try {
        edge = TopoDS::Edge(support.shape);
    }
    catch (const Standard_Failure&) {
    }
    if (edge.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "NormalToEdge requires an Edge support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    BRepAdaptor_Curve curve(edge);
    double first = curve.FirstParameter();
    double last = curve.LastParameter();
    if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) {
        first = 0.0;
        last = 1.0;
    }
    const double u = first + parameter * (last - first);
    gp_Pnt point;
    gp_Vec tangent;
    curve.D1(u, point, tangent);
    if (tangent.Magnitude() < Precision::Confusion()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapPathParameter",
                                     "NormalToEdge path derivative is too small at MapPathParameter",
                                     "attachment_parameter_invalid");
        return std::nullopt;
    }
    return placementFromAxes(point, gp_Dir(tangent), gp_Vec(), mapReverse, true);
}

inline gp_Pnt projectReferenceOriginToLine(const gp_Pnt& lineBasePoint,
                                           const gp_Dir& lineDirection,
                                           const gp_Pnt& referenceOrigin)
{
    const gp_Vec originVector(lineBasePoint.XYZ());
    const gp_Vec referenceVector(referenceOrigin.XYZ());
    const gp_Vec directionVector(lineDirection);
    return gp_Pnt((originVector + directionVector * directionVector.Dot(referenceVector - originVector)).XYZ());
}

inline std::optional<gp_Trsf> linePlacementFromFreeCADFactory(const std::vector<SupportResolution>& supports,
                                                             const gp_Pnt& lineBasePoint,
                                                             const gp_Dir& lineDirection,
                                                             bool mapReverse)
{
    const gp_Pnt referenceOrigin = supports.empty() ? gp_Pnt(0.0, 0.0, 0.0)
                                                    : transformOrigin(supports.front().placement);
    const gp_Pnt projectedOrigin = projectReferenceOriginToLine(lineBasePoint, lineDirection, referenceOrigin);
    return placementFromAxes(projectedOrigin, lineDirection, gp_Vec(), mapReverse, true);
}

inline void addLineFamilyDiagnostic(runtime::ComputeContext& context,
                                    const app::DocumentObject& object,
                                    const std::vector<SupportResolution>& supports,
                                    const std::string& message,
                                    const std::string& code)
{
    static const app::Link emptyLink;
    const app::Link& link = supports.empty() ? emptyLink : supports.front().link;
    addDatumAttachmentDiagnostic(context, object, link, "MapMode", message, code);
}

inline std::optional<gp_Trsf> twoPointLinePlacement(const std::vector<SupportResolution>& supports,
                                                   bool mapReverse,
                                                   runtime::ComputeContext& context,
                                                   const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::_calculateAttachedPlacement(), case "mm1TwoPoints", collects Vertex
    // points or Edge first/last parameter points until "points.size() >= 2", then uses
    // "LineDir = gp_Dir(gp_Vec(p0, p1)); LineBasePoint = p0".
    std::vector<gp_Pnt> points;
    for (const auto& support : supports) {
        const TopoDS_Shape& shape = support.shape;
        if (shape.IsNull()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "TwoPointLine requires non-null Vertex or Edge support shapes",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        if (shape.ShapeType() == TopAbs_VERTEX) {
            points.push_back(BRep_Tool::Pnt(TopoDS::Vertex(shape)));
        }
        else if (shape.ShapeType() == TopAbs_EDGE) {
            const TopoDS_Edge edge = TopoDS::Edge(shape);
            BRepAdaptor_Curve curve(edge);
            double first = curve.FirstParameter();
            double last = curve.LastParameter();
            if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) {
                first = 0.0;
                last = 1.0;
            }
            points.push_back(curve.Value(first));
            points.push_back(curve.Value(last));
        }
        if (points.size() >= 2U) {
            break;
        }
    }

    if (points.size() < 2U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "TwoPointLine requires two vertex points or one edge support",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }
    const gp_Vec direction(points[0], points[1]);
    if (direction.Magnitude() < Precision::Confusion()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "TwoPointLine cannot derive a line from coincident points",
                                "attachment_parameter_invalid");
        return std::nullopt;
    }
    return linePlacementFromFreeCADFactory(supports, points[0], gp_Dir(direction), mapReverse);
}

inline std::optional<gp_Trsf> intersectionLinePlacement(const std::vector<SupportResolution>& supports,
                                                       bool mapReverse,
                                                       runtime::ComputeContext& context,
                                                       const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::_calculateAttachedPlacement(), case "mm1Intersection", requires two
    // Face supports, runs "GeomAPI_IntSS(hSurf1, hSurf2, Precision::Confusion())", then accepts
    // exactly one intersection curve and only when "adapt.GetType() == GeomAbs_Line".
    if (supports.size() < 2U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine requires two Face support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    TopoDS_Face face1;
    TopoDS_Face face2;
    try {
        face1 = TopoDS::Face(supports[0].shape);
        face2 = TopoDS::Face(supports[1].shape);
    }
    catch (const Standard_Failure&) {
    }
    if (face1.IsNull() || face2.IsNull()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine requires two Face support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    GeomAPI_IntSS intersector(BRep_Tool::Surface(face1), BRep_Tool::Surface(face2), Precision::Confusion());
    if (!intersector.IsDone()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine surface intersection failed",
                                "execution_failed");
        return std::nullopt;
    }
    if (intersector.NbLines() == 0) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine support faces do not intersect",
                                "no_intersection");
        return std::nullopt;
    }
    if (intersector.NbLines() != 1) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine support faces do not produce a single curve",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    GeomAdaptor_Curve curve(intersector.Line(1));
    if (curve.GetType() != GeomAbs_Line) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "IntersectionLine support faces do not produce a straight line",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }
    return linePlacementFromFreeCADFactory(supports,
                                           curve.Line().Location(),
                                           curve.Line().Direction(),
                                           mapReverse);
}

inline std::optional<gp_Trsf> proximityLinePlacement(const std::vector<SupportResolution>& supports,
                                                     bool mapReverse,
                                                     runtime::ComputeContext& context,
                                                     const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::_calculateAttachedPlacement(), case "mm1Proximity", runs
    // "BRepExtrema_DistShapeShape" on the first two shapes and derives "LineDir" from
    // "PointOnShape1(1)" to "PointOnShape2(1)" unless the distance is below Precision::Confusion().
    if (supports.size() < 2U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ProximityLine requires two support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }
    if (supports[0].shape.IsNull() || supports[1].shape.IsNull()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ProximityLine requires non-null support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    BRepExtrema_DistShapeShape distancer(supports[0].shape, supports[1].shape);
    if (!distancer.IsDone()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ProximityLine distance calculation failed",
                                "execution_failed");
        return std::nullopt;
    }

    const gp_Pnt point1 = distancer.PointOnShape1(1);
    const gp_Pnt point2 = distancer.PointOnShape2(1);
    const gp_Vec direction(point1, point2);
    if (direction.Magnitude() < Precision::Confusion()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ProximityLine cannot derive a line because support shapes touch or intersect",
                                "no_intersection");
        return std::nullopt;
    }
    return linePlacementFromFreeCADFactory(supports, point1, gp_Dir(direction), mapReverse);
}

inline bool isConicLineLandmarkMode(const std::string& mode)
{
    return mode == "Directrix1" || mode == "Directrix2" || mode == "Asymptote1"
        || mode == "Asymptote2";
}

inline bool isConicPointLandmarkMode(const std::string& mode)
{
    return mode == "Focus1" || mode == "Focus2";
}

inline std::optional<TopoDS_Edge> conicLandmarkEdge(const SupportResolution& support,
                                                    runtime::ComputeContext& context,
                                                    const app::DocumentObject& object,
                                                    const std::string& mode)
{
    TopoDS_Edge edge;
    try {
        edge = TopoDS::Edge(support.shape);
    }
    catch (const Standard_Failure&) {
    }
    if (edge.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     mode + " requires a non-null Edge support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }
    return edge;
}

inline std::optional<gp_Trsf> conicLineLandmarkPlacement(const std::string& mode,
                                                        const std::vector<SupportResolution>& supports,
                                                        bool mapReverse,
                                                        runtime::ComputeContext& context,
                                                        const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::AttachEngineLine(), lines 2413-2419, registers "mm1Asymptote1/2"
    // as rtHyperbola, "mm1Directrix1" as rtConic, and "mm1Directrix2" as rtEllipse/rtHyperbola.
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::_calculateAttachedPlacement(), lines 2613-2702, reads
    // "BRepAdaptor_Curve adapt(e)", then "hyp.Asymptote1/2().Direction()" or
    // ellipse/hyperbola/parabola "Directrix1/2()"; parabola "has no second directrix".
    const SupportResolution& support = supports.front();
    const auto edge = conicLandmarkEdge(support, context, object, mode);
    if (!edge) {
        return std::nullopt;
    }

    BRepAdaptor_Curve adapt(*edge);
    if (mode == "Asymptote1" || mode == "Asymptote2") {
        if (adapt.GetType() != GeomAbs_Hyperbola) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " requires a hyperbola-shaped Edge support",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        const gp_Hypr hyperbola = adapt.Hyperbola();
        const gp_Ax1 asymptote = mode == "Asymptote1" ? hyperbola.Asymptote1() : hyperbola.Asymptote2();
        return linePlacementFromFreeCADFactory(supports,
                                               hyperbola.Location(),
                                               asymptote.Direction(),
                                               mapReverse);
    }

    gp_Ax1 directrix;
    switch (adapt.GetType()) {
        case GeomAbs_Ellipse: {
            const gp_Elips ellipse = adapt.Ellipse();
            directrix = mode == "Directrix1" ? ellipse.Directrix1() : ellipse.Directrix2();
        } break;
        case GeomAbs_Hyperbola: {
            const gp_Hypr hyperbola = adapt.Hyperbola();
            directrix = mode == "Directrix1" ? hyperbola.Directrix1() : hyperbola.Directrix2();
        } break;
        case GeomAbs_Parabola: {
            if (mode == "Directrix2") {
                addDatumAttachmentDiagnostic(context,
                                             object,
                                             support.link,
                                             "MapMode",
                                             "Directrix2 is not available for a parabola Edge support",
                                             "attachment_support_invalid_shape");
                return std::nullopt;
            }
            directrix = adapt.Parabola().Directrix();
        } break;
        default:
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " requires an ellipse, hyperbola, or parabola Edge support",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
    }
    return linePlacementFromFreeCADFactory(supports,
                                           directrix.Location(),
                                           directrix.Direction(),
                                           mapReverse);
}

inline std::optional<gp_Pnt> proximityPointEdgeFaceIntersection(const SupportResolution& support1,
                                                               const SupportResolution& support2)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::getProximityPoint(), bug note "#0003921", normalizes EDGE/FACE
    // input order, loads "GeomAdaptor::MakeCurve(crv)" to preserve Location/orientation, then
    // calls "BRepIntCurveSurface_Inter::Init(face, typedcrv, Precision::Confusion())" and
    // returns the first hit before the distance fallback.
    try {
        TopoDS_Face face;
        TopoDS_Edge edge;
        if (support1.shape.ShapeType() == TopAbs_FACE && support2.shape.ShapeType() == TopAbs_EDGE) {
            face = TopoDS::Face(support1.shape);
            edge = TopoDS::Edge(support2.shape);
        }
        else if (support1.shape.ShapeType() == TopAbs_EDGE && support2.shape.ShapeType() == TopAbs_FACE) {
            edge = TopoDS::Edge(support1.shape);
            face = TopoDS::Face(support2.shape);
        }
        if (edge.IsNull() || face.IsNull()) {
            return std::nullopt;
        }

        BRepAdaptor_Curve curve(edge);
        GeomAdaptor_Curve typedCurve;
        try {
            typedCurve.Load(GeomAdaptor::MakeCurve(curve));
        }
        catch (const Standard_DomainError&) {
            Handle(Geom_Curve) copiedCurve = curve.Curve().Curve();
            if (copiedCurve.IsNull()) {
                typedCurve = curve.Curve();
            }
            else {
                copiedCurve = Handle(Geom_Curve)::DownCast(copiedCurve->Copy());
                copiedCurve->Transform(curve.Trsf());
                typedCurve.Load(copiedCurve);
            }
        }

        BRepIntCurveSurface_Inter intersector;
        intersector.Init(face, typedCurve, Precision::Confusion());
        if (intersector.More()) {
            return intersector.Pnt();
        }
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
    return std::nullopt;
}

inline std::optional<gp_Trsf> conicPointLandmarkPlacement(const std::string& mode,
                                                         const std::vector<SupportResolution>& supports,
                                                         runtime::ComputeContext& context,
                                                         const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::AttachEnginePoint(), lines 2842-2845, registers "mm0Focus1" as
    // rtConic and "mm0Focus2" as rtEllipse/rtHyperbola. In
    // ::AttachEnginePoint::_calculateAttachedPlacement(), lines 2937-2990, the point branch
    // reads ellipse/hyperbola "Focus1/2()" or parabola "Focus()", and reports
    // "Parabola has no second focus" for Focus2.
    const SupportResolution& support = supports.front();
    const auto edge = conicLandmarkEdge(support, context, object, mode);
    if (!edge) {
        return std::nullopt;
    }

    BRepAdaptor_Curve adapt(*edge);
    gp_Pnt focus;
    switch (adapt.GetType()) {
        case GeomAbs_Ellipse: {
            const gp_Elips ellipse = adapt.Ellipse();
            focus = mode == "Focus1" ? ellipse.Focus1() : ellipse.Focus2();
        } break;
        case GeomAbs_Hyperbola: {
            const gp_Hypr hyperbola = adapt.Hyperbola();
            focus = mode == "Focus1" ? hyperbola.Focus1() : hyperbola.Focus2();
        } break;
        case GeomAbs_Parabola: {
            if (mode == "Focus2") {
                addDatumAttachmentDiagnostic(context,
                                             object,
                                             support.link,
                                             "MapMode",
                                             "Focus2 is not available for a parabola Edge support",
                                             "attachment_support_invalid_shape");
                return std::nullopt;
            }
            focus = adapt.Parabola().Focus();
        } break;
        default:
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " requires an ellipse, hyperbola, or parabola Edge support",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
    }
    return pointDatumPlacement(focus);
}

inline std::optional<gp_Trsf> proximityPointPlacement(const std::string& mode,
                                                      const std::vector<SupportResolution>& supports,
                                                      runtime::ComputeContext& context,
                                                      const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::_calculateAttachedPlacement(), cases "mm0ProximityPoint1" and
    // "mm0ProximityPoint2", require two shapes, call "getProximityPoint(...)", then use
    // "placementFactory(... BasePoint ...)". ::getProximityPoint() falls back to
    // "BRepExtrema_DistShapeShape" and returns "PointOnShape1(1)" for ProximityPoint1 or
    // "PointOnShape2(1)" for ProximityPoint2.
    if (supports.size() < 2U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "DatumPoint ProximityPoint requires two support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }
    if (supports[0].shape.IsNull() || supports[1].shape.IsNull()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "DatumPoint ProximityPoint requires non-null support shapes",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    if (const auto intersection = proximityPointEdgeFaceIntersection(supports[0], supports[1])) {
        return pointDatumPlacement(*intersection);
    }

    try {
        BRepExtrema_DistShapeShape distancer(supports[0].shape, supports[1].shape);
        if (!distancer.IsDone() || distancer.NbSolution() < 1) {
            addLineFamilyDiagnostic(context,
                                    object,
                                    supports,
                                    "DatumPoint ProximityPoint distance calculation failed",
                                    "execution_failed");
            return std::nullopt;
        }

        const gp_Pnt point = mode == "ProximityPoint1" ? distancer.PointOnShape1(1)
                                                       : distancer.PointOnShape2(1);
        return pointDatumPlacement(point);
    }
    catch (const Standard_Failure&) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "DatumPoint ProximityPoint distance calculation failed",
                                "execution_failed");
        return std::nullopt;
    }
}

inline std::optional<gp_Trsf> pointAtVertexOrEdgeStartPlacement(const SupportResolution& support,
                                                                runtime::ComputeContext& context,
                                                                const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::_calculateAttachedPlacement(), case "mm0Vertex", consumes a Vertex
    // point or the first parameter point of an Edge.
    try {
        const TopoDS_Vertex vertex = TopoDS::Vertex(support.shape);
        if (!vertex.IsNull()) {
            return pointDatumPlacement(BRep_Tool::Pnt(vertex));
        }
    }
    catch (const Standard_Failure&) {
    }

    try {
        const TopoDS_Edge edge = TopoDS::Edge(support.shape);
        if (!edge.IsNull()) {
            BRepAdaptor_Curve curve(edge);
            double first = curve.FirstParameter();
            if (Precision::IsInfinite(first)) {
                addDatumAttachmentDiagnostic(context,
                                             object,
                                             support.link,
                                             "MapMode",
                                             "Vertex MapMode cannot use an infinite edge parameter",
                                             "attachment_parameter_invalid");
                return std::nullopt;
            }
            return pointDatumPlacement(curve.Value(first));
        }
    }
    catch (const Standard_Failure&) {
    }

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support.link,
                                 "MapMode",
                                 "Vertex MapMode requires a Vertex support or finite Edge support",
                                 "attachment_support_invalid_shape");
    return std::nullopt;
}

inline std::optional<gp_Trsf> pointOnEdgePlacement(const SupportResolution& support,
                                                   double parameter,
                                                   runtime::ComputeContext& context,
                                                   const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::_calculateAttachedPlacement(), case "mm0OnEdge", remaps to
    // "mmNormalToPath" and uses attachParameter to place the point on the curve.
    try {
        const TopoDS_Edge edge = TopoDS::Edge(support.shape);
        if (!edge.IsNull()) {
            BRepAdaptor_Curve curve(edge);
            double first = curve.FirstParameter();
            double last = curve.LastParameter();
            if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) {
                first = 0.0;
                last = 1.0;
            }
            return pointDatumPlacement(curve.Value(first + parameter * (last - first)));
        }
    }
    catch (const Standard_Failure&) {
    }

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support.link,
                                 "MapMode",
                                 "OnEdge MapMode requires an Edge support",
                                 "attachment_support_invalid_shape");
    return std::nullopt;
}

inline std::optional<gp_Trsf> centerOfMassPlacement(const SupportResolution& support,
                                                    runtime::ComputeContext& context,
                                                    const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEnginePoint::_calculateAttachedPlacement(), case "mm0CenterOfMass", calls
    // "AttachEngine::getInertialPropsOfShape(shapes)" and uses "gpr.CentreOfMass()".
    if (support.shape.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "CenterOfMass MapMode requires non-null support shape",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    GProp_GProps properties;
    BRepGProp::VolumeProperties(support.shape, properties);
    if (properties.Mass() <= Precision::Confusion()) {
        properties = GProp_GProps();
        BRepGProp::SurfaceProperties(support.shape, properties);
    }
    if (properties.Mass() <= Precision::Confusion()) {
        properties = GProp_GProps();
        BRepGProp::LinearProperties(support.shape, properties);
    }
    if (properties.Mass() <= Precision::Confusion()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "CenterOfMass MapMode could not derive inertial properties from support shape",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }
    return pointDatumPlacement(properties.CentreOfMass());
}

inline std::optional<gp_Trsf> translatePlacement(const SupportResolution& support,
                                                runtime::ComputeContext& context,
                                                const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::_calculateAttachedPlacement(), case "mmTranslate", requires
    // "need one vertex", then calls "plm.setPosition(plm.getPosition() +
    // this->attachmentOffset.getPosition())" and keeps "origPlacement.getRotation()".
    TopoDS_Vertex vertex;
    try {
        vertex = TopoDS::Vertex(support.shape);
    }
    catch (const Standard_Failure&) {
    }
    if (vertex.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "Translate requires one Vertex support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    gp_Pnt point = BRep_Tool::Pnt(vertex);
    const gp_Pnt offset = transformOrigin(attachmentOffsetPlacement(object));
    point.Translate(gp_Vec(offset.XYZ()));
    gp_Trsf placement = placementForObject(object.name, context);
    placement.SetTranslationPart(gp_Vec(point.XYZ()));
    return placement;
}

inline bool appendThreePointSupportPoints(const SupportResolution& support,
                                          std::vector<gp_Pnt>& points,
                                          runtime::ComputeContext& context,
                                          const app::DocumentObject& object)
{
    if (support.shape.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support.link,
                                     "MapMode",
                                     "ThreePointsPlane/ThreePointsNormal require non-null Vertex or Edge supports",
                                     "attachment_support_invalid_shape");
        return false;
    }
    if (support.shape.ShapeType() == TopAbs_VERTEX) {
        points.push_back(BRep_Tool::Pnt(TopoDS::Vertex(support.shape)));
        return true;
    }
    if (support.shape.ShapeType() == TopAbs_EDGE) {
        const TopoDS_Edge edge = TopoDS::Edge(support.shape);
        BRepAdaptor_Curve curve(edge);
        double first = curve.FirstParameter();
        double last = curve.LastParameter();
        if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) {
            first = 0.0;
            last = 1.0;
        }
        points.push_back(curve.Value(first));
        points.push_back(curve.Value(last));
        return true;
    }

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support.link,
                                 "MapMode",
                                 "ThreePointsPlane/ThreePointsNormal require Vertex supports or Edge endpoints",
                                 "attachment_support_invalid_shape");
    return false;
}

inline std::optional<gp_Trsf> threePointsPlanePlacement(const std::string& mode,
                                                       const std::vector<SupportResolution>& supports,
                                                       bool mapReverse,
                                                       runtime::ComputeContext& context,
                                                       const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::_calculateAttachedPlacement(), cases "mmThreePointsPlane" and
    // "mmThreePointsNormal", collect Vertex points or Edge first/last parameter points until
    // "points.size() >= 3"; ThreePointsPlane uses "vec01.Crossed(vec02)" and centroid,
    // ThreePointsNormal projects p2 to "new Geom_Plane(p0, gp_Dir(norm))".
    std::vector<gp_Pnt> points;
    for (const auto& support : supports) {
        if (!appendThreePointSupportPoints(support, points, context, object)) {
            return std::nullopt;
        }
        if (points.size() >= 3U) {
            break;
        }
    }
    if (points.size() < 3U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ThreePointsPlane/ThreePointsNormal require at least three points from Vertex or Edge supports",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    const gp_Pnt p0 = points[0];
    const gp_Pnt p1 = points[1];
    const gp_Pnt p2 = points[2];
    gp_Vec vec01(p0, p1);
    gp_Vec vec02(p0, p2);
    if (vec01.Magnitude() < Precision::Confusion() || vec02.Magnitude() < Precision::Confusion()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "ThreePointsPlane/ThreePointsNormal cannot derive a plane from coincident points",
                                "attachment_parameter_invalid");
        return std::nullopt;
    }
    vec01.Normalize();
    vec02.Normalize();

    gp_Vec normal;
    gp_Pnt basePoint;
    if (mode == "ThreePointsPlane") {
        normal = vec01.Crossed(vec02);
        if (normal.Magnitude() < Precision::Confusion()) {
            addLineFamilyDiagnostic(context,
                                    object,
                                    supports,
                                    "ThreePointsPlane cannot derive a plane from collinear points",
                                    "attachment_parameter_invalid");
            return std::nullopt;
        }
        basePoint = gp_Pnt(
            gp_Vec(p0.XYZ()).Added(p1.XYZ()).Added(p2.XYZ()).Multiplied(1.0 / 3.0).XYZ()
        );
    }
    else {
        normal = vec02.Subtracted(vec01.Multiplied(vec02.Dot(vec01))).Reversed();
        if (normal.Magnitude() < Precision::Confusion()) {
            addLineFamilyDiagnostic(context,
                                    object,
                                    supports,
                                    "ThreePointsNormal cannot derive a plane from collinear points",
                                    "attachment_parameter_invalid");
            return std::nullopt;
        }
        Handle(Geom_Plane) plane = new Geom_Plane(p0, gp_Dir(normal));
        GeomAPI_ProjectPointOnSurf projector(p2, plane);
        if (projector.NbPoints() == 0) {
            addLineFamilyDiagnostic(context,
                                    object,
                                    supports,
                                    "ThreePointsNormal could not project the third point onto the derived plane",
                                    "execution_failed");
            return std::nullopt;
        }
        basePoint = projector.NearestPoint();
    }

    return placementFromAxes(basePoint, gp_Dir(normal), gp_Vec(), mapReverse);
}

inline std::optional<gp_Dir> freecadFaceNormalAt(const TopoDS_Face& face, double u, double v)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp
    // ::Tools::getNormal(const TopoDS_Face&, ...), builds "BRepLProp_SLProps", calls
    // "getNormalBySLProp(...)" with CSLib fallback, then reverses when
    // "face.Orientation() == TopAbs_REVERSED".
    BRepAdaptor_Surface adapt(face);
    BRepLProp_SLProps prop(adapt, u, v, 1, Precision::Confusion());
    gp_Dir direction;
    Standard_Boolean done = Standard_False;
    if (prop.D1U().Magnitude() > Precision::Confusion()
        && prop.D1V().Magnitude() > Precision::Confusion() && prop.IsNormalDefined()) {
        direction = prop.Normal();
        done = Standard_True;
    }
    else {
        CSLib_NormalStatus status;
        CSLib::Normal(prop.D1U(),
                      prop.D1V(),
                      prop.D2U(),
                      prop.D2V(),
                      prop.DUV(),
                      Precision::Confusion(),
                      done,
                      status,
                      direction);
        if (status == CSLib_D1NuIsNull) {
            if (std::abs(adapt.LastVParameter() - v) < Precision::Confusion()) {
                direction.Reverse();
            }
        }
        else if (status == CSLib_D1NvIsNull || status == CSLib_D1NuIsParallelD1Nv) {
            if (std::abs(adapt.LastUParameter() - u) < Precision::Confusion()) {
                direction.Reverse();
            }
        }
    }
    if (!done) {
        return std::nullopt;
    }
    if (face.Orientation() == TopAbs_REVERSED) {
        direction.Reverse();
    }
    return direction;
}

inline std::optional<gp_Trsf> tangentPlanePlacement(const std::vector<SupportResolution>& supports,
                                                   bool mapReverse,
                                                   runtime::ComputeContext& context,
                                                   const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::_calculateAttachedPlacement(), case "mmTangentPlane", accepts
    // "rtFace, rtVertex" and "rtVertex, rtFace"; vertex-first uses the vertex as base point,
    // otherwise it uses the projected point, and sets "SketchXAxis = gp_Vec(dirX).Reversed()".
    if (supports.size() < 2U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "TangentPlane requires one Face and one Vertex support",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    const SupportResolution* faceSupport = &supports[0];
    const SupportResolution* vertexSupport = &supports[1];
    bool throughVertex = false;
    if (supports[0].shape.ShapeType() == TopAbs_VERTEX) {
        faceSupport = &supports[1];
        vertexSupport = &supports[0];
        throughVertex = true;
    }

    TopoDS_Face face;
    TopoDS_Vertex vertex;
    try {
        face = TopoDS::Face(faceSupport->shape);
        vertex = TopoDS::Vertex(vertexSupport->shape);
    }
    catch (const Standard_Failure&) {
    }
    if (face.IsNull() || vertex.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     supports.front().link,
                                     "MapMode",
                                     "TangentPlane requires one Face and one Vertex support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    if (surface.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     faceSupport->link,
                                     "MapMode",
                                     "TangentPlane Face support has no surface",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }
    const gp_Pnt vertexPoint = BRep_Tool::Pnt(vertex);
    GeomAPI_ProjectPointOnSurf projector(vertexPoint, surface);
    if (projector.NbPoints() == 0) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     vertexSupport->link,
                                     "MapMode",
                                     "TangentPlane could not project the Vertex support onto the Face surface",
                                     "execution_failed");
        return std::nullopt;
    }

    double u = 0.0;
    double v = 0.0;
    projector.LowerDistanceParameters(u, v);
    const auto normal = freecadFaceNormalAt(face, u, v);
    if (!normal) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     faceSupport->link,
                                     "MapMode",
                                     "TangentPlane could not derive the surface normal at the projected point",
                                     "execution_failed");
        return std::nullopt;
    }

    BRepAdaptor_Surface adaptedSurface(face);
    BRepLProp_SLProps props(adaptedSurface, u, v, 1, Precision::Confusion());
    gp_Dir xDirection;
    if (props.IsTangentUDefined()) {
        props.TangentU(xDirection);
        if (face.Orientation() == TopAbs_REVERSED) {
            xDirection.Reverse();
        }
    }
    else if (props.IsTangentVDefined()) {
        gp_Dir yDirection;
        props.TangentV(yDirection);
        xDirection = yDirection.Crossed(*normal);
    }
    else {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     faceSupport->link,
                                     "MapMode",
                                     "TangentPlane could not derive a tangent axis at the projected point",
                                     "execution_failed");
        return std::nullopt;
    }

    const gp_Pnt basePoint = throughVertex ? vertexPoint : projector.NearestPoint();
    return placementFromAxes(basePoint, *normal, gp_Vec(xDirection).Reversed(), mapReverse);
}

inline bool isCurveFrameMode(const std::string& mode)
{
    return mode == "FrenetNB" || mode == "FrenetTN" || mode == "FrenetTB" || mode == "Concentric"
        || mode == "SectionOfRevolution";
}

inline bool isCurveFrameAliasMode(const std::string& mode)
{
    return mode == "AxisOfCurvature" || mode == "Normal" || mode == "Binormal"
        || mode == "CenterOfCurvature";
}

inline std::string curveFrameSourceMode(DatumAttachmentEngine engine, const std::string& mode)
{
    if (engine == DatumAttachmentEngine::Line) {
        if (mode == "AxisOfCurvature") {
            return "SectionOfRevolution";
        }
        if (mode == "Normal") {
            return "FrenetTB";
        }
        if (mode == "Binormal") {
            return "FrenetTN";
        }
    }
    if (engine == DatumAttachmentEngine::Point && mode == "CenterOfCurvature") {
        return "SectionOfRevolution";
    }
    return mode;
}

inline gp_Trsf axisOfCurvaturePresuperPlacement()
{
    return placementFromAxes(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0), gp_Vec(1.0, 0.0, 0.0), false);
}

inline std::optional<gp_Trsf> curveFramePlacement(const std::string& mode,
                                                 const std::vector<SupportResolution>& supports,
                                                 bool mapReverse,
                                                 double parameter,
                                                 runtime::ComputeContext& context,
                                                 const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::_calculateAttachedPlacement(), cases "mmFrenetNB", "mmFrenetTN",
    // "mmFrenetTB", "mmRevolutionSection" and "mmConcentric", consume "need one edge,
    // and an optional vertex", project the vertex with "GeomAPI_ProjectPointOnCurve", then
    // derive "T = d.Normalized()", "N = dd - T * (dd dot T)" and "B = T x N".
    if (supports.empty()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Datum curve-frame MapMode requires one Edge support and an optional Vertex",
                                "missing_link_target");
        return std::nullopt;
    }

    const SupportResolution* pathSupport = &supports.front();
    const SupportResolution* vertexSupport = supports.size() >= 2U ? &supports[1] : nullptr;
    bool throughVertex = false;
    if (supports.front().shape.ShapeType() == TopAbs_VERTEX && supports.size() >= 2U) {
        pathSupport = &supports[1];
        vertexSupport = &supports.front();
        throughVertex = true;
    }

    TopoDS_Edge path;
    try {
        path = TopoDS::Edge(pathSupport->shape);
    }
    catch (const Standard_Failure&) {
    }
    if (path.IsNull()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     pathSupport->link,
                                     "MapMode",
                                     "Datum curve-frame MapMode requires an Edge path support",
                                     "attachment_support_invalid_shape");
        return std::nullopt;
    }

    BRepAdaptor_Curve curve(path);
    double first = curve.FirstParameter();
    double last = curve.LastParameter();
    if (Precision::IsInfinite(first) || Precision::IsInfinite(last)) {
        first = 0.0;
        last = 1.0;
    }

    double u = first + parameter * (last - first);
    gp_Pnt inputPoint;
    if (vertexSupport != nullptr) {
        TopoDS_Vertex vertex;
        try {
            vertex = TopoDS::Vertex(vertexSupport->shape);
        }
        catch (const Standard_Failure&) {
        }
        if (vertex.IsNull()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         vertexSupport->link,
                                         "MapMode",
                                         "Datum curve-frame optional point support must be a Vertex",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        inputPoint = BRep_Tool::Pnt(vertex);

        TopLoc_Location location;
        Handle(Geom_Curve) projectedCurve = BRep_Tool::Curve(path, location, first, last);
        if (projectedCurve.IsNull()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         pathSupport->link,
                                         "MapMode",
                                         "Datum curve-frame Edge support has no curve for vertex projection",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        if (!location.IsIdentity()) {
            projectedCurve = Handle(Geom_Curve)::DownCast(projectedCurve->Copy());
            projectedCurve->Transform(location.Transformation());
        }

        GeomAPI_ProjectPointOnCurve projector(inputPoint, projectedCurve, first, last);
        if (projector.NbPoints() < 1) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         vertexSupport->link,
                                         "MapMode",
                                         "Datum curve-frame could not project the Vertex support onto the path curve",
                                         "projection_failed");
            return std::nullopt;
        }
        u = projector.LowerDistanceParameter();
    }

    gp_Pnt point;
    gp_Vec tangent;
    try {
        curve.D1(u, point, tangent);
    }
    catch (const Standard_Failure&) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     pathSupport->link,
                                     "MapPathParameter",
                                     "Datum curve-frame could not evaluate the path derivative at the selected parameter",
                                     "attachment_parameter_invalid");
        return std::nullopt;
    }
    if (tangent.Magnitude() < Precision::Confusion()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     pathSupport->link,
                                     "MapPathParameter",
                                     "Datum curve-frame path derivative is too small at the selected parameter",
                                     "attachment_parameter_invalid");
        return std::nullopt;
    }

    gp_Pnt basePoint = throughVertex ? inputPoint : point;
    gp_Vec secondDerivative;
    try {
        curve.D2(u, point, tangent, secondDerivative);
    }
    catch (const Standard_Failure&) {
        secondDerivative = gp_Vec(0.0, 0.0, 0.0);
    }

    gp_Vec tangentAxis = tangent.Normalized();
    gp_Vec normalAxis = secondDerivative.Subtracted(tangentAxis.Multiplied(secondDerivative.Dot(tangentAxis)));
    gp_Vec binormalAxis;
    if (normalAxis.Magnitude() > Precision::SquareConfusion()) {
        normalAxis.Normalize();
        binormalAxis = tangentAxis.Crossed(normalAxis);
    }
    else {
        normalAxis = gp_Vec(0.0, 0.0, 0.0);
        binormalAxis = gp_Vec(0.0, 0.0, 0.0);
    }

    const bool requiresDefinedNormal = mode == "FrenetNB" || mode == "FrenetTN" || mode == "FrenetTB"
        || mode == "Concentric" || mode == "SectionOfRevolution";
    if (requiresDefinedNormal && normalAxis.Magnitude() == 0.0) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     pathSupport->link,
                                     "MapMode",
                                     mode == "Concentric" || mode == "SectionOfRevolution"
                                         ? "Datum curve-frame path has infinite radius of curvature at the selected point"
                                         : "Datum curve-frame Frenet normal is undefined at the selected point",
                                     mode == "Concentric" || mode == "SectionOfRevolution"
                                         ? "infinite_curvature_radius"
                                         : "undefined_frenet_normal");
        return std::nullopt;
    }

    gp_Dir sketchNormal(0.0, 0.0, 1.0);
    gp_Vec sketchXAxis(1.0, 0.0, 0.0);
    if (mode == "FrenetNB" || mode == "SectionOfRevolution") {
        sketchNormal = gp_Dir(tangentAxis.Reversed());
        sketchXAxis = normalAxis.Reversed();
    }
    else if (mode == "FrenetTN" || mode == "Concentric") {
        sketchNormal = gp_Dir(binormalAxis);
        sketchXAxis = tangentAxis;
    }
    else if (mode == "FrenetTB") {
        sketchNormal = gp_Dir(normalAxis.Reversed());
        sketchXAxis = tangentAxis;
    }

    if (mode == "Concentric" || mode == "SectionOfRevolution") {
        const double curvature = secondDerivative.Dot(normalAxis) / std::pow(tangent.Magnitude(), 2.0);
        if (std::abs(curvature) < Precision::Confusion()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         pathSupport->link,
                                         "MapMode",
                                         "Datum curve-frame path has infinite radius of curvature at the selected point",
                                         "infinite_curvature_radius");
            return std::nullopt;
        }
        gp_Vec baseVector(point.XYZ());
        baseVector.Add(normalAxis.Multiplied(1.0 / curvature));
        basePoint = gp_Pnt(baseVector.XYZ());
    }

    return placementFromAxes(basePoint, sketchNormal, sketchXAxis, mapReverse);
}

inline std::optional<double> calculateFoldAngle(const std::array<gp_Vec, 4>& dirs,
                                                runtime::ComputeContext& context,
                                                const app::DocumentObject& object,
                                                const std::vector<SupportResolution>& supports)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::calculateFoldAngle(), normalizes "axA, axB, edA, edB", rejects
    // "Folding axes are parallel", rejects "axisA and edgeA are parallel", then returns
    // "acos(cos_unfold)" for the sheet-fold placement math.
    gp_Vec axA = dirs[1];
    gp_Vec axB = dirs[2];
    gp_Vec edA = dirs[0];
    gp_Vec edB = dirs[3];
    axA.Normalize();
    axB.Normalize();
    edA.Normalize();
    edB.Normalize();

    gp_Vec norm = axA.Crossed(axB);
    if (norm.Magnitude() < Precision::Confusion()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Folding axes are parallel, folding angle cannot be computed",
                                "attachment_parameter_invalid");
        return std::nullopt;
    }
    norm.Normalize();

    const double a = edA.Dot(axA);
    const double ra = edA.Crossed(axA).Magnitude();
    if (std::abs(ra) < Precision::Confusion()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Folding axisA and edgeA are parallel, folding angle cannot be computed",
                                "attachment_parameter_invalid");
        return std::nullopt;
    }

    const double b = edB.Dot(axB);
    const double costheta = axB.Dot(axA);
    const double sintheta = axA.Crossed(axB).Dot(norm);
    const double singama = -costheta;
    const double cosgama = sintheta;
    const double k = b * cosgama;
    const double l = a + b * singama;
    const double xa = k + l * singama / cosgama;
    const double cosUnfold = -xa / ra;
    if (std::abs(cosUnfold) > 0.999) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Folding cosine is too close to or above 1, folding angle cannot be computed",
                                "attachment_parameter_invalid");
        return std::nullopt;
    }
    return std::acos(cosUnfold);
}

inline std::optional<gp_Trsf> foldingPlacement(const std::vector<SupportResolution>& supports,
                                               bool mapReverse,
                                               runtime::ComputeContext& context,
                                               const app::DocumentObject& object)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngine3D::_calculateAttachedPlacement(), case "mmFolding", expects four ordered
    // "edgeA, fold axis A, fold axis B, edgeB" rtLine supports, finds one common vertex,
    // flips each line direction to point away from that vertex, calls "calculateFoldAngle",
    // then sets "SketchNormal", "SketchXAxis" and "SketchBasePoint".
    if (supports.size() < 4U) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Folding requires four ordered line supports: edgeA, axisA, axisB, edgeB",
                                "attachment_support_invalid_shape");
        return std::nullopt;
    }

    std::array<BRepAdaptor_Curve, 4> curves;
    std::array<gp_Lin, 4> lines;
    for (std::size_t index = 0; index < 4U; ++index) {
        TopoDS_Edge edge;
        try {
            edge = TopoDS::Edge(supports[index].shape);
        }
        catch (const Standard_Failure&) {
        }
        if (edge.IsNull()) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         supports[index].link,
                                         "MapMode",
                                         "Folding requires each ordered support to resolve to a non-null Edge",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        curves[index] = BRepAdaptor_Curve(edge);
        if (curves[index].GetType() != GeomAbs_Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         supports[index].link,
                                         "MapMode",
                                         "Folding requires straight Edge supports",
                                         "attachment_support_invalid_shape");
            return std::nullopt;
        }
        lines[index] = curves[index].Line();
    }

    gp_Pnt sharedPoint;
    std::array<double, 4> signs = {0.0, 0.0, 0.0, 0.0};
    const gp_Pnt p1 = curves[0].Value(curves[0].FirstParameter());
    const gp_Pnt p2 = curves[0].Value(curves[0].LastParameter());
    const gp_Pnt p3 = curves[1].Value(curves[1].FirstParameter());
    const gp_Pnt p4 = curves[1].Value(curves[1].LastParameter());
    if (p1.Distance(p3) < Precision::Confusion()) {
        sharedPoint = p3;
        signs[0] = +1.0;
        signs[1] = +1.0;
    }
    else if (p1.Distance(p4) < Precision::Confusion()) {
        sharedPoint = p4;
        signs[0] = +1.0;
        signs[1] = -1.0;
    }
    else if (p2.Distance(p3) < Precision::Confusion()) {
        sharedPoint = p3;
        signs[0] = -1.0;
        signs[1] = +1.0;
    }
    else if (p2.Distance(p4) < Precision::Confusion()) {
        sharedPoint = p4;
        signs[0] = -1.0;
        signs[1] = -1.0;
    }
    else {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Folding edgeA and axisA supports do not share a vertex",
                                "no_intersection");
        return std::nullopt;
    }

    for (std::size_t index = 2U; index < 4U; ++index) {
        const gp_Pnt first = curves[index].Value(curves[index].FirstParameter());
        const gp_Pnt last = curves[index].Value(curves[index].LastParameter());
        if (sharedPoint.Distance(first) < Precision::Confusion()) {
            signs[index] = +1.0;
        }
        else if (sharedPoint.Distance(last) < Precision::Confusion()) {
            signs[index] = -1.0;
        }
        else {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         supports[index].link,
                                         "MapMode",
                                         "Folding supports must all share the same vertex",
                                         "no_intersection");
            return std::nullopt;
        }
    }

    std::array<gp_Vec, 4> dirs;
    for (std::size_t index = 0; index < 4U; ++index) {
        dirs[index] = gp_Vec(lines[index].Direction()).Multiplied(signs[index]);
    }

    const auto angle = calculateFoldAngle(dirs, context, object, supports);
    if (!angle) {
        return std::nullopt;
    }

    gp_Vec normal = dirs[1].Crossed(dirs[2]);
    normal.Rotate(gp_Ax1(gp_Pnt(), gp_Dir(dirs[1])), -*angle);
    return placementFromAxes(sharedPoint, gp_Dir(normal.Reversed()), dirs[1], mapReverse);
}

inline std::optional<gp_Trsf> selectedDatumPlacement(const app::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    DatumAttachmentEngine engine,
                                                    const std::string& mode,
                                                    const std::vector<SupportResolution>& supports,
                                                    bool mapReverse,
                                                    double parameter)
{
    if (supports.empty()) {
        addLineFamilyDiagnostic(context,
                                object,
                                supports,
                                "Datum selected MapMode requires AttachmentSupport request evidence",
                                "missing_link_target");
        return std::nullopt;
    }
    const SupportResolution& support = supports.front();
    if (mode == "Translate") {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "Translate is supported for DatumPlane and CoordinateSystem in this C5-M15 batch");
            return std::nullopt;
        }
        return translatePlacement(support, context, object);
    }
    if (mode == "FlatFace") {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "FlatFace is supported for DatumPlane and CoordinateSystem in this S5 batch");
            return std::nullopt;
        }
        return flatFacePlacement(support, mapReverse, context, object);
    }
    if (mode == "TangentPlane") {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "TangentPlane is supported for DatumPlane and CoordinateSystem in this C5-M15 batch");
            return std::nullopt;
        }
        return tangentPlanePlacement(supports, mapReverse, context, object);
    }
    if (mode == "ThreePointsPlane" || mode == "ThreePointsNormal") {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumPlane and CoordinateSystem in this C5-M15 batch");
            return std::nullopt;
        }
        return threePointsPlanePlacement(mode, supports, mapReverse, context, object);
    }
    if (mode == "Folding") {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "Folding is supported for DatumPlane and CoordinateSystem in this C5-M18 batch");
            return std::nullopt;
        }
        return foldingPlacement(supports, mapReverse, context, object);
    }
    if (mode == "ObjectXY" || mode == "ObjectXZ" || mode == "ObjectYZ") {
        if (engine == DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is not a Point AttachEngine selected mode");
            return std::nullopt;
        }
        return placementFromObjectMode(mode, support.placement, mapReverse);
    }
    if (mode == "ObjectOrigin") {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "ObjectOrigin is supported for DatumPoint in this S5 batch");
            return std::nullopt;
        }
        return placementFromObjectMode(mode, support.placement, mapReverse);
    }
    if (mode == "ObjectX" || mode == "ObjectY" || mode == "ObjectZ") {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumLine in this S5 batch");
            return std::nullopt;
        }
        return placementFromObjectMode(mode, support.placement, mapReverse);
    }
    if (mode == "NormalToEdge" || mode == "Tangent") {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "NormalToEdge is supported for DatumLine in this S5 batch");
            return std::nullopt;
        }
        return normalToEdgePlacement(support, mapReverse, parameter, context, object);
    }
    if (mode == "TwoPointLine") {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "TwoPointLine is supported for DatumLine in this C51X batch");
            return std::nullopt;
        }
        return twoPointLinePlacement(supports, mapReverse, context, object);
    }
    if (mode == "IntersectionLine") {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "IntersectionLine is supported for DatumLine in this C51X batch");
            return std::nullopt;
        }
        return intersectionLinePlacement(supports, mapReverse, context, object);
    }
    if (mode == "ProximityLine") {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "ProximityLine is supported for DatumLine in this C51X batch");
            return std::nullopt;
        }
        return proximityLinePlacement(supports, mapReverse, context, object);
    }
    if (isConicLineLandmarkMode(mode)) {
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumLine in this C5-M17 batch");
            return std::nullopt;
        }
        return conicLineLandmarkPlacement(mode, supports, mapReverse, context, object);
    }
    if (mode == "Vertex") {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "Vertex is supported for DatumPoint in this C51X batch");
            return std::nullopt;
        }
        return pointAtVertexOrEdgeStartPlacement(support, context, object);
    }
    if (mode == "OnEdge") {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "OnEdge is supported for DatumPoint in this C51X batch");
            return std::nullopt;
        }
        return pointOnEdgePlacement(support, parameter, context, object);
    }
    if (mode == "CenterOfMass") {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "CenterOfMass is supported for DatumPoint in this C51X batch");
            return std::nullopt;
        }
        return centerOfMassPlacement(support, context, object);
    }
    if (mode == "ProximityPoint1" || mode == "ProximityPoint2") {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         "ProximityPoint1/2 is supported for DatumPoint in this C5-M14 batch");
            return std::nullopt;
        }
        return proximityPointPlacement(mode, supports, context, object);
    }
    if (isConicPointLandmarkMode(mode)) {
        if (engine != DatumAttachmentEngine::Point) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumPoint in this C5-M17 batch");
            return std::nullopt;
        }
        return conicPointLandmarkPlacement(mode, supports, context, object);
    }
    if (isCurveFrameMode(mode)) {
        if (engine != DatumAttachmentEngine::Plane && engine != DatumAttachmentEngine::CoordinateSystem) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumPlane and CoordinateSystem in this C5-M16 batch");
            return std::nullopt;
        }
        return curveFramePlacement(mode, supports, mapReverse, parameter, context, object);
    }
    if (isCurveFrameAliasMode(mode)) {
        const std::string sourceMode = curveFrameSourceMode(engine, mode);
        if (mode == "CenterOfCurvature") {
            if (engine != DatumAttachmentEngine::Point) {
                addDatumAttachmentDiagnostic(context,
                                             object,
                                             support.link,
                                             "MapMode",
                                             "CenterOfCurvature is supported for DatumPoint in this C5-M16 batch");
                return std::nullopt;
            }
            return curveFramePlacement(sourceMode, supports, mapReverse, parameter, context, object);
        }
        if (engine != DatumAttachmentEngine::Line) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support.link,
                                         "MapMode",
                                         mode + " is supported for DatumLine in this C5-M16 batch");
            return std::nullopt;
        }
        auto placement = curveFramePlacement(sourceMode, supports, mapReverse, parameter, context, object);
        if (!placement) {
            return std::nullopt;
        }
        if (mode == "AxisOfCurvature") {
            *placement = *placement * axisOfCurvaturePresuperPlacement();
        }
        return placement;
    }

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support.link,
                                 "MapMode",
                                 "Datum MapMode=" + mode
                                     + " is not in the C51X selected non-GUI AttachEngine batch");
    return std::nullopt;
}

inline std::optional<DatumAttachmentPlacement> datumAttachmentPlacement(const app::DocumentObject& object,
                                                                       runtime::ComputeContext& context,
                                                                       DatumAttachmentEngine engine)
{
    const std::vector<app::Link> supportLinks = app::readLinks(object, "AttachmentSupport");
    app::Link support;
    bool hasAttachmentSupport = false;
    for (const auto& link : supportLinks) {
        if (link.object.empty()) {
            continue;
        }
        support = link;
        hasAttachmentSupport = true;
        break;
    }

    const bool hasActiveMapMode = !isDefaultDatumMapMode(object);
    const bool hasActiveAttachmentOffset = hasNonDefaultPlacement(object, "AttachmentOffset");
    const bool hasActiveMapReversed = app::readBool(object, "MapReversed").value_or(false)
        || app::readBool(object, "Reverse").value_or(false);
    const auto pathParameter = datumPathParameter(object);
    const bool hasActiveMapPathParameter = pathParameter.has_value() && std::abs(*pathParameter) > 1.0e-12;

    if (!hasAttachmentSupport && !hasActiveMapMode && !hasActiveAttachmentOffset && !hasActiveMapReversed
        && !hasActiveMapPathParameter) {
        return DatumAttachmentPlacement{false, placementForObject(object.name, context), "Deactivated", "Deactivated"};
    }
    if (!hasActiveMapMode) {
        if (hasActiveAttachmentOffset || hasActiveMapReversed || hasActiveMapPathParameter) {
            addDatumAttachmentDiagnostic(context,
                                         object,
                                         support,
                                         "MapMode",
                                         "Datum AttachmentOffset/MapReversed/MapPathParameter require an active selected MapMode");
            return std::nullopt;
        }
        return DatumAttachmentPlacement{false, placementForObject(object.name, context), "Deactivated", "Deactivated"};
    }
    if (!hasAttachmentSupport) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support,
                                     "AttachmentSupport",
                                     "Datum selected MapMode requires AttachmentSupport request evidence",
                                     "missing_link_target");
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::positionBySupport() calls "setOffset(AttachmentOffset.getValue() *
    // basePlacement.inverse())" and then "calculateAttachedPlacement(plaOriginal, &subChanged)".
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp
    // ::AttachEngineLine::_calculateAttachedPlacement() remaps ObjectX/Y/Z to ObjectYZ/XZ/XY, and
    // ::AttachEnginePoint::_calculateAttachedPlacement() remaps ObjectOrigin to ObjectXY.
    // This request-local subset implements those selected modes without GUI editor/session state.
    const std::string mode = datumMapModeLabel(object);
    const bool usesLineFamilySupports = mode == "TwoPointLine" || mode == "IntersectionLine"
        || mode == "ProximityLine";
    const bool usesPointProximitySupports = engine == DatumAttachmentEngine::Point
        && (mode == "ProximityPoint1" || mode == "ProximityPoint2");
    const bool usesTangentPlaneSupports = mode == "TangentPlane";
    const bool usesThreePointPlaneSupports = mode == "ThreePointsPlane" || mode == "ThreePointsNormal";
    const bool usesDatum3DPlaneMultiSupports = usesTangentPlaneSupports || usesThreePointPlaneSupports;
    const bool usesCurveFrameSupports = isCurveFrameMode(mode) || isCurveFrameAliasMode(mode);
    const bool usesFoldingSupports = mode == "Folding";
    const bool requiresSubshape = mode == "FlatFace" || mode == "NormalToEdge" || mode == "Tangent"
        || mode == "Vertex" || mode == "OnEdge" || mode == "Translate" || mode == "TangentPlane"
        || mode == "ThreePointsPlane" || mode == "ThreePointsNormal" || usesCurveFrameSupports
        || usesFoldingSupports || isConicLineLandmarkMode(mode) || isConicPointLandmarkMode(mode);
    std::vector<SupportResolution> resolvedSupports;
    const std::size_t supportCount = usesLineFamilySupports ? supportLinks.size()
        : (usesPointProximitySupports ? 2U
                                      : (usesTangentPlaneSupports ? 2U
                                                                  : (usesThreePointPlaneSupports ? supportLinks.size()
                                                                      : (usesCurveFrameSupports ? 2U
                                                                         : (usesFoldingSupports ? 4U : 1U)))));
    for (std::size_t index = 0; index < supportLinks.size() && resolvedSupports.size() < supportCount; ++index) {
        const auto& link = supportLinks.at(index);
        if (link.object.empty()) {
            continue;
        }
        auto resolvedSupport = resolveAttachmentSupport(object, context, link, requiresSubshape);
        if (!resolvedSupport) {
            return std::nullopt;
        }
        resolvedSupports.push_back(std::move(*resolvedSupport));
    }
    if (resolvedSupports.empty()) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support,
                                     "AttachmentSupport",
                                     "Datum selected MapMode requires AttachmentSupport request evidence",
                                     "missing_link_target");
        return std::nullopt;
    }
    auto placement = selectedDatumPlacement(object,
                                           context,
                                           engine,
                                           mode,
                                           resolvedSupports,
                                           hasActiveMapReversed,
                                           pathParameter.value_or(0.0));
    if (!placement) {
        return std::nullopt;
    }
    if (hasActiveAttachmentOffset && mode != "Translate") {
        *placement = *placement * attachmentOffsetPlacement(object);
    }
    if (usesPointProximitySupports || usesDatum3DPlaneMultiSupports || usesCurveFrameSupports
        || usesFoldingSupports) {
        appendAttachmentSupportsWriteback(object, resolvedSupports, context);
    }
    else if (!usesLineFamilySupports && hasReferenceStabilityEvidence(support)) {
        appendAttachmentSupportWriteback(object, resolvedSupports.front(), context);
    }
    return DatumAttachmentPlacement{true, *placement, mode, curveFrameSourceMode(engine, mode)};
}

} // namespace cad_core::part_design::detail
