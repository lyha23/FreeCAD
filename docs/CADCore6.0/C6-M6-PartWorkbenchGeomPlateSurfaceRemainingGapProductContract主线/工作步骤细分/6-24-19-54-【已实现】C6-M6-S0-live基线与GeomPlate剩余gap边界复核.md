# 【已实现】C6-M6-S0 live 基线与 GeomPlate 剩余 gap 边界复核

## 目标

冻结 C6-M6 起点：live HEAD、C6-M1 到 C6-M5 queue 状态、`part_workbench.geomplate` 当前 capability、旧 C5-M7 / C5-M13 GeomPlate 结论，以及 4 个 active `remaining_gaps`。S0 只做复核和矩阵基线，不改业务 C++。

## 必读

- `docs/CADCore6.0/README.md`
- `docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/README.md`
- `docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/6-21-00-43-C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分/6-22-04-07-【已实现】C5-M13-S4-geomplateNativeOracle修复.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- 更新 `矩阵/c6m6_geomplate_remaining_gap_scope_review_matrix.tsv` 的 baseline 行。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_blocker_queue.tsv` 的 `BLK-000` 和 4 个 live blocker 行。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_validation_matrix.tsv` 的 S0 证据。
- 不删除 `remaining_gaps`，不新增 fixtures，不改 executor。

## S0 收口结论

- live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=fa5e3ebe33 docs: 完成 C6-M5 S6 发布闸门收口`。
- 起点工作区已有 C6-M6 package 文件和 `docs/CADCore6.0/README.md` 变更；S0 未修改 `cad-core` 源码、未新增 fixture、未运行 FreeCADCmd。
- C6-M1 到 C6-M5 queue 均返回空表；C6-M6 queue 在 S0 完成后应从 S1 开始。
- `part_workbench.geomplate.status=supported_expected_backed_projected_initial_surface_with_curve_wrapper_diagnostics`，并保留 4 个 active `remaining_gaps`：`g1_curve_on_surface_native_hidden_diagnostic_only`、`projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`、`curve_constraint_criteria_setters_not_implemented`、`platesurface_curves_wrapper_lifecycle`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/工作步骤细分 --format markdown
rg -n 'part_workbench\.geomplate|g1_curve_on_surface_native_hidden_diagnostic_only|projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker|curve_constraint_criteria_setters_not_implemented|platesurface_curves_wrapper_lifecycle' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线 docs/CADCore6.0/README.md
```

验收通过后，本文已重命名为 `6-24-19-54-【已实现】C6-M6-S0-live基线与GeomPlate剩余gap边界复核.md`。

## 非目标

- 不修改 `cad-core/src/part/part_geomplate.cpp`。
- 不运行 FreeCADCmd oracle 采集。
- 不删除或改名任何 C6-M1 到 C6-M5 已实现文件。
