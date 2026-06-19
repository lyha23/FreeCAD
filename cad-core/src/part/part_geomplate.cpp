#include "cad_core/part/part_geomplate.h"

#include "cad_core/app/document.h"
#include "cad_core/part/part_feature.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_executor.h"

#include "part_feature_support.h"

#include <Adaptor3d_Curve.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_Shape.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomPlate_BuildPlateSurface.hxx>
#include <GeomPlate_CurveConstraint.hxx>
#include <GeomPlate_MakeApprox.hxx>
#include <GeomPlate_PointConstraint.hxx>
#include <GeomPlate_Surface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_Curve.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part
{

namespace
{

constexpr const char* kFeatureName = "part_geomplate_surface";
constexpr const char* kHelperName = "Part.GeomPlate.BuildPlateSurface";

GeomAbs_Shape continuityFromName(const std::string& value)
{
    if (value == "C0") {
        return GeomAbs_C0;
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
    if (value == "G1") {
        return GeomAbs_G1;
    }
    return GeomAbs_C1;
}

nlohmann::json errorObject()
{
    return {
        {"status", "error"},
        {"feature", kFeatureName},
        {"helper", kHelperName},
        {"source_backed_helper", true},
        {"freecad_native_document_object", false},
    };
}

void addGeomPlateDiagnostic(
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
    context.objects[object.name] = errorObject();
}

const nlohmann::json* rawPropertyPayload(const app::DocumentObject& object, const std::string& property)
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

bool readFiniteProperty(
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
    if (!value || !std::isfinite(*value)) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part.GeomPlate.BuildPlateSurface " + property + " must be a finite number",
            property
        );
        return false;
    }
    output = *value;
    return true;
}

bool readPositiveIntProperty(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    int& output
)
{
    double value = static_cast<double>(output);
    if (!readFiniteProperty(object, context, property, value)) {
        return false;
    }
    if (value < 1.0 || std::abs(value - std::round(value)) > 1e-9) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part.GeomPlate.BuildPlateSurface " + property + " must be a positive integer",
            property
        );
        return false;
    }
    output = static_cast<int>(std::llround(value));
    return true;
}

bool readNonNegativeIntProperty(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    int& output
)
{
    double value = static_cast<double>(output);
    if (!readFiniteProperty(object, context, property, value)) {
        return false;
    }
    if (value < 0.0 || std::abs(value - std::round(value)) > 1e-9) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part.GeomPlate.BuildPlateSurface " + property + " must be a non-negative integer",
            property
        );
        return false;
    }
    output = static_cast<int>(std::llround(value));
    return true;
}

bool readPositiveNumberProperty(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    double& output
)
{
    if (!readFiniteProperty(object, context, property, output)) {
        return false;
    }
    if (output <= 0.0) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part.GeomPlate.BuildPlateSurface " + property + " must be positive",
            property
        );
        return false;
    }
    return true;
}

bool readBuildParams(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    GeomPlateBuildParams& params
)
{
    if (!readPositiveIntProperty(object, context, "Degree", params.degree)
        || !readPositiveIntProperty(object, context, "NbPtsOnCur", params.nbPtsOnCur)
        || !readPositiveIntProperty(object, context, "NbIter", params.nbIter)
        || !readPositiveNumberProperty(object, context, "Tol2d", params.tol2d)
        || !readPositiveNumberProperty(object, context, "Tol3d", params.tol3d)
        || !readPositiveNumberProperty(object, context, "TolAng", params.tolAng)
        || !readPositiveNumberProperty(object, context, "TolCurv", params.tolCurv)) {
        return false;
    }
    if (app::propertyValue(object, "Anisotropy") != nullptr) {
        const auto anisotropy = app::readBool(object, "Anisotropy");
        if (!anisotropy) {
            addGeomPlateDiagnostic(
                object,
                context,
                "invalid_parameter",
                "Part.GeomPlate.BuildPlateSurface Anisotropy must be boolean",
                "Anisotropy"
            );
            return false;
        }
        params.anisotropy = *anisotropy;
    }
    return true;
}

bool readApproximationParams(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    GeomPlateApproximationParams& params
)
{
    if (!readPositiveNumberProperty(object, context, "ApproxTol3d", params.tol3d)
        || !readPositiveIntProperty(object, context, "ApproxMaxSegments", params.maxSegments)
        || !readPositiveIntProperty(object, context, "ApproxMaxDegree", params.maxDegree)
        || !readPositiveNumberProperty(object, context, "ApproxMaxDistance", params.maxDistance)
        || !readNonNegativeIntProperty(object, context, "ApproxCritOrder", params.critOrder)
        || !readPositiveNumberProperty(object, context, "ApproxEnlargeCoeff", params.enlargeCoeff)) {
        return false;
    }
    if (const auto continuity = app::readString(object, "ApproxContinuity")) {
        static const std::set<std::string> allowed {"C0", "C1", "C2", "C3", "CN", "G1"};
        if (allowed.count(*continuity) == 0U) {
            addGeomPlateDiagnostic(
                object,
                context,
                "invalid_parameter",
                "Part.GeomPlate.BuildPlateSurface ApproxContinuity must be C0/C1/C2/C3/CN/G1",
                "ApproxContinuity"
            );
            return false;
        }
        params.continuity = *continuity;
    }
    return true;
}

std::string stableSubnameForLink(const app::Link& link, std::size_t index)
{
    return index < link.stableSubnames.size() && !link.stableSubnames[index].empty()
        ? link.stableSubnames[index]
        : index < link.subnames.size() ? link.subnames[index] : std::string {};
}

std::optional<TopoDS_Shape> resolveGeomPlateCurveShape(
    const runtime::ComputeContext& context,
    const app::Link& link,
    std::size_t index
)
{
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        return std::nullopt;
    }
    if (index >= link.subnames.size() || link.subnames[index].empty()) {
        return shapeIt->second.shape;
    }
    const auto namedShapeIt = context.namedShapes.find(link.object);
    const NamedShape* namedShape = namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr;
    const std::string subname = link.subnames[index];
    const std::string stable = stableSubnameForLink(link, index);
    if (namedShape != nullptr) {
        if (const auto shape = part::subshapeByName(*namedShape, subname, stable)) {
            return shape;
        }
    }
    if (const auto shape = part::subshapeByName(shapeIt->second.shape, subname)) {
        return shape;
    }
    if (!stable.empty() && stable != subname) {
        return part::subshapeByName(shapeIt->second.shape, stable);
    }
    return std::nullopt;
}

std::optional<double> numberField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_number() || !std::isfinite(it->get<double>())) {
        return std::nullopt;
    }
    return it->get<double>();
}

int intField(const nlohmann::json& value, const std::string& field, int fallback)
{
    const auto number = numberField(value, field);
    if (!number || *number < 1.0 || std::abs(*number - std::round(*number)) > 1e-9) {
        return fallback;
    }
    return static_cast<int>(std::llround(*number));
}

double positiveField(const nlohmann::json& value, const std::string& field, double fallback)
{
    const auto number = numberField(value, field);
    return number && *number > 0.0 ? *number : fallback;
}

std::vector<nlohmann::json> rawCurveConstraintItems(const app::DocumentObject& object)
{
    const auto* payload = rawPropertyPayload(object, "CurveConstraints");
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

std::optional<std::vector<GeomPlateCurveConstraintSource>> readCurveConstraints(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    std::vector<GeomPlateCurveConstraintSource> result;
    const std::vector<app::Link> links = app::readLinks(object, "CurveConstraints");
    const std::vector<nlohmann::json> rawItems = rawCurveConstraintItems(object);
    for (std::size_t linkIndex = 0; linkIndex < links.size(); ++linkIndex) {
        const app::Link& link = links[linkIndex];
        const nlohmann::json raw = linkIndex < rawItems.size() && rawItems[linkIndex].is_object()
            ? rawItems[linkIndex]
            : nlohmann::json::object();
        const std::size_t count = link.subnames.empty() ? 1U : link.subnames.size();
        for (std::size_t subIndex = 0; subIndex < count; ++subIndex) {
            const auto shape = resolveGeomPlateCurveShape(context, link, subIndex);
            if (!shape || shape->IsNull()) {
                addGeomPlateDiagnostic(
                    object,
                    context,
                    "missing_curve_source",
                    "Curve constraint source " + link.object + " did not produce a resolvable edge",
                    "CurveConstraints",
                    link.object
                );
                return std::nullopt;
            }
            GeomPlateCurveConstraintSource source;
            source.objectName = link.object;
            source.shape = *shape;
            if (subIndex < link.subnames.size()) {
                source.subname = link.subnames[subIndex];
                source.stableSubname = stableSubnameForLink(link, subIndex);
            }
            source.order = intField(raw, "Order", source.order);
            source.nbPts = intField(raw, "NbPts", source.nbPts);
            source.tolDist = positiveField(raw, "TolDist", source.tolDist);
            source.tolAng = positiveField(raw, "TolAng", source.tolAng);
            source.tolCurv = positiveField(raw, "TolCurv", source.tolCurv);
            result.push_back(std::move(source));
        }
    }
    return result;
}

std::optional<std::array<double, 3>> pointFromJson(
    const nlohmann::json& value,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    std::size_t index
)
{
    if (!value.is_array() || value.size() != 3U) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_point_constraint",
            "PointConstraints[" + std::to_string(index) + "] must be a three-number vector",
            "PointConstraints"
        );
        return std::nullopt;
    }
    std::array<double, 3> result {};
    for (std::size_t i = 0; i < 3U; ++i) {
        if (!value[i].is_number() || !std::isfinite(value[i].get<double>())) {
            addGeomPlateDiagnostic(
                object,
                context,
                "invalid_point_constraint",
                "PointConstraints[" + std::to_string(index) + "] must contain finite numbers",
                "PointConstraints"
            );
            return std::nullopt;
        }
        result[i] = value[i].get<double>();
    }
    return result;
}

std::optional<std::vector<GeomPlatePointConstraintSource>> readPointConstraints(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    std::vector<GeomPlatePointConstraintSource> result;
    const nlohmann::json* payload = rawPropertyPayload(object, "PointConstraints");
    if (payload == nullptr) {
        return result;
    }
    if (!payload->is_array()) {
        addGeomPlateDiagnostic(
            object,
            context,
            "invalid_point_constraint",
            "PointConstraints must be a list",
            "PointConstraints"
        );
        return std::nullopt;
    }
    for (std::size_t index = 0; index < payload->size(); ++index) {
        const nlohmann::json& item = payload->at(index);
        const nlohmann::json* point = &item;
        GeomPlatePointConstraintSource source;
        if (item.is_object()) {
            const auto pointIt = item.find("Point");
            if (pointIt == item.end()) {
                addGeomPlateDiagnostic(
                    object,
                    context,
                    "invalid_point_constraint",
                    "PointConstraints[" + std::to_string(index) + "] requires Point",
                    "PointConstraints"
                );
                return std::nullopt;
            }
            point = &*pointIt;
            source.order = intField(item, "Order", source.order);
            source.tolDist = positiveField(item, "TolDist", source.tolDist);
        }
        auto parsedPoint = pointFromJson(*point, object, context, index);
        if (!parsedPoint) {
            return std::nullopt;
        }
        source.point = *parsedPoint;
        result.push_back(source);
    }
    return result;
}

nlohmann::json buildParamsJson(const GeomPlateBuildParams& params)
{
    return {
        {"degree", params.degree},
        {"nb_pts_on_cur", params.nbPtsOnCur},
        {"nb_iter", params.nbIter},
        {"tol_2d", params.tol2d},
        {"tol_3d", params.tol3d},
        {"tol_ang", params.tolAng},
        {"tol_curv", params.tolCurv},
        {"anisotropy", params.anisotropy},
    };
}

nlohmann::json approximationParamsJson(const GeomPlateApproximationParams& params, const GeomPlateBuildResult& result)
{
    nlohmann::json payload = {
        {"status", result.approximationDone ? "ok" : "not_done"},
        {"surface_kind", result.approximationSurfaceKind},
        {"tol_3d", params.tol3d},
        {"max_segments", params.maxSegments},
        {"max_degree", params.maxDegree},
        {"max_distance", params.maxDistance},
        {"crit_order", params.critOrder},
        {"continuity", params.continuity},
        {"enlarge_coeff", params.enlargeCoeff},
    };
    if (result.approxError) {
        payload["approx_error"] = *result.approxError;
    }
    if (result.criterionError) {
        payload["criterion_error"] = *result.criterionError;
    }
    return payload;
}

nlohmann::json optionalErrorJson(const std::optional<double>& value)
{
    return value ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

nlohmann::json sourceEvidenceJson(const std::vector<GeomPlateSourceEvidence>& evidence)
{
    nlohmann::json result = nlohmann::json::array();
    for (const GeomPlateSourceEvidence& item : evidence) {
        nlohmann::json payload = {
            {"kind", item.kind},
            {"order", item.order},
            {"tol_dist", item.tolDist},
        };
        if (item.kind == "curve3d") {
            payload["object"] = item.objectName;
            payload["subname"] = item.subname;
            payload["stable_subname"] = item.stableSubname;
            payload["nb_pts"] = item.nbPts;
            payload["tol_ang"] = item.tolAng;
            payload["tol_curv"] = item.tolCurv;
        }
        else {
            payload["point"] = item.point;
        }
        result.push_back(std::move(payload));
    }
    return result;
}

std::string failureMessage(const Standard_Failure& failure)
{
    const char* message = failure.GetMessageString();
    return message == nullptr || std::string(message).empty() ? "GeomPlate operation failed" : message;
}

std::optional<double> readErrorValue(const std::function<double()>& reader)
{
    try {
        const double value = reader();
        if (std::isfinite(value)) {
            return value;
        }
    }
    catch (const Standard_Failure&) {
    }
    return std::nullopt;
}

GeomPlateBuildResult errorResult(
    std::string code,
    std::string message,
    int curveConstraintCount,
    int pointConstraintCount,
    std::vector<GeomPlateSourceEvidence> sourceEvidence = {}
)
{
    GeomPlateBuildResult result;
    result.errorCode = std::move(code);
    result.errorMessage = std::move(message);
    result.curveConstraintCount = curveConstraintCount;
    result.pointConstraintCount = pointConstraintCount;
    result.sourceEvidence = std::move(sourceEvidence);
    return result;
}

std::optional<GeomPlateSourceEvidence> addCurveConstraint(
    GeomPlate_BuildPlateSurface& builder,
    const GeomPlateCurveConstraintSource& source,
    std::string& error
)
{
    if (source.shape.IsNull() || source.shape.ShapeType() != TopAbs_EDGE) {
        error = "Curve constraint source must resolve to one 3D edge";
        return std::nullopt;
    }

    TopoDS_Edge edge = TopoDS::Edge(source.shape);
    BRepLib::BuildCurves3d(edge);

    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull()) {
        error = "Curve constraint source has no 3D curve";
        return std::nullopt;
    }

    Handle(Adaptor3d_Curve) adaptor = new GeomAdaptor_Curve(curve, first, last);
    Handle(GeomPlate_CurveConstraint) constraint = new GeomPlate_CurveConstraint(
        adaptor,
        source.order,
        source.nbPts,
        source.tolDist,
        source.tolAng,
        source.tolCurv
    );
    builder.Add(constraint);

    GeomPlateSourceEvidence evidence;
    evidence.kind = "curve3d";
    evidence.objectName = source.objectName;
    evidence.subname = source.subname;
    evidence.stableSubname = source.stableSubname;
    evidence.order = source.order;
    evidence.nbPts = source.nbPts;
    evidence.tolDist = source.tolDist;
    evidence.tolAng = source.tolAng;
    evidence.tolCurv = source.tolCurv;
    return evidence;
}

GeomPlateSourceEvidence addPointConstraint(
    GeomPlate_BuildPlateSurface& builder,
    const GeomPlatePointConstraintSource& source
)
{
    Handle(GeomPlate_PointConstraint) constraint = new GeomPlate_PointConstraint(
        gp_Pnt(source.point[0], source.point[1], source.point[2]),
        source.order,
        source.tolDist
    );
    builder.Add(constraint);

    GeomPlateSourceEvidence evidence;
    evidence.kind = "point3d";
    evidence.point = source.point;
    evidence.order = source.order;
    evidence.tolDist = source.tolDist;
    return evidence;
}

}  // namespace

GeomPlateBuildResult makePartGeomPlateSurface(
    const std::vector<GeomPlateCurveConstraintSource>& curveConstraints,
    const std::vector<GeomPlatePointConstraintSource>& pointConstraints,
    const GeomPlateBuildParams& buildParams,
    const GeomPlateApproximationParams& approximationParams
)
{
    GeomPlateBuildResult result;
    result.curveConstraintCount = static_cast<int>(curveConstraints.size());
    result.pointConstraintCount = static_cast<int>(pointConstraints.size());

    try {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp
        // ::makeSurface(), creates GeomPlate_BuildPlateSurface with Degree=3/Tol2d/Tol3d
        // /TolAng/TolCurv defaults, adds G0 3D curve constraints and point constraints, then
        // calls "aPlateBuilder.Perform()" and checks "aPlateBuilder.IsDone()".
        GeomPlate_BuildPlateSurface builder(
            buildParams.degree,
            buildParams.nbPtsOnCur,
            buildParams.nbIter,
            buildParams.tol2d,
            buildParams.tol3d,
            buildParams.tolAng,
            buildParams.tolCurv,
            buildParams.anisotropy
        );

        for (const GeomPlateCurveConstraintSource& source : curveConstraints) {
            std::string error;
            auto evidence = addCurveConstraint(builder, source, error);
            if (!evidence) {
                return errorResult(
                    "invalid_curve_source",
                    error,
                    result.curveConstraintCount,
                    result.pointConstraintCount,
                    result.sourceEvidence
                );
            }
            result.sourceEvidence.push_back(std::move(*evidence));
        }
        for (const GeomPlatePointConstraintSource& source : pointConstraints) {
            result.sourceEvidence.push_back(addPointConstraint(builder, source));
        }

        try {
            builder.Perform();
        }
        catch (const Standard_Failure& failure) {
            return errorResult(
                "perform_failed",
                failureMessage(failure),
                result.curveConstraintCount,
                result.pointConstraintCount,
                result.sourceEvidence
            );
        }

        result.isDone = builder.IsDone();
        if (!result.isDone) {
            return errorResult(
                "surface_not_done",
                "GeomPlate_BuildPlateSurface did not finish",
                result.curveConstraintCount,
                result.pointConstraintCount,
                result.sourceEvidence
            );
        }

        Handle(GeomPlate_Surface) surface = builder.Surface();
        if (surface.IsNull()) {
            return errorResult(
                "surface_not_done",
                "GeomPlate_BuildPlateSurface returned a null surface",
                result.curveConstraintCount,
                result.pointConstraintCount,
                result.sourceEvidence
            );
        }

        result.surfaceKind = "GeomPlate_Surface";
        result.g0Error = readErrorValue([&]() { return builder.G0Error(); });
        result.g1Error = readErrorValue([&]() { return builder.G1Error(); });
        result.g2Error = readErrorValue([&]() { return builder.G2Error(); });

        try {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp
            // ::PlateSurfacePy::makeApprox(), calls GeomPlate_MakeApprox(...).Surface() and
            // raises "Approximation of B-spline surface failed" when the surface is null.
            GeomPlate_MakeApprox approximation(
                surface,
                approximationParams.tol3d,
                approximationParams.maxSegments,
                approximationParams.maxDegree,
                approximationParams.maxDistance,
                approximationParams.critOrder,
                continuityFromName(approximationParams.continuity),
                approximationParams.enlargeCoeff
            );
            Handle(Geom_BSplineSurface) approximateSurface = approximation.Surface();
            if (approximateSurface.IsNull()) {
                return errorResult(
                    "approximation_failed",
                    "Approximation of B-spline surface failed",
                    result.curveConstraintCount,
                    result.pointConstraintCount,
                    result.sourceEvidence
                );
            }

            Standard_Real u1 = 0.0;
            Standard_Real u2 = 0.0;
            Standard_Real v1 = 0.0;
            Standard_Real v2 = 0.0;
            approximateSurface->Bounds(u1, u2, v1, v2);
            BRepBuilderAPI_MakeFace faceBuilder(approximateSurface, u1, u2, v1, v2, Precision::Confusion());
            if (!faceBuilder.IsDone() || faceBuilder.Face().IsNull()) {
                return errorResult(
                    "approximation_failed",
                    "Approximation surface could not be converted to a face",
                    result.curveConstraintCount,
                    result.pointConstraintCount,
                    result.sourceEvidence
                );
            }

            result.approximationDone = true;
            result.approximationSurfaceKind = "Geom_BSplineSurface";
            result.approxError = readErrorValue([&]() { return approximation.ApproxError(); });
            result.criterionError = readErrorValue([&]() { return approximation.CriterionError(); });
            result.shape = faceBuilder.Face();
            return result;
        }
        catch (const Standard_Failure& failure) {
            return errorResult(
                "approximation_failed",
                failureMessage(failure),
                result.curveConstraintCount,
                result.pointConstraintCount,
                result.sourceEvidence
            );
        }
    }
    catch (const Standard_Failure& failure) {
        return errorResult(
            "perform_failed",
            failureMessage(failure),
            result.curveConstraintCount,
            result.pointConstraintCount,
            result.sourceEvidence
        );
    }
}

void executePartGeomPlateSurface(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate
    // /BuildPlateSurfacePyImp.cpp::BuildPlateSurfacePy::PyInit(), creates the transient
    // "GeomPlate_BuildPlateSurface"; CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()
    // accepts a GeometryCurvePy boundary, and PointConstraintPyImp.cpp::PointConstraintPy::PyInit()
    // accepts a Base::Vector point. cad-core models this as a source-backed helper request type,
    // not as a native FreeCAD Part::GeomPlate DocumentObject.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"CurveConstraints",
             "PointConstraints",
             "Degree",
             "NbPtsOnCur",
             "NbIter",
             "Tol2d",
             "Tol3d",
             "TolAng",
             "TolCurv",
             "Anisotropy",
             "ApproxTol3d",
             "ApproxMaxSegments",
             "ApproxMaxDegree",
             "ApproxMaxDistance",
             "ApproxCritOrder",
             "ApproxContinuity",
             "ApproxEnlargeCoeff"}
        )) {
        context.objects[object.name] = errorObject();
        return;
    }

    GeomPlateBuildParams buildParams;
    GeomPlateApproximationParams approximationParams;
    if (!readBuildParams(object, context, buildParams)
        || !readApproximationParams(object, context, approximationParams)) {
        return;
    }

    const auto curveConstraints = readCurveConstraints(object, context);
    if (!curveConstraints) {
        return;
    }
    const auto pointConstraints = readPointConstraints(object, context);
    if (!pointConstraints) {
        return;
    }
    if (curveConstraints->empty() && pointConstraints->empty()) {
        addGeomPlateDiagnostic(
            object,
            context,
            "missing_constraints",
            "Part.GeomPlate.BuildPlateSurface requires curve or point constraints",
            "CurveConstraints"
        );
        return;
    }

    const GeomPlateBuildResult build = makePartGeomPlateSurface(
        *curveConstraints,
        *pointConstraints,
        buildParams,
        approximationParams
    );
    if (!build.errorCode.empty() || build.shape.IsNull()) {
        addGeomPlateDiagnostic(
            object,
            context,
            build.errorCode.empty() ? "approximation_failed" : build.errorCode,
            build.errorMessage.empty() ? "Part.GeomPlate.BuildPlateSurface failed" : build.errorMessage,
            build.errorCode == "invalid_curve_source" ? "CurveConstraints" : "PointConstraints"
        );
        return;
    }

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        {
            {"feature", kFeatureName},
            {"helper", kHelperName},
            {"dto", "PartGeomPlateSurfaceDTO"},
            {"source_backed_helper", true},
            {"freecad_native_document_object", false},
            {"is_done", build.isDone},
            {"surface_kind", build.surfaceKind},
            {"curve_constraint_count", build.curveConstraintCount},
            {"point_constraint_count", build.pointConstraintCount},
            {"g0_error", optionalErrorJson(build.g0Error)},
            {"g1_error", optionalErrorJson(build.g1Error)},
            {"g2_error", optionalErrorJson(build.g2Error)},
            {"build_params", buildParamsJson(buildParams)},
            {"approximation", approximationParamsJson(approximationParams, build)},
            {"source_evidence", sourceEvidenceJson(build.sourceEvidence)},
        }
    );
}

}  // namespace cad_core::part
