#include "cad_core/part/part_feature.h"
#include "cad_core/part/part_extrusion.h"

#include "part_feature_support.h"

#include "cad_core/part/extrusion_helper.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepLib_FindSurface.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part
{

std::optional<PartLinearExtrusionResult> buildLinearExtrusionFromProfile(
    const std::string& owner,
    const std::string& sourceOwner,
    const TopoDS_Shape& profile,
    const PartLinearExtrusionOptions& options,
    const part::NamedShape* sourceNamedShape,
    std::string& error)
{
    error.clear();
    if (profile.IsNull()) {
        error = "Linear extrusion profile is empty";
        return std::nullopt;
    }

    const bool hasForwardLength = std::abs(options.lengthFwd) > Precision::Confusion();
    const bool hasReverseLength = std::abs(options.lengthRev) > Precision::Confusion();
    const bool hasActiveTaperFwd = std::abs(options.taperAngleFwdRadians) > Precision::Angular()
        && hasForwardLength;
    const bool hasActiveTaperRev = std::abs(options.taperAngleRevRadians) > Precision::Angular()
        && hasReverseLength;
    if (hasActiveTaperFwd || hasActiveTaperRev) {
        auto tapered = part::makeTaperedExtrusion(
            profile,
            part::TaperedExtrusionOptions {
                options.direction,
                options.lengthFwd,
                options.taperAngleFwdRadians,
                options.solid,
                options.lengthRev,
                options.taperAngleRevRadians,
            },
            error
        );
        if (!tapered || tapered->shape.IsNull()) {
            if (error.empty()) {
                error = "Could not build tapered linear extrusion";
            }
            return std::nullopt;
        }
        const part::NamedShapeSource profileSource {
            sourceOwner,
            profile,
            sourceNamedShape
        };
        auto namedShape = part::namedShapeForTaperedExtrusionHistory(
            owner,
            *tapered,
            profile,
            profileSource
        );
        return PartLinearExtrusionResult {
            tapered->shape,
            tapered->topoNamingKnownGap,
            !tapered->topoNamingKnownGap,
            std::move(namedShape)
        };
    }

    TopoDS_Shape sourceShape = profile;
    std::optional<part::NamedShape> movedNamedShape;
    const part::NamedShape* currentNamedShape = sourceNamedShape;
    if (hasReverseLength) {
        gp_Trsf reverseTransform;
        reverseTransform.SetTranslation(gp_Vec(options.direction) * (-options.lengthRev));
        BRepBuilderAPI_Transform transform(sourceShape, reverseTransform, Standard_True);
        if (!transform.IsDone()) {
            error = "Could not translate the reverse-length source shape";
            return std::nullopt;
        }
        const TopoDS_Shape previousSourceShape = sourceShape;
        sourceShape = transform.Shape();
        if (sourceNamedShape != nullptr) {
            movedNamedShape = part::namedShapeForTransformedCopy(
                sourceOwner,
                sourceShape,
                part::NamedShapeSource {sourceOwner, previousSourceShape, sourceNamedShape}
            );
            currentNamedShape = &*movedNamedShape;
        }
    }

    try {
        BRepPrimAPI_MakePrism prism(
            sourceShape,
            gp_Vec(options.direction) * (options.lengthFwd + options.lengthRev),
            Standard_True
        );
        prism.Build();
        if (!prism.IsDone() || prism.Shape().IsNull()) {
            error = "Could not extrude the profile shape";
            return std::nullopt;
        }
        const part::NamedShapeSource profileSource {
            sourceOwner,
            sourceShape,
            currentNamedShape
        };
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementPrism(), "return makeElementShape(mkPrism, base, op)".
        auto namedShape = part::namedShapeForMakerHistory(
            owner,
            prism.Shape(),
            std::vector<part::NamedShapeSource> {profileSource},
            prism
        );
        return PartLinearExtrusionResult {prism.Shape(), false, false, std::move(namedShape)};
    }
    catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        return std::nullopt;
    }
}

namespace
{

using part_feature_detail::publishPartShape;
using part_feature_detail::radians;
using part_feature_detail::readNumberProperty;

struct PartExtrusionDirection
{
    gp_Dir direction;
    double magnitude = 1.0;
};
struct PartExtrusionSource
{
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};
struct PartExtrusionShapeBuild
{
    TopoDS_Shape shape;
    bool topoNamingKnownGap = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
    // FeatureExtrusion.cpp::Extrusion::extrudeShape() calls
    // "ExtrusionHelper::makeElementDraft" for tapered extrusion.
    bool taperHistory = false;
    std::optional<part::NamedShape> namedShape;
};
enum class PartExtrusionFaceMaker
{
    Simple,
    Cheese,
    Extrusion,
    Bullseye
};

std::string readEnumStringProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::string& fallback
)
{
    if (const auto stringValue = app::readString(object, property)) {
        return *stringValue;
    }
    if (const auto numberValue = app::readNumber(object, property)) {
        const auto index = static_cast<int>(std::llround(*numberValue));
        if (property == "DirMode") {
            switch (index) {
                case 0:
                    return "Custom";
                case 1:
                    return "Edge";
                case 2:
                    return "Normal";
                default:
                    return fallback;
            }
        }
        if (property == "FaceMakerMode") {
            switch (index) {
                case 0:
                    return "Simple";
                case 1:
                    return "Cheese";
                case 2:
                    return "Extrusion";
                case 3:
                    return "Bullseye";
                default:
                    return fallback;
            }
        }
    }
    return fallback;
}

void addPartExtrusionDiagnostic(
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
    context.objects[object.name] = {{"status", "error"}};
}

std::optional<PartExtrusionFaceMaker> faceMakerClassToMode(const std::string& faceMakerClass)
{
    if (faceMakerClass == "Part::FaceMakerSimple") {
        return PartExtrusionFaceMaker::Simple;
    }
    if (faceMakerClass == "Part::FaceMakerCheese") {
        return PartExtrusionFaceMaker::Cheese;
    }
    if (faceMakerClass == "Part::FaceMakerExtrusion") {
        return PartExtrusionFaceMaker::Extrusion;
    }
    if (faceMakerClass == "Part::FaceMakerBullseye") {
        return PartExtrusionFaceMaker::Bullseye;
    }
    return std::nullopt;
}

std::string faceMakerModeToClass(const std::string& faceMakerMode)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
    // ::enumToClass(), maps "Simple", "Cheese", "Extrusion", "Bullseye" to the concrete
    // "Part::FaceMaker*" class stored in FaceMakerClass.
    if (faceMakerMode == "Simple") {
        return "Part::FaceMakerSimple";
    }
    if (faceMakerMode == "Cheese") {
        return "Part::FaceMakerCheese";
    }
    if (faceMakerMode == "Extrusion") {
        return "Part::FaceMakerExtrusion";
    }
    return "Part::FaceMakerBullseye";
}

std::optional<PartExtrusionFaceMaker> partExtrusionFaceMaker(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    std::string faceMakerClass = app::readString(object, "FaceMakerClass").value_or("");
    if (faceMakerClass.empty() && app::propertyValue(object, "FaceMakerMode") != nullptr) {
        faceMakerClass = faceMakerModeToClass(
            readEnumStringProperty(object, "FaceMakerMode", "Bullseye")
        );
    }
    if (faceMakerClass.empty()) {
        faceMakerClass = "Part::FaceMakerBullseye";
    }

    const auto mode = faceMakerClassToMode(faceMakerClass);
    if (!mode) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Extrusion FaceMakerClass is not supported",
            "FaceMakerClass"
        );
        return std::nullopt;
    }
    return *mode;
}

std::optional<TopoDS_Wire> partExtrusionWireFromEdges(const TopoDS_Shape& shape)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    bool hasEdge = false;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        wireBuilder.Add(TopoDS::Edge(explorer.Current()));
        hasEdge = true;
    }
    if (!hasEdge || !wireBuilder.IsDone()) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<TopoDS_Shape> partExtrusionSolidSourceFromWires(
    const TopoDS_Shape& source,
    PartExtrusionFaceMaker faceMaker
)
{
    std::vector<TopoDS_Wire> wires;
    for (TopExp_Explorer explorer(source, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        wires.push_back(TopoDS::Wire(explorer.Current()));
    }
    if (wires.empty()) {
        auto wire = partExtrusionWireFromEdges(source);
        if (wire) {
            wires.push_back(*wire);
        }
    }
    if (wires.empty()) {
        return std::nullopt;
    }

    switch (faceMaker) {
        case PartExtrusionFaceMaker::Simple:
            return part::makeSeparateFacesFromClosedWires(wires);
        case PartExtrusionFaceMaker::Cheese:
        case PartExtrusionFaceMaker::Extrusion:
            return part::makeCheeseFaceFromClosedWires(wires);
        case PartExtrusionFaceMaker::Bullseye:
            return part::makeFaceWithHolesFromClosedWires(wires);
    }
    return std::nullopt;
}

std::optional<PartExtrusionSource> partExtrusionSourceShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& baseObjectName,
    const TopoDS_Shape& source,
    const part::NamedShape* sourceNamedShape,
    bool solid,
    PartExtrusionFaceMaker faceMaker
)
{
    std::optional<part::NamedShape> copiedSourceNamedShape;
    if (sourceNamedShape != nullptr) {
        copiedSourceNamedShape = *sourceNamedShape;
    }
    if (!solid) {
        return PartExtrusionSource {source, copiedSourceNamedShape};
    }
    for (TopExp_Explorer explorer(source, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return PartExtrusionSource {source, copiedSourceNamedShape};
    }

    auto faceShape = partExtrusionSolidSourceFromWires(source, faceMaker);
    if (!faceShape || faceShape->IsNull()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "execution_failed",
            "Part::Extrusion Solid=true could not create a face from the linked Base shape",
            "Solid"
        );
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
    // ::Extrusion::extrudeShape(), for Solid=true wires calls "myShape.makeElementFace(...)"
    // before "result.makeElementPrism(myShape, vec)". cad-core preserves the source-edge subset
    // here; full FaceMaker history remains owned by the P5/P6 FaceMaker migration.
    const part::NamedShapeSource baseSource {baseObjectName, source, sourceNamedShape};
    return PartExtrusionSource {
        *faceShape,
        part::namedShapeForPreservedSources(baseObjectName, *faceShape, {baseSource})
    };
}

std::optional<PartExtrusionDirection> partExtrusionDirectionFromEdge(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto axisLink = app::readLink(object, "DirLink");
    if (!axisLink || axisLink->object.empty()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "missing_property",
            "Part::Extrusion DirMode=Edge requires DirLink",
            "DirLink"
        );
        return std::nullopt;
    }

    const auto axisIt = context.shapes.find(axisLink->object);
    if (axisIt == context.shapes.end() || axisIt->second.shape.IsNull()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::Extrusion DirLink target " + axisLink->object + " did not produce a shape",
            "DirLink",
            axisLink->object
        );
        return std::nullopt;
    }

    TopoDS_Shape axisShape = axisIt->second.shape;
    if (!axisLink->subnames.empty()) {
        if (axisLink->subnames.size() != 1U || axisLink->subnames.front().empty()) {
            addPartExtrusionDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Part::Extrusion DirLink must select exactly one edge subshape",
                "DirLink",
                axisLink->object
            );
            return std::nullopt;
        }
        const auto subshape = part::subshapeByName(axisShape, axisLink->subnames.front());
        if (!subshape || subshape->IsNull()) {
            addPartExtrusionDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Part::Extrusion DirLink target " + axisLink->object + " has no subshape "
                    + axisLink->subnames.front(),
                "DirLink",
                axisLink->object
            );
            return std::nullopt;
        }
        axisShape = *subshape;
    }

    if (axisShape.ShapeType() != TopAbs_EDGE) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "unsupported_subshape_kind",
            "Part::Extrusion DirLink shape must be an edge",
            "DirLink",
            axisLink->object
        );
        return std::nullopt;
    }

    const TopoDS_Edge edge = TopoDS::Edge(axisShape);
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "unsupported_subshape_kind",
            "Part::Extrusion DirLink edge must be a line",
            "DirLink",
            axisLink->object
        );
        return std::nullopt;
    }

    gp_Pnt start = curve.Value(curve.FirstParameter());
    gp_Pnt end = curve.Value(curve.LastParameter());
    if (edge.Orientation() == TopAbs_REVERSED) {
        std::swap(start, end);
    }
    const gp_Vec vector(start, end);
    if (vector.Magnitude() < Precision::Confusion()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "invalid_direction",
            "Part::Extrusion DirLink edge must not be zero-length",
            "DirLink",
            axisLink->object
        );
        return std::nullopt;
    }
    return PartExtrusionDirection {gp_Dir(vector), vector.Magnitude()};
}

std::optional<gp_Dir> partExtrusionNormalForShape(
    const std::string& baseObjectName,
    const runtime::ShapeValue& baseShape,
    const runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
    // ::Extrusion::calculateShapeNormal(), for Part2DObject "use their local Z axis"; otherwise it
    // finds a plane and, when faces exist, uses face orientation to choose the normal direction.
    if (baseShape.kind == runtime::ShapeValue::Kind::Sketch) {
        gp_Dir normal(0.0, 0.0, 1.0);
        const auto placementIt = context.globalPlacements.find(baseObjectName);
        if (placementIt != context.globalPlacements.end()) {
            normal.Transform(placementIt->second);
        }
        return normal;
    }

    for (TopExp_Explorer explorer(baseShape.shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            return std::nullopt;
        }
        gp_Dir normal = surface.Plane().Axis().Direction();
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }
        return normal;
    }

    BRepLib_FindSurface planeFinder(baseShape.shape, -1, Standard_True);
    if (!planeFinder.Found()) {
        return std::nullopt;
    }
    GeomAdaptor_Surface surface(planeFinder.Surface());
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    return surface.Plane().Axis().Direction();
}

std::optional<PartExtrusionDirection> partExtrusionDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& baseObjectName,
    const runtime::ShapeValue& baseShape
)
{
    const std::string dirMode = readEnumStringProperty(object, "DirMode", "Custom");
    std::optional<PartExtrusionDirection> result;
    if (dirMode == "Custom") {
        const auto rawDirection
            = app::readVector3(object, "Dir").value_or(std::array<double, 3> {0.0, 0.0, 1.0});
        const gp_Vec vector(rawDirection[0], rawDirection[1], rawDirection[2]);
        if (vector.Magnitude() < Precision::Confusion()) {
            addPartExtrusionDiagnostic(
                object,
                context,
                "invalid_direction",
                "Part::Extrusion direction must not be zero-length",
                "Dir"
            );
            return std::nullopt;
        }
        result = PartExtrusionDirection {gp_Dir(vector), vector.Magnitude()};
    }
    else if (dirMode == "Edge") {
        result = partExtrusionDirectionFromEdge(object, context);
    }
    else if (dirMode == "Normal") {
        const auto normal = partExtrusionNormalForShape(baseObjectName, baseShape, context);
        if (!normal) {
            addPartExtrusionDiagnostic(
                object,
                context,
                "invalid_direction",
                "Part::Extrusion DirMode=Normal could not find a planar Base normal",
                "DirMode",
                baseObjectName
            );
            return std::nullopt;
        }
        result = PartExtrusionDirection {*normal, 1.0};
    }
    else {
        addPartExtrusionDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Extrusion DirMode is not supported",
            "DirMode"
        );
        return std::nullopt;
    }

    if (!result) {
        return std::nullopt;
    }
    if (app::readBool(object, "Reversed").value_or(false)) {
        result->direction.Reverse();
    }
    return result;
}

std::optional<PartExtrusionShapeBuild> makePartExtrusionShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& baseObjectName,
    const TopoDS_Shape& baseShape,
    const part::NamedShape* baseNamedShape,
    const gp_Dir& direction,
    double lengthFwd,
    double lengthRev,
    double taperAngleFwdDegrees,
    double taperAngleRevDegrees,
    bool solid,
    PartExtrusionFaceMaker faceMaker
)
{
    auto source = partExtrusionSourceShape(
        object,
        context,
        baseObjectName,
        baseShape,
        baseNamedShape,
        solid,
        faceMaker
    );
    if (!source) {
        return std::nullopt;
    }

    std::string error;
    auto shape = part::buildLinearExtrusionFromProfile(
        object.name,
        baseObjectName,
        source->shape,
        part::PartLinearExtrusionOptions {
            direction,
            lengthFwd,
            lengthRev,
            radians(taperAngleFwdDegrees),
            radians(taperAngleRevDegrees),
            solid,
        },
        source->namedShape ? &*source->namedShape : nullptr,
        error
    );
    if (!shape) {
        const bool hasForwardTaper = std::abs(taperAngleFwdDegrees) > Precision::Angular();
        const bool hasReverseTaper = std::abs(taperAngleRevDegrees) > Precision::Angular();
        addPartExtrusionDiagnostic(
            object,
            context,
            hasForwardTaper || hasReverseTaper ? "invalid_taper" : "execution_failed",
            error.empty() ? "Part::Extrusion could not extrude the linked Base shape" : error,
            hasReverseTaper && !hasForwardTaper ? "TaperAngleRev" : "Base"
        );
        return std::nullopt;
    }
    return PartExtrusionShapeBuild {
        shape->shape,
        shape->topoNamingKnownGap,
        shape->taperHistory,
        std::move(shape->namedShape)
    };
}

}  // namespace

void executePartExtrusion(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
    // ::Extrusion::execute(), links "Base", computes final Dir/LengthFwd/LengthRev/Solid flags,
    // optionally makes faces from wires when "Solid" is true, then extrudes the whole Base shape.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base",
             "Dir",
             "DirMode",
             "DirLink",
             "LengthFwd",
             "LengthRev",
             "Solid",
             "Reversed",
             "Symmetric",
             "TaperAngle",
             "TaperAngleRev",
             "FaceMakerClass",
             "FaceMakerMode",
             "InnerWireTaper"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto baseLink = app::readLink(object, "Base");
    if (!baseLink || baseLink->object.empty()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "missing_property",
            "Part::Extrusion Base must link to an object",
            "Base"
        );
        return;
    }

    const auto baseIt = context.shapes.find(baseLink->object);
    if (baseIt == context.shapes.end() || baseIt->second.shape.IsNull()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::Extrusion Base target " + baseLink->object + " did not produce a shape",
            "Base",
            baseLink->object
        );
        return;
    }
    const auto baseNamedShapeIt = context.namedShapes.find(baseLink->object);
    const part::NamedShape* baseNamedShape = baseNamedShapeIt != context.namedShapes.end()
        ? &baseNamedShapeIt->second
        : nullptr;
    const auto faceMaker = partExtrusionFaceMaker(object, context);
    if (!faceMaker) {
        return;
    }

    const double taperAngle = readNumberProperty(object, "TaperAngle", 0.0);
    const double taperAngleRev = readNumberProperty(object, "TaperAngleRev", 0.0);
    constexpr double maxTaperDegrees = 90.0;
    if (std::abs(taperAngle) >= maxTaperDegrees || std::abs(taperAngleRev) >= maxTaperDegrees) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "invalid_taper",
            "Part::Extrusion taper angle must be smaller than 90 degrees",
            std::abs(taperAngle) >= maxTaperDegrees ? "TaperAngle" : "TaperAngleRev"
        );
        return;
    }

    auto direction = partExtrusionDirection(object, context, baseLink->object, baseIt->second);
    if (!direction) {
        return;
    }

    double lengthFwd = readNumberProperty(object, "LengthFwd", 0.0);
    double lengthRev = readNumberProperty(object, "LengthRev", 0.0);
    if (std::abs(lengthFwd) < Precision::Confusion() && std::abs(lengthRev) < Precision::Confusion()) {
        lengthFwd = direction->magnitude;
    }
    if (app::readBool(object, "Symmetric").value_or(false)) {
        lengthRev = lengthFwd * 0.5;
        lengthFwd = lengthFwd * 0.5;
    }
    if (std::abs(lengthFwd + lengthRev) < Precision::Confusion()) {
        addPartExtrusionDiagnostic(
            object,
            context,
            "invalid_length",
            "Part::Extrusion total length must not be zero",
            "LengthFwd"
        );
        return;
    }

    const bool solid = app::readBool(object, "Solid").value_or(false);
    auto shape = makePartExtrusionShape(
        object,
        context,
        baseLink->object,
        baseIt->second.shape,
        baseNamedShape,
        direction->direction,
        lengthFwd,
        lengthRev,
        taperAngle,
        taperAngleRev,
        solid,
        *faceMaker
    );
    if (!shape) {
        return;
    }

    nlohmann::json metadata = {
        {"feature", "part_extrusion"},
        {"source_base", baseLink->object},
        {"solid", solid},
        {"length_fwd", lengthFwd},
        {"length_rev", lengthRev},
        {"reversed", app::readBool(object, "Reversed").value_or(false)},
        {"symmetric", app::readBool(object, "Symmetric").value_or(false)}
    };
    const auto sourceMetadataIt = context.objects.find(baseLink->object);
    if (sourceMetadataIt != context.objects.end()) {
        const auto& sourceMetadata = sourceMetadataIt->second;
        constexpr std::array<std::pair<const char*, const char*>, 5> sourceMetadataKeys {{
            {"feature", "source_feature"},
            {"dto", "source_dto"},
            {"curve_kind", "source_curve_kind"},
            {"curve_type", "source_curve_type"},
            {"part_geometry_type", "source_part_geometry_type"},
        }};
        for (const auto& [sourceKey, targetKey] : sourceMetadataKeys) {
            const auto item = sourceMetadata.find(sourceKey);
            if (item != sourceMetadata.end()) {
                metadata[targetKey] = *item;
            }
        }
    }
    if (shape->taperHistory) {
        metadata["topo_naming_history"] = "maker_history:taper_thru_sections";
    }
    else if (shape->topoNamingKnownGap) {
        metadata["topo_naming_history"] = "history_partial:taper_thru_sections";
    }

    publishPartShape(object, context, shape->shape, metadata, shape->namedShape);
}

}  // namespace cad_core::part
