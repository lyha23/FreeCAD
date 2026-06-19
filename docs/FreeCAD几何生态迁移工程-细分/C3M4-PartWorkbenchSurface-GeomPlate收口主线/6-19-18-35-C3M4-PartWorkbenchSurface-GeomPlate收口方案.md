# C3M4 Part Workbench Surface GeomPlate 收口方案

## 目标

实现 cad-core 后端必须具备的 GeomPlate surface 能力：CurveConstraint / PointConstraint / BuildPlateSurface / PlateSurface 的 source-backed geometry helper、oracle fixture、diagnostics 和 capability 发布。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::Tools::makeSurface()`：用 `GeomPlate_BuildPlateSurface` 添加 G0/G1 curve constraint 和 point constraint，`Perform()` 后通过 `GeomPlate_MakeApprox` 生成 approximated surface。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`：Python API 支持 `Init()`、`LoadInitSurface()`、`Add(CurveConstraint/PointConstraint)`、`SetNbBounds()`、`Perform()`、`IsDone()`、`Surface()`、`SurfInit()`、constraint inspection 和 error fields。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp` 与 `PointConstraintPyImp.cpp`：定义曲线约束、点约束、order、G0/G1/G2 criteria 和 projected curve。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`：`GeomPlateSurface` wrapper 和 approximation 路径。

## 首批范围

- Geometry-level DTO：curve constraints + point constraints -> GeomPlate surface。
- Surface export metadata：surface kind、approximation status、errors。
- 能转 face 的 fixture 才进入 shape parity；不能转 face 的保留 geometry result + diagnostics。

## 非目标

- 不伪造 `Part::GeomPlate` DocumentObject。
- 不迁移 Python wrapper 的所有 inspection methods 作为第一批。
- 不把 GeomPlate 与 Filling 的 BRepFill_Filling 混用成同一 capability。
