#include "cad_core/geometry/placement.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Vec.hxx>

namespace cad_core::geometry {

gp_Trsf placementFromComponents(const std::array<double, 3>& base, const std::array<double, 4>& rotationComponents)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::GeoFeature(),
    // adds App::PropertyPlacement named "Placement"; globalPlacement() returns that property value
    // when no GeoFeatureGroup parent contributes another transform.
    gp_Trsf rotation;
    rotation.SetRotation(gp_Quaternion(rotationComponents.at(0),
                                       rotationComponents.at(1),
                                       rotationComponents.at(2),
                                       rotationComponents.at(3)));
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(base.at(0), base.at(1), base.at(2)));
    return translation * rotation;
}

TopoDS_Shape transformShape(const TopoDS_Shape& shape, const gp_Trsf& transform)
{
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

}  // namespace cad_core::geometry
