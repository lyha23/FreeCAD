#include "cad_core/part/topo_shape_expansion.h"

#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"

#include <algorithm>
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

}  // namespace

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
