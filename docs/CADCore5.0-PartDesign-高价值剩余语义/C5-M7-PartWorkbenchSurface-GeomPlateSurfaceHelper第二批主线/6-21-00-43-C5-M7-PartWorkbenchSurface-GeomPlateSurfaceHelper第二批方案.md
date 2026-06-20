# C5-M7 Part Workbench Surface GeomPlateSurface Helper 第二批方案

## 当前基线

- 当前 live 代码已经不是空白起点：`part_workbench.geomplate` capability 已列出 c3m4/c4m1 guard、c5m7 initial/G1 fixtures 和 S3 2D fixtures。
- `cad-core/src/part/part_geomplate.cpp` 当前支持 3D curve G0 constraints、3D point constraints、point custom criteria、`InitialSurface` / `Surface` reference、G1 curve-on-surface source evidence、`Curve2dOnSurface`、`ProjectedCurve2d`、`Point2dOnSurface`、build params、advanced approximation params 和 makeApprox face；curve criteria setter 与 `PlateSurfaceCurves` 由 concrete diagnostics 表达。
- `cad-core/tests/test_p8_features.py` 已有：
  - `test_c3m4_part_geomplate_curve_point_default_is_helper_expected_backed`
  - `test_c3m4_part_geomplate_invalid_inputs_have_stable_diagnostics`
  - `test_c4m1_part_geomplate_advanced_constraints_are_expected_backed`
  - `test_c4m1_part_geomplate_advanced_wrappers_are_concrete_deferred`
- 本包不是重新实现第一批，而是在 live-supported guard 上，把剩余 advanced criteria / wrapper 状态收进同一 DTO / API 批次。

## FreeCAD 调用链

本包只围绕 `Part.GeomPlate.BuildPlateSurface` helper，不进入 GUI 或原生 DocumentObject：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp::BuildPlateSurfacePy::PyInit()`：解析 `Surface`、`Degree`、`NbPtsOnCur`、`NbIter`、`Tol2d`、`Tol3d`、`TolAng`、`TolCurv`、`Anisotropy`，并在有 `Surface` 时调用 `LoadInitSurface(handle)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp::BuildPlateSurfacePy::loadInitSurface()`：后续显式 `LoadInitSurface(handle)` 入口。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()`：从 3D `Boundary` 构造 `GeomPlate_CurveConstraint(..., Order, NbPts, TolDist, TolAng, TolCurv)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setCurve2dOnSurf()`：调用 `SetCurve2dOnSurf(curve2)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setProjectedCurve()`：调用 `SetProjectedCurve(hCurve, tolU, tolV)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::PointConstraintPy::PyInit()`：从 3D point 构造 `GeomPlate_PointConstraint(gp_Pnt(...), Order, TolDist)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setPnt2dOnSurf()`：调用 `SetPnt2dOnSurf(gp_Pnt2d(x, y))`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp::PlateSurfacePy::makeApprox()`：解析 `Tol3d`、`MaxSegments`、`MaxDegree`、`MaxDistance`、`CritOrder`、`Continuity`、`EnlargeCoeff` 并调用 `GeomPlate_MakeApprox`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::Part::Tools::makeSurface()`：证明同一底层 builder 能同时消费 G0 curve、G1 curve-on-surface 和 point constraint。

## cad-core 落点

- DTO / public API：`cad-core/include/cad_core/part/part_geomplate.h`
- executor / parser：`cad-core/src/part/part_geomplate.cpp`
- feature registry：`cad-core/src/runtime/feature_registry.cpp`
- expected collector：`cad-core/tools/collect_freecad_expected.py`
- fixtures：`cad-core/fixtures/c5m7/`
- tests：`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py`
- capability metadata：`cad-core/src/adapters/c_api/c_api.cpp`

## 方案范围

### S0 live 基线与 scope 冻结

核准当前第一批和 c4m1 advanced fixture 的真实覆盖：3D G0 curve、3D point、advanced approximation params 已有 expected-backed 证据；initial surface / 2D wrapper 状态仍是 deferred diagnostic。

### S1 现有 advanced constraints 收口（已实现）

把 `c4m1/part-geomplate-advanced-constraints` 作为本包 guard，而不是新需求；确认它只覆盖 explicit approximation params 和 source evidence，不误宣称 G1 / 2D / initial surface 已支持。

S1 已收口：`cad-core/tests/test_p8_features.py` 明确断言该 fixture 只有 4 条 G0 `curve3d` source evidence 和 1 条 `point3d` source evidence，并验证 `ApproxTol3d`、`ApproxMaxSegments`、`ApproxMaxDegree`、`ApproxContinuity` 等 explicit approximation metadata；S1 时 `part_workbench.geomplate` status 收窄为 `supported_expected_backed_explicit_approximation_params_with_deferred_wrappers`。S2 已进一步把 InitialSurface 移入 expected-backed supported slice，并把 G1 curve-on-surface 记录为 source-backed with native oracle blocker；S3 已把 Curve2dOnSurface、Point2dOnSurface 和 G0+2D mixed 移入 expected-backed slice，并把 ProjectedCurve2d 记录为 source-backed with native oracle blocker。S4 已把 point criteria 移入 expected-backed supported slice，并把 curve criteria setter / `Part.PlateSurface.Curves` 改成 concrete diagnostics。

### S2 InitialSurface 与 G1 curve-on-surface（已实现，G1 native oracle blocked）

扩展 `PartGeomPlateSurfaceDTO`，让 request graph 能表达：

- initial `Surface` / `InitialSurface` reference；
- 3D curve-on-surface / G1 constraint；
- G0 与 G1 混合边界。

S2 已产出两个 c5m7 fixture：`part-geomplate-initial-surface-g0` 与 `part-geomplate-g1-curve-on-surface`。其中 InitialSurface 代表场景已由 FreeCADCmd collector 采集 expected；G1 curve-on-surface 已在 cad-core 中按 `Tools.cpp::makeSurface()` 的 `Adaptor3d_CurveOnSurface -> GeomPlate_CurveConstraint(..., 1 /*GeomAbs_G1*/, ...)` 路径实现并有 source evidence，但 FreeCADCmd Python wrapper 只能暴露 `Part.GeomPlate.CurveConstraint` / `setCurve2dOnSurf`，probe 会终止原生进程，因此 expected 文件保留 `known_gap` blocker，不从 cad-core 输出倒推 bbox / topology。

### S3 projected 2D curve 与 2D point-on-surface（已实现，Projected native oracle blocked）

沿同一 DTO/API 继续扩展：

- `Curve2dOnSurface`；
- `ProjectedCurve2d`，含 `tolU / tolV`；
- `Point2dOnSurface`。

S3 已产出四个 c5m7 fixture：`part-geomplate-curve2d-on-surface`、`part-geomplate-projected-curve2d`、`part-geomplate-point2d-on-surface`、`part-geomplate-mixed-surface-constraints`。其中 Curve2dOnSurface、Point2dOnSurface 和 G0+2D mixed 已采集 FreeCAD expected；ProjectedCurve2d 已在 cad-core 中按 `CurveConstraintPyImp.cpp::setProjectedCurve()` 的 `SetProjectedCurve(hCurve, tolU, tolV)` 语义实现并输出 source evidence，但 FreeCADCmd Python wrapper probe 会终止原生进程，因此 expected 文件保留 `geomplate_projected_curve2d_native_oracle_blocked` known_gap。

S3 的 mixed fixture 约束 G0 curve + curve2d + point2d 的组合语义。包含 G1 curve-on-surface 的 mixed expected 仍受 S2 `setCurve2dOnSurf` / `Adaptor3d_CurveOnSurface` native oracle blocker 约束，不能写成 expected-backed。

### S4 custom criteria 与 PlateSurface.Curves wrapper（已实现）

S4 已补齐 criteria 和 wrapper 边界：

- point constraint 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 进入同一 `PartGeomPlateSurfaceDTO`，executor 调用 `GeomPlate_PointConstraint::SetG*Criterion()`，`part-geomplate-point-custom-criteria` 已采集 FreeCAD expected。
- curve constraint 的 criteria setter 仍是 FreeCAD wrapper `NotImplementedError`，cad-core 输出 `unsupported_curve_criteria` locatable diagnostic，fixture 为 `part-geomplate-curve-criteria-diagnostic`。
- `Part.PlateSurface.Curves` 仍不是 same-DTO 生命周期：`PlateSurfacePyImp.cpp` 的 `Curves` 分支是 `TODO`，`GeomPlateSurface::Save/Restore` 仍 `NotImplementedError`，cad-core 输出 `unsupported_wrapper_lifecycle` locatable diagnostic，fixture 为 `part-geomplate-wrapper-boundary`。

### S5 capability 与文档收口

同步 `part_workbench.geomplate` capability、CADCore3.0 gaps、C5 根矩阵和本包矩阵：

- supported 只写入 expected-backed 的场景；
- remaining gaps 不再保留 broad `advanced constraints`；
- wrapper 若仍未支持，必须有明确 future owner / diagnostic fixture。

## fixture / expected 批次

| fixture | 类型 | 目标 |
| --- | --- | --- |
| `c3m4/part-geomplate-curve-point-default` | existing guard | 第一批 3D G0 + 3D point 不回退 |
| `c4m1/part-geomplate-advanced-constraints` | existing guard | explicit approximation params + 3D source evidence expected-backed；不误宣称 InitialSurface/G1/2D/wrapper |
| `c5m7/part-geomplate-initial-surface-g0` | expected-backed | initial surface reference + G0 curve |
| `c5m7/part-geomplate-g1-curve-on-surface` | source-backed + native oracle blocker | curve-on-surface G1；FreeCAD expected 暂以 `known_gap` 记录 Adaptor3d_CurveOnSurface oracle blocker |
| `c5m7/part-geomplate-curve2d-on-surface` | expected-backed | explicit 2D curve on surface |
| `c5m7/part-geomplate-projected-curve2d` | source-backed + native oracle blocker | projected 2D curve + tolerance；FreeCAD expected 暂以 `known_gap` 记录 `setProjectedCurve` oracle blocker |
| `c5m7/part-geomplate-point2d-on-surface` | expected-backed | 2D point on surface |
| `c5m7/part-geomplate-mixed-surface-constraints` | expected-backed | G0 curve + curve2d + point2d constraints mixed；G1 curve-on-surface mixed expected 仍受 S2 blocker 限制 |
| `c5m7/part-geomplate-point-custom-criteria` | expected-backed | point `G0Criterion` / `G1Criterion` / `G2Criterion` setter path |
| `c5m7/part-geomplate-curve-criteria-diagnostic` | diagnostic-backed | curve criteria setters remain FreeCAD `NotImplementedError` |
| `c5m7/part-geomplate-wrapper-boundary` | diagnostic-backed | `Part.PlateSurface.Curves` wrapper lifecycle boundary |

## 非目标

- 不做 GUI GeomPlate feature、TaskPanel 或 command UI。
- 不伪造原生 FreeCAD `Part::GeomPlate` DocumentObject；本包保持 source-backed helper。
- 不处理 Filling `BRepOffsetAPI_MakeFilling` 的 surface/support/order/param family。
- 不把 `Part.PlateSurface.Curves` 硬塞进 supported；必须先证明同一 DTO 生命周期可承接。
- 不用结果 bbox、输出顺序、fixture 名称或后处理修剪替代 FreeCAD constraint wrapper 语义。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/工作步骤细分 --format markdown
```

focused 回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
