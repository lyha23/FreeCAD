#include "assembly_support.h"

#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

namespace cad_core::assembly::assembly_detail {

const app::DocumentObject* documentObjectByName(const runtime::ComputeContext& context,
                                                     const std::string& name)
{
    const auto objectIt = context.documentObjects.find(name);
    if (objectIt == context.documentObjects.end()) {
        return nullptr;
    }
    return objectIt->second;
}

bool isAssemblyJointFeaturePython(const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py
    // ::Joint.__init__() adds "JointType", while ::GroundedJoint.__init__() adds
    // "ObjectToGround"; both are stored as App::FeaturePython objects inside Assembly::JointGroup.
    return object.typeId == "App::FeaturePython"
        && (app::propertyValue(object, "JointType") != nullptr
            || app::propertyValue(object, "ObjectToGround") != nullptr);
}

nlohmann::json linkNamesJson(const std::vector<app::Link>& links)
{
    nlohmann::json names = nlohmann::json::array();
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

nlohmann::json jointReferenceJson(const app::DocumentObject& object, const std::string& property)
{
    const auto link = app::readLink(object, property);
    if (!link) {
        return nullptr;
    }
    return {
        {"object", link->object},
        {"subnames", link->subnames},
    };
}

std::vector<std::string> jointNames(const app::DocumentObject& object,
                                    const runtime::ComputeContext& context)
{
    std::vector<std::string> names;
    for (const auto& link : app::readLinks(object, "Group")) {
        const app::DocumentObject* child = documentObjectByName(context, link.object);
        if (child == nullptr) {
            continue;
        }
        if (isAssemblyJointFeaturePython(*child)) {
            names.push_back(link.object);
            continue;
        }
        if (child->typeId != "Assembly::JointGroup") {
            continue;
        }
        for (const auto& jointLink : app::readLinks(*child, "Group")) {
            const app::DocumentObject* joint = documentObjectByName(context, jointLink.object);
            if (joint != nullptr && isAssemblyJointFeaturePython(*joint)) {
                names.push_back(jointLink.object);
            }
        }
    }
    return names;
}

std::vector<std::string> jointGroupNames(const app::DocumentObject& object,
                                         const runtime::ComputeContext& context)
{
    std::vector<std::string> names;
    for (const auto& link : app::readLinks(object, "Group")) {
        const app::DocumentObject* child = documentObjectByName(context, link.object);
        if (child != nullptr && child->typeId == "Assembly::JointGroup") {
            names.push_back(link.object);
        }
    }
    return names;
}

SolverSummary solverSummary(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::solve(), calls "syncGroundedJoints()", then "fixGroundedParts()";
    // if no part is fixed it returns -6, and full solving proceeds through
    // "mbdAssembly->runPreDrag()". cad-core keeps this as a stateless adapter boundary.
    nlohmann::json groundedJoints = nlohmann::json::array();
    nlohmann::json supportedJoints = nlohmann::json::array();
    nlohmann::json unsupportedJoints = nlohmann::json::array();

    for (const auto& jointName : jointNames(object, context)) {
        const app::DocumentObject* joint = documentObjectByName(context, jointName);
        if (joint == nullptr) {
            continue;
        }
        if (app::propertyValue(*joint, "ObjectToGround") != nullptr) {
            groundedJoints.push_back(jointName);
            continue;
        }
        if (app::propertyValue(*joint, "JointType") != nullptr) {
            supportedJoints.push_back(jointName);
            unsupportedJoints.push_back({
                {"object", jointName},
                {"joint_type", app::readString(*joint, "JointType").value_or("")},
            });
        }
    }

    if (groundedJoints.empty() && supportedJoints.empty()) {
        return {
            "skipped_no_joints",
            {
                {"status", "skipped"},
                {"reason", "no_joints"},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"unsupported_joints", unsupportedJoints},
            },
        };
    }

    if (supportedJoints.empty()) {
        return {
            "solved_noop",
            {
                {"status", "solved"},
                {"mode", "grounded_only_noop"},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"unsupported_joints", unsupportedJoints},
            },
        };
    }

    for (const auto& unsupported : unsupportedJoints) {
        runtime::addDiagnostic(context.diagnostics,
                               "warning",
                               "unsupported_assembly_solver",
                               "Assembly solver adapter does not yet solve JointType "
                                   + unsupported["joint_type"].get<std::string>(),
                               object.name,
                               "Group",
                               "runtime",
                               unsupported["object"].get<std::string>());
    }

    return {
        "unsupported",
        {
            {"status", "unsupported"},
            {"reason", "joint_type_not_migrated"},
            {"grounded_joints", groundedJoints},
            {"joints", supportedJoints},
            {"unsupported_joints", unsupportedJoints},
        },
    };
}

runtime::ShapeValue::Kind shapeKindForShape(const TopoDS_Shape& shape)
{
    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    return solidExplorer.More() ? runtime::ShapeValue::Kind::Solid
                                : runtime::ShapeValue::Kind::PartPrimitive;
}

namespace {

std::string shapeLabelForShape(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        default:
            return "occt_shape";
    }
}

}  // namespace

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.size() == 1U) {
        return shapes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        builder.Add(compound, shape);
    }
    return compound;
}

void publishLinkedShape(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const TopoDS_Shape& shape,
                        runtime::ShapeValue::Kind kind,
                        const nlohmann::json& metadata,
                        std::optional<part::NamedShape> namedShape)
{
    context.shapes[object.name] = runtime::ShapeValue{kind, shape};
    context.mesh[object.name] = cad_core::part::meshForShape(shape);
    context.subshapes[object.name] = part::subshapeMapForShape(shape);
    context.namedShapes[object.name] = namedShape.value_or(part::indexedNamedShapeForObject(object.name, shape));

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabelForShape(shape);
    result["bbox"] = cad_core::part::bboxForShape(shape);
    result["volume"] = cad_core::part::volumeForShape(shape);
    result["kernel"] = cad_core::part::kernelVersion();
    context.objects[object.name] = result;
}

void publishEmptyResult(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const nlohmann::json& metadata)
{
    nlohmann::json result = metadata;
    result["status"] = "ok";
    context.objects[object.name] = result;
}

}  // namespace cad_core::assembly::assembly_detail
