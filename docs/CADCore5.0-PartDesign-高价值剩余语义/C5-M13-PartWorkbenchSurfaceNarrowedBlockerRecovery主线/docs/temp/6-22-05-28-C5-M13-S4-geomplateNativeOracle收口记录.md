# C5-M13-S4 GeomPlate native oracle 收口记录

状态：`done_C5M13-S4_geomplate_native_oracle`

## 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`7fe26bcd24`
- `git log -1 --oneline`：`7fe26bcd24 docs: 收口 C5-M13 S3 Filling expected`
- 起始 `git -c core.quotepath=false status --short -uall`：存在 unrelated `cad-core/src/part_design/*`、`cad-core/src/runtime/recompute.cpp`、P6/P7 tests、README、BUG 文档和 c3m5 fixture 脏改动，本轮不触碰。
- 队列确认：`step_goal_queue.py .../C5-M13.../工作步骤细分/6-22-04-07-C5-M13-S4-geomplateNativeOracle修复.md --format markdown` 从 S4 开始，S5 仍 pending。
- `FreeCADCmd --version`：`FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()` 构造普通 `GeomPlate_CurveConstraint(Boundary, Order, NbPts, TolDist, TolAng, TolCurv)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setProjectedCurve()` 调用 `SetProjectedCurve(hCurve, tolU, tolV)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion()` 均为 `NotImplementedError("Not yet implemented")`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::Part::Tools::makeSurface()` 对 G1 使用 `Adaptor3d_CurveOnSurface` / `Adaptor3d_HCurveOnSurface`，这是 Python helper 当前不可直接暴露的 native path。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp::PlateSurfacePy::PyInit()` 解析 `Curves`，但循环内仍是 `TODO`；`Geometry.cpp::GeomPlateSurface::Save/Restore` 继续抛 `NotImplementedError`。

## Probe

Probe 脚本：

`docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-05-28-c5m13-s4-geomplate-native-oracle-probe.py`

命令模板：

```bash
cd /Users/li/Chili3DProject/FreeCAD
C5M13_S4_PROBE_CASE=<case> FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-05-28-c5m13-s4-geomplate-native-oracle-probe.py', encoding='utf-8').read(), 'c5m13_s4_geomplate_probe.py', 'exec'))"
```

## 结论矩阵

| case | 变体 | 输出 / 错误 | 分类 | S4 处理 |
| --- | --- | --- | --- | --- |
| `geomplate_g1_curve_on_surface_variants` | `line_z0_surface_range_0_4`、`line_z1_surface_range_0_4`、`line_z0_surface_range_0_1` | `setCurve2dOnSurf` 成功，但 `BuildPlateSurface.perform` 为 `RuntimeError: GeomPlate_CurveConstraint.cxx : Curve must be on a Surface`；`setG1Criterion` 仍 `NotImplementedError` | native-hidden diagnostic-only | 保留 `part-geomplate-g1-curve-on-surface` known_gap，补 S4 probe evidence |
| `geomplate_g1_curve_on_surface_variants` | `line_z0_no_initial_range_0_4` | `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` | native-runtime blocker | 不替代 `Adaptor3d_CurveOnSurface` G1 oracle |
| `geomplate_projected_curve2d_variants` | 无 `InitialSurface` 的 `range_0_1_tol_0p001`、`range_0_4_tol_0p01`、`range_0_4_tol_0p1` | 均为 `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` | initial-surface-required narrowed blocker | delete condition 保留在 expected metadata |
| `geomplate_projected_curve2d_variants` | `range_0_4_tol_0p01_initial_surface` | `builder.isDone=True`，返回稳定 Face，topology `faces=1 edges=4 vertices=4` | expected-backed | `c5m7/part-geomplate-projected-curve2d` 改为 FreeCAD expected；新增 `c5m13/part-geomplate-projected-curve2d-initial-surface` |
| `geomplate_curve_criteria_setters` | `setG0Criterion/setG1Criterion/setG2Criterion` | 全部 `NotImplementedError: Not yet implemented` | FreeCAD NotImplemented diagnostic | 保留 `unsupported_curve_criteria`，补 S4 metadata |
| `geomplate_plate_surface_curves` | `Part.PlateSurface(Curves=[line_curve()])` | `FreeCADCmd` exit 139 / SIGSEGV，无 JSON | wrapper lifecycle non-goal | 保留 `unsupported_wrapper_lifecycle`，补 S4 metadata |

## 更新产物

- collector：`cad-core/tools/collect_freecad_expected.py` 现在真实执行 `ProjectedCurve2d + InitialSurface` helper expected collection，并在 expected metadata 记录无初始面的 `V1==V2` blocker。
- expected-backed：`cad-core/fixtures/c5m7/expected/part-geomplate-projected-curve2d.freecad.json` 改为 FreeCADCmd geometry expected。
- 新 representative：`cad-core/fixtures/c5m13/part-geomplate-projected-curve2d-initial-surface.json` 与 expected。
- 保留 blocker/diagnostic：`part-geomplate-g1-curve-on-surface`、`part-geomplate-curve-criteria-diagnostic`、`part-geomplate-wrapper-boundary` 均补 S4 证据但不提升支持。
- focused tests：`cad-core/tests/test_p8_features.py` 与 `cad-core/tests/test_expected_fixtures.py` 锁定 Projected expected-backed、G1 S4 variants、criteria NotImplemented 和 wrapper SIGSEGV metadata。

## 删除条件

- G1：只有 Python/native helper 能稳定构造 `Adaptor3d_CurveOnSurface` 或等价 surface-bound curve，并返回 FreeCAD geometry expected 后，才能删除 G1 known_gap。
- ProjectedCurve2d：带 `InitialSurface` 的 representative 已 expected-backed；无 `InitialSurface` path 仍须等 FreeCAD helper 不再返回 `Geom_RectangularTrimmedSurface::V1==V2` 后再声明更宽支持。
- Curve criteria：FreeCAD source 实现 `setG0Criterion/setG1Criterion/setG2Criterion` 后再删除 diagnostic。
- `PlateSurface.Curves`：产品批准 request-local wrapper lifecycle 且 FreeCAD runtime 不再 SIGSEGV 后再删除 non-goal。
