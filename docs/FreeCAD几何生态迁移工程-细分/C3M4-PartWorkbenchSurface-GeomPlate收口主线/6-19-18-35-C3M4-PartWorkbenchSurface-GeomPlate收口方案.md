# C3M4 Part Workbench Surface GeomPlate 收口方案

## 目标

实现 cad-core 后端必须具备的 GeomPlate surface 能力：CurveConstraint / PointConstraint / BuildPlateSurface / PlateSurface 的 source-backed geometry helper、oracle fixture、diagnostics 和 capability 发布。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:138` 的 `Tools::makeSurface()` 用 `GeomPlate_BuildPlateSurface` 添加 G0/G1 curve constraint 和 point constraint；`theTol` 在当前实现中被 `(void)` 丢弃，实际常量是 `Degree=3`、`Tol2d=1e-5`、`Tol3d=1e-4`、`TolAng=0.01`、`TolCurv=0.1`、`MaxSegments=10000`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:231` 到 `:251`：`Perform()` 后若 `IsDone()` 为假返回 null surface；成功时用 `G0Error()`、`Disc2dContour(4)`、`Disc3dContour(4, 0)` 和 `GeomPlate_MakeApprox` 生成 approximated surface。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp:90` 到 `:153`：Python builder 支持 `Surface/Degree/NbPtsOnCur/NbIter/Tol2d/Tol3d/TolAng/TolCurv/Anisotropy`，可加载 initial surface。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp:212` 到 `:233`：`Add()` 只接收 `PointConstraint` 或 `CurveConstraint`，且复制 constraint；`:260` 到 `:305` 提供 `Perform()`、`IsDone()`、`Surface()`；`:507` 到 `:552` 暴露 G0/G1/G2 error。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp:52` 到 `:79` 与 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp:47` 到 `:72`：定义首批 DTO 所需的 boundary curve、point、order、sample count 和 tolerance 字段。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp:57` 到 `:165`：`Part.PlateSurface` 是 `GeomPlateSurface` wrapper；points 分支添加 point constraint，curves 分支仍是 `TODO`；`:175` 到 `:254` 的 `makeApprox()` 返回 `BSplineSurface`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp:5996` 到 `:6051` 与 `:6991` 到 `:6993`：`GeomPlateSurface` 是 geometry wrapper，不可持久化，`makeFromSurface()` 将 `GeomPlate_Surface` 包装成 `GeomPlateSurface`。

## 首批范围

- Geometry-level helper DTO：`PartGeomPlateSurfaceDTO`，首批只表达 request-local geometry helper，不注册或伪造 FreeCAD `DocumentObject`。
- Constraint DTO：source-backed 3D curve constraints 与 3D point constraints；首批 success fixture 至少覆盖 G0 curve constraints + point constraint 组合。G1 curve-on-surface、2D projected curve、initial surface 和 `Part.PlateSurface` curves wrapper 不进第一批发布。
- Build/result metadata：builder params、constraint counts、`isDone`、surface kind、G0/G1/G2 errors、approximation params/status、source evidence。
- Oracle route：FreeCADCmd 中用 `Part.GeomPlate.BuildPlateSurface()` 创建 constraints、`perform()`、`surface()`，再通过 `Part.PlateSurface.makeApprox()` 或等价 surface-to-shape 路径采集可转 face 的 shape expected。不能稳定转 face 的 case 只保留 geometry result + diagnostics。

## 非目标

- 不伪造 `Part::GeomPlate` DocumentObject。
- 不迁移 Python wrapper 的所有 inspection methods 作为第一批。
- 不把 GeomPlate 与 Filling 的 BRepFill_Filling 混用成同一 capability。
- 不把 `Part.makeFilledSurface()` / `BRepOffsetAPI_MakeFilling` 当作 GeomPlate oracle。
- 不发布 `Part.PlateSurface(Curves=...)`，因为当前 wrapper curves 分支仍是 TODO。

## S0 输出矩阵

- `矩阵/part_surface_geomplate_scope.tsv`：收口范围、状态和 S1/S2 边界。
- `矩阵/part_surface_geomplate_source_matrix.tsv`：FreeCAD 调用链和源码裁决。
- `矩阵/part_surface_geomplate_dto_matrix.tsv`：首批 DTO 字段、默认值、验证和 deferred 字段。
- `矩阵/part_surface_geomplate_fixture_oracle_matrix.tsv`：S1 required / optional / deferred oracle fixture 形态。
- `矩阵/part_surface_geomplate_diagnostics_matrix.tsv`：diagnostics 与 result metadata 合同。

## S1 实现边界

- 新增 cad-core geometry helper / DTO parser / collector route / focused tests，不改 FreeCAD upstream 源码。
- helper 语义落在 geometry/part surface helper 层，adapter 只做协议转换；不能靠输出修正替代 `GeomPlate_BuildPlateSurface` 主路径。
- S1 可实现默认参数、G0 curve、point、perform success/failure、approx surface metadata 和 required fixtures；G1 curve-on-surface、initial surface、projected 2D curve、PlateSurface curves wrapper 和 capability 发布留给后续矩阵行。
