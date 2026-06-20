#pragma once

#include "cad_core/app/document.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Plane.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
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
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
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

inline std::optional<gp_Trsf> selectedDatumPlacement(const app::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    DatumAttachmentEngine engine,
                                                    const std::string& mode,
                                                    const SupportResolution& support,
                                                    bool mapReverse,
                                                    double parameter)
{
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

    addDatumAttachmentDiagnostic(context,
                                 object,
                                 support.link,
                                 "MapMode",
                                 "Datum MapMode=" + mode
                                     + " is not in the C51-S5 selected non-GUI AttachEngine batch");
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
        return DatumAttachmentPlacement{false, placementForObject(object.name, context)};
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
        return DatumAttachmentPlacement{false, placementForObject(object.name, context)};
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
    const bool requiresSubshape = mode == "FlatFace" || mode == "NormalToEdge" || mode == "Tangent"
        || mode == "Vertex" || mode == "OnEdge";
    auto resolvedSupport = resolveAttachmentSupport(object, context, support, requiresSubshape);
    if (!resolvedSupport) {
        return std::nullopt;
    }
    auto placement = selectedDatumPlacement(object,
                                           context,
                                           engine,
                                           mode,
                                           *resolvedSupport,
                                           hasActiveMapReversed,
                                           pathParameter.value_or(0.0));
    if (!placement) {
        return std::nullopt;
    }
    if (hasActiveAttachmentOffset) {
        *placement = *placement * attachmentOffsetPlacement(object);
    }
    if (hasReferenceStabilityEvidence(support)) {
        appendAttachmentSupportWriteback(object, *resolvedSupport, context);
    }
    return DatumAttachmentPlacement{true, *placement};
}

} // namespace cad_core::part_design::detail
