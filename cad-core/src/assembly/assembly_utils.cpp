#include "assembly_support.h"

#include "cad_core/app/property_geo.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>

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

bool isSupportedRepresentativeJointType(const std::string& jointType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::makeMbdJointOfType(), maps "Fixed", "Revolute", "Slider",
    // "Ball", "Distance" and "Angle" to Ondsel ASMT joint classes. cad-core mirrors these
    // as stateless representative solver DTO paths.
    static const std::set<std::string> supported = {
        "Fixed",
        "Revolute",
        "Slider",
        "Ball",
        "Distance",
        "Angle",
    };
    return supported.count(jointType) != 0U;
}

std::array<double, 4> identityRotation()
{
    return {0.0, 0.0, 0.0, 1.0};
}

app::Placement placementForObject(const app::DocumentObject& object)
{
    return app::readPlacement(object, "Placement").value_or(app::Placement{{0.0, 0.0, 0.0}, identityRotation()});
}

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

bool samePlacement(const app::Placement& left, const app::Placement& right)
{
    for (std::size_t index = 0; index < 3U; ++index) {
        if (std::abs(left.base.at(index) - right.base.at(index)) > 1e-9) {
            return false;
        }
    }
    for (std::size_t index = 0; index < 4U; ++index) {
        if (std::abs(left.rotation.at(index) - right.rotation.at(index)) > 1e-9) {
            return false;
        }
    }
    return true;
}

std::optional<app::Placement> representativeSolvedPlacement(
    const app::DocumentObject& joint,
    const app::DocumentObject& reference1,
    const app::DocumentObject& reference2
)
{
    const std::string jointType = app::readString(joint, "JointType").value_or("");
    app::Placement solved = placementForObject(reference1);
    if (jointType == "Slider" || jointType == "Distance") {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
        // AssemblyObject.cpp::makeMbdJointDistance(), reads "getJointDistance(joint)";
        // Slider also adds translation limits from "LengthMin" / "LengthMax". This stateless
        // representative path uses the Distance property as the translation delta along the
        // Reference1 local X axis.
        solved.base.at(0) += app::readNumber(joint, "Distance").value_or(0.0);
    }
    if (jointType == "Angle") {
        // FreeCAD: AssemblyObject.cpp::makeMbdJointOfType(), for Angle stores
        // "mbdJoint->theIzJz = angle"; placement writeback still happens through
        // setNewPlacements(), so the representative DTO keeps translation from Reference1.
        (void)app::readNumber(joint, "Angle");
    }
    const app::Placement current = placementForObject(reference2);
    return samePlacement(current, solved) ? std::nullopt : std::optional<app::Placement>{solved};
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
    // ::setNewPlacements() writes "propPlacement->setValue(newPlacement)". cad-core mirrors a
    // stateless representative solver DTO for migrated JointTypes and returns the same writeback
    // as documentObjectUpdates without retaining an Ondsel session.
    nlohmann::json groundedJoints = nlohmann::json::array();
    nlohmann::json supportedJoints = nlohmann::json::array();
    nlohmann::json unsupportedJoints = nlohmann::json::array();
    nlohmann::json solverJoints = nlohmann::json::array();
    nlohmann::json placementUpdates = nlohmann::json::array();

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
            const std::string jointType = app::readString(*joint, "JointType").value_or("");
            nlohmann::json solverJoint = {
                {"object", jointName},
                {"joint_type", jointType},
                {"reference1", jointReferenceJson(*joint, "Reference1")},
                {"reference2", jointReferenceJson(*joint, "Reference2")},
                {"suppressed", app::readBool(*joint, "Suppressed").value_or(false)},
            };
            if (jointType == "Distance" || jointType == "Slider") {
                solverJoint["distance"] = app::readNumber(*joint, "Distance").value_or(0.0);
            }
            if (jointType == "Angle") {
                solverJoint["angle"] = app::readNumber(*joint, "Angle").value_or(0.0);
            }
            solverJoints.push_back(solverJoint);
            supportedJoints.push_back(jointName);
            if (!isSupportedRepresentativeJointType(jointType)) {
                unsupportedJoints.push_back({
                    {"object", jointName},
                    {"joint_type", jointType},
                    {"supported_representative_path", false},
                });
                continue;
            }

            const auto reference1 = app::readLink(*joint, "Reference1");
            const auto reference2 = app::readLink(*joint, "Reference2");
            const app::DocumentObject* reference1Object =
                reference1 ? documentObjectByName(context, reference1->object) : nullptr;
            const app::DocumentObject* reference2Object =
                reference2 ? documentObjectByName(context, reference2->object) : nullptr;
            if (reference1Object == nullptr || reference2Object == nullptr) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "missing_target",
                                       "Assembly joint reference target is missing",
                                       object.name,
                                       "Group",
                                       "runtime",
                                       jointName);
                continue;
            }
            if (const auto solved = representativeSolvedPlacement(*joint, *reference1Object, *reference2Object)) {
                nlohmann::json update = {
                    {"action", "assembly_set_placement"},
                    {"reason", "assembly_solver_placement_writeback"},
                    {"object", reference2Object->name},
                    {"objectId", reference2Object->id},
                    {"typeId", reference2Object->typeId},
                    {"assembly", object.name},
                    {"joint", jointName},
                    {"joint_type", jointType},
                    {"properties", {{"Placement", placementJson(*solved)}}},
                };
                placementUpdates.push_back(update);
                context.documentObjectUpdates.push_back(update);
            }
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

    if (!unsupportedJoints.empty()) {
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
                {"reason", "unsupported_joint_type"},
                {"grounded_joints", groundedJoints},
                {"joints", supportedJoints},
                {"solver_joints", solverJoints},
                {"unsupported_joints", unsupportedJoints},
            },
        };
    }

    return {
        "solved",
        {
            {"status", "solved"},
            {"mode", "representative_ondsel_solver"},
            {"grounded_joints", groundedJoints},
            {"joints", supportedJoints},
            {"solver_joints", solverJoints},
            {"placement_updates", placementUpdates},
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
