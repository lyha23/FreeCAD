#include "cad_core/assembly/joint_solver.h"

#include "cad_core/app/property_geo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <unordered_map>

#if CAD_CORE_HAS_ONDSEL_SOLVER
#include <OndselSolver/ASMTAssembly.h>
#include <OndselSolver/ASMTAngleJoint.h>
#include <OndselSolver/ASMTFixedJoint.h>
#include <OndselSolver/ASMTMarker.h>
#include <OndselSolver/ASMTParallelAxesJoint.h>
#include <OndselSolver/ASMTPart.h>
#include <OndselSolver/ASMTPrincipalMassMarker.h>
#include <OndselSolver/ASMTRevoluteJoint.h>
#include <OndselSolver/ASMTSphericalJoint.h>
#include <OndselSolver/ASMTSphSphJoint.h>
#include <OndselSolver/ASMTTranslationalJoint.h>
#endif

namespace cad_core::assembly {
namespace {

constexpr double kPlacementTolerance = 1e-9;

const app::DocumentObject* documentObjectByName(const runtime::ComputeContext& context,
                                                const std::string& name)
{
    const auto objectIt = context.documentObjects.find(name);
    if (objectIt == context.documentObjects.end()) {
        return nullptr;
    }
    return objectIt->second;
}

std::array<double, 4> identityRotation()
{
    return {0.0, 0.0, 0.0, 1.0};
}

app::Placement placementForObject(const app::DocumentObject& object)
{
    return app::readPlacement(object, "Placement")
        .value_or(app::Placement {{0.0, 0.0, 0.0}, identityRotation()});
}

bool samePlacement(const app::Placement& left, const app::Placement& right)
{
    for (std::size_t index = 0; index < 3U; ++index) {
        if (std::abs(left.base.at(index) - right.base.at(index)) > kPlacementTolerance) {
            return false;
        }
    }
    for (std::size_t index = 0; index < 4U; ++index) {
        if (std::abs(left.rotation.at(index) - right.rotation.at(index)) > kPlacementTolerance) {
            return false;
        }
    }
    return true;
}

std::optional<AssemblyPartRef> partByName(const AssemblySolveRequest& request,
                                          const std::string& object)
{
    const auto partIt = std::find_if(
        request.parts.begin(),
        request.parts.end(),
        [&](const AssemblyPartRef& part) {
            return part.object == object;
        }
    );
    if (partIt == request.parts.end()) {
        return std::nullopt;
    }
    return *partIt;
}

AssemblyJointReference jointReference(const app::DocumentObject& joint,
                                      const std::string& referenceProperty,
                                      const std::string& placementProperty)
{
    AssemblyJointReference reference;
    if (const auto link = app::readLink(joint, referenceProperty)) {
        reference.object = link->object;
        reference.subnames = link->subnames;
    }
    reference.connectorPlacement = app::readPlacement(joint, placementProperty);
    return reference;
}

std::optional<app::Placement> representativeSolvedPlacement(const JointConstraint& joint,
                                                            const AssemblyPartRef& reference1,
                                                            const AssemblyPartRef& reference2)
{
    app::Placement solved = reference1.placement;
    if (joint.jointType == "Slider" || joint.jointType == "Distance") {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
        // AssemblyObject.cpp::AssemblyObject::makeMbdJointDistance(), reads
        // "getJointDistance(joint)"; Slider later adds translation limits from "LengthMin" /
        // "LengthMax". CAD Core落点: representative adapter keeps only the existing transport
        // contract and must not be treated as real Ondsel solve output.
        solved.base.at(0) += joint.distance.value_or(0.0);
    }
    if (joint.jointType == "Angle") {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
        // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), key "mbdJoint->theIzJz =
        // angle"; representative mode preserves Reference1 translation and exposes the DTO only.
        (void)joint.angle;
    }
    return samePlacement(reference2.placement, solved) ? std::nullopt
                                                       : std::optional<app::Placement> {solved};
}

bool isGroundedObject(const AssemblySolveRequest& request, const std::string& object)
{
    const auto part = partByName(request, object);
    return part && part->grounded;
}

#if CAD_CORE_HAS_ONDSEL_SOLVER

std::array<double, 9> rotationMatrixForPlacement(const app::Placement& placement)
{
    const double x = placement.rotation.at(0);
    const double y = placement.rotation.at(1);
    const double z = placement.rotation.at(2);
    const double w = placement.rotation.at(3);
    const double norm = std::sqrt(x * x + y * y + z * z + w * w);
    if (norm <= 0.0) {
        return {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    }

    const double nx = x / norm;
    const double ny = y / norm;
    const double nz = z / norm;
    const double nw = w / norm;
    return {
        1.0 - 2.0 * (ny * ny + nz * nz),
        2.0 * (nx * ny - nz * nw),
        2.0 * (nx * nz + ny * nw),
        2.0 * (nx * ny + nz * nw),
        1.0 - 2.0 * (nx * nx + nz * nz),
        2.0 * (ny * nz - nx * nw),
        2.0 * (nx * nz - ny * nw),
        2.0 * (ny * nz + nx * nw),
        1.0 - 2.0 * (nx * nx + ny * ny),
    };
}

template <typename SpatialItem>
void setOndselPlacement(const std::shared_ptr<SpatialItem>& item, const app::Placement& placement)
{
    const std::array<double, 9> rotation = rotationMatrixForPlacement(placement);
    item->setPosition3D(placement.base.at(0), placement.base.at(1), placement.base.at(2));
    item->setRotationMatrix(rotation.at(0),
                            rotation.at(1),
                            rotation.at(2),
                            rotation.at(3),
                            rotation.at(4),
                            rotation.at(5),
                            rotation.at(6),
                            rotation.at(7),
                            rotation.at(8));
}

std::shared_ptr<MbD::ASMTPart> makeOndselPart(const AssemblyPartRef& sourcePart)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdPart(), sets "setPosition3D" and
    // "setRotationMatrix" from the DocumentObject Placement, plus a principal mass marker.
    // CAD Core落点: request-local ASMTPart creation from AssemblyPartRef.
    auto part = MbD::ASMTPart::With();
    part->setName(sourcePart.object);
    setOndselPlacement(part, sourcePart.placement);

    auto massMarker = MbD::ASMTPrincipalMassMarker::With();
    massMarker->setMass(1.0);
    massMarker->setDensity(1.0);
    massMarker->setMomentOfInertias(1.0, 1.0, 1.0);
    part->setPrincipalMassMarker(massMarker);
    return part;
}

std::shared_ptr<MbD::ASMTMarker> makeOndselMarker(const std::string& name,
                                                  const app::Placement& placement)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdMarker(), key "setPosition3D" /
    // "setRotationMatrix". CAD Core落点: joint connector marker conversion.
    auto marker = MbD::ASMTMarker::With();
    marker->setName(name);
    setOndselPlacement(marker, placement);
    return marker;
}

app::Placement placementFromOndselPart(const std::shared_ptr<MbD::ASMTPart>& part)
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    part->getPosition3D(x, y, z);

    double qw = 1.0;
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    part->getQuarternions(qw, qx, qy, qz);
    return app::Placement {{x, y, z}, {qx, qy, qz, qw}};
}

std::shared_ptr<MbD::ASMTJoint> makeOndselJointOfType(const JointConstraint& joint)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), maps "Fixed" to
    // ASMTFixedJoint, "Revolute" to ASMTRevoluteJoint, "Slider" to ASMTTranslationalJoint,
    // "Ball" to ASMTSphericalJoint and "Angle" to ASMTAngleJoint with "theIzJz".
    // CAD Core落点: real Ondsel adapter joint DTO conversion.
    if (joint.jointType == "Fixed") {
        return MbD::ASMTFixedJoint::With();
    }
    if (joint.jointType == "Revolute") {
        return MbD::ASMTRevoluteJoint::With();
    }
    if (joint.jointType == "Slider") {
        return MbD::ASMTTranslationalJoint::With();
    }
    if (joint.jointType == "Ball") {
        return MbD::ASMTSphericalJoint::With();
    }
    if (joint.jointType == "Distance") {
        auto distanceJoint = MbD::ASMTSphSphJoint::With();
        distanceJoint->distanceIJ = joint.distance.value_or(0.0);
        return distanceJoint;
    }
    if (joint.jointType == "Angle") {
        constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
        const double angleRadians = std::abs(joint.angle.value_or(0.0)) * degreesToRadians;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
        // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), "if (angle == 0) {
        // return CREATE<ASMTParallelAxesJoint>::With(); }" before setting "theIzJz".
        if (angleRadians == 0.0) {
            return MbD::ASMTParallelAxesJoint::With();
        }
        auto angleJoint = MbD::ASMTAngleJoint::With();
        angleJoint->theIzJz = angleRadians;
        return angleJoint;
    }
    return nullptr;
}

void addGroundedJointToOndselAssembly(
    const std::shared_ptr<MbD::ASMTAssembly>& assembly,
    const AssemblyPartRef& sourcePart,
    const std::shared_ptr<MbD::ASMTPart>& part
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::fixGroundedPart(), creates assembly marker
    // "marker-<obj>", part marker "FixingMarker", then ASMTFixedJoint with marker paths
    // "/OndselAssembly/<marker>" and "/OndselAssembly/<part>/FixingMarker".
    // CAD Core落点: fixed body creation for request-local Ondsel solve.
    const std::string assemblyMarkerName = "marker-" + sourcePart.object;
    assembly->addMarker(makeOndselMarker(assemblyMarkerName, sourcePart.placement));

    const std::string partMarkerName = "FixingMarker";
    part->addMarker(makeOndselMarker(partMarkerName, app::Placement {{0.0, 0.0, 0.0}, identityRotation()}));

    auto fixedJoint = MbD::ASMTFixedJoint::With();
    fixedJoint->setName(sourcePart.object);
    fixedJoint->setMarkerI("/OndselAssembly/" + assemblyMarkerName);
    fixedJoint->setMarkerJ("/OndselAssembly/" + sourcePart.object + "/" + partMarkerName);
    assembly->addJoint(fixedJoint);
}

void addConstraintToOndselAssembly(
    const std::shared_ptr<MbD::ASMTAssembly>& assembly,
    const JointConstraint& joint,
    const std::unordered_map<std::string, std::shared_ptr<MbD::ASMTPart>>& parts
)
{
    auto mbdJoint = makeOndselJointOfType(joint);
    if (!mbdJoint) {
        return;
    }
    const std::string markerI = "marker-" + joint.object + "-I";
    const std::string markerJ = "marker-" + joint.object + "-J";
    const app::Placement identity {{0.0, 0.0, 0.0}, identityRotation()};
    const auto& partI = parts.at(joint.reference1.object);
    const auto& partJ = parts.at(joint.reference2.object);
    partI->addMarker(makeOndselMarker(markerI, joint.reference1.connectorPlacement.value_or(identity)));
    partJ->addMarker(makeOndselMarker(markerJ, joint.reference2.connectorPlacement.value_or(identity)));

    mbdJoint->setName(joint.object);
    mbdJoint->setMarkerI("/OndselAssembly/" + joint.reference1.object + "/" + markerI);
    mbdJoint->setMarkerJ("/OndselAssembly/" + joint.reference2.object + "/" + markerJ);
    assembly->addJoint(mbdJoint);
}

AssemblySolveResult solveAssemblyWithRealOndselAdapter(const AssemblySolveRequest& request)
{
    AssemblySolveResult result;
    result.groundedJoints = request.groundedJoints;

    for (const JointConstraint& joint : request.joints) {
        result.joints.push_back(joint.object);
        result.solverJoints.push_back(joint);
        if (!isSupportedRepresentativeJointType(joint.jointType)) {
            result.unsupportedJoints.push_back(UnsupportedAssemblyJoint {
                joint.object,
                joint.jointType,
                false,
            });
        }
    }
    if (!result.unsupportedJoints.empty()) {
        result.solveState = "unsupported";
        result.status = "unsupported";
        result.reason = "unsupported_joint_type";
        for (const UnsupportedAssemblyJoint& unsupported : result.unsupportedJoints) {
            result.diagnostics.push_back(runtime::Diagnostic {
                "warning",
                "unsupported_assembly_solver",
                "Ondsel solver adapter does not yet convert JointType " + unsupported.jointType,
                request.assemblyObject,
                "Group",
                "runtime",
                unsupported.object,
                {},
            });
        }
        return result;
    }

    if (result.groundedJoints.empty() && result.joints.empty()) {
        result.solveState = "skipped_no_joints";
        result.status = "skipped";
        result.reason = "no_joints";
        return result;
    }
    if (result.groundedJoints.empty()) {
        return solveAssemblyWithRepresentativeAdapter(request);
    }
    if (result.joints.empty()) {
        result.solveState = "solved_noop";
        result.status = "solved";
        result.mode = "grounded_only_noop";
        return result;
    }

    std::unordered_map<std::string, std::shared_ptr<MbD::ASMTPart>> mbdParts;
    for (const AssemblyPartRef& sourcePart : request.parts) {
        mbdParts[sourcePart.object] = makeOndselPart(sourcePart);
    }

    for (const JointConstraint& joint : request.joints) {
        if (mbdParts.find(joint.reference1.object) == mbdParts.end()
            || mbdParts.find(joint.reference2.object) == mbdParts.end()) {
            result.diagnostics.push_back(runtime::Diagnostic {
                "error",
                "missing_target",
                "Assembly joint reference target is missing",
                request.assemblyObject,
                "Group",
                "runtime",
                joint.object,
                {},
            });
        }
    }
    if (!result.diagnostics.empty()) {
        result.solveState = "error";
        result.status = "error";
        result.reason = "missing_target";
        return result;
    }

    try {
        auto assembly = MbD::ASMTAssembly::With();
        assembly->setName("OndselAssembly");

        for (const AssemblyPartRef& sourcePart : request.parts) {
            assembly->addPart(mbdParts.at(sourcePart.object));
        }
        for (const AssemblyPartRef& sourcePart : request.parts) {
            if (sourcePart.grounded) {
                addGroundedJointToOndselAssembly(assembly, sourcePart, mbdParts.at(sourcePart.object));
            }
        }
        for (const JointConstraint& joint : request.joints) {
            addConstraintToOndselAssembly(assembly, joint, mbdParts);
        }

        assembly->runPreDrag();

        for (const AssemblyPartRef& sourcePart : request.parts) {
            const app::Placement solved = placementFromOndselPart(mbdParts.at(sourcePart.object));
            if (!samePlacement(sourcePart.placement, solved)) {
                result.placementUpdates.push_back(AssemblyPlacementUpdate {
                    sourcePart.object,
                    sourcePart.objectId,
                    sourcePart.typeId,
                    "OndselSolver",
                    "solver_result",
                    solved,
                });
            }
        }
    }
    catch (const std::exception& exception) {
        result.diagnostics.push_back(runtime::Diagnostic {
            "error",
            "ondsel_solver_failed",
            std::string("Ondsel solver failed: ") + exception.what(),
            request.assemblyObject,
            "Group",
            "runtime",
            {},
            {},
        });
        result.solveState = "error";
        result.status = "error";
        result.reason = "ondsel_solver_failed";
        return result;
    }
    catch (...) {
        result.diagnostics.push_back(runtime::Diagnostic {
            "error",
            "ondsel_solver_failed",
            "Ondsel solver failed with an unknown exception",
            request.assemblyObject,
            "Group",
            "runtime",
            {},
            {},
        });
        result.solveState = "error";
        result.status = "error";
        result.reason = "ondsel_solver_failed";
        return result;
    }

    result.solveState = "solved";
    result.status = "solved";
    result.mode = "real_ondsel_solver";
    validateNewPlacementsEquivalent(request, result);
    return result;
}

#endif

}  // namespace

bool isSupportedRepresentativeJointType(const std::string& jointType)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), maps "Fixed", "Revolute",
    // "Slider", "Ball", "Distance" and "Angle" to ASMT joint classes in the current CAD Core
    // representative matrix. This is not the full Ondsel/MBD session.
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

AssemblySolveRequest buildAssemblySolveRequest(
    const app::DocumentObject& assemblyObject,
    const runtime::ComputeContext& context,
    const std::vector<std::string>& jointNames,
    const std::vector<std::string>& jointGroupNames
)
{
    AssemblySolveRequest request;
    request.assemblyObject = assemblyObject.name;
    request.jointGroups = jointGroupNames;

    for (const auto& link : app::readLinks(assemblyObject, "Group")) {
        const app::DocumentObject* child = documentObjectByName(context, link.object);
        if (child == nullptr || child->typeId == "Assembly::JointGroup") {
            continue;
        }
        if (app::propertyValue(*child, "JointType") != nullptr
            || app::propertyValue(*child, "ObjectToGround") != nullptr) {
            continue;
        }
        request.parts.push_back(AssemblyPartRef {
            child->name,
            child->id,
            child->typeId,
            placementForObject(*child),
            false,
        });
    }

    for (const std::string& jointName : jointNames) {
        const app::DocumentObject* joint = documentObjectByName(context, jointName);
        if (joint == nullptr) {
            continue;
        }
        if (const auto grounded = app::readLink(*joint, "ObjectToGround")) {
            request.groundedJoints.push_back(jointName);
            for (AssemblyPartRef& part : request.parts) {
                if (part.object == grounded->object) {
                    part.grounded = true;
                }
            }
            continue;
        }
        if (app::propertyValue(*joint, "JointType") == nullptr) {
            continue;
        }
        JointConstraint constraint;
        constraint.object = jointName;
        constraint.jointType = app::readString(*joint, "JointType").value_or("");
        constraint.reference1 = jointReference(*joint, "Reference1", "Placement1");
        constraint.reference2 = jointReference(*joint, "Reference2", "Placement2");
        constraint.suppressed = app::readBool(*joint, "Suppressed").value_or(false);
        if (constraint.jointType == "Distance" || constraint.jointType == "Slider") {
            constraint.distance = app::readNumber(*joint, "Distance").value_or(0.0);
        }
        if (constraint.jointType == "Angle") {
            constraint.angle = app::readNumber(*joint, "Angle").value_or(0.0);
        }
        request.joints.push_back(std::move(constraint));
    }

    return request;
}

void validateNewPlacementsEquivalent(const AssemblySolveRequest& request, AssemblySolveResult& result)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::validateNewPlacements(), checks grounded parts first and
    // returns false when "oldPlc.isSame(newPlacement, Precision::Confusion())" fails. CAD Core keeps
    // the same gate before emitting documentObjectUpdates.
    const auto invalidIt = std::find_if(
        result.placementUpdates.begin(),
        result.placementUpdates.end(),
        [&](const AssemblyPlacementUpdate& update) {
            return isGroundedObject(request, update.object);
        }
    );
    if (invalidIt == result.placementUpdates.end()) {
        return;
    }

    result.diagnostics.push_back(runtime::Diagnostic {
        "warning",
        "invalid_assembly_solver_result",
        "Assembly validation rejected solve because a grounded object moved",
        request.assemblyObject,
        "Group",
        "runtime",
        invalidIt->object,
        {},
    });
    result.placementUpdates.clear();
    result.status = "invalid";
    result.solveState = "invalid";
    result.reason = "grounded_object_moved";
}

AssemblySolveResult solveAssemblyWithRepresentativeAdapter(const AssemblySolveRequest& request)
{
    AssemblySolveResult result;
    result.groundedJoints = request.groundedJoints;

    for (const JointConstraint& joint : request.joints) {
        result.joints.push_back(joint.object);
        result.solverJoints.push_back(joint);

        if (!isSupportedRepresentativeJointType(joint.jointType)) {
            result.unsupportedJoints.push_back(UnsupportedAssemblyJoint {
                joint.object,
                joint.jointType,
                false,
            });
            continue;
        }

        const auto reference1 = partByName(request, joint.reference1.object);
        const auto reference2 = partByName(request, joint.reference2.object);
        if (!reference1 || !reference2) {
            result.diagnostics.push_back(runtime::Diagnostic {
                "error",
                "missing_target",
                "Assembly joint reference target is missing",
                request.assemblyObject,
                "Group",
                "runtime",
                joint.object,
                {},
            });
            continue;
        }
        if (const auto solved = representativeSolvedPlacement(joint, *reference1, *reference2)) {
            result.placementUpdates.push_back(AssemblyPlacementUpdate {
                reference2->object,
                reference2->objectId,
                reference2->typeId,
                joint.object,
                joint.jointType,
                *solved,
            });
        }
    }

    if (result.groundedJoints.empty() && result.joints.empty()) {
        result.solveState = "skipped_no_joints";
        result.status = "skipped";
        result.reason = "no_joints";
        return result;
    }

    if (result.joints.empty()) {
        result.solveState = "solved_noop";
        result.status = "solved";
        result.mode = "grounded_only_noop";
        return result;
    }

    if (!result.unsupportedJoints.empty()) {
        result.solveState = "unsupported";
        result.status = "unsupported";
        result.reason = "unsupported_joint_type";
        for (const UnsupportedAssemblyJoint& unsupported : result.unsupportedJoints) {
            result.diagnostics.push_back(runtime::Diagnostic {
                "warning",
                "unsupported_assembly_solver",
                "Assembly solver adapter does not yet solve JointType " + unsupported.jointType,
                request.assemblyObject,
                "Group",
                "runtime",
                unsupported.object,
                {},
            });
        }
        return result;
    }

    result.solveState = "solved";
    result.status = "solved";
    result.mode = "representative_ondsel_solver";
    validateNewPlacementsEquivalent(request, result);
    return result;
}

bool hasOndselSolverAdapter()
{
#if CAD_CORE_HAS_ONDSEL_SOLVER
    return true;
#else
    return false;
#endif
}

AssemblySolveResult solveAssemblyWithOndselAdapter(const AssemblySolveRequest& request)
{
#if CAD_CORE_HAS_ONDSEL_SOLVER
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::solve(), key "auto groundedObjs = fixGroundedParts();
    // if (groundedObjs.empty()) return -6;" CAD Core keeps no-ground requests on the existing
    // representative path unless a valid FreeCAD-style grounded solve can be built.
    if (!request.groundedJoints.empty()) {
        return solveAssemblyWithRealOndselAdapter(request);
    }
#endif
    return solveAssemblyWithRepresentativeAdapter(request);
}

}  // namespace cad_core::assembly
