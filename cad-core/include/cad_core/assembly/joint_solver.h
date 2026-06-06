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
};

struct AssemblyPartRef {
    std::string object;
    long long objectId = 0;
    std::string typeId;
    app::Placement placement;
    bool grounded = false;
};

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::makeMbdJoint(), calls "makeMbdJointOfType(joint,
// jointType)" after resolving "Reference1"/"Reference2" and "Placement1"/"Placement2".
// CAD Core落点: assembly DTO for the solver adapter boundary; executor builds this request,
// adapter converts it to representative or real Ondsel/MBD input.
struct JointConstraint {
    std::string object;
    std::string jointType;
    AssemblyJointReference reference1;
    AssemblyJointReference reference2;
    bool suppressed = false;
    std::optional<double> distance;
    std::optional<double> angle;
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
    bool supportedRepresentativePath = false;
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
// documentObjectUpdates emission. CAD Core applies this to both representative fallback results
// and request-local Ondsel results before exposing placement writeback updates.
void validateNewPlacementsEquivalent(const AssemblySolveRequest& request, AssemblySolveResult& result);

AssemblySolveResult solveAssemblyWithRepresentativeAdapter(const AssemblySolveRequest& request);

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
// AssemblyObject.cpp::AssemblyObject::solve(), key order "makeMbdAssembly()" ->
// "fixGroundedParts()" -> "jointParts(joints)" -> "mbdAssembly->runPreDrag()" ->
// "validateNewPlacements()" / "setNewPlacements()". CAD Core落点: request-local solver
// adapter. When OndselSolver is linked it builds an ASMTAssembly from this DTO; otherwise it
// returns the representative path without claiming full coverage.
AssemblySolveResult solveAssemblyWithOndselAdapter(const AssemblySolveRequest& request);

bool hasOndselSolverAdapter();

bool isSupportedRepresentativeJointType(const std::string& jointType);

}  // namespace cad_core::assembly
