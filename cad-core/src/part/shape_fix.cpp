#include "cad_core/part/shape_fix.h"

#include <BRepTools_History.hxx>
#include <ShapeBuild_ReShape.hxx>
#include <ShapeFix.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wire.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>

namespace cad_core::part
{

ShapeFixHistory::ShapeFixHistory(const TopoDS_Shape& shape)
    : fix_(std::make_unique<ShapeFix_Shape>(shape))
    , context_(new ShapeBuild_ReShape())
    , shape_(shape)
{
    fix_->SetContext(context_);
}

ShapeFixHistory::~ShapeFixHistory() = default;
ShapeFixHistory::ShapeFixHistory(ShapeFixHistory&&) noexcept = default;
ShapeFixHistory& ShapeFixHistory::operator=(ShapeFixHistory&&) noexcept = default;

void ShapeFixHistory::setPrecision(double precision)
{
    if (fix_) {
        fix_->SetPrecision(precision);
    }
}

bool ShapeFixHistory::perform()
{
    if (!fix_) {
        return false;
    }
    if (context_.IsNull()) {
        context_ = new ShapeBuild_ReShape();
        fix_->SetContext(context_);
    }
    const bool performed = fix_->Perform();
    shape_ = fix_->Shape();
    return performed;
}

bool ShapeFixHistory::removeSmallEdges(double tolerance)
{
    if (shape_.IsNull()) {
        return false;
    }
    if (context_.IsNull()) {
        context_ = new ShapeBuild_ReShape();
    }
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ShapeFix/ShapeFix_WirePyImp.cpp
    // ::ShapeFix_WirePy::fixSmall(), calls "FixSmall(Base::asBoolean(lock), prec)";
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::ShapeFixModule::removeSmallEdges(), calls "ShapeFix::RemoveSmallEdges(sh, tol, reshape)".
    // Both producers write into ShapeBuild_ReShape; local FreeCAD
    // tests/src/Mod/Part/App/TopoShapeExpansion.cpp proves MapperHistory(ShapeFix_Root&) and
    // MapperHistory(ShapeBuild_ReShape) share "Context()->History()".
    if (shape_.ShapeType() == TopAbs_WIRE) {
        ShapeFix_Wire wireFix;
        wireFix.SetContext(context_);
        wireFix.Load(TopoDS::Wire(shape_));
        wireFix.SetPrecision(tolerance);
        wireFix.ModifyTopologyMode() = true;
        wireFix.ModifyGeometryMode() = true;
        wireFix.FixReorder();
        wireFix.FixSmall(false, tolerance);
        shape_ = wireFix.Wire();
        return !shape_.IsNull();
    }

    TopoDS_Shape input = shape_;
    shape_ = ShapeFix::RemoveSmallEdges(input, tolerance, context_);
    return !shape_.IsNull();
}

const TopoDS_Shape& ShapeFixHistory::Shape() const
{
    return shape_;
}

const TopTools_ListOfShape& ShapeFixHistory::Modified(const TopoDS_Shape& shape) const
{
    if (!context_.IsNull() && !context_->History().IsNull()) {
        return context_->History()->Modified(shape);
    }
    return empty_;
}

const TopTools_ListOfShape& ShapeFixHistory::Generated(const TopoDS_Shape& shape) const
{
    if (!context_.IsNull() && !context_->History().IsNull()) {
        return context_->History()->Generated(shape);
    }
    return empty_;
}

bool ShapeFixHistory::IsDeleted(const TopoDS_Shape& shape) const
{
    return !context_.IsNull() && !context_->History().IsNull()
        && context_->History()->IsRemoved(shape);
}

}  // namespace cad_core::part
