#include "cad_core/geometry/face_maker.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRep_Builder.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <TopAbs_State.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListOfShape.hxx>

#include <algorithm>
#include <map>

namespace cad_core::geometry {

namespace {

struct WireInfo {
    TopoDS_Wire wire;
    double area = 0.0;
    std::size_t depth = 0;
};

std::optional<double> faceAreaForWire(const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(wire);
    if (!faceBuilder.IsDone()) {
        return std::nullopt;
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(faceBuilder.Face(), props);
    return props.Mass();
}

std::optional<gp_Pln> planeForWire(const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(wire);
    if (!faceBuilder.IsDone()) {
        return std::nullopt;
    }
    GeomAdaptor_Surface surface(BRep_Tool::Surface(faceBuilder.Face()));
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    return surface.Plane();
}

std::optional<int> wireDirection(const gp_Pln& plane, const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(plane, wire, Standard_True);
    if (!faceBuilder.IsDone() || faceBuilder.Face().IsNull()) {
        return std::nullopt;
    }

    TopoDS_Iterator it(faceBuilder.Face(), Standard_False);
    if (!it.More()) {
        return std::nullopt;
    }
    return it.Value().Orientation() == wire.Orientation() ? 1 : -1;
}

std::optional<gp_Pnt> samplePoint(const TopoDS_Wire& wire)
{
    for (TopExp_Explorer explorer(wire, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        return BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
    }
    return std::nullopt;
}

bool wireContainsPoint(const gp_Pln& plane, const TopoDS_Wire& wire, const gp_Pnt& point)
{
    BRepBuilderAPI_MakeFace faceBuilder(plane, wire);
    if (!faceBuilder.IsDone()) {
        return false;
    }
    BRepClass_FaceClassifier classifier(faceBuilder.Face(), point, Precision::Confusion());
    return classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON;
}

TopoDS_Wire orientedWire(const gp_Pln& plane, const TopoDS_Wire& wire, bool outer)
{
    TopoDS_Wire result = wire;
    const auto direction = wireDirection(plane, result);
    if (!direction) {
        return result;
    }
    if (outer && *direction < 0) {
        result.Reverse();
    }
    if (!outer && *direction > 0) {
        result.Reverse();
    }
    return result;
}

std::optional<TopoDS_Shape> compoundOrSingleFace(const std::vector<TopoDS_Face>& faces)
{
    if (faces.empty()) {
        return std::nullopt;
    }
    if (faces.size() == 1U) {
        return faces.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Face& face : faces) {
        builder.Add(compound, face);
    }
    return compound;
}

std::vector<TopoDS_Face> facesForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        faces.push_back(TopoDS::Face(explorer.Current()));
    }
    return faces;
}

}  // namespace

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires)
{
    if (wires.empty()) {
        return std::nullopt;
    }

    std::vector<WireInfo> wireInfos;
    wireInfos.reserve(wires.size());
    for (const TopoDS_Wire& wire : wires) {
        const auto area = faceAreaForWire(wire);
        if (!area) {
            return std::nullopt;
        }
        wireInfos.push_back(WireInfo{wire, *area, 0U});
    }
    std::stable_sort(wireInfos.begin(), wireInfos.end(), [](const WireInfo& lhs, const WireInfo& rhs) {
        return lhs.area > rhs.area;
    });

    const auto plane = planeForWire(wireInfos.front().wire);
    if (!plane) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < wireInfos.size(); ++index) {
        const auto point = samplePoint(wireInfos[index].wire);
        if (!point) {
            return std::nullopt;
        }
        for (std::size_t parent = 0; parent < wireInfos.size(); ++parent) {
            if (parent == index || wireInfos[parent].area <= wireInfos[index].area) {
                continue;
            }
            if (wireContainsPoint(*plane, wireInfos[parent].wire, *point)) {
                ++wireInfos[index].depth;
            }
        }
    }

    std::vector<TopoDS_Face> faces;
    for (std::size_t outerIndex = 0; outerIndex < wireInfos.size(); ++outerIndex) {
        if (wireInfos[outerIndex].depth % 2U != 0U) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBullseye.cpp
        // ::Build_Essence(), "Shape in outer wire but not on face, which means it is within a
        // hole. So it's a hit and we shall make a new face with the wire." This keeps islands
        // as separate faces while odd-depth wires are added as holes to their containing face.
        BRepBuilderAPI_MakeFace faceBuilder(*plane, orientedWire(*plane, wireInfos[outerIndex].wire, true));
        if (!faceBuilder.IsDone()) {
            return std::nullopt;
        }

        for (std::size_t holeIndex = 0; holeIndex < wireInfos.size(); ++holeIndex) {
            if (wireInfos[holeIndex].depth != wireInfos[outerIndex].depth + 1U) {
                continue;
            }
            const auto point = samplePoint(wireInfos[holeIndex].wire);
            if (!point || !wireContainsPoint(*plane, wireInfos[outerIndex].wire, *point)) {
                continue;
            }
            faceBuilder.Add(orientedWire(*plane, wireInfos[holeIndex].wire, false));
            if (!faceBuilder.IsDone()) {
                return std::nullopt;
            }
        }

        faces.push_back(faceBuilder.Face());
    }

    return compoundOrSingleFace(faces);
}

std::optional<TopoDS_Shape> makeFacesFromClosedWiresAndSplitEdges(const std::vector<TopoDS_Wire>& wires,
                                                                  const std::vector<TopoDS_Edge>& splitEdges)
{
    const auto base = makeFaceWithHolesFromClosedWires(wires);
    if (!base || base->IsNull()) {
        return std::nullopt;
    }
    if (splitEdges.empty()) {
        return base;
    }

    TopTools_ListOfShape objects;
    objects.Append(*base);
    TopTools_ListOfShape tools;
    for (const TopoDS_Edge& edge : splitEdges) {
        if (!edge.IsNull()) {
            tools.Append(edge);
        }
    }
    if (tools.IsEmpty()) {
        return base;
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(objects);
    splitter.SetTools(tools);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return std::nullopt;
    }

    const std::vector<TopoDS_Face> baseFaces = facesForShape(*base);
    std::vector<TopoDS_Face> splitFaces = facesForShape(splitter.Shape());
    if (splitFaces.size() <= baseFaces.size()) {
        return std::nullopt;
    }
    return compoundOrSingleFace(splitFaces);
}

}  // namespace cad_core::geometry
