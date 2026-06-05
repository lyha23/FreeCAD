#include "cad_core/assembly/joint_group.h"

#include "assembly_support.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>

namespace cad_core::assembly {

void executeAssemblyJointGroup(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/JointGroup.cpp
    // PROPERTY_SOURCE(Assembly::JointGroup, App::DocumentObjectGroup); JointGroup::getJoints()
    // returns its App::FeaturePython joint children and delegates connector updates to their Proxy.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Group"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    nlohmann::json joints = nlohmann::json::array();
    for (const auto& link : app::readLinks(object, "Group")) {
        const app::DocumentObject* child = assembly_detail::documentObjectByName(context, link.object);
        if (child != nullptr && assembly_detail::isAssemblyJointFeaturePython(*child)) {
            joints.push_back(link.object);
        }
    }

    assembly_detail::publishEmptyResult(
        object,
        context,
        {
            {"assembly", "joint_group"},
            {"group", assembly_detail::linkNamesJson(app::readLinks(object, "Group"))},
            {"joints", joints},
            {"solve", "solver_inputs"},
        }
    );
}

void executeAssemblyFeaturePython(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py
    // class Joint "consists of 2 JCS (joint coordinate systems) and a Joint Type";
    // class GroundedJoint creates "ObjectToGround". cad-core exposes these solver inputs as
    // request-local metadata while the OndselSolver path remains the Assembly P8 known gap.
    const bool grounded = app::propertyValue(object, "ObjectToGround") != nullptr;
    const bool joint = app::propertyValue(object, "JointType") != nullptr;
    if (!grounded && !joint) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_type",
                               "Unsupported App::FeaturePython role " + object.name,
                               object.name,
                               {},
                               "runtime");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::set<std::string> allowedProperties = {"JointType",
                                                     "Reference1",
                                                     "Placement1",
                                                     "Detach1",
                                                     "Offset1",
                                                     "Reference2",
                                                     "Placement2",
                                                     "Detach2",
                                                     "Offset2",
                                                     "Angle",
                                                     "Distance",
                                                     "Distance2",
                                                     "EnableLengthMin",
                                                     "EnableLengthMax",
                                                     "EnableAngleMin",
                                                     "EnableAngleMax",
                                                     "LengthMin",
                                                     "LengthMax",
                                                     "AngleMin",
                                                     "AngleMax",
                                                     "Suppressed",
                                                     "ObjectToGround"};
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (grounded) {
        const auto objectToGround = app::readLink(object, "ObjectToGround");
        if (!objectToGround) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_property",
                                   "Grounded joint ObjectToGround is not set",
                                   object.name,
                                   "ObjectToGround",
                                   "runtime");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        assembly_detail::publishEmptyResult(
            object,
            context,
            {
                {"assembly", "grounded_joint"},
                {"object_to_ground", objectToGround->object},
                {"solve", "grounded_input"},
            }
        );
        return;
    }

    assembly_detail::publishEmptyResult(
        object,
        context,
        {
            {"assembly", "joint"},
            {"joint_type", app::readString(object, "JointType").value_or("")},
            {"reference1", assembly_detail::jointReferenceJson(object, "Reference1")},
            {"reference2", assembly_detail::jointReferenceJson(object, "Reference2")},
            {"suppressed", app::readBool(object, "Suppressed").value_or(false)},
            {"solve", "joint_input"},
        }
    );
}

}  // namespace cad_core::assembly
