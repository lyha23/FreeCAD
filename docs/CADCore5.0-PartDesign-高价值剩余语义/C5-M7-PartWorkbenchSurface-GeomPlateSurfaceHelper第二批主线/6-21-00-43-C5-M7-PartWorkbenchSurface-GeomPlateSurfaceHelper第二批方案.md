# C5-M7 Part Workbench Surface GeomPlateSurface Helper 第二批方案

## 当前基线

- 当前 live 代码已经不是空白起点：`part_workbench.geomplate` capability 已列出 `c3m4/part-geomplate-curve-point-default`、`c3m4/part-geomplate-invalid-inputs`、`c4m1/part-geomplate-advanced-constraints`、`c4m1/part-geomplate-advanced-deferred`。
- `cad-core/src/part/part_geomplate.cpp` 当前支持 3D curve G0 constraints、3D point constraints、build params、advanced approximation params 和 makeApprox face；`InitialSurface`、`Surface`、`Curve2dOnSurface`、`ProjectedCurve2d`、`Point2dOnSurface`、`PlateSurfaceCurves` 仍由 `rejectDeferredGeomPlateAdvancedProperties()` 输出 deferred diagnostic。
- `cad-core/tests/test_p8_features.py` 已有：
  - `test_c3m4_part_geomplate_curve_point_default_is_helper_expected_backed`
  - `test_c3m4_part_geomplate_invalid_inputs_have_stable_diagnostics`
  - `test_c4m1_part_geomplate_advanced_constraints_are_expected_backed`
  - `test_c4m1_part_geomplate_advanced_wrappers_are_concrete_deferred`
- 本包不是重新实现第一批，而是在 live-supported guard 上，把剩余 advanced wrapper 状态收进同一 DTO / API 批次。

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

S1 已收口：`cad-core/tests/test_p8_features.py` 明确断言该 fixture 只有 4 条 G0 `curve3d` source evidence 和 1 条 `point3d` source evidence，并验证 `ApproxTol3d`、`ApproxMaxSegments`、`ApproxMaxDegree`、`ApproxContinuity` 等 explicit approximation metadata；`cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.geomplate` status 已改为 `supported_expected_backed_explicit_approximation_params_with_deferred_wrappers`，并保留 `initial_surface_reference_contract`、`g1_curve_on_surface`、`projected_2d_curve`、`point_2d_on_surface`、`custom_constraint_criteria`、`part_platesurface_curves_wrapper` 作为 remaining gaps。

### S2 InitialSurface 与 G1 curve-on-surface

扩展 `PartGeomPlateSurfaceDTO`，让 request graph 能表达：

- initial `Surface` / `InitialSurface` reference；
- 3D curve-on-surface / G1 constraint；
- G0 与 G1 混合边界。

本步至少产出两个 expected-backed fixture：`part-geomplate-initial-surface-g0`、`part-geomplate-g1-curve-on-surface`；若 FreeCAD oracle 无法稳定采集，必须保留 blocker 和 concrete diagnostic，不得从 cad-core 输出倒推。

### S3 projected 2D curve 与 2D point-on-surface

沿同一 DTO/API 继续扩展：

- `Curve2dOnSurface`；
- `ProjectedCurve2d`，含 `tolU / tolV`；
- `Point2dOnSurface`。

本步至少产出三个 expected-backed fixture：`part-geomplate-curve2d-on-surface`、`part-geomplate-projected-curve2d`、`part-geomplate-point2d-on-surface`；另加一个 mixed fixture 约束 G0 + G1 + 2D point/curve 的组合语义。

### S4 custom criteria 与 PlateSurface.Curves wrapper

补齐 criteria 和 wrapper 边界：

- 支持或诊断 `SetG0Criterion` / `SetG1Criterion` / `SetG2Criterion` 对 point / curve constraints 的影响；
- 判定 `Part.PlateSurface.Curves` 是否能安全映射到同一 DTO；
- 如果 wrapper 需要新的持久 PlateSurface object 或不同 API 生命周期，本轮只产出 locatable `unsupported_property` / `unsupported_wrapper_lifecycle` diagnostic。

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
| `c5m7/part-geomplate-initial-surface-g0` | new expected | initial surface reference + G0 curve |
| `c5m7/part-geomplate-g1-curve-on-surface` | new expected | curve-on-surface G1 |
| `c5m7/part-geomplate-curve2d-on-surface` | new expected | explicit 2D curve on surface |
| `c5m7/part-geomplate-projected-curve2d` | new expected | projected 2D curve + tolerance |
| `c5m7/part-geomplate-point2d-on-surface` | new expected | 2D point on surface |
| `c5m7/part-geomplate-mixed-surface-constraints` | new expected | G0 + G1 + 2D constraints mixed |
| `c5m7/part-geomplate-custom-criteria` | new expected / diagnostic | criteria setters |
| `c5m7/part-geomplate-wrapper-boundary` | diagnostic or expected | `PlateSurface.Curves` owner 判定 |

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
