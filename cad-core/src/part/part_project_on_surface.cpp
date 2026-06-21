#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepProj_Projection.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <ShapeAnalysis.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Wire.hxx>
#include <ShapeFix_Wireframe.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part
{

namespace
{

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::getProjectionShapes(), pairs "Projection.getValues()" with
// "Projection.getSubValues()" by index before createProjectedWire() receives only a shape.
// cad-core keeps that LinkSubList item ledger request-local so later MapperHistory events do not
// infer source ownership from projected output order or geometry similarity.
struct ProjectSubshape
{
    std::string objectName;
    std::string sourceSubname;
    std::string stableSubname;
    std::size_t projectionItemIndex = 0;
    std::string sourceShapeKind;
    TopoDS_Shape shape;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::tryExecute(), appends createProjectedWire() results before filterShapes()
// and createCompound(); ::projectWire() iterates projected edges. These fields carry that source
// item, maker stage, filter stage, and edge fragment evidence into topo MapperHistory.
struct ProjectedShapeEvidence
{
    std::string sourceObject;
    std::string sourceSubname;
    std::string stableSubname;
    std::size_t projectionItemIndex = 0;
    std::string sourceShapeKind;
    std::string mode;
    std::string makerStage;
    std::string projectedResultId;
    std::size_t projectedWireIndex = 0;
    std::size_t edgeFragmentIndex = 0;
    std::size_t filterOutputIndex = 0;
    std::string filterMode;
    std::string preFilterResultId;
    std::string filterStage;
};

struct ProjectedShape
{
    TopoDS_Shape shape;
    ProjectedShapeEvidence evidence;
};

void addProjectOnSurfaceDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {},
    const std::string& subname = {}
)
{
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        code,
        message,
        object.name,
        property,
        "runtime",
        target,
        subname
    );
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_project_on_surface"}};
}

std::string stableSubnameForLink(const app::Link& link)
{
    if (!link.stableSubnames.empty() && !link.stableSubnames.front().empty()) {
        return link.stableSubnames.front();
    }
    if (!link.subnames.empty()) {
        return link.subnames.front();
    }
    return {};
}

std::string sourceSubnameForLink(const app::Link& link)
{
    if (!link.subnames.empty()) {
        return link.subnames.front();
    }
    return {};
}

std::string shapeKindForProjectionShape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return "shape";
    }
    switch (shape.ShapeType()) {
        case TopAbs_EDGE:
            return "edge";
        case TopAbs_WIRE:
            return "wire";
        case TopAbs_FACE:
            return "face";
        case TopAbs_SOLID:
            return "solid";
        default:
            return "shape";
    }
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::createProjectedWire(), routes "TopAbs_WIRE || TopAbs_EDGE" to
// projectWire(); Feature::getTopoShape(... NeedSubElement ...) can resolve a wire subelement
// even though cad-core's general public subshape map still exposes only Face/Edge/Vertex.
std::optional<TopoDS_Shape> wireByName(const TopoDS_Shape& shape, const std::string& name)
{
    constexpr const char* wirePrefix = "Wire";
    if (name.rfind(wirePrefix, 0) != 0U) {
        return std::nullopt;
    }
    const std::string indexText = name.substr(std::string(wirePrefix).size());
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
    int currentIndex = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        ++currentIndex;
        if (currentIndex == index) {
            return TopoDS::Wire(explorer.Current());
        }
    }
    return std::nullopt;
}

std::optional<TopoDS_Shape> linkedSubshape(
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape,
    const app::Link& link
)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return sourceShape;
    }

    const std::string current = link.subnames.front();
    const std::string stable = stableSubnameForLink(link);
    if (namedShape != nullptr) {
        if (auto shape = part::subshapeByName(*namedShape, current, stable)) {
            return shape;
        }
    }
    if (auto shape = part::subshapeByName(sourceShape, current)) {
        return shape;
    }
    if (auto shape = wireByName(sourceShape, current)) {
        return shape;
    }
    if (stable != current) {
        if (auto shape = part::subshapeByName(sourceShape, stable)) {
            return shape;
        }
        return wireByName(sourceShape, stable);
    }
    return std::nullopt;
}

std::optional<ProjectSubshape> resolveSingleSubshapeLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    TopAbs_ShapeEnum expectedKind,
    const std::string& expectedLabel
)
{
    const auto* propertyValue = app::propertyValue(object, property);
    if (propertyValue == nullptr) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_property",
            "Part::ProjectOnSurface " + property + " must be specified",
            property
        );
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, property);
    if (links.empty() || links.front().object.empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface " + property + " target is missing",
            property
        );
        return std::nullopt;
    }
    if (links.size() != 1U || links.front().subnames.size() != 1U
        || links.front().subnames.front().empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface " + property + " must reference exactly one subshape",
            property,
            links.front().object,
            sourceSubnameForLink(links.front())
        );
        return std::nullopt;
    }

    const app::Link& link = links.front();
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface " + property + " target did not produce a shape",
            property,
            link.object,
            sourceSubnameForLink(link)
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link.object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;
    const auto selected = linkedSubshape(shapeIt->second.shape, namedShape, link);
    if (!selected || selected->IsNull()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface " + property + " subshape cannot be resolved",
            property,
            link.object,
            sourceSubnameForLink(link)
        );
        return std::nullopt;
    }
    if (selected->ShapeType() != expectedKind) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_subshape_kind",
            "Part::ProjectOnSurface " + property + " first slice requires a " + expectedLabel,
            property,
            link.object,
            sourceSubnameForLink(link)
        );
        return std::nullopt;
    }

    return ProjectSubshape {
        link.object,
        sourceSubnameForLink(link),
        stableSubnameForLink(link),
        0,
        shapeKindForProjectionShape(*selected),
        *selected
    };
}

std::optional<std::vector<ProjectSubshape>> resolveProjectionShapes(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto* propertyValue = app::propertyValue(object, "Projection");
    if (propertyValue == nullptr) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_property",
            "Part::ProjectOnSurface Projection must be specified",
            "Projection"
        );
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, "Projection");
    if (links.empty() || links.front().object.empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface Projection target is missing",
            "Projection"
        );
        return std::nullopt;
    }

    std::size_t subnameCount = 0;
    for (std::size_t itemIndex = 0; itemIndex < links.size(); ++itemIndex) {
        const app::Link& link = links.at(itemIndex);
        subnameCount += link.subnames.size();
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
    // ::ProjectOnSurface::getProjectionShapes(), reads "Projection.getValues()" and
    // "Projection.getSubValues()"; if object/sub-name counts differ it throws
    // "Number of objects and sub-names differ", otherwise it resolves each pair by index.
    if (links.size() != subnameCount) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface Projection object/subname counts differ",
            "Projection",
            links.front().object,
            sourceSubnameForLink(links.front())
        );
        return std::nullopt;
    }

    std::vector<ProjectSubshape> projections;
    projections.reserve(links.size());
    for (std::size_t itemIndex = 0; itemIndex < links.size(); ++itemIndex) {
        const app::Link& link = links.at(itemIndex);
        if (link.subnames.empty() || link.subnames.front().empty()) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Part::ProjectOnSurface Projection must reference a non-empty subshape",
                "Projection",
                link.object,
                sourceSubnameForLink(link)
            );
            return std::nullopt;
        }

        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "missing_link_target",
                "Part::ProjectOnSurface Projection target did not produce a shape",
                "Projection",
                link.object,
                sourceSubnameForLink(link)
            );
            return std::nullopt;
        }

        const auto namedShapeIt = context.namedShapes.find(link.object);
        const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
            ? &namedShapeIt->second
            : nullptr;
        const auto selected = linkedSubshape(shapeIt->second.shape, namedShape, link);
        if (!selected || selected->IsNull()) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Part::ProjectOnSurface Projection subshape cannot be resolved",
                "Projection",
                link.object,
                sourceSubnameForLink(link)
            );
            return std::nullopt;
        }
        if (selected->ShapeType() != TopAbs_EDGE && selected->ShapeType() != TopAbs_WIRE
            && selected->ShapeType() != TopAbs_FACE) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "unsupported_subshape_kind",
                "Part::ProjectOnSurface projects only edges, wires and faces",
                "Projection",
                link.object,
                sourceSubnameForLink(link)
            );
            return std::nullopt;
        }

        projections.push_back(ProjectSubshape {
            link.object,
            sourceSubnameForLink(link),
            stableSubnameForLink(link),
            itemIndex,
            shapeKindForProjectionShape(*selected),
            *selected
        });
    }

    return projections;
}

std::optional<std::string> readProjectionMode(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
    // ::ProjectOnSurface::ProjectOnSurface(), registers Mode enums "All", "Faces", "Edges".
    std::string mode = "All";
    if (const auto label = app::readString(object, "Mode")) {
        mode = *label;
    }
    else if (const auto value = app::readNumber(object, "Mode")) {
        const auto index = static_cast<int>(std::llround(*value));
        constexpr std::array<const char*, 3> labels = {"All", "Faces", "Edges"};
        if (index >= 0 && index < static_cast<int>(labels.size())) {
            mode = labels.at(static_cast<std::size_t>(index));
        }
    }

    if (mode != "All" && mode != "Faces" && mode != "Edges") {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::ProjectOnSurface mode must be All, Faces or Edges",
            "Mode"
        );
        return std::nullopt;
    }
    return mode;
}

std::optional<gp_Dir> readProjectionDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto vector = app::readVector3(object, "Direction").value_or(std::array<double, 3> {0.0, 0.0, 1.0});
    const double magnitudeSquared =
        vector.at(0) * vector.at(0) + vector.at(1) * vector.at(1) + vector.at(2) * vector.at(2);
    if (magnitudeSquared <= Precision::Confusion() * Precision::Confusion()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_direction",
            "Part::ProjectOnSurface Direction must be non-zero",
            "Direction"
        );
        return std::nullopt;
    }
    return gp_Dir(vector.at(0), vector.at(1), vector.at(2));
}

TopoDS_Wire closestProjectedWire(BRepProj_Projection& projection, const TopoDS_Shape& reference)
{
    double minDistance = std::numeric_limits<double>::max();
    TopoDS_Wire wireToTake;
    for (; projection.More(); projection.Next()) {
        const TopoDS_Wire current = projection.Current();
        BRepExtrema_DistShapeShape distanceMeasure(current, reference);
        distanceMeasure.Perform();
        const double currentDistance = distanceMeasure.Value();
        if (currentDistance > minDistance) {
            continue;
        }
        wireToTake = current;
        minDistance = currentDistance;
    }
    return wireToTake;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::getWires(), calls "ShapeAnalysis::OuterWire(face)" first and appends
// the remaining non-outer wires afterward so createFaceFromParametricWire() can treat the first
// wire as the outer boundary and later wires as inside wires.
std::vector<TopoDS_Wire> faceWires(const TopoDS_Face& face)
{
    std::vector<TopoDS_Wire> wires;
    const TopoDS_Wire outerWire = ShapeAnalysis::OuterWire(face);
    if (!outerWire.IsNull()) {
        wires.push_back(outerWire);
    }
    for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        const TopoDS_Wire currentWire = TopoDS::Wire(explorer.Current());
        if (outerWire.IsNull() || !currentWire.IsSame(outerWire)) {
            wires.push_back(currentWire);
        }
    }
    return wires;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::fixWire(), runs "ConnectEdgesToWires", "ConnectWiresToWires",
// ShapeFix_Wire with FixAddCurve3d/FixAddPCurve and ShapeFix_Wireframe gap/small-edge repair.
TopoDS_Wire fixWireOnSupport(
    const std::vector<TopoDS_Edge>& edges,
    const TopoDS_Face& supportFace
)
{
    Handle(TopTools_HSequenceOfShape) shapeList = new TopTools_HSequenceOfShape;
    Handle(TopTools_HSequenceOfShape) wireHandle;
    Handle(TopTools_HSequenceOfShape) connectedWireHandle;

    for (const TopoDS_Edge& edge : edges) {
        if (!edge.IsNull()) {
            shapeList->Append(edge);
        }
    }

    constexpr double tolerance = 0.0001;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(shapeList, tolerance, false, wireHandle);
    ShapeAnalysis_FreeBounds::ConnectWiresToWires(wireHandle, tolerance, false, connectedWireHandle);
    if (!connectedWireHandle) {
        return {};
    }
    for (int index = 1; index <= connectedWireHandle->Length(); ++index) {
        const TopoDS_Wire wire = TopoDS::Wire(connectedWireHandle->Value(index));
        ShapeFix_Wire wireRepair(wire, supportFace, tolerance);
        wireRepair.FixAddCurve3dMode() = 1;
        wireRepair.FixAddPCurveMode() = 1;
        wireRepair.Perform();

        ShapeFix_Wireframe wireframeFix(wireRepair.Wire());
        wireframeFix.FixWireGaps();
        wireframeFix.FixSmallEdges();
        return TopoDS::Wire(wireframeFix.Shape());
    }
    return {};
}

TopoDS_Wire fixWireOnSupport(const TopoDS_Shape& shape, const TopoDS_Face& supportFace)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return fixWireOnSupport(edges, supportFace);
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::projectFace(), projects each face wire with BRepProj_Projection and fixes
// the selected projected wire before face rebuild.
std::vector<TopoDS_Wire> projectFaceWires(
    const TopoDS_Face& face,
    const TopoDS_Face& supportFace,
    const gp_Dir& direction
)
{
    std::vector<TopoDS_Wire> wires;
    for (const TopoDS_Wire& wire : faceWires(face)) {
        BRepProj_Projection projection(wire, supportFace, direction);
        const TopoDS_Wire projectedWire = closestProjectedWire(projection, face);
        const TopoDS_Wire fixedWire = fixWireOnSupport(projectedWire, supportFace);
        if (!fixedWire.IsNull()) {
            wires.push_back(fixedWire);
        }
    }
    return wires;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::createWiresFromWires(), rebuilds every projected edge from its
// CurveOnSurface on the support face before making the final face.
std::vector<TopoDS_Wire> rebuildWiresInParametricSpace(
    const std::vector<TopoDS_Wire>& projectedWires,
    const TopoDS_Face& supportFace
)
{
    std::vector<TopoDS_Wire> rebuiltWires;
    const auto surface = BRep_Tool::Surface(supportFace);
    for (const TopoDS_Wire& wire : projectedWires) {
        std::vector<TopoDS_Edge> rebuiltEdges;
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            Standard_Real first {};
            Standard_Real last {};
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            const auto currentCurve = BRep_Tool::CurveOnSurface(edge, supportFace, first, last);
            if (currentCurve.IsNull()) {
                continue;
            }
            BRepBuilderAPI_MakeEdge edgeMaker(currentCurve, surface, first, last);
            if (edgeMaker.IsDone()) {
                rebuiltEdges.push_back(edgeMaker.Edge());
            }
        }
        const TopoDS_Wire rebuiltWire = fixWireOnSupport(rebuiltEdges, supportFace);
        if (!rebuiltWire.IsNull()) {
            rebuiltWires.push_back(rebuiltWire);
        }
    }
    return rebuiltWires;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::createFaceFromParametricWire(), uses the first wire as the outer wire,
// later wires as inside wires, and retries with reversed orientation after ShapeFix_Face /
// BRepCheck_Analyzer validation failures.
TopoDS_Face createFaceFromParametricWires(
    const std::vector<TopoDS_Wire>& wires,
    const TopoDS_Face& supportFace
)
{
    const auto surface = BRep_Tool::Surface(supportFace);
    BRepBuilderAPI_MakeFace faceMaker;
    bool first = true;
    for (const TopoDS_Wire& wire : wires) {
        if (wire.IsNull()) {
            continue;
        }
        if (first) {
            first = false;
            TopoDS_Wire currentWire = TopoDS::Wire(wire.Reversed());
            if (supportFace.Orientation() == TopAbs_REVERSED) {
                currentWire = wire;
            }
            faceMaker = BRepBuilderAPI_MakeFace(surface, currentWire);
            if (!faceMaker.IsDone()) {
                return {};
            }
            ShapeFix_Face fix(faceMaker.Face());
            fix.Perform();
            const TopoDS_Face fixedFace = fix.Face();
            BRepCheck_Analyzer checker(fixedFace);
            if (!checker.IsValid()) {
                faceMaker = BRepBuilderAPI_MakeFace(surface, TopoDS::Wire(currentWire.Reversed()));
                if (!faceMaker.IsDone()) {
                    return {};
                }
            }
        }
        else {
            if (!faceMaker.IsDone()) {
                return {};
            }
            const TopoDS_Face tempCopy = BRepBuilderAPI_MakeFace(faceMaker.Face()).Face();
            faceMaker.Add(TopoDS::Wire(wire.Reversed()));
            ShapeFix_Face fix(faceMaker.Face());
            fix.Perform();
            const TopoDS_Face fixedFace = fix.Face();
            BRepCheck_Analyzer checker(fixedFace);
            if (!checker.IsValid()) {
                faceMaker = BRepBuilderAPI_MakeFace(tempCopy);
                faceMaker.Add(TopoDS::Wire(wire));
            }
        }
    }
    if (first || !faceMaker.IsDone()) {
        return {};
    }
    return faceMaker.Face();
}

TopoDS_Face createFaceFromProjectedWires(
    const std::vector<TopoDS_Wire>& projectedWires,
    const TopoDS_Face& supportFace
)
{
    if (projectedWires.empty()) {
        return {};
    }
    return createFaceFromParametricWires(
        rebuildWiresInParametricSpace(projectedWires, supportFace),
        supportFace
    );
}

ProjectedShapeEvidence evidenceForProjection(
    const ProjectSubshape& projection,
    const std::string& mode,
    const std::string& makerStage,
    std::size_t projectedResultIndex
)
{
    ProjectedShapeEvidence evidence;
    evidence.sourceObject = projection.objectName;
    evidence.sourceSubname = projection.sourceSubname;
    evidence.stableSubname = projection.stableSubname;
    evidence.projectionItemIndex = projection.projectionItemIndex;
    evidence.sourceShapeKind = projection.sourceShapeKind;
    evidence.mode = mode;
    evidence.makerStage = makerStage;
    evidence.projectedResultId = "projection_item_" + std::to_string(projection.projectionItemIndex)
        + ":" + makerStage + ":" + std::to_string(projectedResultIndex);
    evidence.projectedWireIndex = 0;
    evidence.edgeFragmentIndex = 0;
    evidence.preFilterResultId = evidence.projectedResultId;
    return evidence;
}

std::vector<ProjectedShape> projectWireEdges(
    const ProjectSubshape& projectionItem,
    const TopoDS_Face& supportFace,
    const gp_Dir& direction,
    const std::string& mode
)
{
    BRepProj_Projection projection(projectionItem.shape, supportFace, direction);
    const TopoDS_Wire projectedWire = closestProjectedWire(projection, projectionItem.shape);
    std::vector<ProjectedShape> edges;
    std::size_t edgeFragmentIndex = 0;
    for (TopExp_Explorer explorer(projectedWire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        ProjectedShapeEvidence evidence = evidenceForProjection(
            projectionItem,
            mode,
            "project_wire",
            edgeFragmentIndex
        );
        evidence.edgeFragmentIndex = edgeFragmentIndex;
        edges.push_back(ProjectedShape {TopoDS::Edge(explorer.Current()), std::move(evidence)});
        ++edgeFragmentIndex;
    }
    return edges;
}

std::vector<ProjectedShape> createProjectedShapes(
    const ProjectSubshape& projectionItem,
    const TopoDS_Face& supportFace,
    const gp_Dir& direction,
    const std::string& mode
)
{
    const TopoDS_Shape& projectionShape = projectionItem.shape;
    if (projectionShape.IsNull()) {
        return {};
    }
    if (projectionShape.ShapeType() == TopAbs_FACE) {
        const std::vector<TopoDS_Wire> projectedWires =
            projectFaceWires(TopoDS::Face(projectionShape), supportFace, direction);
        const TopoDS_Face face = createFaceFromProjectedWires(projectedWires, supportFace);
        if (!face.IsNull()) {
            return {
                ProjectedShape {
                    face,
                    evidenceForProjection(projectionItem, mode, "face_rebuild", 0)
                }
            };
        }
        std::vector<ProjectedShape> wireResults;
        wireResults.reserve(projectedWires.size());
        for (std::size_t index = 0; index < projectedWires.size(); ++index) {
            ProjectedShapeEvidence evidence =
                evidenceForProjection(projectionItem, mode, "project_face_wire", index);
            evidence.projectedWireIndex = index;
            wireResults.push_back(ProjectedShape {projectedWires.at(index), std::move(evidence)});
        }
        return wireResults;
    }
    if (projectionShape.ShapeType() == TopAbs_WIRE || projectionShape.ShapeType() == TopAbs_EDGE) {
        return projectWireEdges(projectionItem, supportFace, direction, mode);
    }
    return {};
}

std::vector<ProjectedShape> filterProjectedShapes(
    const std::vector<ProjectedShape>& shapes,
    const std::string& mode
)
{
    std::vector<ProjectedShape> filtered;
    for (const ProjectedShape& projected : shapes) {
        const TopoDS_Shape& shape = projected.shape;
        if (shape.IsNull()) {
            continue;
        }
        if (mode == "All") {
            ProjectedShape current = projected;
            current.evidence.filterOutputIndex = filtered.size();
            current.evidence.filterMode = mode;
            current.evidence.filterStage = "pass";
            filtered.push_back(std::move(current));
        }
        else if (mode == "Faces") {
            if (shape.ShapeType() == TopAbs_FACE) {
                ProjectedShape current = projected;
                current.evidence.filterOutputIndex = filtered.size();
                current.evidence.filterMode = mode;
                current.evidence.filterStage = "pass";
                filtered.push_back(std::move(current));
            }
        }
        else if (mode == "Edges") {
            if (shape.ShapeType() == TopAbs_EDGE || shape.ShapeType() == TopAbs_WIRE) {
                ProjectedShape current = projected;
                current.evidence.filterOutputIndex = filtered.size();
                current.evidence.filterMode = mode;
                current.evidence.filterStage = "pass";
                filtered.push_back(std::move(current));
            }
            else if (shape.ShapeType() == TopAbs_FACE) {
                std::size_t faceWireIndex = 0;
                for (const TopoDS_Wire& wire : faceWires(TopoDS::Face(shape))) {
                    if (!wire.IsNull()) {
                        ProjectedShape current = projected;
                        current.shape = wire;
                        current.evidence.filterOutputIndex = filtered.size();
                        current.evidence.filterMode = mode;
                        current.evidence.filterStage = "face_to_wire";
                        current.evidence.projectedWireIndex = faceWireIndex;
                        filtered.push_back(std::move(current));
                    }
                    ++faceWireIndex;
                }
            }
        }
    }
    return filtered;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::createSolidIfHeight(), returns the face unless
// "height < Precision::Confusion() || Mode.getValue() != 0L"; otherwise reverses Direction,
// multiplies it by Height, and calls BRepPrimAPI_MakePrism.
TopoDS_Shape createSolidIfHeight(
    const TopoDS_Shape& shape,
    const std::string& mode,
    const gp_Dir& direction,
    double height
)
{
    if (shape.IsNull() || shape.ShapeType() != TopAbs_FACE || mode != "All"
        || height < Precision::Confusion()) {
        return shape;
    }

    gp_Vec directionToExtrude(direction);
    directionToExtrude.Reverse();
    directionToExtrude.Multiply(height);

    BRepPrimAPI_MakePrism extrude(TopoDS::Face(shape), directionToExtrude);
    return extrude.Shape();
}

std::vector<ProjectedShape> createSolidsIfHeight(
    const std::vector<ProjectedShape>& shapes,
    const std::string& mode,
    const gp_Dir& direction,
    double height
)
{
    std::vector<ProjectedShape> results;
    results.reserve(shapes.size());
    for (const ProjectedShape& shape : shapes) {
        ProjectedShape current = shape;
        current.shape = createSolidIfHeight(shape.shape, mode, direction, height);
        results.push_back(std::move(current));
    }
    return results;
}

int countSubshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

int countInnerWires(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const int wireCount = static_cast<int>(faceWires(TopoDS::Face(explorer.Current())).size());
        count += std::max(0, wireCount - 1);
    }
    return count;
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::getOffsetPlacement(), reads "Offset"; for non-zero offset it normalizes
// "Direction", scales it by Offset, builds a translation TopLoc_Location, and
// ::createCompound() adds each child shape as "it.Moved(loc)" after filtering/solid creation.
gp_Vec offsetVectorForDirection(const gp_Dir& direction, double offset)
{
    gp_Vec vector(direction);
    vector.Multiply(offset);
    return vector;
}

TopLoc_Location offsetPlacementForVector(const gp_Vec& offsetVector, double offset)
{
    if (offset == 0.0) {
        return {};
    }
    gp_Trsf transform;
    transform.SetTranslation(offsetVector);
    return TopLoc_Location(transform);
}

TopoDS_Shape compoundOf(const std::vector<ProjectedShape>& shapes, const TopLoc_Location& offsetPlacement)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    const bool isIdentity = offsetPlacement.IsIdentity();
    for (const ProjectedShape& shape : shapes) {
        if (!shape.shape.IsNull()) {
            builder.Add(compound, isIdentity ? shape.shape : shape.shape.Moved(offsetPlacement));
        }
    }
    return compound;
}

nlohmann::json projectionItemsJson(const std::vector<ProjectSubshape>& projections)
{
    nlohmann::json items = nlohmann::json::array();
    for (const ProjectSubshape& projection : projections) {
        items.push_back({{"object", projection.objectName}, {"subshape", projection.stableSubname}});
    }
    return items;
}

nlohmann::json projectionItemLedgerJson(const std::vector<ProjectSubshape>& projections)
{
    nlohmann::json items = nlohmann::json::array();
    for (const ProjectSubshape& projection : projections) {
        items.push_back(
            {
                {"source_object", projection.objectName},
                {"source_subname", projection.sourceSubname},
                {"stable_subname", projection.stableSubname},
                {"projection_item_index", projection.projectionItemIndex},
                {"source_shape_kind", projection.sourceShapeKind},
            }
        );
    }
    return items;
}

std::optional<std::string> targetElementForShape(
    const NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    for (const auto& [elementName, element] : namedShape.elements) {
        if (element.subshape.kind != kind) {
            continue;
        }
        const auto targetShape = subshapeByName(namedShape, elementName);
        if (targetShape && targetShape->IsSame(shape)) {
            return elementName;
        }
    }
    return std::nullopt;
}

nlohmann::json projectOnSurfaceMapperEvidenceJson(
    const ProjectedShapeEvidence& evidence,
    const std::string& mapperHistoryId,
    const std::string& targetElement
)
{
    return {
        {"source_object", evidence.sourceObject},
        {"source_subname", evidence.sourceSubname},
        {"stable_subname", evidence.stableSubname},
        {"projection_item_index", evidence.projectionItemIndex},
        {"source_shape_kind", evidence.sourceShapeKind},
        {"mode", evidence.mode},
        {"maker_stage", evidence.makerStage},
        {"projected_result_id", evidence.projectedResultId},
        {"projected_wire_index", evidence.projectedWireIndex},
        {"edge_fragment_index", evidence.edgeFragmentIndex},
        {"filter_output_index", evidence.filterOutputIndex},
        {"filter_mode", evidence.filterMode},
        {"filter_stage", evidence.filterStage},
        {"pre_filter_result_id", evidence.preFilterResultId},
        {"mapper_history_id", mapperHistoryId},
        {"element_map_target", targetElement},
        {"reference_recovery_hook", "mapper_history_event_target_subname"},
        {"wire_fragment_ownership", {
            {"source_object", evidence.sourceObject},
            {"source_subname", evidence.sourceSubname},
            {"projection_item_index", evidence.projectionItemIndex},
            {"projected_wire_index", evidence.projectedWireIndex},
            {"edge_fragment_index", evidence.edgeFragmentIndex},
        }},
    };
}

nlohmann::json projectedEdgeWireHistoryJson(const std::vector<MapperHistoryEvent>& events)
{
    return mapperHistoryToJson(events);
}

void addDistinctStatus(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

NamedShape namedShapeForProjectOnSurfaceEdgeWireHistory(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const std::vector<ProjectedShape>& filteredShapes,
    const TopLoc_Location& offsetPlacement
)
{
    NamedShape namedShape = indexedNamedShapeForObject(owner, resultShape);
    const bool hasOffset = !offsetPlacement.IsIdentity();
    for (const ProjectedShape& projected : filteredShapes) {
        const ProjectedShapeEvidence& evidence = projected.evidence;
        if (evidence.makerStage != "project_wire") {
            continue;
        }
        if (evidence.sourceShapeKind != "edge" && evidence.sourceShapeKind != "wire") {
            continue;
        }
        const TopAbs_ShapeEnum targetKind = projected.shape.ShapeType() == TopAbs_WIRE
            ? TopAbs_WIRE
            : TopAbs_EDGE;
        const TopoDS_Shape targetShape = hasOffset ? projected.shape.Moved(offsetPlacement)
                                                   : projected.shape;
        const auto targetElement = targetElementForShape(namedShape, targetShape, targetKind);
        if (!targetElement) {
            continue;
        }

        const std::string mapperHistoryId = owner + ":projection_item_"
            + std::to_string(evidence.projectionItemIndex) + ":edge_fragment_"
            + std::to_string(evidence.edgeFragmentIndex);
        const MapperHistoryRelation relation = evidence.sourceShapeKind == "wire"
            ? MapperHistoryRelation::Split
            : MapperHistoryRelation::Generated;
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
        // ::ProjectOnSurface::projectWire(), uses "BRepProj_Projection aProjection(wire,
        // supportFace, dir)" then pushes each "TopoDS::Edge(xp.Current())"; the source endpoint
        // is the Projection LinkSubList item already resolved by getProjectionShapes().
        MapperHistoryEvent event = projectOnSurfaceMapperHistoryEvent(
            MapperHistoryEndpoint {evidence.sourceObject, evidence.stableSubname},
            MapperHistoryEndpoint {owner, *targetElement},
            targetKind == TopAbs_WIRE ? "wire" : "edge",
            relation,
            evidence.makerStage,
            projectOnSurfaceMapperEvidenceJson(evidence, mapperHistoryId, *targetElement),
            MapperHistoryRecoverability::Resolved,
            "project_on_surface_edge_wire_provenance"
        );
        addMapperHistoryEvent(namedShape.mapperHistory, std::move(event));
    }
    if (!namedShape.mapperHistory.empty()) {
        addDistinctStatus(namedShape.elementHistoryStatus, "part_project_on_surface:edge_wire_mapper_history");
        addDistinctStatus(namedShape.elementHistoryStatus, "reference_recovery_hook:mapper_history_event");
    }
    return namedShape;
}

}  // namespace

void executePartProjectOnSurface(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
    // ::ProjectOnSurface::tryExecute(), calls getSupportFace(), getProjectionShapes(),
    // createProjectedWire(), filterShapes(), createCompound(), then restores Placement.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Mode", "Height", "Offset", "Direction", "SupportFace", "Projection"}
        )) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_project_on_surface"}};
        return;
    }

    const auto mode = readProjectionMode(object, context);
    if (!mode) {
        return;
    }
    const double height = app::readNumber(object, "Height").value_or(0.0);
    const double offset = app::readNumber(object, "Offset").value_or(0.0);

    const auto direction = readProjectionDirection(object, context);
    if (!direction) {
        return;
    }
    const auto support = resolveSingleSubshapeLink(
        object,
        context,
        "SupportFace",
        TopAbs_FACE,
        "face"
    );
    if (!support) {
        return;
    }
    const auto projections = resolveProjectionShapes(object, context);
    if (!projections) {
        return;
    }

    try {
        std::vector<ProjectedShape> projectedShapes;
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
        // ::ProjectOnSurface::tryExecute(), iterates getProjectionShapes() in order, calls
        // createProjectedWire(shape, supportFace, dir), and appends each result before the
        // later filterShapes() and createCompound() passes.
        for (const ProjectSubshape& projection : *projections) {
            const std::vector<ProjectedShape> current =
                createProjectedShapes(projection, TopoDS::Face(support->shape), *direction, *mode);
            projectedShapes.insert(projectedShapes.end(), current.begin(), current.end());
        }
        const std::vector<ProjectedShape> solidsIfHeight =
            createSolidsIfHeight(projectedShapes, *mode, *direction, height);
        const std::vector<ProjectedShape> filteredShapes = filterProjectedShapes(solidsIfHeight, *mode);
        if (filteredShapes.empty()) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "execution_failed",
                "Part::ProjectOnSurface did not produce projected shapes for the requested Mode",
                "Projection",
                projections->front().objectName
            );
            return;
        }

        const gp_Vec offsetVector = offsetVectorForDirection(*direction, offset);
        const TopLoc_Location offsetPlacement = offsetPlacementForVector(offsetVector, offset);
        const TopoDS_Shape projectedCompound =
            compoundOf(filteredShapes, offsetPlacement);
        NamedShape namedShape = namedShapeForProjectOnSurfaceEdgeWireHistory(
            object.name,
            projectedCompound,
            filteredShapes,
            offsetPlacement
        );
        nlohmann::json metadata = {
            {"feature", "part_project_on_surface"},
            {"source_support", support->objectName},
            {"support_face", support->stableSubname},
            {"source_projection", projections->front().objectName},
            {"projection_subshape", projections->front().stableSubname},
            {"projection_items", projectionItemsJson(*projections)},
            {"projection_item_ledger", projectionItemLedgerJson(*projections)},
            {"mode", *mode},
            {"height", height},
            {"offset", offset},
            {"topo_naming_history", "indexed_projected_edges_no_mapper_history"},
            {"projected_edge_wire_history", projectedEdgeWireHistoryJson(namedShape.mapperHistory)},
            {"projected_solid_count", countSubshapes(projectedCompound, TopAbs_SOLID)},
            {"projected_face_count", countSubshapes(projectedCompound, TopAbs_FACE)},
            {"projected_wire_count", countSubshapes(projectedCompound, TopAbs_WIRE)},
            {"projected_inner_wire_count", countInnerWires(projectedCompound)},
        };
        if (offset != 0.0) {
            metadata["offset_application"] = "compound_child_moved_after_filter";
            metadata["offset_vector"] = {offsetVector.X(), offsetVector.Y(), offsetVector.Z()};
        }
        part_feature_detail::publishPartShape(
            object,
            context,
            projectedCompound,
            metadata,
            namedShape
        );
    }
    catch (const Standard_Failure& failure) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "execution_failed",
            failure.GetMessageString(),
            "Projection",
            projections->front().objectName
        );
    }
}

}  // namespace cad_core::part
