#include "cad_core/topo/subshape_map.h"

#include "cad_core/geometry/shape_exporter.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <cctype>
#include <string>
#include <vector>

namespace cad_core::topo {

namespace {

struct NamePrefix {
    const char* text;
    TopAbs_ShapeEnum kind;
};

const std::vector<NamePrefix>& supportedPrefixes()
{
    static const std::vector<NamePrefix> prefixes = {
        {"Face", TopAbs_FACE},
        {"Edge", TopAbs_EDGE},
        {"Vertex", TopAbs_VERTEX},
    };
    return prefixes;
}

void addEntries(nlohmann::json& result,
                const TopTools_IndexedMapOfShape& shapes,
                const std::string& prefix,
                const std::string& kind)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        result[prefix + std::to_string(index)] = {
            {"kind", kind},
            {"bbox", geometry::bboxForShape(shapes(index))},
        };
    }
}

}  // namespace

nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape)
{
    nlohmann::json result = nlohmann::json::object();

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);

    addEntries(result, faces, "Face", "face");
    addEntries(result, edges, "Edge", "edge");
    addEntries(result, vertices, "Vertex", "vertex");

    return result;
}

std::optional<SubshapeName> parseSubshapeName(const std::string& name)
{
    for (const auto& prefix : supportedPrefixes()) {
        const std::string text(prefix.text);
        if (name.rfind(text, 0) != 0U) {
            continue;
        }

        const std::string indexText = name.substr(text.size());
        if (indexText.empty()) {
            return std::nullopt;
        }
        int index = 0;
        for (const char item : indexText) {
            if (!std::isdigit(static_cast<unsigned char>(item))) {
                return std::nullopt;
            }
            index = index * 10 + (item - '0');
        }
        if (index <= 0) {
            return std::nullopt;
        }
        return SubshapeName{prefix.kind, index};
    }

    return std::nullopt;
}

std::optional<TopoDS_Shape> subshapeByName(const TopoDS_Shape& shape, const std::string& name)
{
    const auto parsed = parseSubshapeName(name);
    if (!parsed) {
        return std::nullopt;
    }

    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(shape, parsed->kind, shapes);
    if (parsed->index > shapes.Extent()) {
        return std::nullopt;
    }
    return shapes(parsed->index);
}

std::string subshapeKindName(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "face";
        case TopAbs_EDGE:
            return "edge";
        case TopAbs_VERTEX:
            return "vertex";
        default:
            return "unsupported";
    }
}

}  // namespace cad_core::topo
