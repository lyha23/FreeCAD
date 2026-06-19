#include "cad_core/part/topo_shape_expansion.h"

#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFill.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
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

int subshapeCount(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shape, kind, subshapes);
    return subshapes.Extent();
}

std::optional<TopoDS_Shape> singleSubshape(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shape, kind, subshapes);
    if (subshapes.Extent() != 1) {
        return std::nullopt;
    }
    return subshapes(1);
}

std::optional<TopoDS_Wire> wireFromEdges(const TopoDS_Shape& shape)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    bool hasEdge = false;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        wireBuilder.Add(TopoDS::Edge(explorer.Current()));
        hasEdge = true;
    }
    if (!hasEdge) {
        return std::nullopt;
    }
    wireBuilder.Build();
    if (!wireBuilder.IsDone() || wireBuilder.Wire().IsNull()) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<std::vector<TopoDS_Shape>> prepareLoftProfiles(
    const std::vector<NamedShapeSource>& sources,
    std::string& error
)
{
    std::vector<TopoDS_Shape> profiles;
    profiles.reserve(sources.size());
    for (const auto& source : sources) {
        TopoDS_Shape shape = source.shape;
        if (shape.IsNull()) {
            error = "Null input shape";
            return std::nullopt;
        }

        if (subshapeCount(shape, TopAbs_FACE) == 1) {
            if (const auto face = singleSubshape(shape, TopAbs_FACE)) {
                shape = *face;
            }
        }
        else if (subshapeCount(shape, TopAbs_WIRE) == 0 && subshapeCount(shape, TopAbs_EDGE) > 0) {
            const auto wire = wireFromEdges(shape);
            if (!wire) {
                error = "Profile shape is not a single vertex, edge, wire nor face.";
                return std::nullopt;
            }
            shape = *wire;
        }

        if (subshapeCount(shape, TopAbs_WIRE) == 1) {
            profiles.push_back(*singleSubshape(shape, TopAbs_WIRE));
            continue;
        }
        if (subshapeCount(shape, TopAbs_VERTEX) == 1) {
            profiles.push_back(*singleSubshape(shape, TopAbs_VERTEX));
            continue;
        }

        error = "Profile shape is not a single vertex, edge, wire nor face.";
        return std::nullopt;
    }
    if (profiles.empty()) {
        error = "No profile";
        return std::nullopt;
    }
    return profiles;
}

bool loftProfilesHaveSufficientSeparation(const TopoDS_Shape& left, const TopoDS_Shape& right)
{
    try {
        Bnd_Box leftBounds;
        Bnd_Box rightBounds;
        BRepBndLib::Add(left, leftBounds);
        BRepBndLib::Add(right, rightBounds);
        if (leftBounds.IsVoid() || rightBounds.IsVoid()) {
            return false;
        }
        if (!leftBounds.CornerMin().IsEqual(rightBounds.CornerMin(), Precision::Confusion())) {
            return true;
        }
        if (!leftBounds.CornerMax().IsEqual(rightBounds.CornerMax(), Precision::Confusion())) {
            return true;
        }
    }
    catch (const Standard_Failure&) {
        return false;
    }

    auto centerOfGravity = [](const TopoDS_Shape& shape) -> std::optional<gp_Pnt> {
        try {
            if (shape.ShapeType() == TopAbs_VERTEX) {
                return BRep_Tool::Pnt(TopoDS::Vertex(shape));
            }

            GProp_GProps properties;
            if (subshapeCount(shape, TopAbs_SOLID) > 0 || shape.ShapeType() == TopAbs_SOLID
                || shape.ShapeType() == TopAbs_COMPSOLID) {
                BRepGProp::VolumeProperties(shape, properties);
            }
            else if (subshapeCount(shape, TopAbs_FACE) > 0 || shape.ShapeType() == TopAbs_FACE
                     || shape.ShapeType() == TopAbs_SHELL) {
                BRepGProp::SurfaceProperties(shape, properties);
            }
            else {
                BRepGProp::LinearProperties(shape, properties);
            }
            if (std::abs(properties.Mass()) <= Precision::Confusion()) {
                return std::nullopt;
            }
            return properties.CentreOfMass();
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    };

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementLoft() checkProfiles(), after bbox equality calls
    // "getCenterOfGravity(center)" and accepts profiles when centers differ.
    const auto leftCenter = centerOfGravity(left);
    const auto rightCenter = centerOfGravity(right);
    if (!leftCenter || !rightCenter) {
        return true;
    }
    return !leftCenter->IsEqual(*rightCenter, Precision::Confusion());
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

NamedShapeBuild makeElementLoftFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool ruled,
    bool closed,
    int maxDegree
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementLoft(), uses BRepOffsetAPI_ThruSections(isSolid, isRuled),
    // "SetMaxDegree(maxDegree)", profile AddVertex/AddWire, Closed first-profile duplication,
    // CheckCompatibility(Standard_True), Build(), and MapperThruSections history.
    std::string error;
    const auto profiles = prepareLoftProfiles(sources, error);
    if (!profiles) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, error};
    }
    if (sources.size() < 2U) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "Need at least two vertices, edges or wires to create loft face"
        };
    }

    try {
        BRepOffsetAPI_ThruSections generator(
            solid ? Standard_True : Standard_False,
            ruled ? Standard_True : Standard_False
        );
        generator.SetMaxDegree(maxDegree);

        for (std::size_t index = 0; index < profiles->size(); ++index) {
            if (index > 0U
                && !loftProfilesHaveSufficientSeparation(
                    profiles->at(index),
                    profiles->at(index - 1U)
                )) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "Segments of a loft do not have sufficient separation"
                };
            }
            const TopoDS_Shape& profile = profiles->at(index);
            if (profile.ShapeType() == TopAbs_VERTEX) {
                generator.AddVertex(TopoDS::Vertex(profile));
            }
            else {
                generator.AddWire(TopoDS::Wire(profile));
            }
        }

        if (closed && profiles->back().ShapeType() != TopAbs_VERTEX) {
            const TopoDS_Shape& firstProfile = profiles->front();
            if (firstProfile.ShapeType() == TopAbs_VERTEX) {
                generator.AddVertex(TopoDS::Vertex(firstProfile));
            }
            else {
                generator.AddWire(TopoDS::Wire(firstProfile));
            }
        }

        generator.CheckCompatibility(Standard_True);
        generator.Build();
        if (!generator.IsDone() || generator.Shape().IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "ThruSections failed"};
        }

        NamedShape namedShape = namedShapeForThruSectionsHistory(
            owner,
            generator.Shape(),
            sources,
            generator,
            profiles->front(),
            profiles->back()
        );
        addDistinct(namedShape.elementHistoryStatus, "part_loft:thru_sections_history");
        return NamedShapeBuild {generator.Shape(), std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Part::Loft failed"
        };
    }
}

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
