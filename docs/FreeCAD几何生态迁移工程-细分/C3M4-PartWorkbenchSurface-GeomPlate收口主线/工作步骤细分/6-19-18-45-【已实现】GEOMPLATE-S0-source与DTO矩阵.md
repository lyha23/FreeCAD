# 【已实现】GEOMPLATE-S0 source 与 DTO 矩阵

复核 `Tools::makeSurface()`、BuildPlateSurface、CurveConstraint、PointConstraint、PlateSurface wrapper，设计 geometry DTO、oracle 和 diagnostics。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`7a8f236246`。
- `git log -1 --oneline`：`7a8f236246 发布 Part Filling helper capability`。
- `git -c core.quotepath=false status --short -uall`：干净。

## FreeCAD 源码裁决

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:138` 的 `Tools::makeSurface()` 是 C++ helper source：输入是 transient boundary list；plain 3D curve 变 G0 curve constraint，curve-on-surface 变 G1 curve constraint，`Geom_Point` 变 order-0 point constraint；空输入或 null/错误类型分别进入 construction/type mismatch。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:146` 明确 `theTol` 未参与算法；实际构建常量是 `Degree=3`、`Tol2d=1e-5`、`Tol3d=1e-4`、`TolAng=0.01`、`TolCurv=0.1`、`MaxSegments=10000`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:231` 到 `:251`：先 `Perform()`，`IsDone()==false` 返回 null；成功后取 `Surface()`、`G0Error()` 和 contour samples，走 `GeomPlate_MakeApprox`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp:90` 到 `:153`：`Part.GeomPlate.BuildPlateSurface` 是 Python oracle 主入口，支持 build 参数和 optional initial surface。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp:212` 到 `:305` 和 `:507` 到 `:552`：builder 只添加 CurveConstraint/PointConstraint，提供 perform/isDone/surface 和 G0/G1/G2 error。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp:52` 与 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp:47` 固定首批 constraint DTO 的字段来源：curve boundary/order/nbPts/tolerances，point/order/tolerance 和 optional criteria/2D-on-surface。
- `PlateSurfacePyImp.cpp` 的实时位置是 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`，不在 `src/Mod/Part/App/GeomPlate/` 下。其 curves 分支在 `:157` 到 `:160` 仍是 TODO；`makeApprox()` 在 `:175` 到 `:254` 返回 `BSplineSurface`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp:5996` 到 `:6051` 表明 `GeomPlateSurface` 是 geometry wrapper，持久化和 equality 不可作为能力发布项。

## S0 矩阵结论

- 更新 `矩阵/part_surface_geomplate_scope.tsv`：拆分 source、DTO、build、oracle、diagnostics、publish 和 deferred/non-goal 行。
- 新增 `矩阵/part_surface_geomplate_source_matrix.tsv`：记录 `Tools::makeSurface()`、`BuildPlateSurfacePy`、constraints、`PlateSurfacePy`、`GeomPlateSurface` 与 cad-core Filling 边界。
- 新增 `矩阵/part_surface_geomplate_dto_matrix.tsv`：定义 S1 的 `PartGeomPlateSurfaceDTO`、build params、constraints、approximation、result metadata 和 deferred 字段。
- 新增 `矩阵/part_surface_geomplate_fixture_oracle_matrix.tsv`：S1 required 固定为 curve+point success fixture 和 invalid diagnostics fixture；initial surface、G1 curve-on-surface、PlateSurface curves wrapper 和 direct Tools helper smoke 均不作为第一批发布必需项。
- 新增 `矩阵/part_surface_geomplate_diagnostics_matrix.tsv`：固定 parse/build/approx/result metadata diagnostics，避免 S1 靠 shape 输出倒推语义。
- 更新父级收口方案：明确 GeomPlate 是独立 geometry backend helper，不复用 Filling capability，也不伪造 DocumentObject。

## S1 输入

- 实现 cad-core request-local `PartGeomPlateSurfaceDTO` parser、geometry helper、collector route、required fixtures 和 focused tests。
- required success fixture：四条 source-backed 3D curve G0 constraints + 一个 3D point constraint，默认 `BuildPlateSurface` params，`perform()` 成功后记录 raw surface metadata，并通过 `surface().makeApprox()` 采集可转 face 的 expected。
- required diagnostics fixture：空 constraints、缺失/错误 curve source、错误 point vector、错误 numeric param、unsupported `Part.PlateSurface(Curves=...)`。
- S1 不发布 capability；S2 才更新 CADCore3.0 docs、C API capability metadata 和 adapter tests。

## 非目标

- 不实现 C++ helper、cad-core parser、collector 或 adapter。
- 不采集 FreeCAD oracle，不新增/修改 fixture expected。
- 不发布 capability。
- 不把 GeomPlate 与 `BRepOffsetAPI_MakeFilling` / `Part.makeFilledSurface()` 混成同一能力。
- 不声明 `Part.PlateSurface(Curves=...)`、initial surface、G1 curve-on-surface、2D projected curve 已支持。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py '/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-GeomPlate收口主线/工作步骤细分' --format markdown
git diff --check
```

完成状态：本文件已按完成规则命名为 `6-19-18-45-【已实现】GEOMPLATE-S0-source与DTO矩阵.md`。
