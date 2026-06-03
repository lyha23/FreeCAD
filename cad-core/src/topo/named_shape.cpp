#include "cad_core/topo/named_shape.h"

#include "cad_core/geometry/extrusion_helper.h"
#include "cad_core/geometry/refine_model.h"
#include "cad_core/topo/element_map.h"

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_BuilderAlgo.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>

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

void addDistinctString(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
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

void addTerminalHistory(NamedShape& namedShape, const ElementHistory& entry);

std::optional<TopAbs_ShapeEnum> elementKindFromName(const std::string& elementName)
{
    const std::size_t dot = elementName.rfind('.');
    const std::string localName = dot == std::string::npos ? elementName : elementName.substr(dot + 1);
    const auto parsed = parseSubshapeName(localName);
    if (!parsed) {
        return std::nullopt;
    }
    return parsed->kind;
}

std::set<std::string> targetsOfKind(const std::set<std::string>& targets, TopAbs_ShapeEnum kind)
{
    std::set<std::string> result;
    for (const std::string& target : targets) {
        if (elementKindFromName(target) == kind) {
            result.insert(target);
        }
    }
    return result;
}

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

int findSameShapeIndex(const TopTools_IndexedMapOfShape& shapes, const TopoDS_Shape& shape)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return index;
        }
    }
    return 0;
}

void addGeneratedHistory(NamedShape& namedShape,
                         const std::string& targetElement,
                         const std::vector<std::string>& sources)
{
    auto elementIt = namedShape.elements.find(targetElement);
    if (targetElement.empty() || sources.empty() || elementIt == namedShape.elements.end()) {
        return;
    }
    elementIt->second.status = ElementHistoryKind::Generated;
    for (const std::string& source : sources) {
        addDistinctString(elementIt->second.sources, source);
    }
    const auto duplicate = std::find_if(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return entry.kind == ElementHistoryKind::Generated && entry.element == targetElement
                && entry.sources == sources;
        }
    );
    if (duplicate == namedShape.history.end()) {
        namedShape.history.push_back(ElementHistory {ElementHistoryKind::Generated, targetElement, sources});
    }
}

bool applyHistoryShape(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& historyShape,
    ElementHistoryKind historyKind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    bool applied = false;
    for (const TopAbs_ShapeEnum kind : mappableKinds()) {
        const auto elementName = findElementName(namedShape, historyShape, kind);
        if (!elementName) {
            continue;
        }
        auto& element = namedShape.elements[*elementName];
        element.status = historyKind;
        if (std::find(element.sources.begin(), element.sources.end(), sourceName)
            == element.sources.end()) {
            element.sources.push_back(sourceName);
        }
        const auto duplicate = std::find_if(
            namedShape.history.begin(),
            namedShape.history.end(),
            [&](const ElementHistory& entry) {
                return entry.kind == historyKind && entry.element == *elementName
                    && entry.sources == std::vector<std::string>{sourceName};
            }
        );
        if (duplicate == namedShape.history.end()) {
            namedShape.history.push_back(ElementHistory {historyKind, *elementName, {sourceName}});
        }
        sourceTargets[sourceName].history.insert(*elementName);
        applied = true;
    }
    return applied;
}

bool applyHistoryList(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopTools_ListOfShape& historyShapes,
    ElementHistoryKind historyKind,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    bool applied = false;
    for (TopTools_ListIteratorOfListOfShape it(historyShapes); it.More(); it.Next()) {
        applied = applyHistoryShape(namedShape, sourceName, it.Value(), historyKind, sourceTargets)
            || applied;
    }
    return applied;
}

bool shapeContains(const TopoDS_Shape& container, const TopoDS_Shape& shape)
{
    if (container.IsNull() || shape.IsNull()) {
        return false;
    }
    if (container.IsSame(shape)) {
        return true;
    }

    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(container, shape.ShapeType(), subshapes);
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        if (subshapes(index).IsSame(shape)) {
            return true;
        }
    }
    return false;
}

bool applyThruSectionsGeneratedHistory(
    NamedShape& namedShape,
    const std::string& sourceName,
    const TopoDS_Shape& sourceShape,
    const TopoDS_Shape& sourceElement,
    BRepOffsetAPI_ThruSections& maker,
    const TopoDS_Shape& firstProfile,
    const TopoDS_Shape& lastProfile,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MapperThruSections::generated(), after MapperMaker::generated(s) is empty, tries
    // "tmaker.GeneratedFace(s)" and maps source shapes found in the first or last profile to
    // "tmaker.FirstShape()" / "tmaker.LastShape()".
    bool applied = false;
    try {
        const TopoDS_Shape generatedFace = maker.GeneratedFace(sourceElement);
        if (!generatedFace.IsNull()) {
            applied = applyHistoryShape(namedShape,
                                        sourceName,
                                        generatedFace,
                                        ElementHistoryKind::Generated,
                                        sourceTargets)
                || applied;
        }
        if (applied) {
            return true;
        }

        if (sourceElement.ShapeType() == TopAbs_FACE && sourceShape.IsSame(sourceElement)
            && !maker.FirstShape().IsNull()) {
            return applyHistoryShape(namedShape,
                                     sourceName,
                                     maker.FirstShape(),
                                     ElementHistoryKind::Generated,
                                     sourceTargets);
        }
        if (shapeContains(firstProfile, sourceElement) && !maker.FirstShape().IsNull()) {
            return applyHistoryShape(namedShape,
                                     sourceName,
                                     maker.FirstShape(),
                                     ElementHistoryKind::Generated,
                                     sourceTargets);
        }
        if (shapeContains(lastProfile, sourceElement) && !maker.LastShape().IsNull()) {
            return applyHistoryShape(namedShape,
                                     sourceName,
                                     maker.LastShape(),
                                     ElementHistoryKind::Generated,
                                     sourceTargets);
        }
    }
    catch (const Standard_Failure&) {
        return applied;
    }
    return applied;
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
    for (const std::string& aliasOwner : source.ownerAliases) {
        if (!aliasOwner.empty()) {
            addDistinctString(names, aliasOwner + "." + localElementName);
        }
    }
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

std::string taperComponentOwner(const std::string& historyOwner, std::size_t index, std::size_t count)
{
    if (count <= 1U) {
        return historyOwner;
    }
    if (index == 0U) {
        return historyOwner + ".Outer";
    }
    return historyOwner + ".Inner" + std::to_string(index);
}

NamedShape namedShapeForTaperComponent(const std::string& componentOwner,
                                       const geometry::TaperedExtrusionHistoryComponent& component,
                                       const TopoDS_Shape& profile,
                                       const NamedShapeSource& profileSource)
{
    if (component.historyMaker && !component.historySources.empty()) {
        std::vector<NamedShapeSource> sources;
        sources.reserve(component.historySources.size());
        sources.push_back(NamedShapeSource{profileSource.owner, profile, profileSource.namedShape});
        for (std::size_t index = 1; index < component.historySources.size(); ++index) {
            sources.push_back(NamedShapeSource{componentOwner + ".TaperSection" + std::to_string(index + 1),
                                               component.historySources.at(index)});
        }
        if (auto* thruSections = dynamic_cast<BRepOffsetAPI_ThruSections*>(component.historyMaker.get())) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::MapperThruSections::generated(), adds "GeneratedFace(s)", "FirstShape()" and
            // "LastShape()" to the generic BRepBuilderAPI_MakeShape mapper.
            return namedShapeForThruSectionsHistory(componentOwner,
                                                    component.shape,
                                                    sources,
                                                    *thruSections,
                                                    component.historySources.front(),
                                                    component.historySources.back());
        }
        return namedShapeForMakerHistory(componentOwner, component.shape, sources, *component.historyMaker);
    }
    return namedShapeForPreservedSources(componentOwner, component.shape, {profileSource});
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

bool sameRefineSurface(const TopoDS_Face& sourceFace, const TopoDS_Face& resultFace)
{
    const GeomAbs_SurfaceType sourceType = geometry::model_refine::FaceTypedBase::getFaceType(sourceFace);
    if (sourceType != geometry::model_refine::FaceTypedBase::getFaceType(resultFace)) {
        return false;
    }

    switch (sourceType) {
        case GeomAbs_Plane:
            return geometry::model_refine::getPlaneObject().isEqual(sourceFace, resultFace);
        case GeomAbs_Cylinder:
            return geometry::model_refine::getCylinderObject().isEqual(sourceFace, resultFace);
        case GeomAbs_BSplineSurface:
            return geometry::model_refine::getBSplineObject().isEqual(sourceFace, resultFace);
        default:
            return false;
    }
}

void applyRefineGenericGeneratedHistory(
    NamedShape& namedShape,
    const NamedShapeSource& source,
    const TopoDS_Shape& resultShape,
    std::map<std::string, SourceTargets>& sourceTargets
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementRefine(), "GenericShapeMapper mapper; mkRefine.populate(mapper);
    // mapper.init(shape, mkRefine.Shape())". GenericShapeMapper::init() marks result faces
    // absent from the source as generated from a source face sharing two edges, or from a matching
    // surface among candidate source faces.
    if (source.shape.IsNull() || resultShape.IsNull()) {
        return;
    }

    TopTools_IndexedMapOfShape sourceFaces;
    TopTools_IndexedMapOfShape sourceEdges;
    TopTools_IndexedMapOfShape resultFaces;
    TopTools_IndexedDataMapOfShapeListOfShape edgeToFaces;
    TopExp::MapShapes(source.shape, TopAbs_FACE, sourceFaces);
    TopExp::MapShapes(source.shape, TopAbs_EDGE, sourceEdges);
    TopExp::MapShapes(resultShape, TopAbs_FACE, resultFaces);
    TopExp::MapShapesAndAncestors(source.shape, TopAbs_EDGE, TopAbs_FACE, edgeToFaces);

    for (int faceIndex = 1; faceIndex <= resultFaces.Extent(); ++faceIndex) {
        const TopoDS_Shape& resultFaceShape = resultFaces(faceIndex);
        if (findSameShapeIndex(sourceFaces, resultFaceShape) != 0) {
            continue;
        }
        const auto resultElementName = findElementName(namedShape, resultFaceShape, TopAbs_FACE);
        if (!resultElementName) {
            continue;
        }
        const auto resultElement = namedShape.elements.find(*resultElementName);
        if (resultElement != namedShape.elements.end()
            && resultElement->second.status == ElementHistoryKind::Modified) {
            continue;
        }

        std::map<int, int> sourceFaceEdgeCount;
        int generatedSourceFace = 0;
        for (TopExp_Explorer edgeIt(resultFaceShape, TopAbs_EDGE); edgeIt.More(); edgeIt.Next()) {
            const int sourceEdgeIndex = findSameShapeIndex(sourceEdges, edgeIt.Current());
            if (sourceEdgeIndex == 0 || !edgeToFaces.Contains(sourceEdges(sourceEdgeIndex))) {
                continue;
            }

            const TopoDS_Edge sourceEdge = TopoDS::Edge(sourceEdges(sourceEdgeIndex));
            const TopTools_ListOfShape& faces = edgeToFaces.FindFromKey(sourceEdges(sourceEdgeIndex));
            for (TopTools_ListIteratorOfListOfShape faceIt(faces); faceIt.More(); faceIt.Next()) {
                const int sourceFaceIndex = findSameShapeIndex(sourceFaces, faceIt.Value());
                if (sourceFaceIndex == 0) {
                    continue;
                }
                if (BRep_Tool::IsClosed(sourceEdge)) {
                    generatedSourceFace = sourceFaceIndex;
                    break;
                }
                if (++sourceFaceEdgeCount[sourceFaceIndex] == 2) {
                    generatedSourceFace = sourceFaceIndex;
                    break;
                }
            }
            if (generatedSourceFace != 0) {
                break;
            }
        }

        if (generatedSourceFace == 0) {
            const TopoDS_Face resultFace = TopoDS::Face(resultFaceShape);
            for (const auto& item : sourceFaceEdgeCount) {
                if (sameRefineSurface(TopoDS::Face(sourceFaces(item.first)), resultFace)) {
                    generatedSourceFace = item.first;
                    break;
                }
            }
        }
        if (generatedSourceFace == 0) {
            continue;
        }

        const std::string localSourceName = "Face" + std::to_string(generatedSourceFace);
        for (const std::string& sourceName : sourceElementNames(source, localSourceName)) {
            applyHistoryShape(namedShape,
                              sourceName,
                              resultFaceShape,
                              ElementHistoryKind::Generated,
                              sourceTargets);
        }
    }
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
        // The same function walks Vertex, Edge and Face ShapeInfo separately and skips modified
        // history when "newInfo.type != newShape.ShapeType()", while generated lower elements
        // remain available as generated names. cad-core follows that priority: preserved source
        // subelements resolve first; one-to-one same-kind history fills the remaining keys;
        // one-to-many same-kind history is recorded as split and left unresolved.
        if (targets.preserved.size() == 1U) {
            namedShape.elementMap[sourceName] = *targets.preserved.begin();
            continue;
        }
        if (targets.preserved.size() > 1U) {
            applySplit(sourceName, targets.preserved);
            continue;
        }

        const auto sourceKind = elementKindFromName(sourceName);
        if (sourceKind) {
            const std::set<std::string> sameKindHistory = targetsOfKind(targets.history, *sourceKind);
            if (sameKindHistory.size() == 1U) {
                namedShape.elementMap[sourceName] = *sameKindHistory.begin();
                continue;
            }
            if (sameKindHistory.size() > 1U) {
                applySplit(sourceName, sameKindHistory);
                continue;
            }
        }

        if (targets.history.size() == 1U) {
            namedShape.elementMap[sourceName] = *targets.history.begin();
            continue;
        }
        if (targets.history.size() > 1U) {
            applySplit(sourceName, targets.history);
            continue;
        }
        addTerminalHistory(namedShape, ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}});
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
    for (const std::string& aliasOwner : source.ownerAliases) {
        addRetagAlias(namedShape, aliasOwner + "." + stableName, targetName);
    }
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
    if (kind == ElementHistoryKind::Merge
        && elementIt->second.status != ElementHistoryKind::Split) {
        elementIt->second.status = kind;
    }
    else if (elementIt->second.status == ElementHistoryKind::Indexed
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

void addSplitHistory(NamedShape& namedShape, const std::string& sourceName, const std::string& targetName)
{
    auto elementIt = namedShape.elements.find(targetName);
    if (sourceName.empty() || elementIt == namedShape.elements.end()) {
        return;
    }
    elementIt->second.status = ElementHistoryKind::Split;
    addDistinctString(elementIt->second.sources, sourceName);
    addTerminalHistory(namedShape, ElementHistory {ElementHistoryKind::Split, targetName, {sourceName}});
}

void propagateNestedSourceHistory(NamedShape& namedShape, const std::vector<NamedShapeSource>& sources)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap(), calls "mapSubElement(shapes)" before MapperMaker;
    // MapperHistory then queries "Modified(s)" and "Generated(s)". Chained makers keep the
    // existing ElementMap ledger first, so generated/modified/merge history from the source
    // remains observable after a later maker or App::Link retag.
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
                && entry.kind != ElementHistoryKind::Modified
                && entry.kind != ElementHistoryKind::Merge) {
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

void consumeSketchInternalGeneratedFaceHistory(NamedShape& namedShape,
                                               const TopoDS_Shape& internalShape,
                                               const nlohmann::json& internalMap,
                                               const SketchInternalHistoryContext* historyContext)
{
    if (!internalMap.is_object()) {
        return;
    }

    TopTools_IndexedMapOfShape internalFaces;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(internalShape, TopAbs_FACE, internalFaces);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);
    for (int faceIndex = 1; faceIndex <= internalFaces.Extent(); ++faceIndex) {
        TopTools_IndexedMapOfShape faceEdges;
        const TopoDS_Face face = TopoDS::Face(internalFaces(faceIndex));
        const TopoDS_Wire outerWire = BRepTools::OuterWire(face);
        TopExp::MapShapes(outerWire.IsNull() ? internalFaces(faceIndex) : outerWire, TopAbs_EDGE, faceEdges);
        std::vector<std::string> sources;
        for (int edgeIndex = 1; edgeIndex <= faceEdges.Extent(); ++edgeIndex) {
            const int internalEdgeIndex = findSameShapeIndex(internalEdges, faceEdges(edgeIndex));
            if (internalEdgeIndex <= 0) {
                continue;
            }
            const std::string internalEdgeName = "InternalEdge" + std::to_string(internalEdgeIndex);
            const auto mappedIt = internalMap.find(internalEdgeName);
            if (mappedIt == internalMap.end() || !mappedIt->is_string()) {
                continue;
            }
            const std::string rawName = mappedIt->get<std::string>();
            if (rawName.rfind("Edge", 0) == 0) {
                addDistinctString(sources, rawName);
            }
        }
        if (sources.empty() && historyContext != nullptr && historyContext->sourceEdgeCount == 1U
            && (historyContext->preSplitHistory || historyContext->splitterHistory)) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
            // ::splitSelfIntersecting() records the single original edge in myPreSplitHistory;
            // all bounded regions returned by BuilderFace are generated from that source even when
            // no exact InternalEdge alias survives getInternalElementMap().
            sources.push_back("Edge1");
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
        // ::FaceMaker::postBuild(), consumes FaceMaker history before
        // SketchObject::getInternalElementMap() publishes InternalEdge aliases. cad-core does not
        // invent terminal split/deleted history here; it only retags each generated InternalFace
        // with exact source edges already present in the request-local InternalEdge map.
        addGeneratedHistory(namedShape, "InternalFace" + std::to_string(faceIndex), sources);
    }
}

void consumeSketchInternalTerminalHistory(NamedShape& namedShape,
                                          const TopoDS_Shape& rawShape,
                                          const TopoDS_Shape& internalShape,
                                          const nlohmann::json& internalMap,
                                          const SketchInternalHistoryContext& history)
{
    const bool hasHistoryStage = history.preSplitHistory || history.splitterHistory
        || history.preSplitEdgeCount > history.sourceEdgeCount
        || history.splitterEdgeCount > history.sourceEdgeCount;
    if (!hasHistoryStage) {
        return;
    }
    const bool hasSplitHistoryStage = history.preSplitHistory || history.splitterHistory;

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape rawVertices;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);

    std::vector<std::string> splitTargets;
    for (int index = 1; index <= internalEdges.Extent(); ++index) {
        const std::string internalName = "InternalEdge" + std::to_string(index);
        const auto mappedIt = internalMap.find(internalName);
        if (mappedIt != internalMap.end() && mappedIt->is_string()
            && mappedIt->get<std::string>().rfind("Edge", 0) == 0) {
            continue;
        }
        splitTargets.push_back(internalName);
    }

    const int sourceEdgeCount = std::min(rawEdges.Extent(), static_cast<int>(history.sourceEdgeCount));
    for (int index = 1; index <= sourceEdgeCount; ++index) {
        const std::string sourceName = "Edge" + std::to_string(index);
        if (internalMap.contains(sourceName)) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
        // ::FaceMaker::postBuild(), chains MapperHistory(myPreSplitHistory) and
        // MapperMaker(mySplitter). When those histories exist and getInternalElementMap() cannot
        // produce an exact EdgeN alias, the source edge reached terminal split/deleted history.
        // This consumes the maker-history stage plus exact ElementMap absence; it does not sample
        // raw/internal geometry to invent ownership.
        if (!hasSplitHistoryStage || splitTargets.empty()) {
            addTerminalHistory(namedShape, ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}});
            continue;
        }
        for (const std::string& targetName : splitTargets) {
            addSplitHistory(namedShape, sourceName, targetName);
        }
    }

    if (!hasSplitHistoryStage) {
        for (int index = 1; index <= rawVertices.Extent(); ++index) {
            const std::string sourceName = "Vertex" + std::to_string(index);
            if (!internalMap.contains(sourceName)) {
                addTerminalHistory(namedShape,
                                   ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}});
            }
        }
    }
}

bool resultWireOpenExportEntryRequiresProducerIdentity(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry)
{
    return entry.helperOpenExportOverride;
}

bool resultWireProducerIdentityMissing(const SketchInternalWireJoinerOpenExportHistoryEntry& entry)
{
    if (!resultWireOpenExportEntryRequiresProducerIdentity(entry)) {
        return false;
    }
    if (entry.resultWireProducerKind.empty() || entry.resultWireProducerState.empty()
        || entry.resultWireProducerBlocker.empty()) {
        return true;
    }
    return entry.resultWireProducerKind == "None" && entry.resultWireProducerBlocker == "None";
}

bool sourceEdgeLineageConsumedByNamedShape(const NamedShape& namedShape,
                                           const std::string& sourceName)
{
    if (namedShape.elementMap.count(sourceName) != 0U) {
        return true;
    }
    return std::any_of(
        namedShape.history.begin(),
        namedShape.history.end(),
        [&](const ElementHistory& entry) {
            return std::find(entry.sources.begin(), entry.sources.end(), sourceName)
                != entry.sources.end();
        });
}

bool resultWireElementMapIdentityMismatch(const NamedShape& namedShape,
                                          const SketchInternalWireJoinerOpenExportHistoryEntry& entry)
{
    if (!resultWireOpenExportEntryRequiresProducerIdentity(entry)) {
        return false;
    }
    if (entry.sourceEdgeIndices.empty()) {
        return true;
    }

    return std::any_of(
        entry.sourceEdgeIndices.begin(),
        entry.sourceEdgeIndices.end(),
        [&](std::size_t sourceEdgeIndex) {
            const std::string sourceName = "Edge" + std::to_string(sourceEdgeIndex + 1U);
            return !sourceEdgeLineageConsumedByNamedShape(namedShape, sourceName);
        });
}

void refreshSketchInternalResultWireIdentityCounters(NamedShape& namedShape)
{
    if (!namedShape.sketchInternalHistory) {
        return;
    }

    SketchInternalHistoryContext& history = *namedShape.sketchInternalHistory;
    history.namedShapeHistoryMissingResultWireIdentityCount = 0;
    history.elementMapResultWireIdentityMismatchCount = 0;
    for (const SketchInternalWireJoinerOpenExportHistoryEntry& entry :
         history.wireJoinerOpenExportHistoryEntries) {
        if (resultWireProducerIdentityMissing(entry)) {
            ++history.namedShapeHistoryMissingResultWireIdentityCount;
        }
        if (resultWireElementMapIdentityMismatch(namedShape, entry)) {
            ++history.elementMapResultWireIdentityMismatchCount;
        }
    }
}

nlohmann::json sketchInternalHistoryToJson(const SketchInternalHistoryContext& history)
{
    nlohmann::json wireJoinerOpenExportEntries = nlohmann::json::array();
    for (const SketchInternalWireJoinerOpenExportHistoryEntry& entry :
         history.wireJoinerOpenExportHistoryEntries) {
        wireJoinerOpenExportEntries.push_back({
            {"open_export_index", entry.openExportIndex},
            {"edge_info_index", entry.edgeInfoIndex},
            {"result_wire_producer_kind", entry.resultWireProducerKind},
            {"result_wire_producer_state", entry.resultWireProducerState},
            {"result_wire_producer_blocker", entry.resultWireProducerBlocker},
            {"result_wire_producer_source_edge_info_index",
             entry.resultWireProducerSourceEdgeInfoIndex},
            {"result_wire_producer_root_edge_info_index", entry.resultWireProducerRootEdgeInfoIndex},
            {"result_wire_producer_current_member_edge_info_index",
             entry.resultWireProducerCurrentMemberEdgeInfoIndex},
            {"result_wire_producer_child_wire_info_index",
             entry.resultWireProducerChildWireInfoIndex},
            {"source_edge_indices", entry.sourceEdgeIndices},
            {"source_lineage_from_splitter_history", entry.sourceLineageFromSplitterHistory},
            {"helper_open_export_override", entry.helperOpenExportOverride},
            {"helper_open_export_override_reason", entry.helperOpenExportOverrideReason},
            {"purge_bridge", entry.purgeBridge},
        });
    }
    return {
        {"source_edge_count", history.sourceEdgeCount},
        {"pre_split_edge_count", history.preSplitEdgeCount},
        {"splitter_edge_count", history.splitterEdgeCount},
        {"bounded_face_count", history.boundedFaceCount},
        {"pre_split_history", history.preSplitHistory},
        {"splitter_history", history.splitterHistory},
        {"wire_joiner_source_edge_count", history.wireJoinerSourceEdgeCount},
        {"wire_joiner_split_result_edge_count", history.wireJoinerSplitResultEdgeCount},
        {"wire_joiner_open_export_edge_count", history.wireJoinerOpenExportEdgeCount},
        {"wire_joiner_open_export_source_lineage_edge_count",
         history.wireJoinerOpenExportSourceLineageEdgeCount},
        {"wire_joiner_open_export_missing_source_lineage_edge_count",
         history.wireJoinerOpenExportMissingSourceLineageEdgeCount},
        {"wire_joiner_open_export_helper_override_edge_count",
         history.wireJoinerOpenExportHelperOverrideEdgeCount},
        {"wire_joiner_open_export_helper_override_missing_source_lineage_edge_count",
         history.wireJoinerOpenExportHelperOverrideMissingSourceLineageEdgeCount},
        {"wire_joiner_open_export_purge_bridge_edge_count",
         history.wireJoinerOpenExportPurgeBridgeEdgeCount},
        {"wire_joiner_open_export_history_entries", std::move(wireJoinerOpenExportEntries)},
        {"named_shape_history_missing_result_wire_identity_count",
         history.namedShapeHistoryMissingResultWireIdentityCount},
        {"element_map_result_wire_identity_mismatch_count",
         history.elementMapResultWireIdentityMismatchCount},
        {"wire_joiner_modified_source_edge_count", history.wireJoinerModifiedSourceEdgeCount},
        {"wire_joiner_modified_history_count", history.wireJoinerModifiedHistoryCount},
        {"wire_joiner_generated_history_count", history.wireJoinerGeneratedHistoryCount},
        {"wire_joiner_deleted_history_count", history.wireJoinerDeletedHistoryCount},
        {"wire_joiner_splitter_history", history.wireJoinerSplitterHistory},
        {"wire_joiner_final_export_history", history.wireJoinerFinalExportHistory},
    };
}

std::vector<std::string> elementHistoryStatusForNamedShape(const NamedShape& namedShape)
{
    std::vector<std::string> statuses;
    bool hasGenerated = false;
    bool hasModified = false;
    bool hasDeleted = false;
    bool hasSplit = false;
    bool hasMerge = false;
    for (const ElementHistory& entry : namedShape.history) {
        hasGenerated = hasGenerated || entry.kind == ElementHistoryKind::Generated;
        hasModified = hasModified || entry.kind == ElementHistoryKind::Modified;
        hasDeleted = hasDeleted || entry.kind == ElementHistoryKind::Deleted;
        hasSplit = hasSplit || entry.kind == ElementHistoryKind::Split;
        hasMerge = hasMerge || entry.kind == ElementHistoryKind::Merge;
    }
    if (hasGenerated || hasModified) {
        statuses.push_back("history_consumed:generated_modified");
    }
    if (hasSplit || hasDeleted) {
        statuses.push_back("terminal_history:split_deleted");
    }
    if (hasMerge) {
        statuses.push_back("history_consumed:merge");
    }
    return statuses;
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

NamedShape namedShapeForSketchInternalShape(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    std::optional<SketchInternalHistoryContext> historyContext
)
{
    NamedShape namedShape;
    namedShape.owner = owner + ".InternalShape";
    namedShape.shape = internalShape;
    namedShape.sketchInternalHistory = historyContext;

    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(internalShape, TopAbs_FACE, faces);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, edges);
    TopExp::MapShapes(internalShape, TopAbs_VERTEX, vertices);

    addIndexedElements(namedShape, faces, TopAbs_FACE, "InternalFace");
    addIndexedElements(namedShape, edges, TopAbs_EDGE, "InternalEdge");
    addIndexedElements(namedShape, vertices, TopAbs_VERTEX, "InternalVertex");

    const nlohmann::json internalMap = internalElementMapForSketch(rawShape, internalShape);
    if (!internalMap.is_object()) {
        return namedShape;
    }

    for (const auto& [name, mapped] : internalMap.items()) {
        if (!mapped.is_string()) {
            continue;
        }
        const std::string target = mapped.get<std::string>();
        if (name.rfind("Internal", 0) == 0) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::getInternalElementMap(), stores both "internalElementMap[prefix] = names.front()"
            // and "internalElementMap[names.front()] = prefix". Face entries are absent because
            // the function iterates only TopAbs_VERTEX and TopAbs_EDGE.
            addRetagAlias(namedShape, target, name);
        }
    }
    consumeSketchInternalGeneratedFaceHistory(
        namedShape, internalShape, internalMap, historyContext ? &*historyContext : nullptr);
    if (historyContext) {
        consumeSketchInternalTerminalHistory(namedShape, rawShape, internalShape, internalMap, *historyContext);
    }

    // Geometry-only generated/split/deleted history probes used to write NamedShape.history here.
    // FreeCAD's real path is FaceMaker::postBuild() -> MapperHistory(myPreSplitHistory) /
    // MapperMaker(mySplitter) -> TopoShape::makeShapeWithElementMap(); until cad-core carries that
    // history context, topo must not invent terminal history from raw/internal shape sampling.
    if (historyContext && historyContext->preSplitHistory) {
        addDistinctString(namedShape.elementHistoryStatus, "facemaker_history:pre_split");
    }
    if (historyContext && historyContext->splitterHistory) {
        addDistinctString(namedShape.elementHistoryStatus, "facemaker_history:splitter");
    }
    if (historyContext && historyContext->wireJoinerSplitterHistory) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(), passes "MapperHistory(aHistory)" into
        // makeShapeWithElementMap(). This status marks consumption of WireJoiner-produced
        // splitter history summary only; it does not infer element history from geometry.
        addDistinctString(namedShape.elementHistoryStatus, "wire_joiner_history:splitter");
    }
    if (historyContext && historyContext->wireJoinerModifiedHistoryCount > 0U) {
        addDistinctString(namedShape.elementHistoryStatus, "wire_joiner_history:modified");
    }
    if (historyContext && historyContext->wireJoinerGeneratedHistoryCount > 0U) {
        addDistinctString(namedShape.elementHistoryStatus, "wire_joiner_history:generated");
    }
    if (historyContext && historyContext->wireJoinerDeletedHistoryCount > 0U) {
        addDistinctString(namedShape.elementHistoryStatus, "wire_joiner_history:deleted");
    }
    if (historyContext && historyContext->wireJoinerFinalExportHistory) {
        addDistinctString(namedShape.elementHistoryStatus, "wire_joiner_history:open_export");
    }
    refreshSketchInternalResultWireIdentityCounters(namedShape);

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

NamedShape namedShapeForThruSectionsHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<NamedShapeSource>& sources,
    BRepOffsetAPI_ThruSections& maker,
    const TopoDS_Shape& firstProfile,
    const TopoDS_Shape& lastProfile
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

                    bool generated = false;
                    try {
                        generated = applyHistoryList(namedShape,
                                                     sourceName,
                                                     maker.Generated(sourceElement),
                                                     ElementHistoryKind::Generated,
                                                     sourceTargets);
                    }
                    catch (const Standard_Failure&) {
                        generated = false;
                    }
                    if (!generated) {
                        applyThruSectionsGeneratedHistory(namedShape,
                                                          sourceName,
                                                          source.shape,
                                                          sourceElement,
                                                          maker,
                                                          firstProfile,
                                                          lastProfile,
                                                          sourceTargets);
                    }
                    try {
                        applyHistoryList(namedShape,
                                         sourceName,
                                         maker.Modified(sourceElement),
                                         ElementHistoryKind::Modified,
                                         sourceTargets);
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

std::optional<NamedShape> namedShapeForTaperedExtrusionHistory(
    const std::string& owner,
    const geometry::TaperedExtrusionResult& tapered,
    const TopoDS_Shape& profile,
    const NamedShapeSource& profileSource
)
{
    if (tapered.historyComponents.empty()) {
        return std::nullopt;
    }

    const std::size_t count = tapered.historyComponents.size();
    std::string currentOwner = taperComponentOwner(owner, 0, count);
    TopoDS_Shape currentShape = tapered.historyComponents.front().shape;
    NamedShape currentNamedShape =
        namedShapeForTaperComponent(currentOwner, tapered.historyComponents.front(), profile, profileSource);

    for (std::size_t index = 1; index < count; ++index) {
        const std::string innerOwner = taperComponentOwner(owner, index, count);
        NamedShape innerNamedShape =
            namedShapeForTaperComponent(innerOwner, tapered.historyComponents.at(index), profile, profileSource);
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp
        // ::ExtrusionHelper::makeElementDraft(), "Inner wires are lofted into separate solids and
        // then cut from the outer solid"; cad-core routes the same owner chain through topo boolean
        // history so inner-wire generated sources survive the final taper result.
        const auto cut = makeElementBooleanFromSources(
            owner,
            {
                NamedShapeSource{currentOwner, currentShape, &currentNamedShape},
                NamedShapeSource{innerOwner, tapered.historyComponents.at(index).shape, &innerNamedShape},
            },
            BooleanOperation::Cut
        );
        if (cut.error.empty() && cut.namedShape) {
            currentOwner = owner + ".InnerCut" + std::to_string(index);
            currentShape = cut.shape;
            currentNamedShape = *cut.namedShape;
        }
    }

    currentNamedShape.owner = owner;
    currentNamedShape.shape = tapered.shape;
    return currentNamedShape;
}

NamedShape namedShapeForRefineHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    geometry::BRepBuilderAPI_RefineModel& maker
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::MyRefineMaker::populate(), "mapper.populate(MappingStatus::Modified, it.Key(),
    // it.Value())"; ::TopoShape::makeElementRefine() then calls "mapper.init(shape,
    // mkRefine.Shape())" before makeShapeWithElementMap().
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    std::map<std::string, SourceTargets> sourceTargets;

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
                    applyHistoryList(namedShape,
                                     sourceName,
                                     maker.Modified(sourceElement),
                                     ElementHistoryKind::Modified,
                                     sourceTargets);
                    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.h
                    // ::BRepBuilderAPI_RefineModel exposes "IsDeleted(const TopoDS_Shape& S)";
                    // TopoShapeExpansion.cpp::makeElementRefine() routes that mapper into
                    // makeShapeWithElementMap(), so refined-away source elements remain terminal
                    // deleted history for later updateElementReference().
                    if (maker.IsDeleted(sourceElement) && sourceTargets[sourceName].preserved.empty()
                        && sourceTargets[sourceName].history.empty()) {
                        addTerminalHistory(
                            namedShape,
                            ElementHistory {ElementHistoryKind::Deleted, sourceName, {sourceName}}
                        );
                    }
                }
                catch (const Standard_Failure&) {
                    continue;
                }
            }
        }
    }

    applyRefineGenericGeneratedHistory(namedShape, source, resultShape, sourceTargets);
    applyHistoryElementMap(namedShape, sourceTargets);
    propagateNestedSourceHistory(namedShape, std::vector<NamedShapeSource> {source});
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
    propagateNestedSourceHistory(namedShape, {source});
    addMergeHistory(namedShape);
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
    return namedShapeForLinkedSubshapes(
        owner,
        resultShape,
        source,
        std::vector<std::pair<std::string, std::string>>{{sourceElementName, targetElementName}}
    );
}

NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<std::pair<std::string, std::string>>& sourceToTargetElements
)
{
    std::vector<LinkedSubshapeRetag> retags;
    retags.reserve(sourceToTargetElements.size());
    for (const auto& [sourceElementName, targetElementName] : sourceToTargetElements) {
        retags.push_back(LinkedSubshapeRetag{sourceElementName, targetElementName, {}});
    }
    return namedShapeForLinkedSubshapes(owner, resultShape, source, retags);
}

NamedShape namedShapeForLinkedSubshapes(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source,
    const std::vector<LinkedSubshapeRetag>& sourceToTargetElements
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::parseSubName() can keep multiple PropertyXLink sub-elements with
    // the same linked-object prefix, and checkGeoElementMap() retags resolved linked topology.
    // cad-core preserves that retag per selected source element when a LinkSub returns a compound.
    for (const auto& retag : sourceToTargetElements) {
        const std::string& sourceElementName = retag.sourceElementName;
        const std::string& targetElementName = retag.targetElementName;
        if (targetElementName.empty() || namedShape.elements.count(targetElementName) == 0U) {
            continue;
        }

        addLinkRetagAlias(namedShape, source, sourceElementName, targetElementName);
        for (const std::string& alias : retag.exactAliases) {
            addRetagAlias(namedShape, alias, targetElementName);
        }
        if (source.namedShape == nullptr) {
            continue;
        }

        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            if (currentName == sourceElementName) {
                addLinkRetagAlias(namedShape, source, stableName, targetElementName);
            }
        }
    }
    if (source.namedShape != nullptr) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::checkGeoElementMap(), after getSubObject() resolves linked
        // geometry, calls "geoData->reTagElementMap(...)" so copied ElementMap terminal
        // outcomes remain visible to later PropertyLinkSub::updateElementReference().
        propagateNestedSourceHistory(namedShape, {source});
        addMergeHistory(namedShape);
    }
    return namedShape;
}

NamedShape namedShapeForTransformedCopy(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const NamedShapeSource& source
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementTransform(), after transforming/copying the shape, calls
    // "copyElementMap(tmp, op)" instead of deriving ownership from result geometry.
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);

    for (const auto& [elementName, element] : namedShape.elements) {
        (void)element;
        addRetagAlias(namedShape, source.owner + "." + elementName, elementName);
    }

    if (source.namedShape != nullptr) {
        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            addRetagAlias(namedShape, stableName, currentName);
        }
        propagateNestedSourceHistory(namedShape, {source});
    }
    addMergeHistory(namedShape);

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
            namedShapeForRefineHistory(owner, resultShape, source, maker),
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
        }
        bool split = false;
        for (const ElementHistory& entry : namedShape.history) {
            if (entry.kind == ElementHistoryKind::Split
                && std::find(entry.sources.begin(), entry.sources.end(), stableSubname)
                    != entry.sources.end()) {
                // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
                // ::updateElementReference() preserves the user-visible subname when an old
                // stable reference cannot be collapsed to one ElementMap target. If SubList
                // already names one concrete split target, cad-core resolves that explicit choice
                // instead of reporting the whole stable reference as ambiguous.
                if (entry.element == subname && namedShape.elements.count(subname) != 0U) {
                    return ElementResolveResult {ElementResolveStatus::Resolved, subname};
                }
                split = true;
            }
        }
        if (split) {
            return ElementResolveResult {ElementResolveStatus::Split, std::nullopt};
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
    std::vector<std::string> elementHistoryStatus = namedShape.elementHistoryStatus;
    for (const std::string& status : elementHistoryStatusForNamedShape(namedShape)) {
        addDistinctString(elementHistoryStatus, status);
    }

    nlohmann::json result = {
        {"owner", namedShape.owner},
        {"element_map_status", hasMappedHistory ? "history_partial" : "indexed_only"},
        {"element_history_status", elementHistoryStatus},
        {"element_map", namedShape.elementMap},
        {"elements", elements},
        {"history", history},
    };
    if (namedShape.sketchInternalHistory) {
        result["sketch_internal_history"] = sketchInternalHistoryToJson(*namedShape.sketchInternalHistory);
        result["sketch_internal_history_status"] = "history_partial:facemaker_buildface";
    }
    return result;
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
