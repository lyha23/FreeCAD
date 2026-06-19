# C3M4 Part Workbench Surface Filling 收口方案

## 目标

实现 FreeCAD Part 后端 filled face / filling surface 能力：`Part.makeFilledFace()`、`TopoShape::makeElementFilledFace()`、`BRepFill_Filling` 参数、oracle fixture 和 capability 发布。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`：解析 `shapes`、`surface`、`supports`、`orders`、`degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments`、`op`，随后调用 `TopoShape(...).makeElementFilledFace(shapes, params, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementFilledFace()`：使用 `BRepFill_Filling`，并包含 wire connection fix，避免 OCCT `BRepFill_Filling.cxx WireFromList()` 崩溃。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.h::BRepFillingParams`：承接 filling 参数。

## 首批范围

- closed boundary edges / wires -> face。
- 参数 diagnostics：空 boundary、非 edge/wire、非法 degree / tolerance。
- support/order 先做 source/oracle 矩阵，能采集 expected 再纳入 S1。

## 非目标

- 不迁移 GUI command。
- 不把 Filling 和 GeomPlate 混成一个实现文件。
- 不绕过 `BRepFill_Filling` 改用普通 FaceMaker 伪装支持。
