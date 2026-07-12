#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design::detail
{

struct DressUpBase
{
    app::Link link;
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};

struct EdgeSelection
{
    TopoDS_Edge edge;
    std::string sourceSubname;
    std::string edgeSubname;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
// ::DressUp::getContinuousEdges(), consumes Base subnames and expands Face/Wire selections.
struct DressUpSelectionEvidence
{
    bool useAllEdges = false;
    std::vector<std::string> requestedSubnames;
    std::vector<std::string> selectedEdgeSubnames;
    std::vector<std::string> selectedEdgeSources;
    int requestedEdgeCount = 0;
    int requestedFaceCount = 0;
    int requestedWireCount = 0;
};

struct DressUpEdgeSelection
{
    std::vector<EdgeSelection> edges;
    DressUpSelectionEvidence evidence;
};

struct DraftFaceSelection
{
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
    DressUpSelectionEvidence evidence;
};

struct ThicknessFaceSelection
{
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
    DressUpSelectionEvidence evidence;
};

struct ThicknessSolidFaces
{
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
};

struct ThicknessSolidBuild
{
    int solidIndex = 0;
    TopoDS_Shape shape;
    part::NamedShape namedShape;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp
// ::Draft::execute(), guesses NeutralPlane from the first selected face when no link is set.
struct DraftNeutralPlane
{
    gp_Pln plane;
    std::string source;
};

struct DressUpResult
{
    std::string mode;
    std::string sourceBase;
    DressUpBase base;
    TopoDS_Shape shape;
    part::NamedShape namedShape;
    bool supportTransform = false;
    std::string supportTransformSource;
    std::string addSubCacheStatus = "empty";
    std::string addSubCacheWarning;
    bool refineApplied = false;
    DressUpSelectionEvidence selection;
    nlohmann::json parameters = nlohmann::json::object();
};

struct ShapeSlotBuild
{
    bool ok = true;
    std::optional<TopoDS_Shape> shape;
    std::optional<part::NamedShape> namedShape;
};

enum class AddSubKind
{
    Additive,
    Subtractive,
    Unknown,
};

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property);
std::string readEnumProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::vector<std::string>& values,
    const std::string& fallback
);
bool readBoolProperty(
    const app::DocumentObject& object,
    const std::string& property,
    bool fallback = false
);
double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback);
bool hasSolid(const TopoDS_Shape& shape);
std::vector<TopoDS_Shape> solidSubshapes(const TopoDS_Shape& shape);
std::optional<DressUpBase> resolveDressUpBase(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<TopoDS_Shape> resolveReferenceSubshape(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    TopAbs_ShapeEnum expectedKind
);
std::optional<DressUpEdgeSelection> selectedDressUpEdges(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<DraftFaceSelection> selectedDraftFaces(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<ThicknessFaceSelection> selectedThicknessFaces(
    const DressUpBase& base,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<short> thicknessModeIndex(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<short> thicknessJoinIndex(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::string thicknessModeName(short mode);
std::string thicknessJoinName(short join);
std::optional<std::map<int, ThicknessSolidFaces>> thicknessFacesBySolid(
    const TopoDS_Shape& baseShape,
    const std::vector<TopoDS_Shape>& solids,
    const ThicknessFaceSelection& selection,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
bool cacheDressUpAddSubShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    DressUpResult& result
);
bool applyDressUpRefine(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    DressUpResult& result
);
nlohmann::json selectionEvidenceJson(const DressUpSelectionEvidence& evidence);
void publishDressUpResult(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const DressUpResult& result
);

}  // namespace cad_core::part_design::detail
