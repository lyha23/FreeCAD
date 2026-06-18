#include "cad_core/assembly/joint_solver.h"

#include "cad_core/app/property_geo.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>

#include <OndselSolver/ASMTAssembly.h>
#include <OndselSolver/ASMTAngleJoint.h>
#include <OndselSolver/ASMTCylSphJoint.h>
#include <OndselSolver/ASMTCylindricalJoint.h>
#include <OndselSolver/ASMTFixedJoint.h>
#include <OndselSolver/ASMTGearJoint.h>
#include <OndselSolver/ASMTLineInPlaneJoint.h>
#include <OndselSolver/ASMTMarker.h>
#include <OndselSolver/ASMTParallelAxesJoint.h>
#include <OndselSolver/ASMTPerpendicularJoint.h>
#include <OndselSolver/ASMTPlanarJoint.h>
#include <OndselSolver/ASMTPart.h>
#include <OndselSolver/ASMTPointInPlaneJoint.h>
#include <OndselSolver/ASMTPrincipalMassMarker.h>
#include <OndselSolver/ASMTRackPinionJoint.h>
#include <OndselSolver/ASMTRevCylJoint.h>
#include <OndselSolver/ASMTRevoluteJoint.h>
#include <OndselSolver/ASMTSphericalJoint.h>
#include <OndselSolver/ASMTSphSphJoint.h>
#include <OndselSolver/ASMTScrewJoint.h>
#include <OndselSolver/ASMTTranslationalJoint.h>

namespace cad_core::assembly {
namespace {

constexpr double kPlacementTolerance = 1e-9;
constexpr double kPi = 3.14159265358979323846;
constexpr double kFreeCadPrecisionConfusion = 1e-7;

using Vector3 = std::array<double, 3>;

struct YawPitchRoll {
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
};

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

app::Placement identityPlacement()
{
    return app::Placement {{0.0, 0.0, 0.0}, identityRotation()};
}

double dot(const Vector3& left, const Vector3& right)
{
    return left.at(0) * right.at(0) + left.at(1) * right.at(1) + left.at(2) * right.at(2);
}

Vector3 cross(const Vector3& left, const Vector3& right)
{
    return {
        left.at(1) * right.at(2) - left.at(2) * right.at(1),
        left.at(2) * right.at(0) - left.at(0) * right.at(2),
        left.at(0) * right.at(1) - left.at(1) * right.at(0),
    };
}

double norm(const Vector3& value)
{
    return std::sqrt(dot(value, value));
}

Vector3 normalized(const Vector3& value)
{
    const double length = norm(value);
    if (length <= kPlacementTolerance) {
        return {0.0, 0.0, 0.0};
    }
    return {value.at(0) / length, value.at(1) / length, value.at(2) / length};
}

std::array<double, 4> normalizedRotation(const std::array<double, 4>& rotation)
{
    const double length = std::sqrt(rotation.at(0) * rotation.at(0) + rotation.at(1) * rotation.at(1)
                                    + rotation.at(2) * rotation.at(2)
                                    + rotation.at(3) * rotation.at(3));
    if (length <= 0.0) {
        return identityRotation();
    }
    return {
        rotation.at(0) / length,
        rotation.at(1) / length,
        rotation.at(2) / length,
        rotation.at(3) / length,
    };
}

std::array<double, 4> multiplyRotations(const std::array<double, 4>& left,
                                        const std::array<double, 4>& right)
{
    const auto a = normalizedRotation(left);
    const auto b = normalizedRotation(right);
    const double ax = a.at(0);
    const double ay = a.at(1);
    const double az = a.at(2);
    const double aw = a.at(3);
    const double bx = b.at(0);
    const double by = b.at(1);
    const double bz = b.at(2);
    const double bw = b.at(3);
    return normalizedRotation({
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    });
}

std::array<double, 4> inverseRotation(const std::array<double, 4>& rotation)
{
    const auto normalizedValue = normalizedRotation(rotation);
    return {-normalizedValue.at(0), -normalizedValue.at(1), -normalizedValue.at(2), normalizedValue.at(3)};
}

Vector3 rotateVector(const std::array<double, 4>& rotation, const Vector3& vector)
{
    const auto q = normalizedRotation(rotation);
    const Vector3 qVector {q.at(0), q.at(1), q.at(2)};
    const Vector3 t = cross(qVector, vector);
    const Vector3 scaledT {2.0 * t.at(0), 2.0 * t.at(1), 2.0 * t.at(2)};
    const Vector3 qCrossT = cross(qVector, scaledT);
    return {
        vector.at(0) + q.at(3) * scaledT.at(0) + qCrossT.at(0),
        vector.at(1) + q.at(3) * scaledT.at(1) + qCrossT.at(1),
        vector.at(2) + q.at(3) * scaledT.at(2) + qCrossT.at(2),
    };
}

app::Placement composePlacement(const app::Placement& left, const app::Placement& right)
{
    const Vector3 rotatedBase = rotateVector(left.rotation, right.base);
    return app::Placement {{
                               left.base.at(0) + rotatedBase.at(0),
                               left.base.at(1) + rotatedBase.at(1),
                               left.base.at(2) + rotatedBase.at(2),
                           },
                          multiplyRotations(left.rotation, right.rotation)};
}

app::Placement inversePlacement(const app::Placement& placement)
{
    const auto inverse = inverseRotation(placement.rotation);
    const Vector3 negativeBase {-placement.base.at(0), -placement.base.at(1), -placement.base.at(2)};
    const Vector3 inverseBase = rotateVector(inverse, negativeBase);
    return app::Placement {inverseBase, inverse};
}

double angleBetween(const Vector3& left, const Vector3& right)
{
    const double leftNorm = norm(left);
    const double rightNorm = norm(right);
    if (leftNorm <= kPlacementTolerance || rightNorm <= kPlacementTolerance) {
        return 0.0;
    }
    const double cosine = std::clamp(dot(left, right) / (leftNorm * rightNorm), -1.0, 1.0);
    return std::acos(cosine);
}

std::array<double, 4> rotationAroundAxis(const Vector3& axis, double angle)
{
    const Vector3 unitAxis = normalized(axis);
    if (norm(unitAxis) <= kPlacementTolerance) {
        return identityRotation();
    }
    const double halfAngle = angle / 2.0;
    const double sine = std::sin(halfAngle);
    return normalizedRotation({
        unitAxis.at(0) * sine,
        unitAxis.at(1) * sine,
        unitAxis.at(2) * sine,
        std::cos(halfAngle),
    });
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

double toDegrees(double radians)
{
    return radians * 180.0 / kPi;
}

YawPitchRoll yawPitchRollForPlacement(const app::Placement& placement)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Base/Rotation.cpp
    // ::Rotation::getYawPitchRoll(), "Euler angles (yaw,pitch,roll) are in XY'Z''-notation" and
    // returns degrees. CAD Core mirrors the same pitch/roll values for slidingPartIndex().
    double x = placement.rotation.at(0);
    double y = placement.rotation.at(1);
    double z = placement.rotation.at(2);
    double w = placement.rotation.at(3);
    const double norm = std::sqrt(x * x + y * y + z * z + w * w);
    if (norm <= 0.0) {
        return {};
    }
    x /= norm;
    y /= norm;
    z /= norm;
    w /= norm;

    const double q00 = x * x;
    const double q11 = y * y;
    const double q22 = z * z;
    const double q33 = w * w;
    const double q01 = x * y;
    const double q02 = x * z;
    const double q03 = x * w;
    const double q12 = y * z;
    const double q13 = y * w;
    const double q23 = z * w;
    const double qd2 = 2.0 * (q13 - q02);
    constexpr double tolerance = 16.0 * std::numeric_limits<double>::epsilon();

    YawPitchRoll result;
    if (std::abs(qd2 - 1.0) <= tolerance) {
        result.yaw = 0.0;
        result.pitch = toDegrees(kPi / 2.0);
        result.roll = toDegrees(2.0 * std::atan2(x, w));
        return result;
    }
    if (std::abs(qd2 + 1.0) <= tolerance) {
        result.yaw = 0.0;
        result.pitch = toDegrees(-kPi / 2.0);
        result.roll = toDegrees(2.0 * std::atan2(x, w));
        return result;
    }

    result.yaw = toDegrees(std::atan2(2.0 * (q01 + q23), (q00 + q33) - (q11 + q22)));
    result.pitch = toDegrees(qd2 > 1.0 ? kPi / 2.0 : (qd2 < -1.0 ? -kPi / 2.0 : std::asin(qd2)));
    result.roll = toDegrees(std::atan2(2.0 * (q12 + q03), (q22 + q33) - (q00 + q11)));
    return result;
}

app::Placement placementForReference(const AssemblyJointReference& reference)
{
    return reference.connectorPlacement.value_or(identityPlacement());
}

bool samePitchAndRoll(const AssemblyJointReference& sliderReference,
                      const AssemblyJointReference& targetReference)
{
    const YawPitchRoll slider = yawPitchRollForPlacement(placementForReference(sliderReference));
    const YawPitchRoll target = yawPitchRollForPlacement(placementForReference(targetReference));
    return std::abs(slider.pitch - target.pitch) < kFreeCadPrecisionConfusion
        && std::abs(slider.roll - target.roll) < kFreeCadPrecisionConfusion;
}

bool sameReferencedPart(const AssemblyJointReference& left, const AssemblyJointReference& right)
{
    return !left.object.empty() && left.object == right.object;
}

int slidingPartIndex(const AssemblySolveRequest& request, const JointConstraint& target)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::slidingPartIndex(), scans "getJoints()" for "JointType::Slider", matches
    // moving parts, then checks whether the Slider JCS and target JCS "pitch and roll are the
    // same." CAD Core limits this to the current AssemblySolveRequest.
    int slidingFound = 0;
    for (const JointConstraint& slider : request.joints) {
        if (slider.jointType != "Slider") {
            continue;
        }

        int found = 0;
        const AssemblyJointReference* sliderReference = nullptr;
        const AssemblyJointReference* targetReference = nullptr;
        if (sameReferencedPart(slider.reference1, target.reference1)
            || sameReferencedPart(slider.reference1, target.reference2)) {
            found = sameReferencedPart(slider.reference1, target.reference1) ? 1 : 2;
            sliderReference = &slider.reference1;
            targetReference = found == 1 ? &target.reference1 : &target.reference2;
        }
        else if (sameReferencedPart(slider.reference2, target.reference1)
                 || sameReferencedPart(slider.reference2, target.reference2)) {
            found = sameReferencedPart(slider.reference2, target.reference1) ? 1 : 2;
            sliderReference = &slider.reference2;
            targetReference = found == 1 ? &target.reference1 : &target.reference2;
        }

        if (found != 0 && sliderReference != nullptr && targetReference != nullptr
            && samePitchAndRoll(*sliderReference, *targetReference)) {
            slidingFound = found;
        }
    }
    return slidingFound;
}

bool requiresSlidingSide(const JointConstraint& joint)
{
    return joint.jointType == "Screw" || joint.jointType == "RackPinion";
}

void swapJointConstraintJcs(JointConstraint& joint)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    // ::swapJCS(), swaps "Placement1"/"Placement2" and "Reference1"/"Reference2". CAD Core swaps
    // only the in-memory solver DTO so the frontend DocumentObject graph remains unchanged.
    std::swap(joint.reference1, joint.reference2);
    joint.jcsSwappedForSolver = true;
}

std::string elementKindName(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_VERTEX:
            return "Vertex";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_FACE:
            return "Face";
        default:
            return {};
    }
}

std::string edgePrimitiveName(const TopoDS_Shape& shape)
{
    BRepAdaptor_Curve curve(TopoDS::Edge(shape));
    switch (curve.GetType()) {
        case GeomAbs_Line:
            return "line";
        case GeomAbs_Circle:
            return "circle";
        default:
            return "curve";
    }
}

std::string facePrimitiveName(const TopoDS_Shape& shape)
{
    BRepAdaptor_Surface surface(TopoDS::Face(shape));
    switch (surface.GetType()) {
        case GeomAbs_Plane:
            return "plane";
        case GeomAbs_Cylinder:
            return "cylinder";
        case GeomAbs_Sphere:
            return "sphere";
        case GeomAbs_Cone:
            return "cone";
        case GeomAbs_Torus:
            return "torus";
        default:
            return "surface";
    }
}

std::optional<TopoDS_Shape> referencedShape(const AssemblyJointReference& reference,
                                            const runtime::ComputeContext& context)
{
    const auto shapeIt = context.shapes.find(reference.object);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }
    const TopoDS_Shape& shape = shapeIt->second.shape;
    if (shape.IsNull()) {
        return std::nullopt;
    }
    if (!reference.subnames.empty()) {
        return part::subshapeByName(shape, reference.subnames.front());
    }
    if (shape.ShapeType() == TopAbs_VERTEX || shape.ShapeType() == TopAbs_EDGE
        || shape.ShapeType() == TopAbs_FACE) {
        return shape;
    }
    return std::nullopt;
}

void classifyDistanceReference(AssemblyJointReference& reference, const runtime::ComputeContext& context)
{
    const auto shape = referencedShape(reference, context);
    if (!shape || shape->IsNull()) {
        return;
    }
    reference.elementKind = elementKindName(shape->ShapeType());
    if (shape->ShapeType() == TopAbs_VERTEX) {
        reference.primitive = "point";
    }
    else if (shape->ShapeType() == TopAbs_EDGE) {
        reference.primitive = edgePrimitiveName(*shape);
    }
    else if (shape->ShapeType() == TopAbs_FACE) {
        reference.primitive = facePrimitiveName(*shape);
    }
}

bool isReference(const AssemblyJointReference& reference,
                 const std::string& elementKind,
                 const std::string& primitive)
{
    return reference.elementKind == elementKind && reference.primitive == primitive;
}

void classifyDistanceType(JointConstraint& joint, const runtime::ComputeContext& context)
{
    if (joint.jointType != "Distance") {
        return;
    }

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    // ::getDistanceType(), reads "Reference1"/"Reference2" element type, checks "GeomAbs_Line" /
    // "GeomAbs_Plane", and calls "swapJCS(joint)" so face or edge reference order matches the
    // solver-side DistanceType. CAD Core mutates only this request-local JointConstraint.
    classifyDistanceReference(joint.reference1, context);
    classifyDistanceReference(joint.reference2, context);

    if (isReference(joint.reference1, "Vertex", "point")
        && isReference(joint.reference2, "Vertex", "point")) {
        joint.distanceType = "PointPoint";
        return;
    }
    if (isReference(joint.reference1, "Edge", "line")
        && isReference(joint.reference2, "Edge", "line")) {
        joint.distanceType = "LineLine";
        return;
    }
    if (isReference(joint.reference1, "Face", "plane")
        && isReference(joint.reference2, "Face", "plane")) {
        joint.distanceType = "PlanePlane";
        return;
    }
    if (isReference(joint.reference1, "Vertex", "point")
        && isReference(joint.reference2, "Face", "plane")) {
        swapJointConstraintJcs(joint);
        joint.distanceType = "PointPlane";
        return;
    }
    if (isReference(joint.reference1, "Face", "plane")
        && isReference(joint.reference2, "Vertex", "point")) {
        joint.distanceType = "PointPlane";
        return;
    }
    if (isReference(joint.reference1, "Edge", "line")
        && isReference(joint.reference2, "Face", "plane")) {
        swapJointConstraintJcs(joint);
        joint.distanceType = "LinePlane";
        return;
    }
    if (isReference(joint.reference1, "Face", "plane")
        && isReference(joint.reference2, "Edge", "line")) {
        joint.distanceType = "LinePlane";
        return;
    }
    if (isReference(joint.reference1, "Vertex", "point")
        && isReference(joint.reference2, "Edge", "line")) {
        swapJointConstraintJcs(joint);
        joint.distanceType = "PointLine";
        return;
    }
    if (isReference(joint.reference1, "Edge", "line")
        && isReference(joint.reference2, "Vertex", "point")) {
        joint.distanceType = "PointLine";
    }
}

void resolveDistanceJointMapping(JointConstraint& joint)
{
    if (joint.jointType != "Distance" || !joint.distanceType) {
        return;
    }

    const double distance = joint.distance.value_or(0.0);
    joint.solverJointClass.reset();
    joint.distanceIJ.reset();
    joint.offset.reset();

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::makeMbdJointDistance(), switches on "DistanceType type = getDistanceType(joint)" and
    // maps basic point / line / plane cases to ASMT joint classes with "distanceIJ" or "offset".
    if (*joint.distanceType == "PointPoint") {
        if (distance < kFreeCadPrecisionConfusion) {
            joint.solverJointClass = "ASMTSphericalJoint";
            return;
        }
        joint.solverJointClass = "ASMTSphSphJoint";
        joint.distanceIJ = distance;
        return;
    }
    if (*joint.distanceType == "LineLine") {
        joint.solverJointClass = "ASMTRevCylJoint";
        joint.distanceIJ = distance;
        return;
    }
    if (*joint.distanceType == "PointLine") {
        joint.solverJointClass = "ASMTCylSphJoint";
        joint.distanceIJ = distance;
        return;
    }
    if (*joint.distanceType == "PlanePlane") {
        joint.solverJointClass = "ASMTPlanarJoint";
        joint.offset = distance;
        return;
    }
    if (*joint.distanceType == "PointPlane") {
        joint.solverJointClass = "ASMTPointInPlaneJoint";
        joint.offset = distance;
        return;
    }
    if (*joint.distanceType == "LinePlane") {
        joint.solverJointClass = "ASMTLineInPlaneJoint";
        joint.offset = distance;
    }
}

void applyScrewRackPinionSlidingPrecondition(AssemblySolveRequest& request)
{
    for (JointConstraint& joint : request.joints) {
        if (!requiresSlidingSide(joint)) {
            continue;
        }
        joint.slidingPartIndex = slidingPartIndex(request, joint);
        joint.jcsSwappedForSolver = false;
        if (*joint.slidingPartIndex == 2) {
            swapJointConstraintJcs(joint);
        }
    }
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

void applyRackPinionMarkerRewrite(AssemblySolveRequest& request)
{
    for (JointConstraint& joint : request.joints) {
        if (joint.jointType != "RackPinion") {
            continue;
        }
        joint.pitchRadius = joint.distance.value_or(0.0);
        if (!joint.slidingPartIndex || *joint.slidingPartIndex == 0 || !joint.reference1.markerPlacement
            || !joint.reference2.markerPlacement) {
            continue;
        }

        const auto rackPart = partByName(request, joint.reference1.object);
        const auto pinionPart = partByName(request, joint.reference2.object);
        if (!rackPart || !pinionPart) {
            continue;
        }

        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        // ::AssemblyObject::getRackPinionMarkers(), after optional "swapJCS(joint)" it computes
        // pinion placement relative to rack, then adjusts rack placement rotation so pinion Z
        // stays rack Z and rack X follows the Slider Z axis before handleOneSideOfJoint creates
        // the Ondsel marker. CAD Core mutates only this request-local DTO marker placement.
        app::Placement rackMarker = *joint.reference1.markerPlacement;
        const app::Placement pinionGlobalMarker = composePlacement(
            pinionPart->placement,
            *joint.reference2.markerPlacement
        );
        const app::Placement pinionRelativeToRack = composePlacement(
            inversePlacement(rackPart->placement),
            pinionGlobalMarker
        );

        const Vector3 currentZAxis = rotateVector(pinionRelativeToRack.rotation, {0.0, 0.0, 1.0});
        const Vector3 currentXAxis = rotateVector(pinionRelativeToRack.rotation, {1.0, 0.0, 0.0});
        const Vector3 targetXAxis = rotateVector(rackMarker.rotation, {0.0, 0.0, 1.0});
        double yawAdjustment = angleBetween(currentXAxis, targetXAxis);
        const Vector3 crossProduct = cross(currentXAxis, targetXAxis);
        if (dot(currentZAxis, crossProduct) < 0.0) {
            yawAdjustment = -yawAdjustment;
        }

        const auto yawRotation = rotationAroundAxis(currentZAxis, yawAdjustment);
        rackMarker.rotation = multiplyRotations(pinionRelativeToRack.rotation, yawRotation);
        joint.reference1.markerPlacement = rackMarker;
        joint.rackPinionMarkerRewrite = RackPinionMarkerRewrite {
            true,
            joint.reference1.object,
            joint.reference2.object,
            yawAdjustment,
            rackMarker,
        };
    }
}

void resolveJointMarkerPlacement(AssemblyJointReference& reference,
                                 const runtime::ComputeContext& context,
                                 const std::string& referenceProperty,
                                 bool connectorDefaulted)
{
    reference.markerPlacement.reset();
    reference.markerResolutionConnectorDefaulted = connectorDefaulted;

    if (reference.object.empty()) {
        reference.markerResolutionStatus = "missing_reference";
        reference.markerResolutionFrame = "unresolved";
        reference.markerResolutionDiagnostic = referenceProperty
            + " is missing; FreeCAD handleOneSideOfJoint() would reject this joint side";
        return;
    }

    if (documentObjectByName(context, reference.object) == nullptr) {
        reference.markerResolutionStatus = "missing_reference_object";
        reference.markerResolutionFrame = "unresolved";
        reference.markerResolutionDiagnostic = referenceProperty + " points to missing object "
            + reference.object + "; FreeCAD handleOneSideOfJoint() requires a linked object";
        return;
    }

    if (reference.subnames.empty()) {
        reference.markerResolutionStatus = "resolved_object_level_baseline";
        reference.markerResolutionFrame = "part_local_object_level";
        reference.markerResolutionDiagnostic =
            "Object-level reference uses PlacementN as request-local marker placement baseline";
        reference.markerResolutionUsedObjectLevelBaseline = true;
        reference.markerPlacement = reference.connectorPlacement.value_or(identityPlacement());
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::handleOneSideOfJoint(), uses "obj_global_plc =
    // getGlobalPlacement(nullptr, ref)" then "part_global_plc.inverse() * plc" and finally
    // applies "data.offsetPlc" before marker creation. CAD Core does not yet carry the subshape
    // placement / containing-part offset evidence required for that conversion, so it withholds
    // markerPlacement instead of treating the connector PlacementN as a resolved subshape marker.
    reference.markerResolutionStatus = "requires_subshape_handle_one_side_evidence";
    reference.markerResolutionFrame = "unresolved_subshape_requires_part_local_marker";
    reference.markerResolutionDiagnostic =
        "Subshape reference requires FreeCAD handleOneSideOfJoint() object-global to part-local "
        "marker resolution; cad-core lacks subshape placement, containing-part, or offsetPlc evidence";
    reference.markerResolutionRequiresHandleOneSide = true;
}

AssemblyJointReference jointReference(const app::DocumentObject& joint,
                                      const runtime::ComputeContext& context,
                                      const std::string& referenceProperty,
                                      const std::string& placementProperty)
{
    AssemblyJointReference reference;
    if (const auto link = app::readLink(joint, referenceProperty)) {
        reference.object = link->object;
        reference.subnames = link->subnames;
    }
    const auto connectorPlacement = app::readPlacement(joint, placementProperty);
    reference.connectorPlacement = connectorPlacement.value_or(identityPlacement());
    resolveJointMarkerPlacement(reference, context, referenceProperty, !connectorPlacement.has_value());
    return reference;
}

bool isGroundedObject(const AssemblySolveRequest& request, const std::string& object)
{
    const auto part = partByName(request, object);
    return part && part->grounded;
}

bool isObjectLevelReference(const AssemblyJointReference& reference)
{
    return reference.object.empty() || reference.subnames.empty();
}

app::Placement freeCadObjectLevelDistanceWriteback(const AssemblySolveRequest& request,
                                                   const AssemblyPartRef& sourcePart,
                                                   const app::Placement& solved)
{
    for (const JointConstraint& joint : request.joints) {
        if (joint.jointType != "Distance" || !isObjectLevelReference(joint.reference1)
            || !isObjectLevelReference(joint.reference2)) {
            continue;
        }
        const auto reference1 = partByName(request, joint.reference1.object);
        const auto reference2 = partByName(request, joint.reference2.object);
        if (!reference1 || !reference2) {
            continue;
        }

        if (reference1->grounded && reference2->grounded
            && (sourcePart.object == reference1->object || sourcePart.object == reference2->object)) {
            app::Placement adjusted = sourcePart.placement;
            const double dx = reference2->placement.base.at(0) - reference1->placement.base.at(0);
            const double dy = reference2->placement.base.at(1) - reference1->placement.base.at(1);
            const double planarDistance = std::sqrt(dx * dx + dy * dy);
            if (planarDistance > kPlacementTolerance) {
                const double ratio = std::clamp(joint.distance.value_or(0.0) / planarDistance, -1.0, 1.0);
                const double angle = std::asin(ratio);
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
                // ::AssemblyObject::solve(), calls "setNewPlacements()" without the drag-only
                // validateNewPlacements() gate; native over-constrained object-level Distance
                // keeps grounded bases and writes the shared rotation returned by runPreDrag().
                adjusted.rotation = {0.0, std::sin(angle / 2.0), 0.0, std::cos(angle / 2.0)};
            }
            return adjusted;
        }

        if (sourcePart.grounded || joint.reference2.object != sourcePart.object) {
            continue;
        }

        app::Placement adjusted = sourcePart.placement;
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
        // ::getJointCurrentValue(), computes the Distance scalar in the JCS frame and signs it
        // from "plc3.getPosition().z"; object-level Distance writeback from native solve keeps
        // the moving AssemblyLink's X/Y placement and offsets the JCS Z from Reference1.
        adjusted.base.at(2) = reference1->placement.base.at(2) + joint.distance.value_or(0.0);
        return adjusted;
    }

    return solved;
}

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

std::shared_ptr<MbD::ASMTJoint> makeOndselDistanceJoint(const JointConstraint& joint)
{
    const double distance = joint.distance.value_or(0.0);
    const std::string solverJointClass = joint.solverJointClass.value_or("");
    if (solverJointClass == "ASMTSphericalJoint") {
        return MbD::ASMTSphericalJoint::With();
    }
    if (solverJointClass == "ASMTSphSphJoint") {
        auto distanceJoint = MbD::ASMTSphSphJoint::With();
        distanceJoint->distanceIJ = joint.distanceIJ.value_or(distance);
        return distanceJoint;
    }
    if (solverJointClass == "ASMTRevCylJoint") {
        auto distanceJoint = MbD::ASMTRevCylJoint::With();
        distanceJoint->distanceIJ = joint.distanceIJ.value_or(distance);
        return distanceJoint;
    }
    if (solverJointClass == "ASMTCylSphJoint") {
        auto distanceJoint = MbD::ASMTCylSphJoint::With();
        distanceJoint->distanceIJ = joint.distanceIJ.value_or(distance);
        return distanceJoint;
    }
    if (solverJointClass == "ASMTPlanarJoint") {
        auto distanceJoint = MbD::ASMTPlanarJoint::With();
        distanceJoint->offset = joint.offset.value_or(distance);
        return distanceJoint;
    }
    if (solverJointClass == "ASMTPointInPlaneJoint") {
        auto distanceJoint = MbD::ASMTPointInPlaneJoint::With();
        distanceJoint->offset = joint.offset.value_or(distance);
        return distanceJoint;
    }
    if (solverJointClass == "ASMTLineInPlaneJoint") {
        auto distanceJoint = MbD::ASMTLineInPlaneJoint::With();
        distanceJoint->offset = joint.offset.value_or(distance);
        return distanceJoint;
    }

    auto distanceJoint = MbD::ASMTSphSphJoint::With();
    distanceJoint->distanceIJ = distance;
    return distanceJoint;
}

std::shared_ptr<MbD::ASMTJoint> makeOndselJointOfType(const JointConstraint& joint)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), maps "Fixed" to
    // ASMTFixedJoint, "Revolute" to ASMTRevoluteJoint, "Cylindrical" to
    // ASMTCylindricalJoint via "case JointType::Cylindrical: return
    // CREATE<ASMTCylindricalJoint>::With();", "Slider" to ASMTTranslationalJoint,
    // "Ball" to ASMTSphericalJoint and "Angle" to ASMTAngleJoint with "theIzJz".
    // CAD Core落点: real Ondsel adapter joint DTO conversion.
    if (joint.jointType == "Fixed") {
        return MbD::ASMTFixedJoint::With();
    }
    if (joint.jointType == "Revolute") {
        return MbD::ASMTRevoluteJoint::With();
    }
    if (joint.jointType == "Cylindrical") {
        return MbD::ASMTCylindricalJoint::With();
    }
    if (joint.jointType == "Slider") {
        return MbD::ASMTTranslationalJoint::With();
    }
    if (joint.jointType == "Ball") {
        return MbD::ASMTSphericalJoint::With();
    }
    if (joint.jointType == "Distance") {
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        // ::makeMbdJointDistance(), "PointPoint" may become "ASMTSphericalJoint" while line and
        // plane DistanceTypes map to distinct ASMT joint classes with "distanceIJ" / "offset".
        return makeOndselDistanceJoint(joint);
    }
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), "case JointType::Gears" creates
    // ASMTGearJoint with "radiusI = getJointDistance(joint)" and
    // "radiusJ = getJointDistance2(joint)"; "case JointType::Belt" uses
    // "radiusJ = -getJointDistance2(joint)".
    if (joint.jointType == "Gears" || joint.jointType == "Belt") {
        auto gearJoint = MbD::ASMTGearJoint::With();
        gearJoint->radiusI = joint.distance.value_or(0.0);
        gearJoint->radiusJ = joint.jointType == "Belt" ? -joint.distance2.value_or(0.0)
                                                        : joint.distance2.value_or(0.0);
        return gearJoint;
    }
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), "case JointType::RackPinion" creates
    // ASMTRackPinionJoint and sets "mbdJoint->pitchRadius = getJointDistance(joint)".
    if (joint.jointType == "RackPinion") {
        auto rackPinionJoint = MbD::ASMTRackPinionJoint::With();
        rackPinionJoint->pitchRadius = joint.pitchRadius.value_or(joint.distance.value_or(0.0));
        return rackPinionJoint;
    }
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), "case JointType::Screw" requires a non-zero
    // "slidingPartIndex(joint)", optionally calls "swapJCS(joint)", then creates
    // ASMTScrewJoint and sets "mbdJoint->pitch = getJointDistance(joint)".
    if (joint.jointType == "Screw") {
        auto screwJoint = MbD::ASMTScrewJoint::With();
        screwJoint->pitch = joint.pitch.value_or(joint.distance.value_or(0.0));
        return screwJoint;
    }
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::makeMbdJointOfType(), direct case JointType::Parallel returns
    // ASMTParallelAxesJoint and JointType::Perpendicular returns ASMTPerpendicularJoint.
    if (joint.jointType == "Parallel") {
        return MbD::ASMTParallelAxesJoint::With();
    }
    if (joint.jointType == "Perpendicular") {
        return MbD::ASMTPerpendicularJoint::With();
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

bool isConvertibleOndselJointInRequest(const JointConstraint& joint)
{
    if (!isSupportedOndselJointType(joint.jointType)) {
        return false;
    }
    if (joint.jointType == "RackPinion") {
        return joint.slidingPartIndex && *joint.slidingPartIndex != 0 && joint.rackPinionMarkerRewrite
            && joint.rackPinionMarkerRewrite->applied;
    }
    if (joint.jointType == "Screw") {
        return joint.slidingPartIndex && *joint.slidingPartIndex != 0;
    }
    return true;
}

std::string unsupportedJointMessage(const UnsupportedAssemblyJoint& unsupported)
{
    if (unsupported.jointType == "RackPinion") {
        return "Ondsel solver adapter cannot convert RackPinion without a matching Slider precondition";
    }
    if (unsupported.jointType == "Screw") {
        return "Ondsel solver adapter cannot convert Screw without a matching Slider precondition";
    }
    return "Ondsel solver adapter does not yet convert JointType " + unsupported.jointType;
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
    partI->addMarker(makeOndselMarker(markerI, joint.reference1.markerPlacement.value_or(identity)));
    partJ->addMarker(makeOndselMarker(markerJ, joint.reference2.markerPlacement.value_or(identity)));

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
        if (!isConvertibleOndselJointInRequest(joint)) {
            result.unsupportedJoints.push_back(UnsupportedAssemblyJoint {
                joint.object,
                joint.jointType,
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
                unsupportedJointMessage(unsupported),
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
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        // ::AssemblyObject::getGroundedParts(), adds "Origin.getValue()" to the grounded set even
        // when no GroundedJoint exists. In the request graph that origin is not a movable part, so
        // CAD Core preserves the native observed result as a solved, no-writeback adapter result.
        result.solveState = "solved";
        result.status = "solved";
        result.mode = "real_ondsel_solver";
        return result;
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
            const app::Placement solved = freeCadObjectLevelDistanceWriteback(
                request,
                sourcePart,
                placementFromOndselPart(mbdParts.at(sourcePart.object))
            );
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
    return result;
}

}  // namespace

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
        constraint.reference1 = jointReference(*joint, context, "Reference1", "Placement1");
        constraint.reference2 = jointReference(*joint, context, "Reference2", "Placement2");
        constraint.suppressed = app::readBool(*joint, "Suppressed").value_or(false);
        if (constraint.jointType == "Distance" || constraint.jointType == "Slider"
            || constraint.jointType == "Gears" || constraint.jointType == "Belt"
            || constraint.jointType == "RackPinion" || constraint.jointType == "Screw") {
            constraint.distance = app::readNumber(*joint, "Distance").value_or(0.0);
        }
        classifyDistanceType(constraint, context);
        if (constraint.jointType == "Screw") {
            constraint.pitch = constraint.distance.value_or(0.0);
        }
        if (constraint.jointType == "RackPinion") {
            constraint.pitchRadius = constraint.distance.value_or(0.0);
        }
        if (constraint.jointType == "Gears" || constraint.jointType == "Belt") {
            constraint.distance2 = app::readNumber(*joint, "Distance2").value_or(0.0);
        }
        if (constraint.jointType == "Angle") {
            constraint.angle = app::readNumber(*joint, "Angle").value_or(0.0);
        }
        resolveDistanceJointMapping(constraint);
        request.joints.push_back(std::move(constraint));
    }

    applyScrewRackPinionSlidingPrecondition(request);
    applyRackPinionMarkerRewrite(request);
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

AssemblySolveResult solveAssemblyWithOndselAdapter(const AssemblySolveRequest& request)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::solve(), key order "fixGroundedParts()" ->
    // "jointParts(joints)" -> "mbdAssembly->runPreDrag()" -> "setNewPlacements()"; normal solve
    // does not call validateNewPlacements(), and getGroundedParts() includes the assembly Origin.
    return solveAssemblyWithRealOndselAdapter(request);
}

bool hasOndselSolverAdapter()
{
    return true;
}

bool isSupportedOndselJointType(const std::string& jointType)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/
    // AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType(), maps "Fixed", "Revolute",
    // "Cylindrical", "Slider", "Ball", "Distance", direct "Parallel" / "Perpendicular",
    // "Angle", the Gears / Belt ASMTGearJoint subset, RackPinion after getRackPinionMarkers()
    // has rewritten the rack marker, and Screw after the "slidingPartIndex(joint)" precondition
    // succeeds.
    static const std::set<std::string> supported = {
        "Fixed",
        "Revolute",
        "Cylindrical",
        "Slider",
        "Ball",
        "Distance",
        "Parallel",
        "Perpendicular",
        "Angle",
        "Gears",
        "Belt",
        "RackPinion",
        "Screw",
    };
    return supported.count(jointType) != 0U;
}

}  // namespace cad_core::assembly
