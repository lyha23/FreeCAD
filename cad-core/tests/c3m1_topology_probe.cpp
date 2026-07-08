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
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
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

const nlohmann::json& propertyPayload(const nlohmann::json& value)
{
    if (value.is_object() && value.contains("PropertyType") && value.contains("value")) {
        return value.at("value");
    }
    return value;
}

nlohmann::json normalizedProbeFixture(const nlohmann::json& fixture)
{
    if (!fixture.is_object() || !fixture.contains("Objects") || !fixture.at("Objects").is_array()) {
        throw std::runtime_error("C3-M1 topology probe fixture must use top-level Objects");
    }
    for (const auto& object : fixture.at("Objects")) {
        if (!object.is_object() || object.value("TypeId", "") != "CadCore::C3M1TopologyProbe") {
            continue;
        }
        const auto propertiesIt = object.find("Properties");
        if (propertiesIt == object.end() || !propertiesIt->is_object()) {
            throw std::runtime_error("C3-M1 topology probe object requires Properties");
        }
        const auto caseIt = propertiesIt->find("ProbeCase");
        if (caseIt == propertiesIt->end() || !propertyPayload(*caseIt).is_string()) {
            throw std::runtime_error("C3-M1 topology probe object requires string ProbeCase");
        }

        nlohmann::json normalized = {{"case", propertyPayload(*caseIt).get<std::string>()}};
        const auto shapeIt = propertiesIt->find("Shape");
        if (shapeIt != propertiesIt->end()) {
            normalized["shape"] = propertyPayload(*shapeIt);
        }
        return normalized;
    }
    throw std::runtime_error("C3-M1 topology probe fixture is missing CadCore::C3M1TopologyProbe object");
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

TopoDS_Shape twoEdgeWire(double xOffset)
{
    BRepBuilderAPI_MakeWire wire;
    BRepBuilderAPI_MakeEdge edge1(gp_Pnt(xOffset, 0.0, 0.0), gp_Pnt(xOffset + 1.0, 0.0, 0.0));
    BRepBuilderAPI_MakeEdge edge2(gp_Pnt(xOffset + 1.0, 0.0, 0.0), gp_Pnt(xOffset + 1.0, 1.0, 0.0));
    if (!edge1.IsDone() || !edge2.IsDone()) {
        throw std::runtime_error("could not build source wire edges");
    }
    wire.Add(edge1.Edge());
    wire.Add(edge2.Edge());
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build source wire");
    }
    return wire.Shape();
}

TopoDS_Shape oneEdgeWire(double xOffset)
{
    BRepBuilderAPI_MakeEdge edge(gp_Pnt(xOffset, 0.0, 0.0), gp_Pnt(xOffset + 1.0, 0.0, 0.0));
    if (!edge.IsDone()) {
        throw std::runtime_error("could not build single edge wire source");
    }
    BRepBuilderAPI_MakeWire wire(edge.Edge());
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build single edge wire");
    }
    return wire.Shape();
}

TopoDS_Shape multiEdgeWire(double xOffset, int edgeCount)
{
    BRepBuilderAPI_MakeWire wire;
    for (int index = 0; index < edgeCount; ++index) {
        BRepBuilderAPI_MakeEdge edge(
            gp_Pnt(xOffset + static_cast<double>(index), 0.0, 0.0),
            gp_Pnt(xOffset + static_cast<double>(index + 1), 0.0, 0.0)
        );
        if (!edge.IsDone()) {
            throw std::runtime_error("could not build multi-edge wire source");
        }
        wire.Add(edge.Edge());
    }
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build multi-edge wire");
    }
    return wire.Shape();
}

TopoDS_Shape makePartnerCompound(const std::vector<TopoDS_Shape>& shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
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

nlohmann::json runElementMapPolicyPropagateWire(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementWires(), after MakeWire updates connected edge identity, preserves
    // mapping with "wires.back().mapSubElement(edges, op)" under ElementMapPolicy::Propagate.
    BRepBuilderAPI_MakeEdge edgeA(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(1.0, 0.0, 0.0));
    BRepBuilderAPI_MakeEdge edgeB(gp_Pnt(1.0, 0.0, 0.0), gp_Pnt(1.0, 1.0, 0.0));
    if (!edgeA.IsDone() || !edgeB.IsDone()) {
        throw std::runtime_error("could not build propagated wire source edges");
    }
    const cad_core::part::NamedShape namedA
        = cad_core::part::indexedNamedShapeForObject("EdgeA", edgeA.Edge());
    const cad_core::part::NamedShape namedB
        = cad_core::part::indexedNamedShapeForObject("EdgeB", edgeB.Edge());
    const std::vector<cad_core::part::NamedShapeSource> sources {
        {"EdgeA", edgeA.Edge(), &namedA},
        {"EdgeB", edgeB.Edge(), &namedB},
    };
    const cad_core::part::NamedShapeBuild propagated
        = cad_core::part::makeElementWiresWithPropagatedSources("Wire", sources, "WIR");
    if (!propagated.error.empty()) {
        throw std::runtime_error(propagated.error);
    }
    if (!propagated.namedShape) {
        throw std::runtime_error("makeElementWires did not return NamedShape history");
    }
    return {
        {"case", fixture.value("case", "element-map-propagate-wire")},
        {"objects", {{"Wire", {{"status", "ok"}, {"shape", "occt_wire"}}}}},
        {"named_shapes", {{"Wire", cad_core::part::namedShapeToJson(*propagated.namedShape)}}},
    };
}

nlohmann::json runElementMapPolicyPropagateShell(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementShell(), adds every source Face to a TopoDS_Shell, then under
    // ElementMapPolicy::Propagate calls "tmp.mapSubElement(*this, op)" for the shell map.
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(2.0, 3.0, 4.0).Shape();
    BRep_Builder builder;
    TopoDS_Compound faceCompound;
    builder.MakeCompound(faceCompound);
    int faceCount = 0;
    for (TopExp_Explorer explorer(box, TopAbs_FACE); explorer.More(); explorer.Next()) {
        builder.Add(faceCompound, TopoDS::Face(explorer.Current()));
        ++faceCount;
    }
    if (faceCount != 6) {
        throw std::runtime_error("box did not expose six faces");
    }

    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("FaceCompound", faceCompound);
    const cad_core::part::NamedShapeSource source {
        "FaceCompound",
        faceCompound,
        &sourceNamedShape,
    };
    const cad_core::part::NamedShapeBuild shell
        = cad_core::part::makeElementShellWithPropagatedSource("Shell", source, "SH1");
    if (!shell.error.empty()) {
        throw std::runtime_error(shell.error);
    }
    if (!shell.namedShape) {
        throw std::runtime_error("makeElementShell did not return NamedShape history");
    }
    return {
        {"case", fixture.value("case", "element-map-propagate-shell")},
        {"objects", {{"Shell", {{"status", "ok"}, {"shape", "occt_shell"}}}}},
        {"named_shapes", {{"Shell", cad_core::part::namedShapeToJson(*shell.namedShape)}}},
    };
}

nlohmann::json runElementMapChildMapPostfix(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::createChildMap(), copies the operation string into
    // "MappedChildElements::postfix"; ElementMap.cpp::ElementMap::addChildElements() composes
    // grandchild postfixes before storing the child map.
    const TopoDS_Shape sketchA = twoEdgeWire(0.0);
    const TopoDS_Shape sketchB = twoEdgeWire(10.0);
    const TopoDS_Shape sketchC = oneEdgeWire(20.0);
    const cad_core::part::NamedShape namedA
        = cad_core::part::indexedNamedShapeForObject("SketchA", sketchA);
    const cad_core::part::NamedShape namedB
        = cad_core::part::indexedNamedShapeForObject("SketchB", sketchB);
    const cad_core::part::NamedShape namedC
        = cad_core::part::indexedNamedShapeForObject("SketchC", sketchC);

    const TopoDS_Shape compoundAB = makePartnerCompound({sketchA, sketchB});
    const cad_core::part::NamedShape compoundABNamedShape = cad_core::part::namedShapeForPreservedSources(
        "CompoundAB",
        compoundAB,
        {
            cad_core::part::NamedShapeSource {"SketchA", sketchA, &namedA, {}, ";:SOURCE"},
            cad_core::part::NamedShapeSource {"SketchB", sketchB, &namedB, {}, ";:SOURCE"},
        }
    );

    const TopoDS_Shape compoundNested = makePartnerCompound({compoundAB, sketchC});
    const cad_core::part::NamedShape compoundNestedNamedShape = cad_core::part::namedShapeForPreservedSources(
        "CompoundNested",
        compoundNested,
        {
            cad_core::part::NamedShapeSource {"CompoundAB", compoundAB, &compoundABNamedShape, {}, ";:PARENT"},
            cad_core::part::NamedShapeSource {"SketchC", sketchC, &namedC, {}, ";:PARENT"},
        }
    );

    return {
        {"case", "element-map-child-map-postfix-compound"},
        {"objects", {{"CompoundNested", {{"status", "ok"}, {"shape", "occt_compound"}}}}},
        {"named_shapes",
         {
             {"CompoundAB", cad_core::part::namedShapeToJson(compoundABNamedShape)},
             {"CompoundNested", cad_core::part::namedShapeToJson(compoundNestedNamedShape)},
         }},
    };
}

nlohmann::json runElementMapChildMapHashKey(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), "do child mapping only if the child element count >= 5";
    // ::ElementMap::hashChildMaps(), rewrites eligible child map postfixes under
    // "MAPPED_CHILD_ELEMENTS_PREFIX". Six edges keeps this probe out of the count==5 tag skip.
    const TopoDS_Shape sketchA = multiEdgeWire(0.0, 6);
    const TopoDS_Shape sketchB = multiEdgeWire(20.0, 6);
    const cad_core::part::NamedShape namedA
        = cad_core::part::indexedNamedShapeForObject("SketchA", sketchA);
    const cad_core::part::NamedShape namedB
        = cad_core::part::indexedNamedShapeForObject("SketchB", sketchB);

    const TopoDS_Shape compoundAB = makePartnerCompound({sketchA, sketchB});
    const cad_core::part::NamedShape compoundABNamedShape = cad_core::part::namedShapeForPreservedSources(
        "CompoundAB",
        compoundAB,
        {
            cad_core::part::NamedShapeSource {"SketchA", sketchA, &namedA, {}, ";:SOURCE"},
            cad_core::part::NamedShapeSource {"SketchB", sketchB, &namedB, {}, ";:SOURCE"},
        }
    );

    const TopoDS_Shape compoundNested = makePartnerCompound({compoundAB});
    const cad_core::part::NamedShape compoundNestedNamedShape = cad_core::part::namedShapeForPreservedSources(
        "CompoundNested",
        compoundNested,
        {
            cad_core::part::NamedShapeSource {"CompoundAB", compoundAB, &compoundABNamedShape, {}, ";:PARENT"},
        }
    );

    return {
        {"case", "element-map-child-map-hash-key-compound"},
        {"objects", {{"CompoundNested", {{"status", "ok"}, {"shape", "occt_compound"}}}}},
        {"named_shapes",
         {
             {"CompoundAB", cad_core::part::namedShapeToJson(compoundABNamedShape)},
             {"CompoundNested", cad_core::part::namedShapeToJson(compoundNestedNamedShape)},
         }},
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
    const cad_core::part::NamedShape namedShape
        = cad_core::part::namedShapeForMakerHistory("Split", splitter.Shape(), {source}, splitter);
    return {
        {"case", "mapper-history-ambiguous-split"},
        {"objects", {{"Split", {{"status", "ok"}, {"shape", "splitter"}}}}},
        {"named_shapes", {{"Split", cad_core::part::namedShapeToJson(namedShape)}}},
    };
}

nlohmann::json runMakeElementSolidFromShell(const nlohmann::json& fixture)
{
    (void)fixture;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementSolid(), when no compsolid exists, iterates shells and calls
    // "mkSolid.Add(TopoDS::Shell(s))" before makeElementShape(mkSolid, shape, op).
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(2.0, 3.0, 4.0).Shape();
    TopExp_Explorer shellExplorer(box, TopAbs_SHELL);
    if (!shellExplorer.More()) {
        throw std::runtime_error("box did not expose a shell");
    }
    const TopoDS_Shape shell = TopoDS::Shell(shellExplorer.Current());
    const cad_core::part::NamedShape sourceNamedShape
        = cad_core::part::indexedNamedShapeForObject("SourceShell", shell);
    const cad_core::part::NamedShapeSource source {
        "SourceShell",
        shell,
        &sourceNamedShape,
    };
    const cad_core::part::NamedShapeBuild solid
        = cad_core::part::makeElementSolidFromSource("Solid", source);
    if (!solid.error.empty()) {
        throw std::runtime_error(solid.error);
    }
    if (!solid.namedShape) {
        throw std::runtime_error("makeElementSolid did not return NamedShape history");
    }
    return {
        {"case", fixture.value("case", "make-element-solid-from-shell")},
        {"objects", {{"Solid", {{"status", "ok"}, {"shape", "occt_solid"}}}}},
        {"named_shapes", {{"Solid", cad_core::part::namedShapeToJson(*solid.namedShape)}}},
    };
}

nlohmann::json runFixture(const nlohmann::json& fixture)
{
    const nlohmann::json normalized = normalizedProbeFixture(fixture);
    const std::string fixtureCase = normalized.at("case").get<std::string>();
    if (fixtureCase == "shapefix-delete-small-edge") {
        return runShapeFixDeleteSmallEdge(normalized);
    }
    if (fixtureCase == "element-map-policy-drop") {
        return runElementMapPolicyDrop(normalized);
    }
    if (fixtureCase == "element-map-propagate-wire") {
        return runElementMapPolicyPropagateWire(normalized);
    }
    if (fixtureCase == "element-map-propagate-shell") {
        return runElementMapPolicyPropagateShell(normalized);
    }
    if (fixtureCase == "element-map-child-map-postfix-compound") {
        return runElementMapChildMapPostfix(normalized);
    }
    if (fixtureCase == "element-map-child-map-hash-key-compound") {
        return runElementMapChildMapHashKey(normalized);
    }
    if (fixtureCase == "shapefix-wireframe-modified-history") {
        return runShapeFixWireframeModifiedHistory(normalized);
    }
    if (fixtureCase == "mapper-history-ambiguous-split") {
        return runMapperHistoryAmbiguousSplit(normalized);
    }
    if (fixtureCase == "make-element-solid-from-shell") {
        return runMakeElementSolidFromShell(normalized);
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
