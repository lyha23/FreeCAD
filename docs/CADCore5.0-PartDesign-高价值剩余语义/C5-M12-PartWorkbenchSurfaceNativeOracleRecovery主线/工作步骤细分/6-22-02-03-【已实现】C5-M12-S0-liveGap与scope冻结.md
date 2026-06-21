# 【已实现】C5-M12-S0 live gap 与 scope 冻结

状态：`【已实现】`

## S0 live 冻结结论

S0 只冻结当前 live blockers 和 scope，不实现 collector、不采集 expected、不改变 capability 支持声明，也不改 cad-core 行为。C5-M12 整体仍为 pending，下一步进入 S1 FreeCADCmd native helper / wrapper probe 矩阵。

当前冻结的 collectable 候选与 blocker：

- Sweep：候选为 valid `SpineSupport` / `SupportMode` representative、`add(Profile, Location, WithContact, WithCorrection)` located profile、advanced combined payload。当前 known_gap 为 `part_sweep_support_mode_fixture_diagnostic_only`、`part_sweep_located_profile_freecadcmd_wrapper_build_blocker`、`part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`；删除条件分别是补到 valid support wrapper expected、FreeCADCmd 对 Location overload 返回稳定 `shape_summary`、Location blocker 解除后 combined auxiliary / section / tolerance metadata 可采。
- Loft：候选为 `complex_profile_family` 的 face / vertex / wire / sketch subelement representatives。当前 known_gap 是 C5-M6 后只发布 face / vertex profile 与 `Linearize=true` expected-backed slice，complex profile 尚无 C5-M12 expected；删除条件是 representative complex profiles 形成 expected-backed 或 diagnostic-backed fixtures。
- Filling：候选为 `surface` / support / order native helper expected、G2 support/order、explicit non-default constructor params、non-boundary edge support/order。当前 known_gap 为 `surface_support_order_native_helper_expected`、`filling_support_order_g2_expected`、`non_default_params_native_helper_expected`、`non_boundary_edge_support_order_native_helper_expected`；删除条件是 `Part.makeFilledFace(...)` 对应 helper path 在 FreeCADCmd 中无 timeout、无 process termination、无 status 245 或 collector unsupported guard，并返回稳定 geometry expected。
- GeomPlate：候选为 G1 curve-on-surface native oracle 与 ProjectedCurve2d native oracle。当前 known_gap 为 `g1_curve_on_surface_native_oracle`、`projected_2d_curve_native_oracle`；删除条件是 native oracle 能稳定构造 `Adaptor3d_CurveOnSurface` G1 constraint，或稳定调用 `CurveConstraint.setProjectedCurve(...)`，并写入 FreeCAD expected geometry payload。

非目标保持：完整 Part surface family、GUI / TaskPanel / ViewProvider、native `Part::Sweep` advanced direct properties、PartDesign product support、persistent Python helper / wrapper lifecycle、cad-core-output-derived expected。

## 目标

冻结 C5-M12 起点：确认 C5-M6/M7/M8/M11 后剩余工作只包含 Part Workbench surface native oracle / expected recovery 的精确 blockers，并记录 fixture、expected、capability、delete condition 和 non-goal。S0 不改 cad-core 行为。

## 必读

- 本包总入口与方案。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- C5-M6、C5-M7、C5-M8、C5-M11 已实现总结和矩阵。
- `cad-core/src/adapters/c_api/c_api.cpp`
- 当前相关 expected：`cad-core/fixtures/c5m10/expected`、`cad-core/fixtures/c5m8/expected`、`cad-core/fixtures/c5m7/expected`、`cad-core/fixtures/c4m1/expected`。

## 产物

- 更新 `C5M12-BLK-000`、`C5M12-SCOPE-000`、`C5M12-ORC-000` 为 S0 完成态。
- Root C5 `C5-BLK-1201`、`C5-SCOPE-1201`、`C5-ORC-1201` 补 S0 live guard 结论但保持 C5-M12 pending。
- 明确 Sweep / Loft / Filling / GeomPlate 的 collectable 候选、当前 known_gap、delete condition、非目标。

## 非目标

- 不实现 collector。
- 不采集 expected。
- 不改变 capability 支持声明。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分 --format markdown
```
