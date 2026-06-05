#pragma once

// Part-layer ShapeFix history helper aligned with FreeCAD Part/App TopoShape
// ShapeFix consumption.
#include <TopoDS_Shape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <Standard_Handle.hxx>

#include <memory>

class ShapeFix_Shape;
class ShapeBuild_ReShape;

namespace cad_core::part
{

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
// ::TopoShape::fix(), calls "ShapeFix_Shape fixThis(this->_Shape)" and then
// "makeShapeWithElementMap(fixThis.Shape(), MapperHistory(fixThis), {*this})" because
// "ShapeFix_Shape may delete (e.g. small edges) or modify the input shape".
class ShapeFixHistory
{
public:
    explicit ShapeFixHistory(const TopoDS_Shape& shape);
    ~ShapeFixHistory();

    ShapeFixHistory(const ShapeFixHistory&) = delete;
    ShapeFixHistory& operator=(const ShapeFixHistory&) = delete;
    ShapeFixHistory(ShapeFixHistory&&) noexcept;
    ShapeFixHistory& operator=(ShapeFixHistory&&) noexcept;

    void setPrecision(double precision);
    bool perform();
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ShapeFix/ShapeFix_WirePyImp.cpp
    // ::ShapeFix_WirePy::fixSmall(), calls "FixSmall(Base::asBoolean(lock), prec)".
    // This small-edge producer writes into the same ShapeBuild_ReShape context consumed by
    // MapperHistory.
    bool removeSmallEdges(double tolerance);
    const TopoDS_Shape& Shape() const;
    const TopTools_ListOfShape& Modified(const TopoDS_Shape& shape) const;
    const TopTools_ListOfShape& Generated(const TopoDS_Shape& shape) const;
    bool IsDeleted(const TopoDS_Shape& shape) const;

private:
    std::unique_ptr<ShapeFix_Shape> fix_;
    Handle(ShapeBuild_ReShape) context_;
    TopoDS_Shape shape_;
    mutable TopTools_ListOfShape empty_;
};

}  // namespace cad_core::part
