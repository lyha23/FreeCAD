#include "assembly_support.h"

#include "cad_core/assembly/joint_solver.h"
#include "cad_core/app/property_geo.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <cmath>
#include <map>
#include <optional>

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

namespace {

nlohmann::json placementJson(const app::Placement& placement)
{
    return {
        {"PropertyType", "App::PropertyPlacement"},
        {"Base", {placement.base.at(0), placement.base.at(1), placement.base.at(2)}},
        {"Rotation",
         {placement.rotation.at(0),
          placement.rotation.at(1),
          placement.rotation.at(2),
          placement.rotation.at(3)}},
    };
}

nlohmann::json jointReferenceJson(const AssemblyJointReference& reference)
{
    if (reference.object.empty()) {
        return nullptr;
    }
    return {
        {"object", reference.object},
        {"subnames", reference.subnames},
    };
}

nlohmann::json solverJointJson(const JointConstraint& joint)
{
    nlohmann::json solverJoint = {
        {"object", joint.object},
        {"joint_type", joint.jointType},
        {"reference1", jointReferenceJson(joint.reference1)},
        {"reference2", jointReferenceJson(joint.reference2)},
        {"suppressed", joint.suppressed},
    };
    if (joint.distance) {
        solverJoint["distance"] = *joint.distance;
    }
    if (joint.distance2) {
        solverJoint["distance2"] = *joint.distance2;
    }
    if (joint.jointType == "Gears" || joint.jointType == "Belt") {
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        // ::AssemblyObject::makeMbdJointOfType(), Gears maps "radiusJ = getJointDistance2(joint)"
        // and Belt maps "radiusJ = -getJointDistance2(joint)" for ASMTGearJoint.
        const double distance = joint.distance.value_or(0.0);
        const double distance2 = joint.distance2.value_or(0.0);
        solverJoint["radius_i"] = distance;
        solverJoint["radius_j"] = joint.jointType == "Belt" ? -distance2 : distance2;
    }
    if (joint.angle) {
        solverJoint["angle"] = *joint.angle;
    }
    if (joint.slidingPartIndex) {
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        // ::AssemblyObject::slidingPartIndex() returns "slidingFound" 1/2/0; DTO JSON exposes the
        // request-local value used before any future Screw/RackPinion Ondsel conversion.
        solverJoint["sliding_part_index"] = *joint.slidingPartIndex;
        solverJoint["jcs_swapped_for_solver"] = joint.jcsSwappedForSolver;
    }
    return solverJoint;
}

nlohmann::json solverJointsJson(const std::vector<JointConstraint>& joints)
{
    nlohmann::json result = nlohmann::json::array();
    for (const JointConstraint& joint : joints) {
        result.push_back(solverJointJson(joint));
    }
    return result;
}

nlohmann::json unsupportedJointsJson(const std::vector<UnsupportedAssemblyJoint>& unsupportedJoints)
{
    nlohmann::json result = nlohmann::json::array();
    for (const UnsupportedAssemblyJoint& unsupported : unsupportedJoints) {
        result.push_back({
            {"object", unsupported.object},
            {"joint_type", unsupported.jointType},
        });
    }
    return result;
}

nlohmann::json placementUpdateJson(const AssemblyPlacementUpdate& update,
                                   const std::string& assemblyObject)
{
    return {
        {"action", "assembly_set_placement"},
        {"reason", "assembly_solver_placement_writeback"},
        {"object", update.object},
        {"objectId", update.objectId},
        {"typeId", update.typeId},
        {"assembly", assemblyObject},
        {"joint", update.joint},
        {"joint_type", update.jointType},
        {"properties", {{"Placement", placementJson(update.placement)}}},
    };
}

nlohmann::json placementUpdatesJson(const std::vector<AssemblyPlacementUpdate>& updates,
                                    const std::string& assemblyObject)
{
    nlohmann::json result = nlohmann::json::array();
    for (const AssemblyPlacementUpdate& update : updates) {
        result.push_back(placementUpdateJson(update, assemblyObject));
    }
    return result;
}

}  // namespace

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
    // ::AssemblyObject::solve(), calls "syncGroundedJoints()", "fixGroundedParts()",
    // builds joints with "makeMbdJointOfType()", then runs "mbdAssembly->runPreDrag()";
    // ::setNewPlacements() writes "propPlacement->setValue(newPlacement)". CAD Core builds a
    // request-local Ondsel adapter without retaining a backend solver session.
    const std::vector<std::string> requestJointNames = jointNames(object, context);
    const AssemblySolveRequest request =
        buildAssemblySolveRequest(object, context, requestJointNames, jointGroupNames(object, context));
    AssemblySolveResult result = solveAssemblyWithOndselAdapter(request);
    for (const runtime::Diagnostic& diagnostic : result.diagnostics) {
        context.diagnostics.push_back(diagnostic);
    }
    for (const AssemblyPlacementUpdate& update : result.placementUpdates) {
        context.documentObjectUpdates.push_back(placementUpdateJson(update, object.name));
    }

    nlohmann::json groundedJoints = result.groundedJoints;
    nlohmann::json supportedJoints = result.joints;
    nlohmann::json unsupportedJoints = unsupportedJointsJson(result.unsupportedJoints);
    nlohmann::json solverJoints = solverJointsJson(result.solverJoints);
    nlohmann::json placementUpdates = placementUpdatesJson(result.placementUpdates, object.name);

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
            result.placementUpdates,
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
            result.placementUpdates,
        };
    }

    if (!unsupportedJoints.empty()) {
        return {
            "unsupported",
            {
                {"status", "unsupported"},
                {"reason", "unsupported_joint_type"},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"solver_joints", solverJoints},
                {"unsupported_joints", unsupportedJoints},
            },
            result.placementUpdates,
        };
    }

    if (result.status == "error") {
        return {
            result.solveState,
            {
                {"status", result.status},
                {"reason", result.reason},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"solver_joints", solverJoints},
                {"unsupported_joints", unsupportedJoints},
            },
            result.placementUpdates,
        };
    }

    if (result.status == "invalid") {
        return {
            result.solveState,
            {
                {"status", result.status},
                {"reason", result.reason},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"solver_joints", solverJoints},
                {"placement_updates", placementUpdates},
                {"unsupported_joints", unsupportedJoints},
            },
            result.placementUpdates,
        };
    }

    return {
        "solved",
        {
            {"status", "solved"},
            {"mode", result.mode},
            {"grounded_joints", groundedJoints},
            {"joints", supportedJoints},
            {"solver_joints", solverJoints},
            {"placement_updates", placementUpdates},
            {"unsupported_joints", unsupportedJoints},
        },
        result.placementUpdates,
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
