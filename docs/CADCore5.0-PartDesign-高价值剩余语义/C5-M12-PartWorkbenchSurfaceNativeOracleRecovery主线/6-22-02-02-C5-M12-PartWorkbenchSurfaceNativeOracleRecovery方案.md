# C5-M12 Part Workbench Surface Native Oracle Recovery 方案

## 当前基线

C5-M6 到 C5-M11 已把 Part Workbench surface family 拆成多个可发布 slice。当前仍有价值的剩余工作不再是 broad surface family，而是同一类 native oracle / expected recovery：cad-core 已有 source-backed helper / wrapper 实现或 diagnostic-backed contract，但 FreeCADCmd native helper / wrapper expected 仍未稳定落库。

本包纳入四组代表场景：

- `part_workbench.sweep`：C5-M11 后 `SpineSupport` / `SupportMode` 是 diagnostic-only narrowed blocker；`SectionOptions[].Location` / `WithContact` / `WithCorrection` 与 `advanced_combination` 卡在 FreeCADCmd `OCCError: NCollection_Array1::Value`。
- `part_workbench.loft`：`complex_profile_family` 是 C5-M6 后唯一 loft remaining gap，需要按 FreeCAD `Part::Loft` sections/profile 语义拆分 face / vertex / wire / sketch subelement 场景。
- `part_workbench.filling`：C5-M8 后仍有 `surface_support_order_native_helper_expected`、`filling_support_order_g2_expected`、`non_default_params_native_helper_expected`、`non_boundary_edge_support_native_helper_expected`。
- `part_workbench.geomplate`：C5-M7 后仍有 G1 curve-on-surface native expected oracle 与 ProjectedCurve2d native expected oracle；curve criteria setter 和 PlateSurface.Curves 当前仍是 FreeCAD wrapper diagnostic/lifecycle boundary。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`：读取 `Sections`、`Solid`、`Ruled`、`Closed`、`Linearize`、`MaxDegree` 并调用 `makeElementLoft`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`：`setAuxiliarySpine`、`setSpineSupport`、`setBiNormalMode`、`add(Profile, Location, WithContact, WithCorrection)`、`setTolerance`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 与 `TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`：构造 `BRepOffsetAPI_MakeFilling`，添加 boundary、support/order、non-boundary edge/wire/face/vertex constraints。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`、`CurveConstraintPyImp.cpp`、`PointConstraintPyImp.cpp`、`Tools.cpp::makeSurface()`：构造 `GeomPlate_BuildPlateSurface` 并导出 transient helper geometry。

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py`：扩展或新增 probe / collector 分支，只从 FreeCADCmd helper / wrapper 收集 expected，不读 cad-core recompute 输出。
- `cad-core/fixtures/{c4m1,c5m7,c5m8,c5m10,c5m12}/`：新增或替换 representative fixtures / expected。
- `cad-core/src/part/part_loft.cpp`、`part_filling.cpp`、`part_geomplate.cpp`、`part_sweep.cpp`：只在 oracle 证明 cad-core semantic gap 时补实现。
- `cad-core/src/adapters/c_api/c_api.cpp` 与 `cad-core/tests/test_adapters.py`：更新 capability metadata 与 focused assertions。
- `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`：保护 expected-backed payload、narrowed blockers、diagnostics 和 metadata regression。

## 批量闭环标准

- S1 必须一次探测四组 surface helper / wrapper，不允许只处理 Sweep 或 Loft 单个 fixture 后宣称 C5-M12 完成。
- S2-S4 可以按 family 分步落库，但每步都必须保留同一批次矩阵状态，避免丢失未完成 sibling blockers。
- expected 必须由 FreeCADCmd native helper / wrapper 采集；若 FreeCADCmd 因 OCCT / wrapper 暴露限制失败，expected 文件只能记录 narrowed blocker。
- capability wording 必须区分 expected-backed、source-backed known_gap、diagnostic-backed、narrowed blocker、non-goal。

## 实施顺序

1. S0：冻结 current live blockers、fixture / expected 状态、source authority 和非目标。
2. S1：批量 probe FreeCADCmd native helper / wrapper，生成 collectable / blocker 分流矩阵。
3. S2：恢复 Sweep wrapper support/location/combined expected 或保留更窄 blocker。
4. S3：实现 Loft complex profile family expected / diagnostics。
5. S4：恢复 Filling + GeomPlate native helper expected 或收窄 blockers。
6. S5：同步 tests、capability、C3 gap、C5 root、本包矩阵和 README。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 收口标准

- C5-M12 package queue 为空。
- collectable native helper / wrapper blockers 已替换为 expected-backed JSON。
- uncollectable 场景只剩明确 narrowed blocker，带 FreeCADCmd 错误、source authority、未采字段和下一批条件。
- `docs/CADCore3.0/capabilities-gap对照表.md`、C5 root matrices、本包矩阵、adapter capability metadata 与 focused tests 一致。
