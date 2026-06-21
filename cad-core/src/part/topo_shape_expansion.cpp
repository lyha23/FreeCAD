#include "cad_core/part/topo_shape_expansion.h"

#include "cad_core/part/face_maker.h"
#include "cad_core/part/topo_shape_mapper.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_TransitionMode.hxx>
#include <BRep_Builder.hxx>
#include <BRepFeat_MakeRevol.hxx>
#include <BRepFill.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepLib_FindSurface.hxx>
#include <BRepOffsetAPI_MakeFilling.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <Bnd_Box.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <GeomAbs_Shape.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <ShapeFix_Wire.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
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

void addImportAlias(
    NamedShape& namedShape,
    const std::string& owner,
    const std::string& elementName,
    const ImportElementMapSource& source
)
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

TopoDS_Shape copiedShape(const TopoDS_Shape& shape)
{
    BRepBuilderAPI_Copy copy(shape, Standard_True, Standard_True);
    if (copy.IsDone() && !copy.Shape().IsNull()) {
        return copy.Shape();
    }
    return shape;
}

TopoDS_Edge copiedEdge(const TopoDS_Edge& edge)
{
    return TopoDS::Edge(copiedShape(edge));
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

std::array<gp_Pnt, 2> curveSamplePoints(const TopoDS_Shape& curveShape)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementRuledSurface(), Automatic orientation samples
    // "0.9 * FirstParameter() + 0.1 * LastParameter()" and the opposite point on either a
    // BRepAdaptor_HCurve edge or BRepAdaptor_HCompCurve wire.
    if (curveShape.ShapeType() == TopAbs_WIRE) {
        BRepAdaptor_CompCurve curve(TopoDS::Wire(curveShape));
        gp_Pnt first = curve.Value(0.9 * curve.FirstParameter() + 0.1 * curve.LastParameter());
        gp_Pnt second = curve.Value(0.1 * curve.FirstParameter() + 0.9 * curve.LastParameter());
        if (curveShape.Orientation() == TopAbs_REVERSED) {
            std::swap(first, second);
        }
        return {first, second};
    }

    BRepAdaptor_Curve curve(TopoDS::Edge(curveShape));
    gp_Pnt first = curve.Value(0.9 * curve.FirstParameter() + 0.1 * curve.LastParameter());
    gp_Pnt second = curve.Value(0.1 * curve.FirstParameter() + 0.9 * curve.LastParameter());
    if (curveShape.Orientation() == TopAbs_REVERSED) {
        std::swap(first, second);
    }
    return {first, second};
}

bool automaticRuledSurfaceReversesSecondCurve(const TopoDS_Shape& first, const TopoDS_Shape& second)
{
    const auto firstPoints = curveSamplePoints(first);
    const auto secondPoints = curveSamplePoints(second);
    const gp_Pnt& p1 = firstPoints[0];
    const gp_Pnt& p2 = firstPoints[1];
    const gp_Pnt& p3 = secondPoints[0];
    const gp_Pnt& p4 = secondPoints[1];

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

void addDistinctEvidence(
    std::vector<FilledFaceBoundaryEvidence>& values,
    const FilledFaceBoundaryEvidence& value
)
{
    const auto duplicate
        = std::find_if(values.begin(), values.end(), [&](const FilledFaceBoundaryEvidence& current) {
              return current.objectName == value.objectName && current.subname == value.subname
                  && current.stableSubname == value.stableSubname
                  && current.shapeKind == value.shapeKind;
          });
    if (duplicate == values.end()) {
        values.push_back(value);
    }
}

std::string evidenceKindName(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_FACE:
            return "face";
        case TopAbs_WIRE:
            return "wire";
        case TopAbs_EDGE:
            return "edge";
        case TopAbs_VERTEX:
            return "vertex";
        case TopAbs_COMPOUND:
            return "compound";
        default:
            return "shape";
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
    TopoDS_Wire wire = wireBuilder.Wire();
    BRepLib::BuildCurves3d(wire);
    BRepLib::SameParameter(wire, Precision::Confusion(), Standard_True);
    return wire;
}

std::vector<TopoDS_Edge> orderedEdges(const TopoDS_Wire& wire)
{
    std::vector<TopoDS_Edge> edges;
    for (BRepTools_WireExplorer explorer(wire); explorer.More(); explorer.Next()) {
        edges.push_back(explorer.Current());
    }
    if (!edges.empty()) {
        return edges;
    }
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

bool wireHasEdges(const TopoDS_Wire& wire)
{
    TopExp_Explorer explorer(wire, TopAbs_EDGE);
    return explorer.More();
}

TopoDS_Wire fixedFillingBoundaryWire(const TopoDS_Wire& wire)
{
    TopoDS_Wire fixed = wire;
    BRepLib::BuildCurves3d(fixed);
    BRepLib::SameParameter(fixed, Precision::Confusion(), Standard_True);

    ShapeFix_Wire fixer;
    fixer.Load(fixed);
    fixer.SetPrecision(Precision::Confusion());
    fixer.ClosedWireMode() = BRep_Tool::IsClosed(fixed);
    fixer.FixReorder();
    fixer.FixConnected(Precision::Confusion());
    if (BRep_Tool::IsClosed(fixed)) {
        fixer.FixClosed(Precision::Confusion());
    }
    fixer.FixEdgeCurves();
    TopoDS_Wire apiWire = fixer.WireAPIMake();
    if (!apiWire.IsNull()) {
        fixed = apiWire;
    }
    BRepLib::BuildCurves3d(fixed);
    BRepLib::SameParameter(fixed, Precision::Confusion(), Standard_True);
    return fixed;
}

std::optional<TopoDS_Wire> singleWireFromShape(const TopoDS_Shape& shape, std::string& error)
{
    if (shape.IsNull()) {
        error = "Null input shape";
        return std::nullopt;
    }

    if (subshapeCount(shape, TopAbs_WIRE) == 1) {
        TopoDS_Wire wire = TopoDS::Wire(*singleSubshape(shape, TopAbs_WIRE));
        BRepLib::BuildCurves3d(wire);
        BRepLib::SameParameter(wire, Precision::Confusion(), Standard_True);
        return wire;
    }
    if (subshapeCount(shape, TopAbs_WIRE) == 0 && subshapeCount(shape, TopAbs_EDGE) > 0) {
        const auto wire = wireFromEdges(shape);
        if (wire) {
            return wire;
        }
    }

    error = "Spine shape cannot form a single wire";
    return std::nullopt;
}

struct FilledFaceWorkingShape
{
    TopoDS_Shape shape;
    const FilledFaceSource* source = nullptr;
};

struct FilledFaceBoundaryCandidate
{
    TopoDS_Wire wire;
    std::string mode;
    std::vector<FilledFaceBoundaryEvidence> evidence;
};

bool isCompoundShape(const TopoDS_Shape& shape)
{
    return !shape.IsNull()
        && (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID);
}

void expandFilledFaceSource(
    const FilledFaceSource& source,
    const TopoDS_Shape& shape,
    std::vector<FilledFaceWorkingShape>& output
)
{
    if (shape.IsNull()) {
        return;
    }
    if (!isCompoundShape(shape)) {
        output.push_back(FilledFaceWorkingShape {shape, &source});
        return;
    }
    for (TopoDS_Iterator it(shape); it.More(); it.Next()) {
        expandFilledFaceSource(source, it.Value(), output);
    }
}

std::vector<FilledFaceWorkingShape> expandedFilledFaceSources(
    const std::vector<FilledFaceSource>& sources
)
{
    std::vector<FilledFaceWorkingShape> output;
    for (const FilledFaceSource& source : sources) {
        expandFilledFaceSource(source, source.shape, output);
    }
    return output;
}

int compoundFilledFaceSourceCount(const std::vector<FilledFaceSource>& sources)
{
    return static_cast<int>(std::count_if(
        sources.begin(),
        sources.end(),
        [](const FilledFaceSource& source) { return isCompoundShape(source.shape); }
    ));
}

std::optional<std::string> sourceEdgeElementName(const FilledFaceSource& source, const TopoDS_Edge& edge)
{
    if (!source.subname.empty() && source.shape.ShapeType() == TopAbs_EDGE) {
        return source.objectName + "."
            + (source.stableSubname.empty() ? source.subname : source.stableSubname);
    }
    if (source.namedShape == nullptr) {
        return std::nullopt;
    }

    for (const auto& [elementName, element] : source.namedShape->elements) {
        if (element.subshape.kind != TopAbs_EDGE) {
            continue;
        }
        const auto shape = subshapeByName(source.namedShape->shape, element.subshape);
        if (!shape || shape->IsNull() || shape->ShapeType() != TopAbs_EDGE) {
            continue;
        }
        if (shape->IsSame(edge)) {
            return source.objectName + "." + elementName;
        }
    }
    return std::nullopt;
}

FilledFaceBoundaryEvidence evidenceFromFullName(
    const std::string& fullName,
    const FilledFaceSource& fallback,
    const std::string& kind
)
{
    const std::size_t dot = fullName.rfind('.');
    if (dot == std::string::npos) {
        return FilledFaceBoundaryEvidence {
            fallback.objectName,
            fullName,
            fullName,
            kind,
        };
    }
    return FilledFaceBoundaryEvidence {
        fullName.substr(0, dot),
        fullName.substr(dot + 1),
        fullName.substr(dot + 1),
        kind,
    };
}

FilledFaceBoundaryEvidence fallbackEvidence(const FilledFaceSource& source, TopAbs_ShapeEnum kind)
{
    return FilledFaceBoundaryEvidence {
        source.objectName,
        source.subname,
        source.stableSubname.empty() ? source.subname : source.stableSubname,
        evidenceKindName(kind),
    };
}

FilledFaceConstraintEvidence nonBoundaryEvidenceFromBoundaryEvidence(
    const FilledFaceBoundaryEvidence& source,
    const std::string& builderCall
)
{
    return FilledFaceConstraintEvidence {
        source.objectName,
        source.subname,
        source.stableSubname,
        source.shapeKind,
        builderCall,
        false,
    };
}

bool filledFaceSourceMatchesEdge(const FilledFaceSource& source, const TopoDS_Edge& edge)
{
    if (source.shape.IsNull()) {
        return false;
    }
    if (source.shape.ShapeType() == TopAbs_EDGE) {
        const TopoDS_Edge sourceEdge = TopoDS::Edge(source.shape);
        return sourceEdge.IsSame(edge) || sameEndpointPair(sourceEdge, edge);
    }
    for (TopExp_Explorer explorer(source.shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge sourceEdge = TopoDS::Edge(explorer.Current());
        if (sourceEdge.IsSame(edge) || sameEndpointPair(sourceEdge, edge)) {
            return true;
        }
    }
    return false;
}

bool filledFaceSourceMatchesAnyEdge(
    const FilledFaceSource& source,
    const std::vector<TopoDS_Edge>& edges
)
{
    return std::any_of(edges.begin(), edges.end(), [&](const TopoDS_Edge& edge) {
        return filledFaceSourceMatchesEdge(source, edge);
    });
}

std::vector<FilledFaceBoundaryEvidence> evidenceForBoundaryEdge(
    const FilledFaceSource& source,
    const TopoDS_Edge& edge
)
{
    std::vector<FilledFaceBoundaryEvidence> evidence;
    if (const auto elementName = sourceEdgeElementName(source, edge)) {
        evidence.push_back(evidenceFromFullName(*elementName, source, "edge"));
        if (source.namedShape != nullptr) {
            const std::size_t dot = elementName->rfind('.');
            const std::string currentName = dot == std::string::npos ? *elementName
                                                                     : elementName->substr(dot + 1);
            for (const auto& [stableName, mappedName] : source.namedShape->elementMap) {
                if (mappedName != currentName || stableName == currentName) {
                    continue;
                }
                evidence.push_back(evidenceFromFullName(stableName, source, "edge"));
            }
        }
        return evidence;
    }

    evidence.push_back(fallbackEvidence(source, source.shape.ShapeType()));
    return evidence;
}

TopoDS_Face supportFaceForBoundaryEdge(
    const TopoDS_Edge& edge,
    const std::vector<FilledFaceSupportSource>& supportSources
)
{
    for (const FilledFaceSupportSource& support : supportSources) {
        if (!filledFaceSourceMatchesEdge(support.target, edge) || support.support.shape.IsNull()
            || support.support.shape.ShapeType() != TopAbs_FACE) {
            continue;
        }
        return TopoDS::Face(support.support.shape);
    }
    return TopoDS_Face {};
}

GeomAbs_Shape orderForBoundaryEdge(
    const TopoDS_Edge& edge,
    const std::vector<FilledFaceOrderSource>& orderSources
)
{
    for (const FilledFaceOrderSource& order : orderSources) {
        if (filledFaceSourceMatchesEdge(order.target, edge)) {
            return order.order;
        }
    }
    return GeomAbs_C0;
}

FilledFaceSupportOrderEvidence evidenceForSupportSource(
    const FilledFaceSupportSource& support,
    bool isBoundary
)
{
    const FilledFaceBoundaryEvidence target
        = fallbackEvidence(support.target, support.target.shape.ShapeType());
    const FilledFaceBoundaryEvidence supportFace = fallbackEvidence(support.support, TopAbs_FACE);
    return FilledFaceSupportOrderEvidence {
        target.objectName,
        target.subname,
        target.stableSubname,
        target.shapeKind,
        isBoundary,
        isBoundary ? "Add(edge, support, order, IsBound=true)"
                   : "Add(edge, support, order, IsBound=false)",
        true,
        supportFace.objectName,
        supportFace.subname,
        supportFace.stableSubname,
        false,
        {},
    };
}

FilledFaceSupportOrderEvidence evidenceForOrderSource(
    const FilledFaceOrderSource& order,
    bool isBoundary
)
{
    const FilledFaceBoundaryEvidence target = fallbackEvidence(order.target, order.target.shape.ShapeType());
    return FilledFaceSupportOrderEvidence {
        target.objectName,
        target.subname,
        target.stableSubname,
        target.shapeKind,
        isBoundary,
        isBoundary ? "Add(edge, support, order, IsBound=true)"
                   : "Add(edge, support, order, IsBound=false)",
        false,
        {},
        {},
        {},
        true,
        order.orderName,
    };
}

bool sameSupportOrderTarget(
    const FilledFaceSupportOrderEvidence& left,
    const FilledFaceSupportOrderEvidence& right
)
{
    return left.targetObject == right.targetObject && left.targetSubname == right.targetSubname
        && left.targetStableSubname == right.targetStableSubname;
}

std::vector<FilledFaceSupportOrderEvidence> supportOrderEvidence(
    const std::vector<FilledFaceSupportSource>& supportSources,
    const std::vector<FilledFaceOrderSource>& orderSources,
    const std::vector<TopoDS_Edge>& boundaryEdges,
    const std::vector<TopoDS_Edge>& nonBoundaryEdges
)
{
    std::vector<FilledFaceSupportOrderEvidence> result;
    auto merge = [&](const FilledFaceSupportOrderEvidence& item) {
        auto existing = std::find_if(
            result.begin(),
            result.end(),
            [&](const FilledFaceSupportOrderEvidence& current) {
                return sameSupportOrderTarget(current, item);
            }
        );
        if (existing == result.end()) {
            result.push_back(item);
            return;
        }
        if (item.hasSupport) {
            existing->hasSupport = true;
            existing->supportObject = item.supportObject;
            existing->supportSubname = item.supportSubname;
            existing->supportStableSubname = item.supportStableSubname;
            existing->isBoundary = item.isBoundary;
            existing->builderCall = item.builderCall;
        }
        if (item.hasOrder) {
            existing->hasOrder = true;
            existing->order = item.order;
            existing->isBoundary = item.isBoundary;
            existing->builderCall = item.builderCall;
        }
    };

    for (const FilledFaceSupportSource& support : supportSources) {
        const bool isBoundary = filledFaceSourceMatchesAnyEdge(support.target, boundaryEdges)
            || !filledFaceSourceMatchesAnyEdge(support.target, nonBoundaryEdges);
        merge(evidenceForSupportSource(support, isBoundary));
    }
    for (const FilledFaceOrderSource& order : orderSources) {
        const bool isBoundary = filledFaceSourceMatchesAnyEdge(order.target, boundaryEdges)
            || !filledFaceSourceMatchesAnyEdge(order.target, nonBoundaryEdges);
        merge(evidenceForOrderSource(order, isBoundary));
    }
    return result;
}

std::vector<FilledFaceBoundaryEvidence> evidenceForBoundaryWire(
    const FilledFaceSource& source,
    const TopoDS_Wire& wire
)
{
    std::vector<FilledFaceBoundaryEvidence> evidence;
    for (const TopoDS_Edge& edge : orderedEdges(wire)) {
        for (const auto& item : evidenceForBoundaryEdge(source, edge)) {
            addDistinctEvidence(evidence, item);
        }
    }
    if (evidence.empty()) {
        evidence.push_back(fallbackEvidence(source, TopAbs_WIRE));
    }
    return evidence;
}

std::optional<FilledFaceBoundaryCandidate> findFilledFaceBoundaryWire(
    std::vector<FilledFaceWorkingShape>& shapes
)
{
    int index = -1;
    int boundaryIndex = -1;
    bool closed = false;
    for (const FilledFaceWorkingShape& item : shapes) {
        ++index;
        if (item.shape.IsNull() || item.shape.ShapeType() != TopAbs_WIRE) {
            continue;
        }
        TopoDS_Wire wire = TopoDS::Wire(item.shape);
        if (!wireHasEdges(wire)) {
            continue;
        }
        if (BRep_Tool::IsClosed(wire)) {
            boundaryIndex = index;
            closed = true;
            break;
        }
        if (boundaryIndex < 0) {
            boundaryIndex = index;
        }
    }
    if (boundaryIndex < 0) {
        return std::nullopt;
    }

    const FilledFaceWorkingShape selected = shapes.at(static_cast<std::size_t>(boundaryIndex));
    shapes.erase(shapes.begin() + boundaryIndex);
    FilledFaceBoundaryCandidate candidate;
    candidate.wire = TopoDS::Wire(selected.shape);
    candidate.mode = closed ? "closed_wire" : "wire";
    if (selected.source != nullptr) {
        candidate.evidence = evidenceForBoundaryWire(*selected.source, candidate.wire);
    }
    return candidate;
}

std::optional<FilledFaceBoundaryCandidate> buildFilledFaceBoundaryWireFromEdges(
    std::vector<FilledFaceWorkingShape>& shapes
)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    std::vector<FilledFaceBoundaryEvidence> evidence;
    bool hasEdge = false;
    for (auto it = shapes.begin(); it != shapes.end();) {
        if (!it->shape.IsNull() && it->shape.ShapeType() == TopAbs_EDGE) {
            const TopoDS_Edge edge = TopoDS::Edge(it->shape);
            wireBuilder.Add(edge);
            hasEdge = true;
            if (it->source != nullptr) {
                for (const auto& item : evidenceForBoundaryEdge(*it->source, edge)) {
                    addDistinctEvidence(evidence, item);
                }
            }
            it = shapes.erase(it);
            continue;
        }
        ++it;
    }
    if (!hasEdge) {
        return std::nullopt;
    }

    wireBuilder.Build();
    if (!wireBuilder.IsDone() || wireBuilder.Wire().IsNull()) {
        return std::nullopt;
    }

    FilledFaceBoundaryCandidate candidate;
    candidate.wire = wireBuilder.Wire();
    candidate.mode = BRep_Tool::IsClosed(candidate.wire) ? "edge_wire_closed" : "edge_wire";
    candidate.evidence = std::move(evidence);
    return candidate;
}

std::optional<std::string> firstFaceName(const NamedShape& namedShape)
{
    for (const auto& [name, element] : namedShape.elements) {
        if (element.subshape.kind == TopAbs_FACE) {
            return name;
        }
    }
    return std::nullopt;
}

void addFilledFaceBoundaryHistory(
    NamedShape& namedShape,
    const std::string& owner,
    const std::vector<FilledFaceBoundaryEvidence>& boundarySources,
    int boundaryEdgeCount
)
{
    const auto targetFace = firstFaceName(namedShape);
    if (!targetFace) {
        return;
    }

    for (const FilledFaceBoundaryEvidence& source : boundarySources) {
        MapperHistoryEvent event;
        event.source = MapperHistoryEndpoint {
            source.objectName,
            source.stableSubname.empty() ? source.subname : source.stableSubname
        };
        event.target = MapperHistoryEndpoint {owner, *targetFace};
        event.shapeKind = "face";
        event.relation = MapperHistoryRelation::Generated;
        event.makerStage = "maker_history:filling_boundary";
        event.evidence = {
            {"freecad_operation", "TopoShape::makeElementFilledFace"},
            {"helper", "Part.makeFilledFace"},
            {"source_kind", source.shapeKind},
            {"source_subname", source.subname},
            {"stable_subname", source.stableSubname},
            {"boundary_edge_count", boundaryEdgeCount},
            {"is_bound", true},
        };
        event.recoverability = MapperHistoryRecoverability::Resolved;
        event.diagnosticStatus = "filling_boundary_source";
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
}

void addFilledFaceInitialSurfaceHistory(
    NamedShape& namedShape,
    const std::string& owner,
    const FilledFaceBoundaryEvidence& source
)
{
    const auto targetFace = firstFaceName(namedShape);
    if (!targetFace) {
        return;
    }

    MapperHistoryEvent event;
    event.source = MapperHistoryEndpoint {
        source.objectName,
        source.stableSubname.empty() ? source.subname : source.stableSubname
    };
    event.target = MapperHistoryEndpoint {owner, *targetFace};
    event.shapeKind = "face";
    event.relation = MapperHistoryRelation::Generated;
    event.makerStage = "maker_history:filling_initial_surface";
    event.evidence = {
        {"freecad_operation", "TopoShape::makeElementFilledFace"},
        {"helper", "Part.makeFilledFace"},
        {"source_kind", source.shapeKind},
        {"source_subname", source.subname},
        {"stable_subname", source.stableSubname},
        {"builder_call", "LoadInitSurface"},
    };
    event.recoverability = MapperHistoryRecoverability::Resolved;
    event.diagnosticStatus = "filling_initial_surface_source";
    addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
}

void addFilledFaceSupportOrderHistory(
    NamedShape& namedShape,
    const std::string& owner,
    const std::vector<FilledFaceSupportOrderEvidence>& sources
)
{
    const auto targetFace = firstFaceName(namedShape);
    if (!targetFace) {
        return;
    }

    for (const FilledFaceSupportOrderEvidence& source : sources) {
        MapperHistoryEvent event;
        event.source = MapperHistoryEndpoint {
            source.targetObject,
            source.targetStableSubname.empty() ? source.targetSubname : source.targetStableSubname
        };
        event.target = MapperHistoryEndpoint {owner, *targetFace};
        event.shapeKind = "face";
        event.relation = MapperHistoryRelation::Generated;
        event.makerStage = "maker_history:filling_support_order";
        event.evidence = {
            {"freecad_operation", "TopoShape::makeElementFilledFace"},
            {"helper", "Part.makeFilledFace"},
            {"source_kind", source.targetShapeKind},
            {"source_subname", source.targetSubname},
            {"stable_subname", source.targetStableSubname},
            {"has_support", source.hasSupport},
            {"support_object", source.supportObject},
            {"support_subname", source.supportSubname},
            {"support_stable_subname", source.supportStableSubname},
            {"has_order", source.hasOrder},
            {"order", source.order},
            {"builder_call", source.builderCall},
            {"is_bound", source.isBoundary},
        };
        event.recoverability = MapperHistoryRecoverability::Resolved;
        event.diagnosticStatus = "filling_support_order_source";
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
}

void addFilledFaceNonBoundaryHistory(
    NamedShape& namedShape,
    const std::string& owner,
    const std::vector<FilledFaceConstraintEvidence>& sources
)
{
    const auto targetFace = firstFaceName(namedShape);
    if (!targetFace) {
        return;
    }

    for (const FilledFaceConstraintEvidence& source : sources) {
        MapperHistoryEvent event;
        event.source = MapperHistoryEndpoint {
            source.objectName,
            source.stableSubname.empty() ? source.subname : source.stableSubname
        };
        event.target = MapperHistoryEndpoint {owner, *targetFace};
        event.shapeKind = "face";
        event.relation = MapperHistoryRelation::Generated;
        event.makerStage = "maker_history:filling_non_boundary_constraint";
        event.evidence = {
            {"freecad_operation", "TopoShape::makeElementFilledFace"},
            {"helper", "Part.makeFilledFace"},
            {"source_kind", source.shapeKind},
            {"source_subname", source.subname},
            {"stable_subname", source.stableSubname},
            {"builder_call", source.builderCall},
            {"is_bound", source.isBoundary},
        };
        event.recoverability = MapperHistoryRecoverability::Resolved;
        event.diagnosticStatus = "filling_non_boundary_constraint_source";
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
}

std::optional<std::vector<TopoDS_Shape>> prepareLoftProfiles(
    const std::vector<NamedShapeSource>& sources,
    std::string& error,
    std::size_t offset = 0U
)
{
    std::vector<TopoDS_Shape> profiles;
    profiles.reserve(sources.size() > offset ? sources.size() - offset : 0U);
    for (std::size_t sourceIndex = offset; sourceIndex < sources.size(); ++sourceIndex) {
        const auto& source = sources[sourceIndex];
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

BRepBuilderAPI_TransitionMode pipeShellTransitionMode(int transition)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::TransitionEnums order is "Transformed", "Right corner", "Round corner".
    switch (transition) {
        case 1:
            return BRepBuilderAPI_RightCorner;
        case 2:
            return BRepBuilderAPI_RoundCorner;
        default:
            return BRepBuilderAPI_Transformed;
    }
}

bool shapeIsClosed(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }

    switch (shape.ShapeType()) {
        case TopAbs_SHELL:
        case TopAbs_WIRE:
        case TopAbs_EDGE:
            return BRep_Tool::IsClosed(shape) == Standard_True;
        case TopAbs_COMPSOLID:
        case TopAbs_SOLID: {
            bool closed = true;
            for (TopExp_Explorer explorer(shape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                closed = closed && BRep_Tool::IsClosed(explorer.Current()) == Standard_True;
            }
            return closed;
        }
        case TopAbs_COMPOUND: {
            bool closed = true;
            TopExp_Explorer explorer;
            for (explorer.Init(shape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                closed = closed && BRep_Tool::IsClosed(explorer.Current()) == Standard_True;
            }
            for (explorer.Init(shape, TopAbs_FACE, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                closed = closed && BRep_Tool::IsClosed(explorer.Current()) == Standard_True;
            }
            for (explorer.Init(shape, TopAbs_WIRE, TopAbs_FACE); explorer.More(); explorer.Next()) {
                closed = closed && BRep_Tool::IsClosed(explorer.Current()) == Standard_True;
            }
            for (explorer.Init(shape, TopAbs_EDGE, TopAbs_WIRE); explorer.More(); explorer.Next()) {
                closed = closed && BRep_Tool::IsClosed(explorer.Current()) == Standard_True;
            }
            return closed;
        }
        default:
            return BRep_Tool::IsClosed(shape) == Standard_True;
    }
}

void configurePipeShellMode(BRepOffsetAPI_MakePipeShell& pipeShell, const PipeShellOptions& options)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::setupAlgorithm(), switch Mode "Fixed/Frenet/Auxiliary/Binormal" into
    // BRepOffsetAPI_MakePipeShell::SetMode overloads before Add()/Build().
    switch (options.mode) {
        case PipeShellMode::Fixed:
            pipeShell.SetMode(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)));
            break;
        case PipeShellMode::Frenet:
            pipeShell.SetMode(Standard_True);
            break;
        case PipeShellMode::Auxiliary:
            pipeShell.SetMode(TopoDS::Wire(options.auxiliarySpine), options.auxiliaryCurvilinear);
            break;
        case PipeShellMode::Binormal:
            pipeShell.SetMode(
                gp_Dir(options.binormal[0], options.binormal[1], options.binormal[2])
            );
            break;
        case PipeShellMode::Standard:
            break;
    }
}

std::optional<TopoDS_Wire> simulatedPipeEndWire(
    const TopTools_ListOfShape& simulated,
    bool front,
    std::string& error
)
{
    if (simulated.Extent() < 2) {
        error = "PipeShell simulation did not produce front/back wires";
        return std::nullopt;
    }
    return singleWireFromShape(front ? simulated.First() : simulated.Last(), error);
}

std::optional<TopoDS_Shape> pipeCapFaceFromWires(const std::vector<TopoDS_Wire>& wires)
{
    if (wires.empty()) {
        return std::nullopt;
    }
    return makeFaceWithHolesFromClosedWires(wires);
}

TopoDS_Shape compoundOfShapes(const std::vector<TopoDS_Shape>& shapes)
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
            else if (
                subshapeCount(shape, TopAbs_FACE) > 0 || shape.ShapeType() == TopAbs_FACE
                || shape.ShapeType() == TopAbs_SHELL
            ) {
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

bool linearizePlanarFaces(TopoDS_Shape& shape)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Loft::execute(), after makeElementLoft() calls
    // "result.linearize(LinearizeFace::linearizeFaces, LinearizeEdge::noEdges)".
    // TopoShapeExpansion.cpp::TopoShape::linearize() skips existing GeomAbs_Plane faces and
    // replaces planar non-plane face geometry through "builder.UpdateFace(...)".
    bool touched = false;
    BRep_Builder builder;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface current(face);
        if (current.GetType() == GeomAbs_Plane) {
            continue;
        }

        BRepLib_FindSurface planeFinder(face, -1, Standard_True);
        if (!planeFinder.Found() || planeFinder.Surface().IsNull()) {
            continue;
        }
        GeomAdaptor_Surface surface(planeFinder.Surface());
        if (surface.GetType() != GeomAbs_Plane) {
            continue;
        }

        builder.UpdateFace(face, planeFinder.Surface(), face.Location(), BRep_Tool::Tolerance(face));
        touched = true;
    }
    return touched;
}

void addRuledSurfaceSourceRelation(
    NamedShape& namedShape,
    const std::string& owner,
    const std::string& objectName,
    const RuledSurfaceEdgeEvidence& source
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
        namedShape.history.push_back(
            ElementHistory {ElementHistoryKind::Modified, *targetName, {sourceName}}
        );

        MapperHistoryEvent event;
        event.source = MapperHistoryEndpoint {objectName, sourceName};
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

void addRuledSurfaceSourceRelations(
    NamedShape& namedShape,
    const std::string& owner,
    const RuledSurfaceCurveSource& source
)
{
    for (const auto& edge : source.edges) {
        addRuledSurfaceSourceRelation(namedShape, owner, source.objectName, edge);
    }
}

std::optional<TopoDS_Wire> curveAsWire(const TopoDS_Shape& curve)
{
    if (curve.ShapeType() == TopAbs_WIRE) {
        TopoDS_Wire wire = TopoDS::Wire(curve);
        BRepLib::BuildCurves3d(wire);
        BRepLib::SameParameter(wire, Precision::Confusion(), Standard_True);
        return wire;
    }
    if (curve.ShapeType() == TopAbs_EDGE) {
        return wireFromEdges(curve);
    }
    return std::nullopt;
}

}  // namespace

NamedShapeBuild makeElementLoftFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool ruled,
    bool closed,
    int maxDegree,
    bool linearizeFaces
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
                && !loftProfilesHaveSufficientSeparation(profiles->at(index), profiles->at(index - 1U))) {
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
        TopoDS_Shape resultShape = generator.Shape();
        const bool linearized = linearizeFaces && linearizePlanarFaces(resultShape);

        NamedShape namedShape = namedShapeForThruSectionsHistory(
            owner,
            resultShape,
            sources,
            generator,
            profiles->front(),
            profiles->back()
        );
        addDistinct(namedShape.elementHistoryStatus, "part_loft:thru_sections_history");
        if (linearizeFaces) {
            addDistinct(
                namedShape.elementHistoryStatus,
                linearized ? "part_loft:linearized_planar_faces" : "part_loft:linearize_noop"
            );
        }
        return NamedShapeBuild {resultShape, std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Part::Loft failed"
        };
    }
}

NamedShapeBuild makeElementRuledSurfaceFromCurves(
    const std::string& owner,
    const std::array<RuledSurfaceCurveSource, 2>& sources,
    short orientation
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementRuledSurface(), normalizes edge/wire inputs, converts mixed edge/wire
    // pairs to wires, "Automatic" samples two curve endpoint pairs and flips S2 when the
    // triangle-normal dot product is negative; "Reversed" directly reverses S2.
    try {
        TopoDS_Shape first = copiedShape(sources[0].curve);
        TopoDS_Shape second = copiedShape(sources[1].curve);
        if (first.ShapeType() != second.ShapeType()) {
            const auto firstWire = curveAsWire(first);
            const auto secondWire = curveAsWire(second);
            if (!firstWire || !secondWire) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "Input shape forms more than one wire"
                };
            }
            first = *firstWire;
            second = *secondWire;
        }

        const bool isWire = first.ShapeType() == TopAbs_WIRE;
        if (first.ShapeType() != TopAbs_EDGE && first.ShapeType() != TopAbs_WIRE) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Input shape has no edge"};
        }

        if (orientation == 0) {
            if (automaticRuledSurfaceReversesSecondCurve(first, second)) {
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

        TopoDS_Shape ruledShape;
        if (isWire) {
            ruledShape = BRepFill::Shell(TopoDS::Wire(first), TopoDS::Wire(second));
        }
        else {
            ruledShape = BRepFill::Face(TopoDS::Edge(first), TopoDS::Edge(second));
        }
        if (ruledShape.IsNull()) {
            return NamedShapeBuild {
                TopoDS_Shape {},
                std::nullopt,
                isWire ? "BRepFill::Shell produced null shape" : "BRepFill::Face produced null shape"
            };
        }

        NamedShape namedShape = indexedNamedShapeForObject(owner, ruledShape);
        addRuledSurfaceSourceRelations(namedShape, owner, sources[0]);
        addRuledSurfaceSourceRelations(namedShape, owner, sources[1]);
        addDistinct(
            namedShape.elementHistoryStatus,
            isWire ? "part_ruled_surface:wire_wire_brepfill_shell"
                   : "part_ruled_surface:shared_vertex_edge_relation"
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

NamedShapeBuild makeElementPipeShellFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    const PipeShellOptions& options
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementPipeShell(), requires "shapes.size() >= 2", converts the first
    // source to a single wire, calls "SetMode(isFrenet)", "SetTransitionMode(transMode)",
    // "Add(profile)", "IsReady()", "Build()", optional "MakeSolid()", then makeElementShape().
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::execute(), for open shells calls "mkPS.Simulate(2, sim)", creates front/back faces,
    // then "BRepBuilderAPI_Sewing" and "Part::MapperSewing(sewer)" before solidification.
    if (sources.size() < 2U) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Not enough input shapes"};
    }

    std::string error;
    const auto spine = singleWireFromShape(sources.front().shape, error);
    if (!spine) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, error};
    }
    const auto profiles = prepareLoftProfiles(sources, error, 1U);
    if (!profiles) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, error};
    }

    try {
        PipeShellOptions effectiveOptions = options;
        if (effectiveOptions.mode == PipeShellMode::Auxiliary) {
            std::string auxiliaryError;
            const auto auxiliaryWire = singleWireFromShape(effectiveOptions.auxiliarySpine, auxiliaryError);
            if (!auxiliaryWire) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    auxiliaryError.empty() ? "Auxiliary spine shape cannot form a single wire"
                                           : auxiliaryError
                };
            }
            effectiveOptions.auxiliarySpine = *auxiliaryWire;
        }

        BRepOffsetAPI_MakePipeShell pipeShell(*spine);
        pipeShell.SetTolerance(Precision::Confusion());
        configurePipeShellMode(pipeShell, effectiveOptions);
        pipeShell.SetTransitionMode(pipeShellTransitionMode(effectiveOptions.transition));
        for (std::size_t index = 0; index < profiles->size(); ++index) {
            pipeShell.Add(profiles->at(index));
        }

        if (!pipeShell.IsReady()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "shape is not ready to build"};
        }
        pipeShell.Build();
        if (options.solid && !options.sewCaps) {
            pipeShell.MakeSolid();
        }
        if (!pipeShell.IsDone() || pipeShell.Shape().IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Part::Sweep failed"};
        }

        TopoDS_Shape resultShape = pipeShell.Shape();
        const bool linearized = options.linearizeFaces && linearizePlanarFaces(resultShape);

        NamedShape namedShape = namedShapeForMakerHistory(owner, resultShape, sources, pipeShell);
        addDistinct(namedShape.elementHistoryStatus, "part_sweep:pipeshell_history");
        if (options.linearizeFaces) {
            addDistinct(
                namedShape.elementHistoryStatus,
                linearized ? "part_sweep:linearized_planar_faces" : "part_sweep:linearize_noop"
            );
        }

        if (options.sewCaps) {
            std::vector<TopoDS_Shape> shellShapes {resultShape};
            if (resultShape.ShapeType() == TopAbs_COMPOUND || resultShape.ShapeType() == TopAbs_COMPSOLID) {
                shellShapes.clear();
                for (TopExp_Explorer explorer(resultShape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                    shellShapes.push_back(explorer.Current());
                }
                if (shellShapes.empty()) {
                    shellShapes.push_back(resultShape);
                }
            }

            std::vector<TopoDS_Wire> frontWires;
            std::vector<TopoDS_Wire> backWires;
            if (!shapeIsClosed(resultShape)) {
                TopTools_ListOfShape simulated;
                pipeShell.Simulate(2, simulated);
                if (profiles->front().ShapeType() != TopAbs_VERTEX) {
                    std::string frontError;
                    const auto wire = simulatedPipeEndWire(simulated, true, frontError);
                    if (!wire) {
                        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, frontError};
                    }
                    frontWires.push_back(*wire);
                }
                if (profiles->back().ShapeType() != TopAbs_VERTEX) {
                    std::string backError;
                    const auto wire = simulatedPipeEndWire(simulated, false, backError);
                    if (!wire) {
                        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, backError};
                    }
                    backWires.push_back(*wire);
                }
            }

            const std::optional<TopoDS_Shape> frontFace = pipeCapFaceFromWires(frontWires);
            const std::optional<TopoDS_Shape> backFace = pipeCapFaceFromWires(backWires);

            if (frontFace || backFace) {
                BRepBuilderAPI_Sewing sewing;
                sewing.SetTolerance(Precision::Confusion());
                for (const TopoDS_Shape& shell : shellShapes) {
                    sewing.Add(shell);
                }
                if (frontFace) {
                    sewing.Add(*frontFace);
                }
                if (backFace) {
                    sewing.Add(*backFace);
                }
                sewing.Perform();
                TopoDS_Shape sewed = sewing.SewedShape();
                if (sewed.IsNull()) {
                    return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Pipe: Failed to create shell"};
                }

                std::vector<NamedShapeSource> sewingSources;
                std::vector<NamedShape> ownedSources;
                ownedSources.reserve(shellShapes.size() + 2U);
                for (std::size_t index = 0; index < shellShapes.size(); ++index) {
                    ownedSources.push_back(namedShapeForMakerHistory(
                        owner + ".Shell" + std::to_string(index + 1U),
                        shellShapes.at(index),
                        sources,
                        pipeShell
                    ));
                    addDistinct(ownedSources.back().elementHistoryStatus, "part_sweep:pipeshell_history");
                    sewingSources.push_back(NamedShapeSource {
                        ownedSources.back().owner,
                        shellShapes.at(index),
                        &ownedSources.back(),
                    });
                }
                if (frontFace) {
                    ownedSources.push_back(namedShapeForPreservedSources(
                        owner + ".FrontFace",
                        *frontFace,
                        {sources.at(1)}
                    ));
                    sewingSources.push_back(NamedShapeSource {
                        ownedSources.back().owner,
                        *frontFace,
                        &ownedSources.back(),
                    });
                }
                if (backFace) {
                    ownedSources.push_back(namedShapeForPreservedSources(
                        owner + ".BackFace",
                        *backFace,
                        {sources.back()}
                    ));
                    sewingSources.push_back(NamedShapeSource {
                        ownedSources.back().owner,
                        *backFace,
                        &ownedSources.back(),
                    });
                }

                NamedShape sewedNamedShape
                    = namedShapeForSewingHistory(owner + ".Sewing", sewed, sewingSources, sewing);
                addDistinct(sewedNamedShape.elementHistoryStatus, "part_design_pipe:sewing");

                if (options.solid) {
                    const NamedShapeSource solidSource {owner + ".Sewing", sewed, &sewedNamedShape};
                    NamedShapeBuild solidBuild = makeElementSolidFromSource(owner, solidSource);
                    if (!solidBuild.error.empty() || solidBuild.shape.IsNull() || !solidBuild.namedShape) {
                        return NamedShapeBuild {
                            TopoDS_Shape {},
                            std::nullopt,
                            solidBuild.error.empty() ? "Pipe: Failed to build solid" : solidBuild.error
                        };
                    }
                    solidBuild.namedShape->owner = owner;
                    solidBuild.namedShape->shape = solidBuild.shape;
                    addDistinct(solidBuild.namedShape->elementHistoryStatus, "part_design_pipe:sewing");
                    addDistinct(solidBuild.namedShape->elementHistoryStatus, "part_design_pipe:solidification");
                    addDistinct(solidBuild.namedShape->elementHistoryStatus, "part_sweep:pipeshell_history");
                    return solidBuild;
                }

                sewedNamedShape.owner = owner;
                sewedNamedShape.shape = sewed;
                addDistinct(sewedNamedShape.elementHistoryStatus, "part_sweep:pipeshell_history");
                return NamedShapeBuild {sewed, std::move(sewedNamedShape), {}};
            }

            if (options.solid) {
                const TopoDS_Shape shellCompound = shellShapes.size() == 1U
                    ? shellShapes.front()
                    : compoundOfShapes(shellShapes);
                NamedShape shellNamedShape = namedShapeForMakerHistory(
                    owner + ".Shell",
                    shellCompound,
                    sources,
                    pipeShell
                );
                addDistinct(shellNamedShape.elementHistoryStatus, "part_sweep:pipeshell_history");
                const NamedShapeSource solidSource {owner + ".Shell", shellCompound, &shellNamedShape};
                NamedShapeBuild solidBuild = makeElementSolidFromSource(owner, solidSource);
                if (!solidBuild.error.empty() || solidBuild.shape.IsNull() || !solidBuild.namedShape) {
                    return NamedShapeBuild {
                        TopoDS_Shape {},
                        std::nullopt,
                        solidBuild.error.empty() ? "Pipe: Failed to build solid" : solidBuild.error
                    };
                }
                solidBuild.namedShape->owner = owner;
                solidBuild.namedShape->shape = solidBuild.shape;
                addDistinct(solidBuild.namedShape->elementHistoryStatus, "part_design_pipe:solidification");
                addDistinct(solidBuild.namedShape->elementHistoryStatus, "part_sweep:pipeshell_history");
                return solidBuild;
            }
        }
        return NamedShapeBuild {resultShape, std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "Part::Sweep failed"
        };
    }
}

NamedShapeBuild makeElementPipeShellFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool frenet,
    int transition,
    bool linearizeFaces
)
{
    PipeShellOptions options;
    options.solid = solid;
    options.mode = frenet ? PipeShellMode::Frenet : PipeShellMode::Standard;
    options.transition = transition;
    options.linearizeFaces = linearizeFaces;
    return makeElementPipeShellFromSources(owner, sources, options);
}

NamedShapeBuild makeElementRevolveFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    const gp_Ax1& axis,
    double angleRadians
)
{
    if (source.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null input shape for revolve operation"};
    }

    try {
        BRepPrimAPI_MakeRevol revolver(source.shape, axis, angleRadians);
        revolver.Build();
        if (!revolver.IsDone()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "PartDesign Revolution/Groove revolve failed"};
        }
        const TopoDS_Shape resultShape = revolver.Shape();
        if (resultShape.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Revolve produced a null shape"};
        }
        NamedShape namedShape = namedShapeForMakerHistory(owner, resultShape, std::vector<NamedShapeSource> {source}, revolver);
        addDistinct(namedShape.elementHistoryStatus, "part_design_revolve:make_revol_history");
        return NamedShapeBuild {resultShape, std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            std::string("PartDesign revolve failed: ") + (message != nullptr ? message : "unknown OCCT error"),
        };
    }
}

NamedShapeBuild makeElementRevolutionUntilFromSources(
    const std::string& owner,
    const NamedShapeSource& baseSource,
    const NamedShapeSource& profileSource,
    const gp_Ax1& axis,
    const TopoDS_Face& supportFace,
    const TopoDS_Face& upToFace,
    int revolMode,
    bool modify
)
{
    if (baseSource.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null base shape for revolution-until operation"};
    }
    if (profileSource.shape.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null profile shape for revolution-until operation"};
    }
    if (upToFace.IsNull()) {
        return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Null up-to face for revolution-until operation"};
    }

    try {
        BRepFeat_MakeRevol revolver;
        TopoDS_Shape currentBase = baseSource.shape;
        bool built = false;
        for (TopExp_Explorer explorer(profileSource.shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
            const TopoDS_Face profileFace = TopoDS::Face(explorer.Current());
            const TopoDS_Face initSupportFace = supportFace.IsNull() ? profileFace : supportFace;
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::TopoShape::makeElementRevolution(), for each profile face calls
            // "mkRevol.Init(base.getShape(), xp.Current(), supportface, axis, static_cast<int>(Mode), Modify)"
            // followed by "mkRevol.Perform(uptoface)".
            revolver.Init(
                currentBase,
                profileFace,
                initSupportFace,
                axis,
                revolMode,
                modify ? Standard_True : Standard_False
            );
            revolver.Perform(upToFace);
            if (!revolver.IsDone()) {
                return NamedShapeBuild {
                    TopoDS_Shape {},
                    std::nullopt,
                    "BRepFeat_MakeRevol could not revolve profile up to face"
                };
            }
            currentBase = revolver.Shape();
            built = true;
        }
        if (!built || currentBase.IsNull()) {
            return NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Revolution-until produced a null shape"};
        }

        NamedShape namedShape = namedShapeForMakerHistory(
            owner,
            currentBase,
            std::vector<NamedShapeSource> {baseSource, profileSource},
            revolver
        );
        addDistinct(namedShape.elementHistoryStatus, "part_design_revolution:brepfeat_make_revol_history");
        return NamedShapeBuild {currentBase, std::move(namedShape), {}};
    }
    catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        return NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            std::string("PartDesign revolution-until failed: ")
                + (message != nullptr ? message : "unknown OCCT error"),
        };
    }
}

FilledFaceBuild makeElementFilledFaceFromSources(
    const std::string& owner,
    const std::vector<FilledFaceSource>& boundarySources,
    const std::vector<NamedShapeSource>& historySources,
    const FilledFaceParams& params,
    const std::optional<FilledFaceSource>& initialSurface,
    const std::vector<FilledFaceSupportSource>& supportSources,
    const std::vector<FilledFaceOrderSource>& orderSources
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementFilledFace(), calls "BRepOffsetAPI_MakeFilling", optional
    // "LoadInitSurface", then "findBoundary(shapes)" or "makeElementWires(...)", fixes the
    // boundary wire, adds ordered boundary edges with "Add(edge, support, order, IsBound true)",
    // adds remaining wire/edge constraints with "IsBound false", adds face/vertex constraints
    // with the matching "maker.Add" overload, and returns makeElementShape(..., FilledFace).
    if (boundarySources.empty()) {
        return FilledFaceBuild {TopoDS_Shape {}, std::nullopt, "No input shape"};
    }

    try {
        std::vector<FilledFaceWorkingShape> shapes = expandedFilledFaceSources(boundarySources);
        const int compoundSourceCount = compoundFilledFaceSourceCount(boundarySources);
        const int expandedSourceCount = static_cast<int>(shapes.size());
        auto boundary = findFilledFaceBoundaryWire(shapes);
        if (!boundary) {
            boundary = buildFilledFaceBoundaryWireFromEdges(shapes);
        }
        if (!boundary) {
            return FilledFaceBuild {TopoDS_Shape {}, std::nullopt, "No boundary wire"};
        }

        BRepOffsetAPI_MakeFilling maker(
            params.degree,
            params.pointsOnCurve,
            params.iterations,
            params.anisotropy,
            params.tolerance2d,
            params.tolerance3d,
            params.toleranceG1,
            params.toleranceG2,
            params.maxDegree,
            params.maxSegments
        );
        if (initialSurface && !initialSurface->shape.IsNull()
            && initialSurface->shape.ShapeType() == TopAbs_FACE) {
            maker.LoadInitSurface(TopoDS::Face(initialSurface->shape));
        }

        TopoDS_Wire fixedBoundary = fixedFillingBoundaryWire(boundary->wire);
        const std::vector<TopoDS_Edge> boundaryEdges = orderedEdges(fixedBoundary);
        if (boundaryEdges.empty()) {
            return FilledFaceBuild {TopoDS_Shape {}, std::nullopt, "No boundary wire"};
        }

        for (const TopoDS_Edge& edge : boundaryEdges) {
            maker.Add(
                edge,
                supportFaceForBoundaryEdge(edge, supportSources),
                orderForBoundaryEdge(edge, orderSources),
                Standard_True
            );
        }

        std::vector<FilledFaceConstraintEvidence> nonBoundaryEvidence;
        std::vector<TopoDS_Edge> nonBoundaryEdges;
        int nonBoundaryConstraintCount = 0;
        auto appendNonBoundaryEdgeEvidence = [&](const FilledFaceWorkingShape& item,
                                                 const TopoDS_Edge& edge) {
            if (item.source == nullptr) {
                return;
            }
            for (const FilledFaceBoundaryEvidence& evidence :
                 evidenceForBoundaryEdge(*item.source, edge)) {
                nonBoundaryEvidence.push_back(nonBoundaryEvidenceFromBoundaryEvidence(
                    evidence,
                    "Add(edge, support, order, IsBound=false)"
                ));
            }
        };
        auto appendNonBoundaryShapeEvidence = [&](const FilledFaceWorkingShape& item,
                                                  TopAbs_ShapeEnum kind,
                                                  const std::string& builderCall) {
            if (item.source == nullptr) {
                return;
            }
            nonBoundaryEvidence.push_back(nonBoundaryEvidenceFromBoundaryEvidence(
                fallbackEvidence(*item.source, kind),
                builderCall
            ));
        };
        auto invalidNonBoundaryBuild = [&](const FilledFaceWorkingShape& item) {
            FilledFaceBuild build;
            build.error = "Part.makeFilledFace non-boundary source must resolve to a wire, edge, face, or vertex";
            build.diagnosticCode = "invalid_non_boundary_source";
            build.diagnosticProperty = "Boundary";
            if (item.source != nullptr) {
                build.diagnosticTarget = item.source->objectName;
                build.diagnosticSubname = item.source->stableSubname.empty() ? item.source->subname
                                                                             : item.source->stableSubname;
            }
            return build;
        };

        for (const FilledFaceWorkingShape& item : shapes) {
            if (item.shape.IsNull()) {
                continue;
            }
            if (item.shape.ShapeType() == TopAbs_WIRE) {
                for (const TopoDS_Edge& edge : orderedEdges(TopoDS::Wire(item.shape))) {
                    maker.Add(
                        edge,
                        supportFaceForBoundaryEdge(edge, supportSources),
                        orderForBoundaryEdge(edge, orderSources),
                        Standard_False
                    );
                    nonBoundaryEdges.push_back(edge);
                    ++nonBoundaryConstraintCount;
                    appendNonBoundaryEdgeEvidence(item, edge);
                }
                continue;
            }
            if (item.shape.ShapeType() == TopAbs_EDGE) {
                const TopoDS_Edge edge = TopoDS::Edge(item.shape);
                maker.Add(
                    edge,
                    supportFaceForBoundaryEdge(edge, supportSources),
                    orderForBoundaryEdge(edge, orderSources),
                    Standard_False
                );
                nonBoundaryEdges.push_back(edge);
                ++nonBoundaryConstraintCount;
                appendNonBoundaryEdgeEvidence(item, edge);
                continue;
            }
            if (item.shape.ShapeType() == TopAbs_FACE) {
                maker.Add(TopoDS::Face(item.shape), GeomAbs_C0);
                ++nonBoundaryConstraintCount;
                appendNonBoundaryShapeEvidence(item, TopAbs_FACE, "Add(face, order)");
                continue;
            }
            if (item.shape.ShapeType() == TopAbs_VERTEX) {
                maker.Add(BRep_Tool::Pnt(TopoDS::Vertex(item.shape)));
                ++nonBoundaryConstraintCount;
                appendNonBoundaryShapeEvidence(item, TopAbs_VERTEX, "Add(point)");
                continue;
            }
            return invalidNonBoundaryBuild(item);
        }

        maker.Build();
        if (!maker.IsDone() || maker.Shape().IsNull()) {
            return FilledFaceBuild {
                TopoDS_Shape {},
                std::nullopt,
                "Failed to created face by filling edges"
            };
        }

        std::vector<NamedShapeSource> sourcesForHistory = historySources;
        if (sourcesForHistory.empty()) {
            sourcesForHistory.reserve(boundarySources.size());
            for (const FilledFaceSource& source : boundarySources) {
                sourcesForHistory.push_back(
                    NamedShapeSource {
                        source.objectName,
                        source.shape,
                        source.namedShape,
                    }
                );
            }
        }
        auto addHistorySourceIfMissing = [&](const FilledFaceSource& source) {
            const auto duplicate = std::find_if(
                sourcesForHistory.begin(),
                sourcesForHistory.end(),
                [&](const NamedShapeSource& current) { return current.owner == source.objectName; }
            );
            if (duplicate == sourcesForHistory.end()) {
                sourcesForHistory.push_back(
                    NamedShapeSource {
                        source.objectName,
                        source.shape,
                        source.namedShape,
                    }
                );
            }
        };
        if (initialSurface) {
            addHistorySourceIfMissing(*initialSurface);
        }
        for (const FilledFaceSupportSource& support : supportSources) {
            addHistorySourceIfMissing(support.target);
            addHistorySourceIfMissing(support.support);
        }
        for (const FilledFaceOrderSource& order : orderSources) {
            addHistorySourceIfMissing(order.target);
        }

        NamedShape namedShape
            = namedShapeForMakerHistory(owner, maker.Shape(), sourcesForHistory, maker);
        addDistinct(namedShape.elementHistoryStatus, "part_filling:filling_history");
        addFilledFaceBoundaryHistory(
            namedShape,
            owner,
            boundary->evidence,
            static_cast<int>(boundaryEdges.size())
        );
        std::optional<FilledFaceBoundaryEvidence> surfaceEvidence;
        if (initialSurface) {
            surfaceEvidence = fallbackEvidence(*initialSurface, TopAbs_FACE);
            addDistinct(namedShape.elementHistoryStatus, "part_filling:initial_surface");
            addFilledFaceInitialSurfaceHistory(namedShape, owner, *surfaceEvidence);
        }
        const std::vector<FilledFaceSupportOrderEvidence> supportOrderSourcesEvidence
            = supportOrderEvidence(supportSources, orderSources, boundaryEdges, nonBoundaryEdges);
        if (!supportOrderSourcesEvidence.empty()) {
            addDistinct(namedShape.elementHistoryStatus, "part_filling:support_order_sources");
            addFilledFaceSupportOrderHistory(namedShape, owner, supportOrderSourcesEvidence);
        }
        if (!nonBoundaryEvidence.empty()) {
            addDistinct(namedShape.elementHistoryStatus, "part_filling:non_boundary_constraints");
            addFilledFaceNonBoundaryHistory(namedShape, owner, nonBoundaryEvidence);
        }
        if (compoundSourceCount > 0) {
            addDistinct(namedShape.elementHistoryStatus, "part_filling:compound_source_expansion");
        }

        return FilledFaceBuild {
            maker.Shape(),
            std::move(namedShape),
            {},
            boundary->mode,
            boundary->evidence,
            surfaceEvidence,
            supportOrderSourcesEvidence,
            nonBoundaryEvidence,
            static_cast<int>(boundaryEdges.size()),
            compoundSourceCount,
            expandedSourceCount,
            nonBoundaryConstraintCount,
            static_cast<int>(supportSources.size()),
            static_cast<int>(orderSources.size()),
        };
    }
    catch (const Standard_Failure& failure) {
        return FilledFaceBuild {
            TopoDS_Shape {},
            std::nullopt,
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to created face by filling edges"
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
