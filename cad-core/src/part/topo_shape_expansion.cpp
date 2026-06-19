#include "cad_core/part/topo_shape_expansion.h"

#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepFill.hxx>
#include <BRep_Tool.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <optional>
#include <utility>

namespace cad_core::part
{

namespace
{

void addImportAlias(NamedShape& namedShape,
                    const std::string& owner,
                    const std::string& elementName,
                    const ImportElementMapSource& source)
{
    const auto elementIt = namedShape.elements.find(elementName);
    if (elementIt == namedShape.elements.end()) {
        return;
    }

    const std::string stableName = owner + "." + elementName;
    namedShape.elementMap[stableName] = elementName;

    auto& element = namedShape.elements[elementName];
    if (std::find(element.sources.begin(), element.sources.end(), stableName)
        == element.sources.end()) {
        element.sources.push_back(stableName);
    }

    MapperHistoryEvent event;
    event.source = MapperHistoryEndpoint {owner, elementName};
    event.target = MapperHistoryEndpoint {owner, elementName};
    event.shapeKind = subshapeKindName(element.subshape.kind);
    event.relation = MapperHistoryRelation::Preserved;
    event.makerStage = "import_shape_element_map";
    event.evidence = {
        {"format", source.format},
        {"file_name", source.fileName},
        {"stable_subname", stableName},
        {"current_subname", elementName},
    };
    event.recoverability = MapperHistoryRecoverability::Resolved;
    event.diagnosticStatus = "import_shape_element_map";
    addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
}

TopoDS_Edge copiedEdge(const TopoDS_Edge& edge)
{
    BRepBuilderAPI_Copy copy(edge, Standard_True, Standard_True);
    if (copy.IsDone() && !copy.Shape().IsNull()) {
        return TopoDS::Edge(copy.Shape());
    }
    return edge;
}

std::array<gp_Pnt, 2> edgeEndpoints(const TopoDS_Edge& edge)
{
    TopoDS_Vertex firstVertex;
    TopoDS_Vertex lastVertex;
    TopExp::Vertices(edge, firstVertex, lastVertex);
    if (!firstVertex.IsNull() && !lastVertex.IsNull()) {
        return {BRep_Tool::Pnt(firstVertex), BRep_Tool::Pnt(lastVertex)};
    }

    BRepAdaptor_Curve curve(edge);
    return {curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter())};
}

bool samePoint(const gp_Pnt& left, const gp_Pnt& right)
{
    return left.SquareDistance(right) <= Precision::SquareConfusion();
}

bool sameEndpointPair(const TopoDS_Edge& left, const TopoDS_Edge& right)
{
    const auto leftEndpoints = edgeEndpoints(left);
    const auto rightEndpoints = edgeEndpoints(right);
    return (samePoint(leftEndpoints[0], rightEndpoints[0])
            && samePoint(leftEndpoints[1], rightEndpoints[1]))
        || (samePoint(leftEndpoints[0], rightEndpoints[1])
            && samePoint(leftEndpoints[1], rightEndpoints[0]));
}

bool automaticRuledSurfaceReversesSecondEdge(const TopoDS_Edge& first, const TopoDS_Edge& second)
{
    BRepAdaptor_Curve firstCurve(first);
    BRepAdaptor_Curve secondCurve(second);

    gp_Pnt p1 = firstCurve.Value(
        0.9 * firstCurve.FirstParameter() + 0.1 * firstCurve.LastParameter()
    );
    gp_Pnt p2 = firstCurve.Value(
        0.1 * firstCurve.FirstParameter() + 0.9 * firstCurve.LastParameter()
    );
    if (first.Orientation() == TopAbs_REVERSED) {
        std::swap(p1, p2);
    }

    gp_Pnt p3 = secondCurve.Value(
        0.9 * secondCurve.FirstParameter() + 0.1 * secondCurve.LastParameter()
    );
    gp_Pnt p4 = secondCurve.Value(
        0.1 * secondCurve.FirstParameter() + 0.9 * secondCurve.LastParameter()
    );
    if (second.Orientation() == TopAbs_REVERSED) {
        std::swap(p3, p4);
    }

    const gp_Vec n1 = gp_Vec(p1, p2).Crossed(gp_Vec(p1, p3));
    const gp_Vec n2 = gp_Vec(p4, p3).Crossed(gp_Vec(p4, p2));
    return n1.Dot(n2) < 0.0;
}

std::optional<std::string> targetEdgeNameForSource(
    const NamedShape& namedShape,
    const TopoDS_Edge& sourceEdge
)
{
    for (const auto& [name, element] : namedShape.elements) {
        if (element.subshape.kind != TopAbs_EDGE) {
            continue;
        }
        const auto targetShape = subshapeByName(namedShape.shape, element.subshape);
        if (!targetShape || targetShape->IsNull() || targetShape->ShapeType() != TopAbs_EDGE) {
            continue;
        }
        if (sameEndpointPair(sourceEdge, TopoDS::Edge(*targetShape))) {
            return name;
        }
    }
    return std::nullopt;
}

void addDistinct(std::vector<std::string>& values, const std::string& value)
{
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void addRuledSurfaceSourceRelation(
    NamedShape& namedShape,
    const std::string& owner,
    const RuledSurfaceEdgeSource& source
)
{
    const auto targetName = targetEdgeNameForSource(namedShape, source.edge);
    if (!targetName) {
        return;
    }

    auto elementIt = namedShape.elements.find(*targetName);
    if (elementIt != namedShape.elements.end()) {
        elementIt->second.status = ElementHistoryKind::Modified;
    }

    for (const std::string& sourceName : source.stableEdgeNames) {
        if (sourceName.empty()) {
            continue;
        }
        namedShape.elementMap[sourceName] = *targetName;
        if (elementIt != namedShape.elements.end()) {
            addDistinct(elementIt->second.sources, sourceName);
        }
        namedShape.history.push_back(ElementHistory {
            ElementHistoryKind::Modified,
            *targetName,
            {sourceName}
        });

        MapperHistoryEvent event;
        event.source = MapperHistoryEndpoint {source.objectName, sourceName};
        event.target = MapperHistoryEndpoint {owner, *targetName};
        event.shapeKind = "edge";
        event.relation = MapperHistoryRelation::Modified;
        event.makerStage = "ruled_surface_shared_vertex_relation";
        event.evidence = {
            {"freecad_operation", "TopoShape::makeElementRuledSurface"},
            {"source_edge", sourceName},
            {"target_edge", *targetName},
        };
        event.recoverability = MapperHistoryRecoverability::Resolved;
        event.diagnosticStatus = "ruled_surface_edge_relation";
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
}

}  // namespace

NamedShapeBuild makeElementRuledSurfaceFromEdges(
    const std::string& owner,
    const std::array<RuledSurfaceEdgeSource, 2>& sources,
    short orientation
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementRuledSurface(), "Automatic" samples two curve endpoint pairs and
    // flips S2 when the triangle-normal dot product is negative; "Reversed" directly reverses S2.
    try {
        TopoDS_Edge first = copiedEdge(sources[0].edge);
        TopoDS_Edge second = copiedEdge(sources[1].edge);
        if (orientation == 0) {
            if (automaticRuledSurfaceReversesSecondEdge(first, second)) {
                second.Reverse();
            }
        }
        else if (orientation == 2) {
            second.Reverse();
        }
        else if (orientation != 1) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Part::RuledSurface Orientation must be Automatic, Forward or Reversed"
            };
        }

        TopoDS_Shape ruledShape = BRepFill::Face(first, second);
        if (ruledShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "BRepFill::Face produced null shape"};
        }

        NamedShape namedShape = indexedNamedShapeForObject(owner, ruledShape);
        addRuledSurfaceSourceRelation(namedShape, owner, sources[0]);
        addRuledSurfaceSourceRelation(namedShape, owner, sources[1]);
        addDistinct(
            namedShape.elementHistoryStatus,
            "part_ruled_surface:shared_vertex_edge_relation"
        );
        return NamedShapeBuild {ruledShape, std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Part::RuledSurface failed"
        };
    }
}

NamedShape namedShapeForImportedShape(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const ImportElementMapSource& source
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportStep.cpp
    // ::ImportStep::execute(), calls "TopoShape aShape; aShape.importStep(...)" and writes
    // "this->Shape.setValue(aShape)"; the imported shape is recomputed from FileName, while
    // the ElementMap carries stable owner-qualified FaceN/EdgeN/VertexN aliases.
    NamedShape namedShape = indexedNamedShapeForObject(owner, shape);
    for (const auto& [elementName, element] : namedShape.elements) {
        (void)element;
        addImportAlias(namedShape, owner, elementName, source);
    }
    namedShape.elementHistoryStatus.push_back("import_shape_element_map");
    return namedShape;
}

}  // namespace cad_core::part
