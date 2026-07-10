#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part
{

namespace
{

TopoDS_Shape makeCompound(const std::vector<TopoDS_Shape>& shapes)
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

int subshapeCount(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, kind, map);
    return map.Extent();
}

std::string kindName(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "face";
        case TopAbs_EDGE:
            return "edge";
        case TopAbs_VERTEX:
            return "vertex";
        default:
            break;
    }
    return "shape";
}

std::string prefixForKind(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "Face";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_VERTEX:
            return "Vertex";
        default:
            break;
    }
    return {};
}

void appendProtocolChildMaps(NamedShape& namedShape,
                             const std::vector<NamedShapeSource>& sources)
{
    int faceOffset = 0;
    int edgeOffset = 0;
    int vertexOffset = 0;
    for (std::size_t childIndex = 0; childIndex < sources.size(); ++childIndex) {
        const NamedShapeSource& source = sources.at(childIndex);
        for (const auto& item : {
                 std::pair<TopAbs_ShapeEnum, int*> {TopAbs_FACE, &faceOffset},
                 std::pair<TopAbs_ShapeEnum, int*> {TopAbs_EDGE, &edgeOffset},
                 std::pair<TopAbs_ShapeEnum, int*> {TopAbs_VERTEX, &vertexOffset},
             }) {
            const TopAbs_ShapeEnum kind = item.first;
            int& offset = *item.second;
            const int count = subshapeCount(source.shape, kind);
            if (count == 0) {
                continue;
            }
            const std::string prefix = prefixForKind(kind);
            NamedShapeChildMap childMap;
            childMap.sourceOwner = source.owner;
            childMap.kind = kindName(kind);
            childMap.indexedName = "Child" + std::to_string(childIndex);
            childMap.offset = offset;
            childMap.count = count;
            childMap.targetStart = prefix + std::to_string(offset + 1);
            childMap.targetEnd = prefix + std::to_string(offset + count);
            childMap.sourceNamedShape = source.namedShape;
            childMap.hasSourceElementMap = source.namedShape != nullptr
                && !source.namedShape->elementMap.empty();
            childMap.sourceElementMapSize = source.namedShape != nullptr
                ? source.namedShape->elementMap.size()
                : 0U;
            childMap.sourceChildMapCount = source.namedShape != nullptr
                ? source.namedShape->childElementMaps.size()
                : 0U;
            namedShape.childElementMaps.push_back(childMap);
            offset += count;
        }
    }
}

}  // namespace

void executePartCompound(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureCompound.cpp
    // ::Compound::execute(), reads "Links", "avoid duplicates without changing the order", then
    // writes "TopoShape().makeElementCompound(shapes)". Some CAD Core fixtures use the
    // protocol-facing "Objects" alias for the same link list; keep both at the Part executor.
    std::vector<app::Link> links = app::readLinks(object, "Links");
    if (links.empty()) {
        links = app::readLinks(object, "Objects");
    }
    std::set<std::string> seen;
    std::vector<std::string> linkedObjects;
    std::vector<TopoDS_Shape> shapes;
    std::vector<NamedShapeSource> sources;

    for (const app::Link& link : links) {
        if (link.object.empty() || !seen.insert(link.object).second) {
            continue;
        }
        linkedObjects.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            continue;
        }
        shapes.push_back(shapeIt->second.shape);

        const auto namedShapeIt = context.namedShapes.find(link.object);
        sources.push_back(NamedShapeSource {
            namedShapeIt != context.namedShapes.end() ? namedShapeIt->second.owner : link.object,
            shapeIt->second.shape,
            namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
        });
    }

    const TopoDS_Shape compound = makeCompound(shapes);
    NamedShape namedShape = sources.empty() ? indexedNamedShapeForObject(object.name, compound)
                                            : namedShapeForPreservedSources(object.name, compound, sources);
    appendProtocolChildMaps(namedShape, sources);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), "try to resolve the grand child map now", then applies
    // direct child ranges before findAll() exposes every mapped target.  Finalize canonical
    // collision evidence only after the FeatureCompound ChildN ranges exist in the Part ledger.
    appendProtocolChildMapCanonicalCollisionHistory(namedShape);
    namedShape.elementHistoryStatus.push_back("part_compound:make_element_compound");

    part_feature_detail::publishPartShape(
        object,
        context,
        compound,
        {{"feature", "part_compound"}, {"links", linkedObjects}},
        namedShape
    );
}

}  // namespace cad_core::part
