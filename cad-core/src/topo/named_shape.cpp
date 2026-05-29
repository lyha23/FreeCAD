#include "cad_core/topo/named_shape.h"

#include "cad_core/geometry/refine_model.h"

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_BuilderAlgo.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace cad_core::topo
{

namespace
{

std::string historyKindName(ElementHistoryKind kind)
{
    switch (kind) {
        case ElementHistoryKind::Indexed:
            return "indexed";
        case ElementHistoryKind::Generated:
            return "generated";
        case ElementHistoryKind::Modified:
            return "modified";
        case ElementHistoryKind::Deleted:
            return "deleted";
        case ElementHistoryKind::Split:
            return "split";
        case ElementHistoryKind::Merge:
            return "merge";
    }
    return "unknown";
}

void addIndexedElements(
    NamedShape& namedShape,
    const TopTools_IndexedMapOfShape& shapes,
    TopAbs_ShapeEnum kind,
    const std::string& prefix
)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        const std::string name = prefix + std::to_string(index);
        namedShape.elements[name]
            = NamedElement {name, SubshapeName {kind, index}, ElementHistoryKind::Indexed, {}};
        namedShape.elementMap[name] = name;
        namedShape.history.push_back(ElementHistory {ElementHistoryKind::Indexed, name, {}});
    }
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
            return {};
    }
}

std::vector<TopAbs_ShapeEnum> mappableKinds()
{
    return {TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX};
}

struct SourceTargets
{
    std::set<std::string> preserved;
    std::set<std::string> history;
};

std::optional<std::string> findElementName(
    const NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    const std::string prefix = prefixForKind(kind);
    if (prefix.empty()) {
        return std::nullopt;
    }
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(namedShape.shape, kind, shapes);
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return prefix + std::to_string(index);
        }
    }
    return std::nullopt;
}

void applyHistoryList(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopTools_ListOfShape& historyShapes,
    ElementHistoryKind historyKind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    for (TopTools_ListIteratorOfListOfShape it(historyShapes); it.More(); it.Next()) {
        const TopoDS_Shape& historyShape = it.Value();
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const auto elementName = findElementName(namedShape, historyShape, kind);
            if (!elementName) {
                continue;
            }
            auto& element = namedShape.elements[*elementName];
            element.status = historyKind;
            element.sources.push_back(sourceName);
            namedShape.history.push_back(ElementHistory {historyKind, *elementName, {sourceName}});
            sourceTargets[sourceName].history.insert(*elementName);
        }
    }
}

std::vector<std::string> sourceElementNames(
    const NamedShapeSource& source,
    const std::string& localElementName
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementShape() and mapSubElement(shapes) carry existing element names
    // through chained makers. When a source already has an ElementMap, cad-core treats those
    // stable keys as aliases of the source-local FaceN/EdgeN/VertexN during the next maker pass.
    std::vector<std::string> names {source.owner + "." + localElementName};
    if (source.namedShape == nullptr) {
        return names;
    }

    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (currentName != localElementName || stableName == localElementName) {
            continue;
        }
        if (std::find(names.begin(), names.end(), stableName) == names.end()) {
            names.push_back(stableName);
        }
    }
    return names;
}

void collectSourceElementMap(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& sourceElement,
    TopAbs_ShapeEnum kind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::makeShapeWithElementMap() calls "mapSubElement(shapes)" before consuming mapper history,
    // preserving input subelement names when the same shape survives in the result.
    const auto elementName = findElementName(namedShape, sourceElement, kind);
    if (!elementName) {
        return;
    }
    sourceTargets[sourceName].preserved.insert(*elementName);
}

void applyHistoryElementMap(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets
)
{
    const auto applySplit = [&](const std::string& sourceName, const std::set<std::string>& targets) {
        for (const std::string& target : targets) {
            auto elementIt = namedShape.elements.find(target);
            if (elementIt == namedShape.elements.end()) {
                continue;
            }
            elementIt->second.status = ElementHistoryKind::Split;
            namedShape.history.push_back(
                ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
            );
        }
    };

    for (const auto& [sourceName, targets] : sourceTargets) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::makeShapeWithElementMap() consumes MapperMaker generated/modified history into
        // ElementMap after first calling "mapSubElement(shapes)" for preserved source elements.
        // cad-core follows that priority: preserved source subelements resolve first; one-to-one
        // history fills the remaining keys; one-to-many history is recorded as split and left
        // unresolved instead of being guessed as a single current subshape.
        if (targets.preserved.size() == 1U) {
            namedShape.elementMap[sourceName] = *targets.preserved.begin();
            continue;
        }
        if (targets.preserved.size() > 1U) {
            applySplit(sourceName, targets.preserved);
            continue;
        }
        if (targets.history.size() == 1U) {
            namedShape.elementMap[sourceName] = *targets.history.begin();
            continue;
        }
        if (targets.history.size() > 1U) {
            applySplit(sourceName, targets.history);
            continue;
        }
        namedShape.history.push_back(
            ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
        );
    }
}

void applyPreservedElementMap(
    NamedShape& namedShape,
    const std::map<std::string, SourceTargets>& sourceTargets
)
{
    for (const auto& [sourceName, targets] : sourceTargets) {
        if (targets.preserved.size() == 1U) {
            namedShape.elementMap[sourceName] = *targets.preserved.begin();
            continue;
        }
        if (targets.preserved.size() <= 1U) {
            continue;
        }
        for (const std::string& target : targets.preserved) {
            auto elementIt = namedShape.elements.find(target);
            if (elementIt == namedShape.elements.end()) {
                continue;
            }
            elementIt->second.status = ElementHistoryKind::Split;
            namedShape.history.push_back(
                ElementHistory {ElementHistoryKind::Split, target, {sourceName}}
            );
        }
    }
}

void addMergeHistory(NamedShape& namedShape)
{
    std::map<std::string, std::set<std::string>> aliasesByTarget;
    for (const auto& [stableName, currentName] : namedShape.elementMap) {
        if (stableName == currentName || namedShape.elements.count(currentName) == 0U) {
            continue;
        }
        aliasesByTarget[currentName].insert(stableName);
    }

    for (const auto& item : aliasesByTarget) {
        const std::string& target = item.first;
        const std::set<std::string>& aliases = item.second;
        if (aliases.size() <= 1U) {
            continue;
        }
        std::vector<std::string> sources(aliases.begin(), aliases.end());
        auto elementIt = namedShape.elements.find(target);
        if (elementIt != namedShape.elements.end()
            && elementIt->second.status != ElementHistoryKind::Split) {
            elementIt->second.status = ElementHistoryKind::Merge;
            for (const std::string& source : sources) {
                if (std::find(elementIt->second.sources.begin(), elementIt->second.sources.end(), source)
                    == elementIt->second.sources.end()) {
                    elementIt->second.sources.push_back(source);
                }
            }
        }
        const auto duplicate = std::find_if(
            namedShape.history.begin(),
            namedShape.history.end(),
            [&](const ElementHistory& entry) {
                return entry.kind == ElementHistoryKind::Merge && entry.element == target
                    && entry.sources == sources;
            }
        );
        if (duplicate == namedShape.history.end()) {
            namedShape.history.push_back(ElementHistory {ElementHistoryKind::Merge, target, sources});
        }
    }
}

void addRetagAlias(NamedShape& namedShape, const std::string& stableName, const std::string& targetName)
{
    if (stableName.empty() || targetName.empty() || stableName == targetName
        || namedShape.elements.count(targetName) == 0U) {
        return;
    }
    namedShape.elementMap[stableName] = targetName;
    auto& element = namedShape.elements[targetName];
    if (element.status == ElementHistoryKind::Indexed) {
        element.status = ElementHistoryKind::Modified;
    }
    if (std::find(element.sources.begin(), element.sources.end(), stableName)
        == element.sources.end()) {
        element.sources.push_back(stableName);
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == ElementHistoryKind::Modified && entry.element == targetName
                && entry.sources == std::vector<std::string>{stableName};
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(ElementHistory {ElementHistoryKind::Modified, targetName, {stableName}});
    }
}

void addLinkRetagAlias(NamedShape& namedShape, const NamedShapeSource& source, const std::string& stableName, const std::string& targetName)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap(), "reTagElementMap(obj->getID(), ...)" retags
    // linked topology under the Link object. cad-core keeps source-prefixed aliases so later
    // LinkSub references can resolve without guessing topology order.
    addRetagAlias(namedShape, source.owner + "." + stableName, targetName);
    if (stableName.find('.') != std::string::npos) {
        addRetagAlias(namedShape, stableName, targetName);
    }
}

void addNestedHistory(
    NamedShape& namedShape,
    ElementHistoryKind kind,
    const std::string& targetElement,
    const std::vector<std::string>& sources
)
{
    if (targetElement.empty() || sources.empty()) {
        return;
    }
    auto elementIt = namedShape.elements.find(targetElement);
    if (elementIt == namedShape.elements.end()) {
        return;
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == kind && entry.element == targetElement && entry.sources == sources;
        }
    );
    if (duplicate != namedShape.history.end()) {
        return;
    }
    if (elementIt->second.status == ElementHistoryKind::Indexed
        && (kind == ElementHistoryKind::Generated || kind == ElementHistoryKind::Modified)) {
        elementIt->second.status = kind;
    }
    for (const std::string& source : sources) {
        if (std::find(elementIt->second.sources.begin(), elementIt->second.sources.end(), source)
            == elementIt->second.sources.end()) {
            elementIt->second.sources.push_back(source);
        }
    }
    namedShape.history.push_back(ElementHistory {kind, targetElement, sources});
}

void addTerminalHistory(NamedShape& namedShape, const ElementHistory& entry)
{
    if (entry.kind != ElementHistoryKind::Deleted && entry.kind != ElementHistoryKind::Split) {
        return;
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& current) {
            return current.kind == entry.kind && current.element == entry.element
                && current.sources == entry.sources;
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(entry);
    }
}

void propagateNestedSourceHistory(NamedShape& namedShape, const std::vector<NamedShapeSource>& sources)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementShape() first calls "mapSubElement(shapes)" and then MapperMaker
    // history, so chained makers keep source-local aliases and previously generated sources.
    // cad-core only forwards nested history when an existing ElementMap entry resolves to one
    // current result element; unresolved split/deleted cases remain represented by diagnostics.
    for (const auto& source : sources) {
        if (source.namedShape == nullptr) {
            continue;
        }
        for (const ElementHistory& entry : source.namedShape->history) {
            if (entry.kind == ElementHistoryKind::Deleted || entry.kind == ElementHistoryKind::Split) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp,
                // MapperHistory keeps terminal "deleted" and "split" outcomes in the element
                // history so later updateElementReference() can still report the old reference
                // state instead of degrading it to an opaque unresolved subname.
                addTerminalHistory(namedShape, entry);
                continue;
            }
            if (entry.kind != ElementHistoryKind::Generated
                && entry.kind != ElementHistoryKind::Modified) {
                continue;
            }
            for (const std::string& sourceName : sourceElementNames(source, entry.element)) {
                const auto mapped = namedShape.elementMap.find(sourceName);
                if (mapped == namedShape.elementMap.end()) {
                    continue;
                }
                addNestedHistory(namedShape, entry.kind, mapped->second, entry.sources);
            }
        }
    }
}

std::string booleanOperationName(BooleanOperation operation)
{
    switch (operation) {
        case BooleanOperation::Fuse:
            return "fuse";
        case BooleanOperation::Cut:
            return "cut";
        case BooleanOperation::Common:
            return "intersect";
    }
    return "boolean";
}

nlohmann::json historyToJson(const ElementHistory& history)
{
    return {
        {"kind", historyKindName(history.kind)},
        {"element", history.element},
        {"sources", history.sources},
    };
}

nlohmann::json elementToJson(const NamedElement& element)
{
    return {
        {"kind", subshapeKindName(element.subshape.kind)},
        {"index", element.subshape.index},
        {"status", historyKindName(element.status)},
        {"sources", element.sources},
    };
}

}  // namespace

NamedShape indexedNamedShapeForObject(const std::string& owner, const TopoDS_Shape& shape)
{
    NamedShape namedShape;
    namedShape.owner = owner;
    namedShape.shape = shape;

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);

    addIndexedElements(namedShape, faces, TopAbs_FACE, "Face");
    addIndexedElements(namedShape, edges, TopAbs_EDGE, "Edge");
    addIndexedElements(namedShape, vertices, TopAbs_VERTEX, "Vertex");

    return namedShape;
}

NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::string& sourceOwner,
    const TopoDS_Shape& sourceShape,
    BRepBuilderAPI_MakeShape& maker
)
{
    return namedShapeForMakerHistory(
        owner,
        resultShape,
        std::vector<NamedShapeSource> {{sourceOwner, sourceShape}},
        maker
    );
}

NamedShape namedShapeForMakerHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepBuilderAPI_MakeShape& maker
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    sourceTargets[sourceName];
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                    try {
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Generated(sourceElement),
                            ElementHistoryKind::Generated,
                            sourceTargets
                        );
                        applyHistoryList(
                            namedShape,
                            sourceName,
                            maker.Modified(sourceElement),
                            ElementHistoryKind::Modified,
                            sourceTargets
                        );
                    }
                    catch (const Standard_Failure&) {
                        continue;
                    }
                }
            }
        }
    }
    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);

    return namedShape;
}

NamedShape namedShapeForPreservedSources(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

    for (const auto& source : sources) {
        for (const TopAbs_ShapeEnum kind : mappableKinds()) {
            const std::string prefix = prefixForKind(kind);
            if (prefix.empty()) {
                continue;
            }
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                const std::string localElementName = prefix + std::to_string(index);
                for (const std::string& sourceName : sourceElementNames(source, localElementName)) {
                    sourceTargets[sourceName];
                    collectSourceElementMap(namedShape, sourceName, sourceElement, kind, sourceTargets);
                }
            }
        }
    }
    applyPreservedElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, sources);
    addMergeHistory(namedShape);

    return namedShape;
}

NamedShape namedShapeForLinkedShape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    if (source.namedShape == nullptr) {
        for (const auto& [localName, element] : namedShape.elements) {
            (void)element;
            addLinkRetagAlias(namedShape, source, localName, localName);
        }
        return namedShape;
    }

    for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
        if (namedShape.elements.count(currentName) == 0U) {
            continue;
        }
        addLinkRetagAlias(namedShape, source, stableName, currentName);
    }
    return namedShape;
}

NamedShape namedShapeForLinkedSubshape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::string& sourceElementName,
    const std::string& targetElementName
)
{
    return namedShapeForLinkedSubshapes(owner, resultShape, source, {{sourceElementName, targetElementName}});
}

NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<std::pair<std::string, std::string>>& sourceToTargetElements
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::parseSubName() can keep multiple PropertyXLink sub-elements with
    // the same linked-object prefix, and checkGeoElementMap() retags resolved linked topology.
    // cad-core preserves that retag per selected source element when a LinkSub returns a compound.
    for (const auto& [sourceElementName, targetElementName] : sourceToTargetElements) {
        if (targetElementName.empty() || namedShape.elements.count(targetElementName) == 0U) {
            continue;
        }

        addLinkRetagAlias(namedShape, source, sourceElementName, targetElementName);
        if (source.namedShape == nullptr) {
            continue;
        }

        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            if (currentName == sourceElementName) {
                addLinkRetagAlias(namedShape, source, stableName, targetElementName);
            }
        }
    }
    return namedShape;
}

NamedShapeBuild makeElementBooleanFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    BooleanOperation operation
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for boolean operation"
            };
        }
    }
    if (sources.size() == 1U) {
        NamedShape namedShape = sources.front().namedShape != nullptr
            ? *sources.front().namedShape
            : indexedNamedShapeForObject(owner, sources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = sources.front().shape;
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    std::unique_ptr<BRepAlgoAPI_BooleanOperation> maker;
    switch (operation) {
        case BooleanOperation::Fuse:
            maker = std::make_unique<BRepAlgoAPI_Fuse>();
            break;
        case BooleanOperation::Cut:
            maker = std::make_unique<BRepAlgoAPI_Cut>();
            break;
        case BooleanOperation::Common:
            maker = std::make_unique<BRepAlgoAPI_Common>();
            break;
    }

    TopTools_ListOfShape arguments;
    TopTools_ListOfShape tools;
    arguments.Append(sources.front().shape);
    for (std::size_t index = 1; index < sources.size(); ++index) {
        tools.Append(sources.at(index).shape);
    }

    maker->SetArguments(arguments);
    maker->SetTools(tools);
    maker->Build();
    if (!maker->IsDone()) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "OCCT could not " + booleanOperationName(operation) + " boolean sources"
        };
    }

    const TopoDS_Shape resultShape = maker->Shape();
    return NamedShapeBuild {
        resultShape,
        namedShapeForMakerHistory(owner, resultShape, sources, *maker),
        {}
    };
}

NamedShapeBuild makeElementXorFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }

    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for XOR operation"};
        }
    }
    if (sources.size() == 1U) {
        std::optional<NamedShape> namedShape;
        if (sources.front().namedShape != nullptr) {
            namedShape = *sources.front().namedShape;
        }
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    TopoDS_Shape currentShape = sources.front().shape;
    std::optional<NamedShape> currentNamedShape;
    if (sources.front().namedShape != nullptr) {
        currentNamedShape = *sources.front().namedShape;
    }
    std::string currentOwner = sources.front().owner;

    for (std::size_t index = 1; index < sources.size(); ++index) {
        const std::vector<NamedShapeSource> stepSources {
            {currentOwner, currentShape, currentNamedShape ? &*currentNamedShape : nullptr},
            sources.at(index),
        };

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementXor(), "Step 1: Union(A, B)" then "Step 2: Common(A, B)"
        // and finally "Cut(Union, Common)" when an intersection exists.
        const auto unionBuild
            = makeElementBooleanFromSources(owner, stepSources, BooleanOperation::Fuse);
        if (!unionBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, unionBuild.error};
        }
        const TopoDS_Shape unionShape = unionBuild.shape;

        const auto commonBuild
            = makeElementBooleanFromSources(owner, stepSources, BooleanOperation::Common);
        if (!commonBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, commonBuild.error};
        }
        const TopoDS_Shape commonShape = commonBuild.shape;
        if (commonShape.IsNull()) {
            currentShape = unionShape;
            currentNamedShape = unionBuild.namedShape;
            currentOwner = owner + ".XorUnion" + std::to_string(index);
            continue;
        }

        const auto cutBuild = makeElementBooleanFromSources(
            owner,
            std::vector<NamedShapeSource> {
                {owner + ".XorUnion" + std::to_string(index),
                 unionShape,
                 unionBuild.namedShape ? &*unionBuild.namedShape : nullptr},
                {owner + ".XorCommon" + std::to_string(index),
                 commonShape,
                 commonBuild.namedShape ? &*commonBuild.namedShape : nullptr},
            },
            BooleanOperation::Cut
        );
        if (!cutBuild.error.empty()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, cutBuild.error};
        }
        currentNamedShape = cutBuild.namedShape;
        currentShape = cutBuild.shape;
        currentOwner = owner + ".XorResult" + std::to_string(index);
    }

    return NamedShapeBuild {currentShape, std::move(currentNamedShape), {}};
}

NamedShapeBuild makeElementSectionFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool approximate
)
{
    if (sources.size() != 2U) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "Section requires exactly two input shapes"
        };
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for section operation"
            };
        }
    }

    try {
        BRepAlgoAPI_Section maker;
        maker.Init1(sources.front().shape);
        maker.Init2(sources.back().shape);
        maker.Approximation(approximate);
        maker.Build();
        if (!maker.IsDone()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Section failed"};
        }
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Resulting shape is null"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForMakerHistory(owner, resultShape, sources, maker),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Section failed"
        };
    }
}

NamedShapeBuild makeElementGeneralFuseFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    double tolerance
)
{
    if (sources.empty()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null shape"};
    }
    for (const auto& source : sources) {
        if (source.shape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Null input shape for general fuse operation"
            };
        }
    }
    if (sources.size() == 1U) {
        NamedShape namedShape = sources.front().namedShape != nullptr
            ? *sources.front().namedShape
            : indexedNamedShapeForObject(owner, sources.front().shape);
        namedShape.owner = owner;
        namedShape.shape = sources.front().shape;
        return NamedShapeBuild {sources.front().shape, std::move(namedShape), {}};
    }

    try {
        BRepAlgoAPI_BuilderAlgo maker;
        maker.SetRunParallel(true);
        TopTools_ListOfShape arguments;
        for (const auto& source : sources) {
            arguments.Append(source.shape);
        }
        maker.SetArguments(arguments);
        if (tolerance > 0.0) {
            maker.SetFuzzyValue(tolerance);
        }
        maker.SetNonDestructive(Standard_True);
        maker.Build();
        if (!maker.IsDone()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "GeneralFuse failed"};
        }
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Resulting shape is null"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForMakerHistory(owner, resultShape, sources, maker),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "GeneralFuse failed"
        };
    }
}

NamedShapeBuild makeElementRefineFromSource(const std::string& owner, const NamedShapeSource& source)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for refine operation"};
    }

    try {
        geometry::BRepBuilderAPI_RefineModel maker(source.shape);
        const TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Refine produced a null shape"};
        }
        return NamedShapeBuild {
            resultShape,
            namedShapeForMakerHistory(owner, resultShape, std::vector<NamedShapeSource> {source}, maker),
            {},
        };
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Refine operation failed"
        };
    }
}

std::optional<std::string> resolveElementName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    auto resolved = resolveElementReference(namedShape, subname, stableSubname);
    if (resolved.status == ElementResolveStatus::Resolved) {
        return resolved.element;
    }
    return std::nullopt;
}

ElementResolveResult resolveElementReference(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::updateElementReference()
    // drives PropertyLinkBase::updateElementReferences() after ElementMap version changes. This
    // is the P6 identity-map baseline: stable indexed names resolve through the object-local map,
    // while opaque mapped names wait for MapperHistory-backed ElementMap entries.
    if (!stableSubname.empty()) {
        const auto mapped = namedShape.elementMap.find(stableSubname);
        if (mapped != namedShape.elementMap.end()) {
            return ElementResolveResult {ElementResolveStatus::Resolved, mapped->second};
        }
        for (const ElementHistory& entry : namedShape.history) {
            if (entry.kind == ElementHistoryKind::Deleted && entry.element == stableSubname) {
                return ElementResolveResult {ElementResolveStatus::Deleted, std::nullopt};
            }
            if (entry.kind == ElementHistoryKind::Split
                && std::find(entry.sources.begin(), entry.sources.end(), stableSubname)
                    != entry.sources.end()) {
                return ElementResolveResult {ElementResolveStatus::Split, std::nullopt};
            }
        }
        return ElementResolveResult {ElementResolveStatus::Unresolved, std::nullopt};
    }

    if (namedShape.elements.count(subname) != 0U) {
        return ElementResolveResult {ElementResolveStatus::Resolved, subname};
    }
    return ElementResolveResult {ElementResolveStatus::Unresolved, std::nullopt};
}

std::optional<TopoDS_Shape> subshapeByName(const NamedShape& namedShape, const std::string& name)
{
    const auto it = namedShape.elements.find(name);
    if (it == namedShape.elements.end()) {
        return std::nullopt;
    }
    return subshapeByName(namedShape.shape, it->second.subshape);
}

std::optional<TopoDS_Shape> subshapeByName(
    const NamedShape& namedShape,
    const std::string& subname,
    const std::string& stableSubname
)
{
    const auto resolved = resolveElementName(namedShape, subname, stableSubname);
    if (!resolved) {
        return std::nullopt;
    }
    return subshapeByName(namedShape, *resolved);
}

nlohmann::json namedShapeToJson(const NamedShape& namedShape)
{
    nlohmann::json elements = nlohmann::json::object();
    for (const auto& [name, element] : namedShape.elements) {
        elements[name] = elementToJson(element);
    }

    nlohmann::json history = nlohmann::json::array();
    for (const auto& entry : namedShape.history) {
        history.push_back(historyToJson(entry));
    }

    const bool hasMappedHistory = std::any_of(
                                      namedShape.history.begin(),
                                      namedShape.history.end(),
                                      [](const ElementHistory& item) {
                                          return item.kind != ElementHistoryKind::Indexed;
                                      }
                                  )
        || std::any_of(namedShape.elementMap.begin(),
                       namedShape.elementMap.end(),
                       [](const auto& item) { return item.first != item.second; });

    return {
        {"owner", namedShape.owner},
        {"element_map_status", hasMappedHistory ? "history_partial" : "indexed_only"},
        {"element_map", namedShape.elementMap},
        {"elements", elements},
        {"history", history},
    };
}

nlohmann::json namedShapesToJson(const std::map<std::string, NamedShape>& namedShapes)
{
    nlohmann::json result = nlohmann::json::object();
    for (const auto& [name, namedShape] : namedShapes) {
        result[name] = namedShapeToJson(namedShape);
    }
    return result;
}

}  // namespace cad_core::topo
