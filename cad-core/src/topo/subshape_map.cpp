#include "cad_core/topo/subshape_map.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <string>

namespace cad_core::topo {

nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape)
{
    nlohmann::json result = nlohmann::json::object();

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);

    for (int index = 1; index <= faces.Extent(); ++index) {
        result["Face" + std::to_string(index)] = {{"kind", "face"}};
    }
    for (int index = 1; index <= edges.Extent(); ++index) {
        result["Edge" + std::to_string(index)] = {{"kind", "edge"}};
    }
    for (int index = 1; index <= vertices.Extent(); ++index) {
        result["Vertex" + std::to_string(index)] = {{"kind", "vertex"}};
    }

    return result;
}

}  // namespace cad_core::topo

