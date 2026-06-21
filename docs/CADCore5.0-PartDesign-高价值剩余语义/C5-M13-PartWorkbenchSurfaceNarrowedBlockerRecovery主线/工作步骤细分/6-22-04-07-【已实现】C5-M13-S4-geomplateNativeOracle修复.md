# C5-M13-S4 GeomPlate native oracle 修复

状态：`done_C5M13-S4_geomplate_native_oracle`

## 目标

处理 GeomPlate helper 的 G1 curve-on-surface 与 ProjectedCurve2d native oracle blockers。S4 要先尊重 FreeCAD wrapper 的真实能力：如果 setter 是 `NotImplementedError` 或 Python wrapper 无法暴露 `Adaptor3d_CurveOnSurface`，只能保留 diagnostic / native-hidden blocker，不能伪造 expected。

## 必读

- S1 probe 矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- 可采集的 G1 / ProjectedCurve2d representative 写 FreeCADCmd expected。
- 不可采集时保留 precise blocker：`NotImplementedError`、native-hidden `Adaptor3d_CurveOnSurface`、`RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` 或更窄条件。
- `curve criteria setter` 与 `Part.PlateSurface.Curves` 只有 FreeCAD runtime 证明 request-local path 可用时才推进，否则保持 diagnostic / non-goal。
- 更新局部矩阵 `C5M13-BLK-401`、`C5M13-SCOPE-401`、`C5M13-ORC-401` 和 root `C5-ORC-1305`。

## S4 收口结论

- `ProjectedCurve2d + InitialSurface` 已 expected-backed：`cad-core/fixtures/c5m7/expected/part-geomplate-projected-curve2d.freecad.json` 改为 FreeCADCmd geometry expected，并新增 `cad-core/fixtures/c5m13/part-geomplate-projected-curve2d-initial-surface.json` 及 expected。
- 无 `InitialSurface` 的 ProjectedCurve2d 变体仍返回 `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2`，保留为更窄 blocker 条件。
- G1 curve-on-surface 仍是 native-hidden / NotImplemented boundary：`setCurve2dOnSurf` 可调用，但 `BuildPlateSurface.perform()` 仍报 `Curve must be on a Surface`，`setG1Criterion()` 仍是 `NotImplementedError`。
- `curve criteria setter` 继续是 `setG0Criterion/setG1Criterion/setG2Criterion` NotImplemented diagnostic。
- `Part.PlateSurface.Curves` 继续是 wrapper lifecycle non-goal，FreeCADCmd probe 仍 exit 139 / SIGSEGV。
- 记录：`docs/temp/6-22-05-28-C5-M13-S4-geomplateNativeOracle收口记录.md`。

## 非目标

- 不创建 fake persistent PlateSurface object。
- 不声明 native `Part::GeomPlate` DocumentObject。
- 不把 Filling 或 GUI GeomPlate feature 混入本步骤。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```
