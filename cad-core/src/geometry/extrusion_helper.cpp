#include "cad_core/geometry/extrusion_helper.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <Precision.hxx>
#include <ShapeFix_Wire.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <memory>
#include <vector>

namespace cad_core::geometry {

namespace {

struct TaperedWireBuild {
    TopoDS_Shape shape;
    std::shared_ptr<BRepBuilderAPI_MakeShape> historyMaker;
    std::vector<TopoDS_Shape> historySources;
};

TopoDS_Wire fixedWire(const TopoDS_Wire& wire)
{
    ShapeFix_Wire fixer;
    fixer.Load(wire);
    fixer.FixReorder();
    fixer.FixConnected();
    fixer.FixClosed();
    return fixer.Wire();
}

std::optional<TopoDS_Wire> wireFromOffsetShape(const TopoDS_Shape& shape, std::string& error)
{
    if (shape.IsNull()) {
        error = "Extrusion: end face of tapered extrusion is empty";
        return std::nullopt;
    }
    if (shape.ShapeType() == TopAbs_WIRE) {
        return TopoDS::Wire(shape);
    }
    if (shape.ShapeType() == TopAbs_EDGE) {
        BRepBuilderAPI_MakeWire wireBuilder(TopoDS::Edge(shape));
        if (!wireBuilder.IsDone()) {
            error = "Extrusion: offset edge could not be converted to a wire";
            return std::nullopt;
        }
        return wireBuilder.Wire();
    }
    if (shape.ShapeType() == TopAbs_COMPOUND) {
        std::optional<TopoDS_Wire> result;
        for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
            if (result) {
                error = "Extrusion: tapered offset produced multiple wires";
                return std::nullopt;
            }
            result = TopoDS::Wire(explorer.Current());
        }
        if (result) {
            return result;
        }
    }

    error = "Extrusion: type of tapered extrusion end face is not supported";
    return std::nullopt;
}

std::optional<TopoDS_Wire> createTaperedPrismOffset(const TopoDS_Wire& sourceWire,
                                                    const gp_Vec& translation,
                                                    double offset,
                                                    std::string& error)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
    // ::ExtrusionHelper::createTaperedPrismOffset(), translates the source wire, then runs
    // BRepOffsetAPI_MakeOffsetFix with "GeomAbs_Intersection" before lofting sections.
    gp_Trsf transform;
    transform.SetTranslation(translation);
    const TopLoc_Location location(transform);
    TopoDS_Wire movedSourceWire = TopoDS::Wire(sourceWire.Moved(location));

    if (std::abs(offset) <= Precision::Confusion()) {
        return movedSourceWire;
    }

    BRepOffsetAPI_MakeOffset offsetMaker;
    offsetMaker.Init(GeomAbs_Intersection);
    offsetMaker.AddWire(movedSourceWire);
    try {
        offsetMaker.Perform(offset);
    }
    catch (const Standard_Failure& failure) {
        error = std::string("Extrusion: Offset could not be created: ") + failure.GetMessageString();
        return std::nullopt;
    }
    if (!offsetMaker.IsDone()) {
        error = "Extrusion: Offset could not be created";
        return std::nullopt;
    }

    return wireFromOffsetShape(offsetMaker.Shape(), error);
}

std::optional<TaperedWireBuild> makeTaperedWireExtrusion(const TopoDS_Wire& sourceWire,
                                                        const TaperedExtrusionOptions& options,
                                                        std::string& error)
{
    std::vector<TopoDS_Wire> sections;
    const TopoDS_Wire source = fixedWire(sourceWire);
    const bool hasForward = std::abs(options.length) > Precision::Confusion();
    const bool hasReverse = std::abs(options.reverseLength) > Precision::Confusion();
    const bool includeSource = !hasForward || !hasReverse || options.length * options.reverseLength > 0.0;

    if (hasReverse) {
        const double offset = std::tan(options.reverseTaperAngleRadians) * options.reverseLength;
        const gp_Vec translation = gp_Vec(options.direction.Reversed()) * options.reverseLength;
        auto reverseWire = createTaperedPrismOffset(source, translation, offset, error);
        if (!reverseWire) {
            return std::nullopt;
        }
        sections.push_back(fixedWire(*reverseWire));
    }

    if (includeSource) {
        sections.push_back(source);
    }

    if (hasForward) {
        const double offset = std::tan(options.taperAngleRadians) * options.length;
        const gp_Vec translation = gp_Vec(options.direction) * options.length;
        auto forwardWire = createTaperedPrismOffset(source, translation, offset, error);
        if (!forwardWire) {
            return std::nullopt;
        }
        sections.push_back(fixedWire(*forwardWire));
    }

    if (sections.size() < 2U) {
        error = "Extrusion: drafted length must not be zero";
        return std::nullopt;
    }

    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
        // ::ExtrusionHelper::makeElementDraft(), assembles reverse/source/forward wire sections
        // and uses BRepOffsetAPI_ThruSections with ruled=true.
        auto loft = std::make_shared<BRepOffsetAPI_ThruSections>(options.solid ? Standard_True : Standard_False,
                                                                 Standard_True,
                                                                 Precision::Confusion());
        for (const auto& section : sections) {
            loft->AddWire(section);
        }
        loft->Build();
        if (!loft->IsDone()) {
            error = "Extrusion: Loft could not be built";
            return std::nullopt;
        }
        std::vector<TopoDS_Shape> historySources;
        historySources.reserve(sections.size());
        for (const TopoDS_Wire& section : sections) {
            historySources.push_back(TopoDS_Shape(section));
        }
        return TaperedWireBuild{loft->Shape(), loft, historySources};
    }
    catch (const Standard_Failure& failure) {
        error = std::string("Extrusion: Loft could not be built: ") + failure.GetMessageString();
        return std::nullopt;
    }
}

std::optional<TaperedExtrusionResult> makeTaperedFaceExtrusion(const TopoDS_Face& face,
                                                               const TaperedExtrusionOptions& options,
                                                               std::string& error)
{
    const TopoDS_Wire outerWire = BRepTools::OuterWire(face);
    if (outerWire.IsNull()) {
        error = "Extrusion: Missing outer wire";
        return std::nullopt;
    }

    auto outer = makeTaperedWireExtrusion(outerWire, options, error);
    if (!outer) {
        return std::nullopt;
    }

    TopoDS_Shape result = outer->shape;
    std::vector<TaperedExtrusionHistoryComponent> components{
        TaperedExtrusionHistoryComponent{outer->shape, outer->historyMaker, outer->historySources},
    };
    bool hasInnerWire = false;
    for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
        if (wire.IsSame(outerWire)) {
            continue;
        }
        hasInnerWire = true;

        auto inner = makeTaperedWireExtrusion(wire, options, error);
        if (!inner) {
            return std::nullopt;
        }
        components.push_back(TaperedExtrusionHistoryComponent{inner->shape, inner->historyMaker, inner->historySources});
        BRepAlgoAPI_Cut cut(result, inner->shape);
        cut.Build();
        if (!cut.IsDone()) {
            error = "Extrusion: Final cut out failed";
            return std::nullopt;
        }
        result = cut.Shape();
    }

    if (!hasInnerWire) {
        return TaperedExtrusionResult{result, true, outer->historyMaker, outer->historySources, components};
    }

    return TaperedExtrusionResult{result, true, nullptr, {}, components};
}

}  // namespace

std::optional<TaperedExtrusionResult> makeTaperedExtrusion(const TopoDS_Shape& profile,
                                                           const TaperedExtrusionOptions& options,
                                                           std::string& error)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
    // ::ExtrusionHelper::makeElementDraft(), supports face and wire profile drafting and leaves
    // topo history to TopoShape::makeElementShape(...). cad-core returns a known topo gap marker.
    constexpr double halfPi = 1.57079632679489661923;
    if (profile.IsNull()) {
        error = "Not a valid shape";
        return std::nullopt;
    }
    if (std::abs(options.length) <= Precision::Confusion()
        && std::abs(options.reverseLength) <= Precision::Confusion()) {
        error = "Extrusion: drafted length must not be zero";
        return std::nullopt;
    }
    if (std::abs(options.taperAngleRadians) >= halfPi - Precision::Angular()
        || std::abs(options.reverseTaperAngleRadians) >= halfPi - Precision::Angular()) {
        error = "Extrusion: taper angle must be smaller than 90 degrees";
        return std::nullopt;
    }

    if (profile.ShapeType() == TopAbs_FACE) {
        auto result = makeTaperedFaceExtrusion(TopoDS::Face(profile), options, error);
        if (!result) {
            return std::nullopt;
        }
        return result;
    }
    if (profile.ShapeType() == TopAbs_WIRE) {
        auto result = makeTaperedWireExtrusion(TopoDS::Wire(profile), options, error);
        if (!result) {
            return std::nullopt;
        }
        return TaperedExtrusionResult{
            result->shape,
            true,
            result->historyMaker,
            result->historySources,
            {TaperedExtrusionHistoryComponent{result->shape, result->historyMaker, result->historySources}},
        };
    }

    error = "Only a wire or a face is supported";
    return std::nullopt;
}

}  // namespace cad_core::geometry
