#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <ShapeBuild_ReShape.hxx>
#include <ShapeFix_Wireframe.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListOfShape.hxx>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

gp_Pnt pointFromJson(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() != 3U) {
        throw std::runtime_error("point must be [x, y, z]");
    }
    return gp_Pnt(value.at(0).get<double>(), value.at(1).get<double>(), value.at(2).get<double>());
}

TopoDS_Shape polygonShapeFromFixture(const nlohmann::json& fixture)
{
    const auto& shape = fixture.at("shape");
    const double sourceEdgeTolerance = shape.value("source_edge_tolerance", 0.0);
    std::vector<gp_Pnt> polygonPoints;
    BRepBuilderAPI_MakeWire wire;

    const auto& points = shape.at("points");
    polygonPoints.reserve(points.size());
    for (const auto& point : points) {
        polygonPoints.push_back(pointFromJson(point));
    }
    if (polygonPoints.size() < 2U) {
        throw std::runtime_error("polygon fixture must have at least two points");
    }
    for (std::size_t index = 0; index < polygonPoints.size(); ++index) {
        const gp_Pnt& start = polygonPoints.at(index);
        const gp_Pnt& end = polygonPoints.at((index + 1U) % polygonPoints.size());
        BRepBuilderAPI_MakeEdge edge(start, end);
        if (!edge.IsDone()) {
            throw std::runtime_error("could not build polygon edge");
        }
        TopoDS_Edge builtEdge = edge.Edge();
        if (sourceEdgeTolerance > 0.0) {
            BRep_Builder builder;
            builder.UpdateEdge(builtEdge, sourceEdgeTolerance);
        }
        wire.Add(builtEdge);
    }
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build polygon wire");
    }

    if (shape.value("kind", "planar_face") == "wire") {
        return wire.Wire();
    }

    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone()) {
        throw std::runtime_error("could not build planar face");
    }
    return face.Shape();
}

TopoDS_Shape boxFromFixture(const nlohmann::json& fixture)
{
    const auto& shape = fixture.at("shape");
    BRepPrimAPI_MakeBox box(
        shape.value("length", 2.0),
        shape.value("width", 3.0),
        shape.value("height", 4.0)
    );
    return box.Shape();
}

nlohmann::json runShapeFixDeleteSmallEdge(const nlohmann::json& fixture)
{
    const TopoDS_Shape sourceShape = polygonShapeFromFixture(fixture);
    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("Source", sourceShape);
    const cad_core::part::NamedShapeSource source {
        "Source",
        sourceShape,
        &sourceNamedShape,
    };
    const cad_core::part::NamedShapeBuild fixed = cad_core::part::makeElementShapeFixFromSource(
        "ShapeFix",
        source,
        fixture.at("shape").value("shape_fix_precision", 0.0),
        fixture.at("shape").value("small_edge_tolerance", 0.0)
    );
    if (!fixed.error.empty()) {
        throw std::runtime_error(fixed.error);
    }
    if (!fixed.namedShape) {
        throw std::runtime_error("ShapeFix did not return NamedShape history");
    }
    return {
        {"case", fixture.value("case", "shapefix-delete-small-edge")},
        {"objects", {{"ShapeFix", {{"status", "ok"}, {"shape", "shape_fix"}}}}},
        {"named_shapes", {{"ShapeFix", cad_core::part::namedShapeToJson(*fixed.namedShape)}}},
    };
}

nlohmann::json runElementMapPolicyDrop(const nlohmann::json& fixture)
{
    const TopoDS_Shape sourceShape = boxFromFixture(fixture);
    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("Source", sourceShape);
    const cad_core::part::NamedShapeSource source {
        "Source",
        sourceShape,
        &sourceNamedShape,
    };
    const cad_core::part::NamedShape dropped
        = cad_core::part::namedShapeForElementMapPolicyDrop("DropResult", sourceShape, {source});
    return {
        {"case", "element-map-policy-drop"},
        {"objects", {{"DropResult", {{"status", "ok"}, {"shape", "element_map_policy_drop"}}}}},
        {"named_shapes", {{"DropResult", cad_core::part::namedShapeToJson(dropped)}}},
    };
}

nlohmann::json runShapeFixWireframeModifiedHistory(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/tests/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperHistoryModified builds this ShapeFix_Wireframe + ShapeBuild_ReShape history and
    // verifies "fixMprHst.modified(edge1).size() == 1".
    TopoDS_Vertex vertex1 = BRepBuilderAPI_MakeVertex(gp_Pnt(-1.0, -1.0, 0.0)).Vertex();
    TopoDS_Vertex vertex2 = BRepBuilderAPI_MakeVertex(gp_Pnt(1.0, 0.0, 0.0)).Vertex();
    TopoDS_Edge edge1 = BRepBuilderAPI_MakeEdge(vertex1, vertex2).Edge();
    TopoDS_Edge edge2 = BRepBuilderAPI_MakeEdge(gp_Pnt(-1.0, 0.0, 0.0), gp_Pnt(0.0, 1.0, 0.0)).Edge();
    TopoDS_Edge edge3 = BRepBuilderAPI_MakeEdge(gp_Pnt(0.0, 1.0, 0.0), gp_Pnt(1.0, 0.0, 0.0)).Edge();
    TopoDS_Wire wire = BRepBuilderAPI_MakeWire(edge1, edge2, edge3).Wire();

    Handle(ShapeBuild_ReShape) reshape = new ShapeBuild_ReShape();
    vertex1 = TopoDS::Vertex(reshape->CopyVertex(vertex1));
    vertex2 = TopoDS::Vertex(reshape->CopyVertex(vertex2));
    reshape->Apply(edge1);

    Handle(ShapeFix_Wireframe) fix = new ShapeFix_Wireframe();
    fix->SetContext(reshape);
    fix->SetPrecision(0.0);
    fix->Load(wire);
    fix->FixWireGaps();

    BRepBuilderAPI_MakeEdge replacement(vertex1, vertex2);
    if (!replacement.IsDone()) {
        throw std::runtime_error("could not build modified replacement edge");
    }
    reshape->Replace(edge1, replacement.Edge());
    const TopoDS_Shape resultShape = reshape->Apply(wire);
    if (resultShape.IsNull()) {
        throw std::runtime_error("ShapeFix Wireframe modified history produced a null shape");
    }

    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("Source", wire);
    const cad_core::part::NamedShapeSource source {
        "Source",
        wire,
        &sourceNamedShape,
    };
    const cad_core::part::NamedShape namedShape
        = cad_core::part::namedShapeForShapeFixRootHistory("ShapeFix", resultShape, source, *fix);
    return {
        {"case", fixture.value("case", "shapefix-modify-face-wire")},
        {"objects", {{"ShapeFix", {{"status", "ok"}, {"shape", "shape_fix_wireframe"}}}}},
        {"named_shapes", {{"ShapeFix", cad_core::part::namedShapeToJson(namedShape)}}},
    };
}

nlohmann::json runMapperHistoryAmbiguousSplit(const nlohmann::json& fixture)
{
    const auto& shape = fixture.at("shape");
    BRepBuilderAPI_MakeEdge sourceEdge(
        pointFromJson(shape.at("source_edge").at(0)),
        pointFromJson(shape.at("source_edge").at(1))
    );
    BRepBuilderAPI_MakeEdge toolEdge(
        pointFromJson(shape.at("tool_edge").at(0)),
        pointFromJson(shape.at("tool_edge").at(1))
    );
    if (!sourceEdge.IsDone() || !toolEdge.IsDone()) {
        throw std::runtime_error("could not build split edges");
    }

    TopTools_ListOfShape arguments;
    arguments.Append(sourceEdge.Edge());
    TopTools_ListOfShape tools;
    tools.Append(toolEdge.Edge());
    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(arguments);
    splitter.SetTools(tools);
    splitter.SetNonDestructive(Standard_True);
    splitter.SetRunParallel(Standard_False);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        throw std::runtime_error("splitter did not produce a result shape");
    }

    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("Source", sourceEdge.Edge());
    const cad_core::part::NamedShapeSource source {
        "Source",
        sourceEdge.Edge(),
        &sourceNamedShape,
    };
    const cad_core::part::NamedShape namedShape = cad_core::part::namedShapeForMakerHistory(
        "Split",
        splitter.Shape(),
        {source},
        splitter
    );
    return {
        {"case", "mapper-history-ambiguous-split"},
        {"objects", {{"Split", {{"status", "ok"}, {"shape", "splitter"}}}}},
        {"named_shapes", {{"Split", cad_core::part::namedShapeToJson(namedShape)}}},
    };
}

nlohmann::json runFixture(const nlohmann::json& fixture)
{
    const std::string fixtureCase = fixture.at("case").get<std::string>();
    if (fixtureCase == "shapefix-delete-small-edge") {
        return runShapeFixDeleteSmallEdge(fixture);
    }
    if (fixtureCase == "element-map-policy-drop") {
        return runElementMapPolicyDrop(fixture);
    }
    if (fixtureCase == "shapefix-wireframe-modified-history") {
        return runShapeFixWireframeModifiedHistory(fixture);
    }
    if (fixtureCase == "mapper-history-ambiguous-split") {
        return runMapperHistoryAmbiguousSplit(fixture);
    }
    throw std::runtime_error("unsupported C3-M1 topology probe case: " + fixtureCase);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: cad-core-c3m1-topology-probe <fixture.json>\n";
        return 2;
    }

    try {
        std::ifstream input(argv[1]);
        if (!input) {
            std::cerr << "failed to open fixture: " << argv[1] << "\n";
            return 2;
        }
        const nlohmann::json fixture = nlohmann::json::parse(input);
        std::cout << runFixture(fixture).dump(2) << "\n";
    }
    catch (const Standard_Failure& failure) {
        std::cerr << (failure.GetMessageString() != nullptr ? failure.GetMessageString() : "OCCT failure")
                  << "\n";
        return 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
    return 0;
}
