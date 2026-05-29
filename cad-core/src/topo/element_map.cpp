#include "cad_core/topo/element_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <string>
#include <utility>

namespace cad_core::topo {

namespace {

bool samePoint3d(const gp_Pnt& left, const gp_Pnt& right)
{
    return left.SquareDistance(right) < Precision::SquareConfusion();
}

void addElementMapPair(nlohmann::json& result,
                       const std::string& internalName,
                       const std::string& rawName)
{
    result[internalName] = rawName;
    result[rawName] = internalName;
}

std::optional<int> matchingVertexIndex(const TopTools_IndexedMapOfShape& rawVertices,
                                       const TopoDS_Vertex& internalVertex)
{
    const gp_Pnt internalPoint = BRep_Tool::Pnt(internalVertex);
    for (int index = 1; index <= rawVertices.Extent(); ++index) {
        if (samePoint3d(BRep_Tool::Pnt(TopoDS::Vertex(rawVertices(index))), internalPoint)) {
            return index;
        }
    }
    return std::nullopt;
}

std::pair<gp_Pnt, gp_Pnt> edgeEndpoints(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    return {curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter())};
}

bool sameEdgeEndpoints(const std::pair<gp_Pnt, gp_Pnt>& left,
                       const std::pair<gp_Pnt, gp_Pnt>& right)
{
    return (samePoint3d(left.first, right.first) && samePoint3d(left.second, right.second))
        || (samePoint3d(left.first, right.second) && samePoint3d(left.second, right.first));
}

std::optional<int> matchingEdgeIndex(const TopTools_IndexedMapOfShape& rawEdges,
                                     const TopoDS_Edge& internalEdge)
{
    const auto internalEndpoints = edgeEndpoints(internalEdge);
    for (int index = 1; index <= rawEdges.Extent(); ++index) {
        if (sameEdgeEndpoints(edgeEndpoints(TopoDS::Edge(rawEdges(index))), internalEndpoints)) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace

nlohmann::json internalElementMapForSketch(const TopoDS_Shape& rawShape,
                                           const TopoDS_Shape& internalShape)
{
    nlohmann::json result = nlohmann::json::object();

    TopTools_IndexedMapOfShape rawVertices;
    TopTools_IndexedMapOfShape internalVertices;
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    TopExp::MapShapes(internalShape, TopAbs_VERTEX, internalVertices);
    for (int index = 1; index <= internalVertices.Extent(); ++index) {
        const auto rawIndex = matchingVertexIndex(rawVertices, TopoDS::Vertex(internalVertices(index)));
        if (rawIndex) {
            addElementMapPair(result,
                              "InternalVertex" + std::to_string(index),
                              "Vertex" + std::to_string(*rawIndex));
        }
    }

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);
    for (int index = 1; index <= internalEdges.Extent(); ++index) {
        const auto rawIndex = matchingEdgeIndex(rawEdges, TopoDS::Edge(internalEdges(index)));
        if (rawIndex) {
            addElementMapPair(result,
                              "InternalEdge" + std::to_string(index),
                              "Edge" + std::to_string(*rawIndex));
        }
    }

    return result;
}

}  // namespace cad_core::topo
