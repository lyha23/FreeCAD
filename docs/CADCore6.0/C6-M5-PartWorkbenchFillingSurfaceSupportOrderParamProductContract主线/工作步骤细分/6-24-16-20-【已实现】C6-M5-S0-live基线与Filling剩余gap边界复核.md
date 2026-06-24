# C6-M5-S0 live 基线与 Filling 剩余 gap 边界复核

## 目标

复核当前 `part_workbench.filling` 的 live capability、tests、fixtures 和 docs，冻结 C6-M5 的声明边界。S0 只做基线确认与矩阵状态更新，不改业务代码。

## 输入

- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `docs/CADCore6.0/README.md`
- 本目录 `矩阵/*.tsv`

## 范围

- 必须确认 `part_workbench.filling` 当前 6 个 `remaining_gaps` 是否仍存在。
- 必须确认 C6-M1 到 C6-M4 队列已关闭，C6-M5 是新主线而不是旧主线追加项。
- 必须确认 expected-backed 子集与 native helper blocker 的区别。
- 必须确认不声明 FreeCAD parity、不实现 native `Part::FilledFace` DocumentObject。

## S0 结论

- live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`；`HEAD=396fcbc7f6`，`git log -1` 为 `396fcbc7f6 docs: 新增 C6-M5 Filling 产品合同方案`；`git status --short -uall` 无输出。
- `cad-core/src/runtime/capability_contract.cpp` 中 `part_workbench.filling` 仍为 `supported_expected_backed_with_c5m13_param_subset_closeout`，`remaining_gaps` 仍包含 6 项：`filling_surface_native_helper_blocker`、`filling_support_order_g1_native_helper_blocker`、`filling_support_order_g2_native_helper_blocker`、`filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker`、`filling_params_all_native_helper_blocker`、`filling_non_boundary_support_order_native_helper_blocker`。
- `step_goal_queue.py` 对 C6-M1、C6-M2、C6-M3、C6-M4 工作步骤目录均只输出空表头，确认旧队列已关闭；C6-M5 是新主线，S0 完成后下一项为 S1。
- `cad-core/tests/test_p8_features.py` 显示 current Filling 覆盖了 Boundary/default params、C5-M13 `Degree` / `NumIter` / `Tol2d+Tol3d` / `MaxDegree` expected-backed 子集，以及 non-boundary edge without support/order expected-backed 子集；Surface、Supports/Orders G1/G2、PtsOnCurve / Anisotropy / TolG1+TolG2 / MaxSegments / all params、non-boundary support/order 仍是 source-backed 或 native helper blocker。
- C6-M5 只推进 CAD Core request-local product contract；不声明 FreeCAD parity，不采集新 oracle，不实现 native `Part::FilledFace` DocumentObject，不删除 `remaining_gaps`。

## 必须回写的矩阵行

- `SCOPE-000`：live baseline 状态。
- `SCOPE-101`、`SCOPE-102`、`SCOPE-201`、`SCOPE-202`：剩余 gap 是否仍在。
- `BLK-000`：是否允许进入 S1/S2。
- `NON-001` 到 `NON-008`：non-goal 是否仍有效。
- `VAL-000`、`VAL-001`、`VAL-002`、`VAL-003`：S0 验收记录。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part_workbench\\.filling|filling_surface_native_helper_blocker|filling_support_order_g1_native_helper_blocker|filling_support_order_g2_native_helper_blocker|filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker|filling_params_all_native_helper_blocker|filling_non_boundary_support_order_native_helper_blocker' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
awk -F '\\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/矩阵/*.tsv
```

验收通过后，将本文重命名为 `6-24-16-20-【已实现】C6-M5-S0-live基线与Filling剩余gap边界复核.md`，并在矩阵中记录 S0 证据。

## 非目标

- 不采集新 oracle。
- 不改 `cad-core` C++。
- 不删除 `remaining_gaps`。
