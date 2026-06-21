# 【已实现】C5-M13-S0 live narrowed blocker 冻结

状态：`【已实现】`

## 完成结论

S0 已按 C5-M12 后的 live docs、capability metadata 和 checked-in expected/known_gap evidence 冻结当前 precise blockers：

- Sweep：located profile 与 advanced combined 均保留 FreeCADCmd `OCCError: NCollection_Array1::Value` blocker。
- Filling：`surface_support_order_native_helper_expected`、`filling_support_order_g2_expected`、`non_default_params_native_helper_expected`、`non_boundary_edge_support_order_native_helper_expected` 保留为 native helper expected blocker。
- GeomPlate：G1 curve-on-surface 保留 native-hidden diagnostic-only；ProjectedCurve2d 保留 `RuntimeError: Geom_RectangularTrimmedSurface::V1==V2` blocker。
- Loft broad `complex_profile_family` 不重开；完整 Part surface family、GUI/native DocumentObject、persistent wrapper lifecycle、cad-core-output-derived expected 均保持非目标。

本步未改代码、未采集 expected、未提升 capability support。Package-local `C5M13-BLK-000`、`C5M13-SCOPE-000`、`C5M13-ORC-001` 已关闭；root `C5-BLK-1301`、`C5-SCOPE-1301`、`C5-ORC-1301` 保持 pending 并补充 S0 live blocker 结论。

## 目标

冻结 C5-M13 起点：以 C5-M12 之后的 live docs、capability metadata、expected files 和 focused tests 为准，记录 Sweep / Filling / GeomPlate 的 precise narrowed blockers。S0 不改代码、不采集 expected、不改变 capability 支持声明。

## 必读

- 本包总入口与方案。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c5m10`、`cad-core/fixtures/c5m8`、`cad-core/fixtures/c5m7`、`cad-core/fixtures/c5m12`

## 产物

- 本包局部矩阵中 `C5M13-BLK-000`、`C5M13-SCOPE-000`、`C5M13-ORC-001` 更新为 S0 完成态。
- Root `C5-BLK-1301`、`C5-SCOPE-1301`、`C5-ORC-1301` 保持 pending，但补充 S0 live blocker 结论。
- 明确 C5-M13 不重开 Loft broad `complex_profile_family`，不声明完整 Part surface family。

## 非目标

- 不实现 probe 或 collector。
- 不替换 expected。
- 不更新 capability support 状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```
