#pragma once

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::geometry {

struct TaperedExtrusionOptions {
    gp_Dir direction;
    double length = 0.0;
    double taperAngleRadians = 0.0;
    bool solid = true;
};

struct TaperedExtrusionHistoryComponent {
    TopoDS_Shape shape;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
    // ::ExtrusionHelper::makeElementDraft(), records each loft through TopoShape::makeElementShape().
    std::shared_ptr<BRepBuilderAPI_MakeShape> historyMaker;
    std::vector<TopoDS_Shape> historySources;
};

struct TaperedExtrusionResult {
    TopoDS_Shape shape;
    bool topoNamingKnownGap = true;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
    // ::ExtrusionHelper::makeElementDraft(), calls TopoShape::makeElementShape(mkGenerator,
    // list_of_sections) after BRepOffsetAPI_ThruSections::Build().
    std::shared_ptr<BRepBuilderAPI_MakeShape> historyMaker;
    std::vector<TopoDS_Shape> historySources;
    std::vector<TaperedExtrusionHistoryComponent> historyComponents;
};

std::optional<TaperedExtrusionResult> makeTaperedExtrusion(const TopoDS_Shape& profile,
                                                           const TaperedExtrusionOptions& options,
                                                           std::string& error);

}  // namespace cad_core::geometry
