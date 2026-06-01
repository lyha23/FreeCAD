#include "cad_core/topo/element_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
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

void addDistinctString(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
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

double distanceFromPointToEdge(const gp_Pnt& point, const TopoDS_Edge& edge)
{
    try {
        const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point);
        BRepExtrema_DistShapeShape distance(vertex, edge);
        distance.Perform();
        if (distance.IsDone() && distance.NbSolution() > 0) {
            return distance.Value();
        }
    }
    catch (const Standard_Failure&) {
    }
    return std::numeric_limits<double>::infinity();
}

bool samplesLieOnEdge(const TopoDS_Edge& source, const TopoDS_Edge& target)
{
    try {
        BRepAdaptor_Curve curve(source);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last)) {
            return false;
        }

        constexpr int sampleCount = 7;
        for (int sample = 0; sample < sampleCount; ++sample) {
            const double ratio = static_cast<double>(sample) / (sampleCount - 1);
            const gp_Pnt point = curve.Value(first + (last - first) * ratio);
            if (distanceFromPointToEdge(point, target) > Precision::Confusion()) {
                return false;
            }
        }
    }
    catch (const Standard_Failure&) {
        return false;
    }
    return true;
}

bool sameEdgeGeometry(const TopoDS_Edge& rawEdge, const TopoDS_Edge& internalEdge)
{
    try {
        BRepAdaptor_Curve rawCurve(rawEdge);
        BRepAdaptor_Curve internalCurve(internalEdge);
        const GeomAbs_CurveType rawType = rawCurve.GetType();
        const GeomAbs_CurveType internalType = internalCurve.GetType();
        if (rawType != internalType) {
            return false;
        }
        if (rawType == GeomAbs_Line) {
            return true;
        }
        return samplesLieOnEdge(rawEdge, internalEdge) && samplesLieOnEdge(internalEdge, rawEdge);
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

std::optional<int> matchingEdgeIndex(const TopTools_IndexedMapOfShape& rawEdges,
                                     const TopoDS_Edge& internalEdge)
{
    const auto internalEndpoints = edgeEndpoints(internalEdge);
    for (int index = 1; index <= rawEdges.Extent(); ++index) {
        const TopoDS_Edge rawEdge = TopoDS::Edge(rawEdges(index));
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::getInternalElementMap() calls Shape.findSubShapesWithSharedVertex(...,
        // CheckGeometry | SingleResult). Matching only edge endpoints can map a request-local
        // InternalEdge arc to a raw line sharing the same vertices, so cad-core mirrors the
        // geometry check here. The loose endpoint-only exception remains line-only, matching
        // TopoShapeExpansion.cpp::findSubShapesWithSharedVertex().
        if (sameEdgeEndpoints(edgeEndpoints(rawEdge), internalEndpoints)
            && sameEdgeGeometry(rawEdge, internalEdge)) {
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
