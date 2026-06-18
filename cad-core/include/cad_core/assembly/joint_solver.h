#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/diagnostics.h"

#include <optional>
#include <string>
#include <vector>

namespace cad_core::assembly {

struct AssemblyJointReference {
    std::string object;
    std::vector<std::string> subnames;
    std::optional<app::Placement> connectorPlacement;
    std::optional<app::Placement> markerPlacement;
    std::optional<app::Placement> objectGlobalPlacement;
    std::optional<app::Placement> partGlobalPlacement;
    std::optional<app::Placement> jcsGlobalPlacement;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::handleOneSideOfJoint(), "plc which is the JCS placement" is first
    // converted through "getGlobalPlacement(nullptr, ref)" and then through
    // "getGlobalPlacement(part, ref).inverse()". CAD Core keeps this resolver evidence
    // request-local so subshape refs do not look like object-level marker parity.
    std::string markerResolutionStatus;
    std::string markerResolutionFrame;
    std::string markerResolutionDiagnostic;
    bool markerResolutionRequiresHandleOneSide = false;
    bool markerResolutionUsedObjectLevelBaseline = false;
    bool markerResolutionConnectorDefaulted = false;
    std::string elementKind;
    std::string primitive;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    // ::getEdgeRadius() returns "sf.Circle().Radius()" only for "GeomAbs_Circle";
    // ::getFaceRadius() returns Cylinder/Sphere "Radius()" and 0.0 for other faces.
    // CAD Core keeps this primitive evidence on the request-local solver DTO.
    std::optional<double> radius;
    std::string radiusSource;
};

struct AssemblyPartRef {
    std::string object;
    long long objectId = 0;
    std::string typeId;
    app::Placement placement;
    bool grounded = false;
};

// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
// ::AssemblyObject::getRackPinionMarkers(), requires rack marker I / pinion marker J, then
// rewrites rack marker rotation so its "X axis parallel to the sliding axis" before marker
// creation. CAD Core keeps this request-local evidence on the solver DTO only.
struct RackPinionMarkerRewrite {
    bool applied = false;
    std::string rackObject;
    std::string pinionObject;
    double yawAdjustment = 0.0;
    app::Placement rackMarkerPlacement;
};

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::makeMbdJoint(), calls "makeMbdJointOfType(joint,
// jointType)" after resolving "Reference1"/"Reference2" and "Placement1"/"Placement2".
// CAD Core落点: assembly DTO for the Ondsel solver adapter boundary; executor builds this
// request and adapter converts it to Ondsel/MBD input.
struct JointConstraint {
    std::string object;
    std::string jointType;
    AssemblyJointReference reference1;
    AssemblyJointReference reference2;
    bool suppressed = false;
    std::optional<double> distance;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    // ::getDistanceType(), reads "Reference1" / "Reference2" element kind and line/plane primitive,
    // then calls "swapJCS(joint)" for solver ordering. CAD Core keeps the DistanceType and
    // reference primitive evidence request-local on this solver DTO.
    std::optional<std::string> distanceType;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::makeMbdJointDistance(), maps each basic "DistanceType" to a resolved ASMT joint class
    // and writes either "distanceIJ" or "offset" from "getJointDistance(joint)".
    std::optional<std::string> solverJointClass;
    std::optional<double> distanceIJ;
    std::optional<double> offset;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::makeMbdJointDistance(), extended radius cases add "getEdgeRadius(...)" or
    // "getFaceRadius(...)" to distanceIJ/offset. S3 exposes the scalar correction evidence only;
    // S4/S6 own ASMT class publication for non-basic DistanceTypes.
    std::optional<double> scalarCorrection;
    std::string scalarCorrectionSource;
    std::string radiusSourceSide;
    std::string distanceTypeMappingStatus;
    std::string distanceTypeBoundary;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py
    // ::JointUsingDistance2 contains "Gears" / "Belt"; AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType() reads "getJointDistance2(joint)" into
    // ASMTGearJoint "radiusJ".
    std::optional<double> distance2;
    std::optional<double> angle;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), Screw first checks "slidingPartIndex(joint)" and
    // then sets "mbdJoint->pitch = getJointDistance(joint)"; CAD Core keeps the scalar
    // Distance-to-pitch conversion request-local on the solver DTO.
    std::optional<double> pitch;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::slidingPartIndex(), returns 1/2/0 after scanning "Slider" joints and
    // comparing JCS "pitch and roll"; used by Screw/RackPinion before solver marker creation.
    std::optional<int> slidingPartIndex;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    // ::swapJCS(), swaps "Placement1"/"Placement2" and "Reference1"/"Reference2"; CAD Core keeps
    // this as request-local DTO ordering only and never mutates the DocumentObject graph.
    bool jcsSwappedForSolver = false;
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), RackPinion sets "mbdJoint->pitchRadius =
    // getJointDistance(joint)"; CAD Core exposes the scalar conversion evidence before public
    // capability publication.
    std::optional<double> pitchRadius;
    std::optional<RackPinionMarkerRewrite> rackPinionMarkerRewrite;
};

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::solve(), call order "fixGroundedParts()" ->
// "jointParts(joints)" -> "mbdAssembly->runPreDrag()" -> "setNewPlacements()".
// CAD Core落点: request-local Assembly solver session input. It is rebuilt from the graph
// every request and does not persist a backend solver session.
struct AssemblySolveRequest {
    std::string assemblyObject;
    std::vector<std::string> jointGroups;
    std::vector<AssemblyPartRef> parts;
    std::vector<std::string> groundedJoints;
    std::vector<JointConstraint> joints;
};

struct AssemblyPlacementUpdate {
    std::string object;
    long long objectId = 0;
    std::string typeId;
    std::string joint;
    std::string jointType;
    app::Placement placement;
};

struct UnsupportedAssemblyJoint {
    std::string object;
    std::string jointType;
    std::string reason;
};

struct AssemblySolveResult {
    std::string solveState;
    std::string status;
    std::string mode;
    std::string reason;
    std::vector<std::string> groundedJoints;
    std::vector<std::string> joints;
    std::vector<JointConstraint> solverJoints;
    std::vector<UnsupportedAssemblyJoint> unsupportedJoints;
    std::vector<AssemblyPlacementUpdate> placementUpdates;
    std::vector<runtime::Diagnostic> diagnostics;
};

AssemblySolveRequest buildAssemblySolveRequest(
    const app::DocumentObject& assemblyObject,
    const runtime::ComputeContext& context,
    const std::vector<std::string>& jointNames,
    const std::vector<std::string>& jointGroupNames
);

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::validateNewPlacements(), key "Ignoring bad solve, a
// grounded object (...) moved." CAD Core落点: solver result validation diagnostic before
// documentObjectUpdates emission. CAD Core applies this to the request-local Ondsel results
// before exposing placement writeback updates.
void validateNewPlacementsEquivalent(const AssemblySolveRequest& request, AssemblySolveResult& result);

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::solve(), key order "makeMbdAssembly()" ->
// "fixGroundedParts()" -> "jointParts(joints)" -> "mbdAssembly->runPreDrag()" ->
// "validateNewPlacements()" / "setNewPlacements()". CAD Core落点: request-local solver
// adapter. It builds an ASMTAssembly from this DTO and requires OndselSolver at build time.
AssemblySolveResult solveAssemblyWithOndselAdapter(const AssemblySolveRequest& request);

bool hasOndselSolverAdapter();

bool isSupportedOndselJointType(const std::string& jointType);

}  // namespace cad_core::assembly
