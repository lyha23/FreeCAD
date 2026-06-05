#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <set>
#include <string>
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

}  // namespace

void executePartCompound(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureCompound.cpp
    // ::Compound::execute(), reads "Links", "avoid duplicates without changing the order", then
    // writes "TopoShape().makeElementCompound(shapes)".
    const std::vector<app::Link> links = app::readLinks(object, "Links");
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
