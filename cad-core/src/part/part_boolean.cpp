#include "cad_core/part/part_boolean.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_CompSolid.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct BooleanInput
{
    std::string objectName;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

std::optional<BooleanInput> resolveBooleanInput(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (app::propertyValue(object, property) == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " link is not set",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const auto link = app::readLink(object, property);
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " must be an App::PropertyLink",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link->object + " did not produce a shape",
            object.name,
            property,
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link->object);
    return BooleanInput {
        link->object,
        shapeIt->second.shape,
        namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
    };
}

part::NamedShapeSource sourceForInput(const BooleanInput& input)
{
    return part::NamedShapeSource {
        input.namedShape != nullptr ? input.namedShape->owner : input.objectName,
        input.shape,
        input.namedShape
    };
}

std::vector<part::NamedShapeSource> sourcesForInputs(const std::vector<BooleanInput>& inputs)
{
    std::vector<part::NamedShapeSource> sources;
    sources.reserve(inputs.size());
    for (const auto& input : inputs) {
        sources.push_back(sourceForInput(input));
    }
    return sources;
}

std::vector<BooleanInput> expandSingleCompoundInput(const std::vector<BooleanInput>& inputs)
{
    if (inputs.size() != 1U || inputs.front().shape.ShapeType() != TopAbs_COMPOUND) {
        return inputs;
    }

    std::vector<BooleanInput> children;
    int childIndex = 1;
    for (TopoDS_Iterator it(inputs.front().shape); it.More(); it.Next(), ++childIndex) {
        children.push_back(
            BooleanInput {
                inputs.front().objectName + ".Child" + std::to_string(childIndex),
                it.Value(),
                nullptr,
            }
        );
    }
    return children.empty() ? inputs : children;
}

std::string shapeLabel(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SHELL:
            return "occt_shell";
        default:
            return "occt_shape";
    }
}

std::optional<std::vector<BooleanInput>> resolveShapeList(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (app::propertyValue(object, property) == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " links are not set",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, property);
    if (links.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " must contain at least two shape links",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    std::vector<BooleanInput> inputs;
    inputs.reserve(links.size());
    for (const auto& link : links) {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_link_target",
                property + " target " + link.object + " did not produce a shape",
                object.name,
                property,
                "runtime",
                link.object
            );
            return std::nullopt;
        }
        const auto namedShapeIt = context.namedShapes.find(link.object);
        inputs.push_back(
            BooleanInput {
                link.object,
                shapeIt->second.shape,
                namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
            }
        );
    }
    return inputs;
}

void publishBoolean(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape,
    const nlohmann::json& metadata,
    runtime::ShapeValue::Kind kind = runtime::ShapeValue::Kind::Solid
)
{
    context.shapes[object.name] = runtime::ShapeValue {kind, shape};
    context.mesh[object.name] = cad_core::part::meshForShape(shape);
    context.subshapes[object.name] = part::subshapeMapForShape(shape);
    context.namedShapes[object.name] = namedShape
        ? *namedShape
        : part::indexedNamedShapeForObject(object.name, shape);

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabel(shape);
    result["bbox"] = cad_core::part::objectBBoxForShape(shape);
    result["volume"] = cad_core::part::volumeForShape(shape);
    result["kernel"] = cad_core::part::kernelVersion();
    context.objects[object.name] = result;
}

std::optional<runtime::RefineShapeResult> refineBooleanBuild(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const part::NamedShapeBuild& build,
    const std::string& failureMessage
);

void executeBinaryPartBoolean(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    part::BooleanOperation operation,
    const std::string& operationName
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartBoolean.cpp
    // ::Boolean::execute(), reads "Base" and "Tool" with ShapeOption::ResolveLink |
    // ShapeOption::Transform, calls makeOperation(BaseShape, ToolShape), routes the maker through
    // "res.makeElementShape(*mkBool, shapes, opCode())", then applies "res.makeElementRefine()"
    // when Refine is true.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Base", "Tool", "History", "Refine"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto base = resolveBooleanInput(object, context, "Base");
    const auto tool = resolveBooleanInput(object, context, "Tool");
    if (!base || !tool) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        part::NamedShapeBuild build = part::makeElementBooleanFromSources(
            object.name,
            {sourceForInput(*base), sourceForInput(*tool)},
            operation
        );
        if (!build.error.empty() || build.shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                build.error.empty() ? "Boolean operation failed" : build.error,
                object.name,
                {},
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        const auto refined = runtime::applyRefineProperty(object, context, build.shape, build.namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", operationName},
             {"base", base->objectName},
             {"tool", tool->objectName},
             {"refine", app::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Boolean operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executeSection(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartSection.cpp
    // ::Section::makeOperation(), reads inherited Boolean "Base" / "Tool", configures
    // FCBRepAlgoAPI_Section with "Approximation", calls setAutoFuzzy(), Build(), and returns
    // opCode() Part::OpCodes::Section for Part::Boolean::execute() to consume via
    // "res.makeElementShape(*mkBool, shapes, opCode())".
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base", "Tool", "History", "Refine", "Approximation"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto base = resolveBooleanInput(object, context, "Base");
    const auto tool = resolveBooleanInput(object, context, "Tool");
    if (!base || !tool) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const bool approximation = app::readBool(object, "Approximation").value_or(false);
    const auto build = part::makeElementSectionFromSources(
        object.name,
        {sourceForInput(*base), sourceForInput(*tool)},
        approximation
    );
    const auto refined = refineBooleanBuild(object, context, build, "Section failed");
    if (!refined) {
        return;
    }

    publishBoolean(
        object,
        context,
        refined->shape,
        refined->namedShape,
        {{"boolean", "section"},
         {"base", base->objectName},
         {"tool", tool->objectName},
         {"approximation", approximation},
         {"refine", app::readBool(object, "Refine").value_or(false)},
         {"refined", refined->applied}},
        runtime::ShapeValue::Kind::PartPrimitive
    );
}

std::optional<runtime::RefineShapeResult> refineBooleanBuild(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const part::NamedShapeBuild& build,
    const std::string& failureMessage
)
{
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            build.error.empty() ? failureMessage : build.error,
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }
    const auto refined = runtime::applyRefineProperty(object, context, build.shape, build.namedShape);
    if (!refined) {
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }
    return refined;
}

void executeMultiFuse(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartFuse.cpp
    // ::MultiFuse::execute(), reads "Shapes", expands a single compound argument to children,
    // requires at least two shapes, runs FCBRepAlgoAPI_Fuse with first shape as Arguments and
    // remaining shapes as Tools, then calls "makeShapeWithElementMap(mkFuse.Shape(),
    // MapperMaker(mkFuse), shapes, OpCodes::Fuse)".
    if (!runtime::rejectUnsupportedProperties(object, context, {"Shapes", "History", "Refine"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Shapes");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    *inputs = expandSingleCompoundInput(*inputs);
    if (inputs->size() < 2U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Not enough shape objects linked",
            object.name,
            "Shapes",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = part::makeElementBooleanFromSources(
            object.name,
            sourcesForInputs(*inputs),
            part::BooleanOperation::Fuse
        );
        const auto refined = refineBooleanBuild(object, context, build, "MultiFusion failed");
        if (!refined) {
            return;
        }

        nlohmann::json shapes = nlohmann::json::array();
        for (const auto& input : *inputs) {
            shapes.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", "multi_fuse"},
             {"shapes", shapes},
             {"refine", app::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "MultiFusion failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

std::string readMultiCommonBehavior(const app::DocumentObject& object)
{
    if (const auto text = app::readString(object, "Behavior")) {
        return *text == "CommonOfFirstAndRest" ? "CommonOfFirstAndRest" : "CommonOfAllShapes";
    }

    const auto* value = app::propertyValue(object, "Behavior");
    if (value == nullptr) {
        return "CommonOfAllShapes";
    }
    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (payload->is_number_integer() && payload->get<int>() == 1) {
        return "CommonOfFirstAndRest";
    }
    return "CommonOfAllShapes";
}

std::string readBooleanFragmentsMode(const app::DocumentObject& object)
{
    if (const auto text = app::readString(object, "Mode")) {
        return *text;
    }

    const auto* value = app::propertyValue(object, "Mode");
    if (value == nullptr) {
        return "Standard";
    }
    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (!payload->is_number_integer()) {
        return "Standard";
    }
    switch (payload->get<int>()) {
        case 0:
            return "Standard";
        case 1:
            return "Split";
        case 2:
            return "CompSolid";
        default:
            return "Standard";
    }
}

std::string prefixForShapeKind(TopAbs_ShapeEnum kind)
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

std::optional<std::string> elementNameForShape(
    const part::NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    const std::string prefix = prefixForShapeKind(kind);
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

std::vector<std::string> sourceRootsForElement(
    const part::NamedShape& namedShape,
    const std::string& elementName
)
{
    const auto elementIt = namedShape.elements.find(elementName);
    if (elementIt == namedShape.elements.end()) {
        return {};
    }
    std::vector<std::string> roots;
    for (const std::string& source : elementIt->second.sources) {
        const std::size_t dot = source.find('.');
        const std::string root = dot == std::string::npos ? source : source.substr(0, dot);
        if (root.empty() || std::find(roots.begin(), roots.end(), root) != roots.end()) {
            continue;
        }
        roots.push_back(root);
    }
    return roots;
}

std::size_t sourceRootCount(
    const part::NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    const auto elementName = elementNameForShape(namedShape, shape, kind);
    if (!elementName) {
        return 1U;
    }
    const std::vector<std::string> roots = sourceRootsForElement(namedShape, *elementName);
    return roots.empty() ? 1U : roots.size();
}

std::vector<std::string> sourceRootsForShape(
    const part::NamedShape& namedShape,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum kind
)
{
    std::vector<std::string> roots;
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shape, kind, subshapes);
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        const auto elementName = elementNameForShape(namedShape, subshapes(index), kind);
        if (!elementName) {
            continue;
        }
        for (const std::string& root : sourceRootsForElement(namedShape, *elementName)) {
            if (std::find(roots.begin(), roots.end(), root) == roots.end()) {
                roots.push_back(root);
            }
        }
    }
    return roots;
}

std::size_t sourceRootCountForSolid(const part::NamedShape& namedShape, const TopoDS_Shape& solid)
{
    const auto roots = sourceRootsForShape(namedShape, solid, TopAbs_FACE);
    return roots.empty() ? 1U : roots.size();
}

bool edgeTouchesVertex(const TopoDS_Shape& edge, const TopoDS_Shape& vertex)
{
    TopTools_IndexedMapOfShape vertices;
    TopExp::MapShapes(edge, TopAbs_VERTEX, vertices);
    for (int index = 1; index <= vertices.Extent(); ++index) {
        if (vertices(index).IsSame(vertex)) {
            return true;
        }
    }
    return false;
}

bool solidTouchesFace(const TopoDS_Shape& solid, const TopoDS_Shape& face)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(solid, TopAbs_FACE, faces);
    for (int index = 1; index <= faces.Extent(); ++index) {
        if (faces(index).IsSame(face)) {
            return true;
        }
    }
    return false;
}

bool faceTouchesEdge(const TopoDS_Shape& face, const TopoDS_Shape& edge)
{
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(face, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        if (edges(index).IsSame(edge)) {
            return true;
        }
    }
    return false;
}

std::vector<TopoDS_Shape> edgeChildren(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> edges;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(explorer.Current());
    }
    return edges;
}

std::vector<TopoDS_Shape> vertexChildren(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> vertices;
    TopTools_IndexedMapOfShape vertexMap;
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertexMap);
    vertices.reserve(static_cast<std::size_t>(vertexMap.Extent()));
    for (int index = 1; index <= vertexMap.Extent(); ++index) {
        vertices.push_back(vertexMap(index));
    }
    return vertices;
}

std::vector<TopoDS_Shape> solidChildren(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> solids;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        solids.push_back(explorer.Current());
    }
    return solids;
}

std::vector<TopoDS_Shape> faceChildren(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> faces;
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    faces.reserve(static_cast<std::size_t>(faceMap.Extent()));
    for (int index = 1; index <= faceMap.Extent(); ++index) {
        faces.push_back(faceMap(index));
    }
    return faces;
}

bool isSplitVertex(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& vertex,
    const std::vector<TopoDS_Shape>& allEdges
)
{
    const std::size_t jointOverlapCount = sourceRootCount(generalFuseNamedShape, vertex, TopAbs_VERTEX);
    if (jointOverlapCount <= 1U) {
        return false;
    }
    for (const auto& edge : allEdges) {
        if (!edgeTouchesVertex(edge, vertex)) {
            continue;
        }
        const std::size_t edgeOverlapCount = sourceRootCount(generalFuseNamedShape, edge, TopAbs_EDGE);
        if (edgeOverlapCount < jointOverlapCount) {
            return true;
        }
    }
    return false;
}

bool isSplitFace(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& face,
    const std::vector<TopoDS_Shape>& allSolids
)
{
    const std::size_t jointOverlapCount = sourceRootCount(generalFuseNamedShape, face, TopAbs_FACE);
    if (jointOverlapCount <= 1U) {
        return false;
    }
    for (const auto& solid : allSolids) {
        if (!solidTouchesFace(solid, face)) {
            continue;
        }
        const std::size_t solidOverlapCount = sourceRootCountForSolid(generalFuseNamedShape, solid);
        if (solidOverlapCount < jointOverlapCount) {
            return true;
        }
    }
    return false;
}

bool isSplitEdge(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& edge,
    const std::vector<TopoDS_Shape>& allFaces
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/GeneralFuseResult.py
    // ::makeSplitPieces(), for "Shell" uses bit_extractor=lambda sh: sh.Faces and
    // joint_extractor=lambda sh: sh.Edges. An edge becomes split_connections when its
    // source overlap count is higher than at least one connected face's overlap count.
    const std::size_t jointOverlapCount = sourceRootCount(generalFuseNamedShape, edge, TopAbs_EDGE);
    if (jointOverlapCount <= 1U) {
        return false;
    }
    for (const auto& face : allFaces) {
        if (!faceTouchesEdge(face, edge)) {
            continue;
        }
        const std::size_t faceOverlapCount = sourceRootCount(generalFuseNamedShape, face, TopAbs_FACE);
        if (faceOverlapCount < jointOverlapCount) {
            return true;
        }
    }
    return false;
}

bool edgeTouchesAnyVertex(const TopoDS_Shape& edge, const std::vector<TopoDS_Shape>& vertices)
{
    for (const auto& vertex : vertices) {
        if (edgeTouchesVertex(edge, vertex)) {
            return true;
        }
    }
    return false;
}

bool edgeIsIn(const TopoDS_Shape& edge, const std::vector<TopoDS_Shape>& edges)
{
    for (const auto& item : edges) {
        if (item.IsSame(edge)) {
            return true;
        }
    }
    return false;
}

bool faceIsIn(const TopoDS_Shape& face, const std::vector<TopoDS_Shape>& faces)
{
    for (const auto& item : faces) {
        if (item.IsSame(face)) {
            return true;
        }
    }
    return false;
}

bool facesConnectedWithoutSplitEdge(
    const TopoDS_Shape& left,
    const TopoDS_Shape& right,
    const std::vector<TopoDS_Shape>& splitEdges
)
{
    for (const auto& edge : edgeChildren(left)) {
        if (edgeIsIn(edge, splitEdges)) {
            continue;
        }
        if (faceTouchesEdge(right, edge)) {
            return true;
        }
    }
    return false;
}

bool edgesConnectedWithoutSplitVertex(
    const TopoDS_Shape& left,
    const TopoDS_Shape& right,
    const std::vector<TopoDS_Shape>& splitVertices
)
{
    for (const auto& vertex : vertexChildren(left)) {
        if (edgeTouchesAnyVertex(vertex, splitVertices)) {
            continue;
        }
        if (edgeTouchesVertex(right, vertex)) {
            return true;
        }
    }
    return false;
}

bool solidsConnectedWithoutSplitFace(
    const std::vector<TopoDS_Shape>& leftFaces,
    const std::vector<TopoDS_Shape>& rightFaces,
    const std::vector<TopoDS_Shape>& splitFaces
)
{
    for (const auto& leftFace : leftFaces) {
        if (faceIsIn(leftFace, splitFaces)) {
            continue;
        }
        for (const auto& rightFace : rightFaces) {
            if (leftFace.IsSame(rightFace)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::vector<std::size_t>> edgeGroupsBySharedVertices(
    const std::vector<TopoDS_Shape>& edges,
    const std::vector<TopoDS_Shape>& splitVertices
)
{
    std::vector<std::vector<std::size_t>> groups;
    std::vector<bool> visited(edges.size(), false);
    for (std::size_t start = 0; start < edges.size(); ++start) {
        if (visited[start]) {
            continue;
        }
        std::vector<std::size_t> group;
        std::vector<std::size_t> stack {start};
        visited[start] = true;
        while (!stack.empty()) {
            const std::size_t current = stack.back();
            stack.pop_back();
            group.push_back(current);
            for (std::size_t candidate = 0; candidate < edges.size(); ++candidate) {
                if (
                    visited[candidate]
                    || !edgesConnectedWithoutSplitVertex(edges[current], edges[candidate], splitVertices)
                ) {
                    continue;
                }
                visited[candidate] = true;
                stack.push_back(candidate);
            }
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<std::vector<std::size_t>> faceGroupsBySharedEdges(
    const std::vector<TopoDS_Shape>& faces,
    const std::vector<TopoDS_Shape>& splitEdges
)
{
    std::vector<std::vector<std::size_t>> groups;
    std::vector<bool> visited(faces.size(), false);
    for (std::size_t start = 0; start < faces.size(); ++start) {
        if (visited[start]) {
            continue;
        }
        std::vector<std::size_t> group;
        std::vector<std::size_t> stack {start};
        visited[start] = true;
        while (!stack.empty()) {
            const std::size_t current = stack.back();
            stack.pop_back();
            group.push_back(current);
            for (std::size_t candidate = 0; candidate < faces.size(); ++candidate) {
                if (visited[candidate]
                    || !facesConnectedWithoutSplitEdge(faces[current], faces[candidate], splitEdges)) {
                    continue;
                }
                visited[candidate] = true;
                stack.push_back(candidate);
            }
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<std::vector<std::size_t>> solidGroupsBySharedFaces(
    const std::vector<TopoDS_Shape>& solids,
    const std::vector<TopoDS_Shape>& splitFaces = {}
)
{
    std::vector<std::vector<TopoDS_Shape>> facesBySolid;
    facesBySolid.reserve(solids.size());
    for (const auto& solid : solids) {
        facesBySolid.push_back(faceChildren(solid));
    }

    std::vector<std::vector<std::size_t>> groups;
    std::vector<bool> visited(solids.size(), false);
    for (std::size_t start = 0; start < solids.size(); ++start) {
        if (visited[start]) {
            continue;
        }
        std::vector<std::size_t> group;
        std::vector<std::size_t> stack {start};
        visited[start] = true;
        while (!stack.empty()) {
            const std::size_t current = stack.back();
            stack.pop_back();
            group.push_back(current);
            for (std::size_t candidate = 0; candidate < solids.size(); ++candidate) {
                if (visited[candidate]
                    || !solidsConnectedWithoutSplitFace(
                        facesBySolid[current],
                        facesBySolid[candidate],
                        splitFaces
                    )) {
                    continue;
                }
                visited[candidate] = true;
                stack.push_back(candidate);
            }
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

std::optional<TopoDS_Wire> makeWireFromEdges(
    const std::vector<TopoDS_Shape>& edges,
    const std::vector<std::size_t>& group
)
{
    BRepBuilderAPI_MakeWire maker;
    for (const std::size_t edgeIndex : group) {
        maker.Add(TopoDS::Edge(edges.at(edgeIndex)));
    }
    if (!maker.IsDone()) {
        return std::nullopt;
    }
    return maker.Wire();
}

TopoDS_CompSolid makeCompSolidFromSolids(
    const std::vector<TopoDS_Shape>& solids,
    const std::vector<std::size_t>& group
)
{
    BRep_Builder builder;
    TopoDS_CompSolid compSolid;
    builder.MakeCompSolid(compSolid);
    for (const std::size_t solidIndex : group) {
        builder.Add(compSolid, solids.at(solidIndex));
    }
    return compSolid;
}

TopoDS_Shell makeShellFromFaces(
    const std::vector<TopoDS_Shape>& faces,
    const std::vector<std::size_t>& group
)
{
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    for (const std::size_t faceIndex : group) {
        builder.Add(shell, faces.at(faceIndex));
    }
    return shell;
}

std::vector<TopoDS_Shape> splitWirePiece(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& wire,
    const std::vector<TopoDS_Shape>& allEdges
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/GeneralFuseResult.py
    // ::makeSplitPieces(), for "Wire" uses bit_extractor=lambda sh: sh.Edges and
    // joint_extractor=lambda sh: sh.Vertexes. Joints whose overlap count is higher than the
    // connected edge overlap count become split_connections, then ShapeMerge.mergeShapes()
    // rebuilds wires without those connection vertices.
    std::vector<TopoDS_Shape> splitVertices;
    for (const auto& vertex : vertexChildren(wire)) {
        if (isSplitVertex(generalFuseNamedShape, vertex, allEdges)) {
            splitVertices.push_back(vertex);
        }
    }
    if (splitVertices.empty()) {
        return {wire};
    }

    const std::vector<TopoDS_Shape> edges = edgeChildren(wire);
    const std::vector<std::vector<std::size_t>> groups
        = edgeGroupsBySharedVertices(edges, splitVertices);
    if (groups.size() <= 1U) {
        return {wire};
    }

    std::vector<TopoDS_Shape> pieces;
    pieces.reserve(groups.size());
    for (const auto& group : groups) {
        const auto rebuilt = makeWireFromEdges(edges, group);
        if (!rebuilt) {
            return {wire};
        }
        pieces.push_back(*rebuilt);
    }
    return pieces;
}

std::vector<TopoDS_Shape> splitShellPiece(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& shell,
    const std::vector<TopoDS_Shape>& allFaces
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/GeneralFuseResult.py
    // ::makeSplitPieces(), for "Shell" uses bit_extractor=lambda sh: sh.Faces and
    // joint_extractor=lambda sh: sh.Edges; ShapeMerge.mergeShapes() dispatches to
    // ShapeMerge.mergeShells(), grouping faces by shared edges while excluding split_edges.
    std::vector<TopoDS_Shape> splitEdges;
    for (const auto& edge : edgeChildren(shell)) {
        if (isSplitEdge(generalFuseNamedShape, edge, allFaces)) {
            splitEdges.push_back(edge);
        }
    }
    if (splitEdges.empty()) {
        return {shell};
    }

    const std::vector<TopoDS_Shape> faces = faceChildren(shell);
    const std::vector<std::vector<std::size_t>> groups = faceGroupsBySharedEdges(faces, splitEdges);
    if (groups.size() <= 1U) {
        return {shell};
    }

    std::vector<TopoDS_Shape> pieces;
    pieces.reserve(groups.size());
    for (const auto& group : groups) {
        pieces.push_back(makeShellFromFaces(faces, group));
    }
    return pieces;
}

std::vector<TopoDS_Shape> splitCompSolidPiece(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& compSolid,
    const std::vector<TopoDS_Shape>& allSolids
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/GeneralFuseResult.py
    // ::makeSplitPieces(), for "CompSolid" uses bit_extractor=lambda sh: sh.Solids and
    // joint_extractor=lambda sh: sh.Faces, then ShapeMerge.mergeShapes(...,
    // bool_compsolid=True) rebuilds compsolids without split face connections.
    std::vector<TopoDS_Shape> splitFaces;
    for (const auto& face : faceChildren(compSolid)) {
        if (isSplitFace(generalFuseNamedShape, face, allSolids)) {
            splitFaces.push_back(face);
        }
    }
    if (splitFaces.empty()) {
        return {compSolid};
    }

    const std::vector<TopoDS_Shape> solids = solidChildren(compSolid);
    const std::vector<std::vector<std::size_t>> groups = solidGroupsBySharedFaces(solids, splitFaces);
    if (groups.size() <= 1U) {
        return {compSolid};
    }

    std::vector<TopoDS_Shape> pieces;
    pieces.reserve(groups.size());
    for (const auto& group : groups) {
        pieces.push_back(makeCompSolidFromSolids(solids, group));
    }
    return pieces;
}

struct SplitFragmentsResult
{
    TopoDS_Shape shape;
    bool changed = false;
    std::string error;
};

SplitFragmentsResult splitAggregatePieces(
    const part::NamedShape& generalFuseNamedShape,
    const TopoDS_Shape& shape,
    const std::vector<TopoDS_Shape>& allEdges,
    const std::vector<TopoDS_Shape>& allFaces
)
{
    if (shape.ShapeType() == TopAbs_WIRE) {
        const auto pieces = splitWirePiece(generalFuseNamedShape, shape, allEdges);
        if (pieces.size() == 1U && pieces.front().IsSame(shape)) {
            return SplitFragmentsResult {shape, false, {}};
        }
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& piece : pieces) {
            builder.Add(compound, piece);
        }
        return SplitFragmentsResult {compound, true, {}};
    }
    if (shape.ShapeType() == TopAbs_SHELL) {
        const auto pieces = splitShellPiece(generalFuseNamedShape, shape, allFaces);
        if (pieces.size() == 1U && pieces.front().IsSame(shape)) {
            return SplitFragmentsResult {shape, false, {}};
        }
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& piece : pieces) {
            builder.Add(compound, piece);
        }
        return SplitFragmentsResult {compound, true, {}};
    }
    if (shape.ShapeType() == TopAbs_COMPSOLID) {
        const auto pieces = splitCompSolidPiece(
            generalFuseNamedShape,
            shape,
            solidChildren(generalFuseNamedShape.shape)
        );
        if (pieces.size() == 1U && pieces.front().IsSame(shape)) {
            return SplitFragmentsResult {shape, false, {}};
        }
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& piece : pieces) {
            builder.Add(compound, piece);
        }
        return SplitFragmentsResult {compound, true, {}};
    }
    if (shape.ShapeType() == TopAbs_COMPOUND) {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        bool changed = false;
        for (TopoDS_Iterator it(shape); it.More(); it.Next()) {
            const auto child
                = splitAggregatePieces(generalFuseNamedShape, it.Value(), allEdges, allFaces);
            if (!child.error.empty()) {
                return child;
            }
            changed = changed || child.changed;
            builder.Add(compound, child.shape);
        }
        return SplitFragmentsResult {changed ? TopoDS_Shape(compound) : shape, changed, {}};
    }
    return SplitFragmentsResult {shape, false, {}};
}

part::NamedShapeBuild makeSplitFragmentsBuild(
    const std::string& owner,
    const part::NamedShapeBuild& generalFuseBuild
)
{
    if (!generalFuseBuild.namedShape) {
        return generalFuseBuild;
    }
    const auto split = splitAggregatePieces(
        *generalFuseBuild.namedShape,
        generalFuseBuild.shape,
        edgeChildren(generalFuseBuild.shape),
        faceChildren(generalFuseBuild.shape)
    );
    if (!split.error.empty()) {
        return part::NamedShapeBuild {TopoDS_Shape {}, std::nullopt, split.error};
    }
    if (!split.changed) {
        return generalFuseBuild;
    }

    const part::NamedShapeSource source {
        owner + ".GeneralFuse",
        generalFuseBuild.shape,
        generalFuseBuild.namedShape ? &*generalFuseBuild.namedShape : nullptr,
    };
    return part::NamedShapeBuild {
        split.shape,
        part::namedShapeForPreservedSources(
            owner,
            split.shape,
            std::vector<part::NamedShapeSource> {source}
        ),
        {},
    };
}

part::NamedShapeBuild makeCompSolidFragmentsBuild(
    const std::string& owner,
    const part::NamedShapeBuild& generalFuseBuild
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/SplitAPI.py
    // ::booleanFragments(), Mode "CompSolid" extracts "pieces.Solids" then calls
    // ShapeMerge.mergeSolids(solids, bool_compsolid=True). ShapeMerge.mergeSolids() groups
    // solids by shared faces and returns Part.makeCompound([Part.CompSolid(group), ...]).
    const std::vector<TopoDS_Shape> solids = solidChildren(generalFuseBuild.shape);
    if (solids.empty()) {
        return part::NamedShapeBuild {
            TopoDS_Shape {},
            std::nullopt,
            "No solids in the result. Can't make CompSolid."
        };
    }

    BRep_Builder builder;
    TopoDS_Compound result;
    builder.MakeCompound(result);
    for (const auto& group : solidGroupsBySharedFaces(solids)) {
        builder.Add(result, makeCompSolidFromSolids(solids, group));
    }

    const part::NamedShapeSource source {
        owner + ".GeneralFuse",
        generalFuseBuild.shape,
        generalFuseBuild.namedShape ? &*generalFuseBuild.namedShape : nullptr,
    };
    return part::NamedShapeBuild {
        result,
        part::namedShapeForPreservedSources(owner, result, std::vector<part::NamedShapeSource> {source}),
        {},
    };
}

part::NamedShapeBuild makeCommonOfAllShapes(
    const std::string& owner,
    const std::vector<BooleanInput>& inputs
)
{
    if (inputs.empty()) {
        return part::NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Input shape is null"};
    }
    if (inputs.size() == 1U) {
        const auto source = sourceForInput(inputs.front());
        return part::NamedShapeBuild {
            inputs.front().shape,
            source.namedShape != nullptr
                ? std::optional<part::NamedShape>(*source.namedShape)
                : std::optional<part::NamedShape>(
                      part::indexedNamedShapeForObject(owner, inputs.front().shape)
                  ),
            {},
        };
    }

    TopoDS_Shape currentShape = inputs.front().shape;
    std::optional<part::NamedShape> currentNamedShape;
    if (inputs.front().namedShape != nullptr) {
        currentNamedShape = *inputs.front().namedShape;
    }
    std::string currentOwner = inputs.front().objectName;

    for (std::size_t index = 1; index < inputs.size(); ++index) {
        const auto currentSource = part::NamedShapeSource {
            currentNamedShape ? currentNamedShape->owner : currentOwner,
            currentShape,
            currentNamedShape ? &*currentNamedShape : nullptr,
        };
        const auto step = part::makeElementBooleanFromSources(
            owner,
            {currentSource, sourceForInput(inputs.at(index))},
            part::BooleanOperation::Common
        );
        if (!step.error.empty() || step.shape.IsNull()) {
            return step;
        }
        currentShape = step.shape;
        currentNamedShape = step.namedShape;
        currentOwner = owner + ".CommonStep" + std::to_string(index);
    }

    return part::NamedShapeBuild {currentShape, std::move(currentNamedShape), {}};
}

void executeMultiCommon(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCommon.cpp
    // ::MultiCommon::execute(), reads "Shapes"; default Behavior is "CommonOfAllShapes",
    // which intersects one shape at a time, while "CommonOfFirstAndRest" calls one
    // makeElementBoolean(OpCodes::Common, shapes) for FreeCAD 1.0 compatibility.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Shapes", "History", "Refine", "Behavior"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Shapes");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::string behavior = readMultiCommonBehavior(object);
    if (behavior == "CommonOfAllShapes") {
        *inputs = expandSingleCompoundInput(*inputs);
    }
    if (inputs->empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Input shape is null",
            object.name,
            "Shapes",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = behavior == "CommonOfFirstAndRest"
            ? part::makeElementBooleanFromSources(
                  object.name,
                  sourcesForInputs(*inputs),
                  part::BooleanOperation::Common
              )
            : makeCommonOfAllShapes(object.name, *inputs);
        const auto refined = refineBooleanBuild(object, context, build, "Resulting shape is null");
        if (!refined) {
            return;
        }

        nlohmann::json shapes = nlohmann::json::array();
        for (const auto& input : *inputs) {
            shapes.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", "multi_common"},
             {"behavior", behavior},
             {"shapes", shapes},
             {"refine", app::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Common operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executeBooleanFragments(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/SplitFeatures.py
    // ::FeatureBooleanFragments.execute(), reads "Objects", expands one Compound via
    // childShapes(), requires at least two shapes, then calls
    // SplitAPI.booleanFragments(shapes, Mode, Tolerance). Standard mode returns "pieces"
    // from "list_of_shapes[0].generalFuse(...)", while Split mode calls
    // GeneralFuseResult.splitAggregates() before Part.makeCompound(gr.pieces).
    // cad-core exposes Part::BooleanFragments / Part::FeatureBooleanFragments as typed aliases
    // until Python proxy dispatch is migrated into document/runtime.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Objects", "Mode", "Tolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::string mode = readBooleanFragmentsMode(object);
    if (mode != "Standard" && mode != "Split" && mode != "CompSolid") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "BooleanFragments mode " + mode + " is not supported yet",
            object.name,
            "Mode",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double tolerance = app::readNumber(object, "Tolerance").value_or(0.0);
    if (tolerance < 0.0) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "BooleanFragments negative auto-fuzzy tolerance is not supported yet",
            object.name,
            "Tolerance",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Objects");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    *inputs = expandSingleCompoundInput(*inputs);
    if (inputs->size() < 2U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "At least two shapes are needed for computing boolean fragments",
            object.name,
            "Objects",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        part::NamedShapeBuild build = part::makeElementGeneralFuseFromSources(
            object.name,
            sourcesForInputs(*inputs),
            tolerance
        );
        if (mode == "CompSolid" && build.error.empty() && !build.shape.IsNull()) {
            build = makeCompSolidFragmentsBuild(object.name, build);
        }
        if (mode == "Split" && build.error.empty() && !build.shape.IsNull()) {
            build = makeSplitFragmentsBuild(object.name, build);
        }
        const auto result = refineBooleanBuild(object, context, build, "GeneralFuse failed");
        if (!result) {
            return;
        }

        nlohmann::json objects = nlohmann::json::array();
        for (const auto& input : *inputs) {
            objects.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            result->shape,
            result->namedShape,
            {{"boolean", "fragments"}, {"objects", objects}, {"mode", mode}, {"tolerance", tolerance}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "GeneralFuse failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executeXor(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/SplitFeatures.py
    // ::FeatureXOR.execute(), reads "Objects", expands a single compound via childShapes(),
    // requires at least two shapes, then calls SplitAPI.xor(shapes, Tolerance). SplitAPI.xor()
    // keeps pieces whose source count is odd; the C++ topo equivalent is
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementXor(), which performs Fuse, Common, then Cut.
    //
    // FreeCAD stores this command as Part::FeaturePython with Proxy.Type "FeatureXOR".
    // cad-core exposes Part::XOR / Part::FeatureXOR as typed aliases until Python proxy
    // dispatch is migrated into document/runtime.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Objects", "Tolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double tolerance = app::readNumber(object, "Tolerance").value_or(0.0);
    if (tolerance != 0.0) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Part::XOR Tolerance is not supported yet",
            object.name,
            "Tolerance",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Objects");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    *inputs = expandSingleCompoundInput(*inputs);
    if (inputs->size() < 2U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "At least two shapes are needed for computing XOR",
            object.name,
            "Objects",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = part::makeElementXorFromSources(object.name, sourcesForInputs(*inputs));
        const auto result = refineBooleanBuild(object, context, build, "XOR operation failed");
        if (!result) {
            return;
        }

        nlohmann::json objects = nlohmann::json::array();
        for (const auto& input : *inputs) {
            objects.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            result->shape,
            result->namedShape,
            {{"boolean", "xor"}, {"objects", objects}, {"tolerance", tolerance}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "XOR operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace

void executePartFuse(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartFuse.cpp
    // ::Fuse::makeOperation(), returns "new FCBRepAlgoAPI_Fuse(base, tool)" and opCode() is
    // Part::OpCodes::Fuse.
    executeBinaryPartBoolean(object, context, part::BooleanOperation::Fuse, "fuse");
}

void executePartCut(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCut.cpp
    // ::Cut::makeOperation(), returns "new FCBRepAlgoAPI_Cut(base, tool)" and opCode() is
    // Part::OpCodes::Cut.
    executeBinaryPartBoolean(object, context, part::BooleanOperation::Cut, "cut");
}

void executePartCommon(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCommon.cpp
    // ::Common::makeOperation(), returns "new FCBRepAlgoAPI_Common(base, tool)" and opCode() is
    // Part::OpCodes::Common.
    executeBinaryPartBoolean(object, context, part::BooleanOperation::Common, "common");
}

void executePartSection(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeSection(object, context);
}

void executePartMultiFuse(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeMultiFuse(object, context);
}

void executePartMultiCommon(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeMultiCommon(object, context);
}

void executePartXor(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeXor(object, context);
}

void executePartBooleanFragments(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeBooleanFragments(object, context);
}

}  // namespace cad_core::part
