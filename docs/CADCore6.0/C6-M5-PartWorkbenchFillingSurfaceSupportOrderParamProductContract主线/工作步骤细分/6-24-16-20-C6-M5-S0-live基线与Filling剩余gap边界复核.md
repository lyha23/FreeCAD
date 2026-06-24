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

## 必须回写的矩阵行

- `SCOPE-000`：live baseline 状态。
- `SCOPE-101`、`SCOPE-102`、`SCOPE-201`、`SCOPE-202`：剩余 gap 是否仍在。
- `BLK-000`：是否允许进入 S1/S2。
- `NON-001` 到 `NON-008`：non-goal 是否仍有效。
- `VAL-000`、`VAL-001`、`VAL-002`、`VAL-003`：S0 验收记录。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part_workbench\\.filling|filling_surface_native_helper_blocker|filling_support_order_g1_native_helper_blocker|filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker|filling_non_boundary_support_order_native_helper_blocker' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
```

验收通过后，将本文重命名为 `6-24-16-20-【已实现】C6-M5-S0-live基线与Filling剩余gap边界复核.md`，并在矩阵中记录 S0 证据。

## 非目标

- 不采集新 oracle。
- 不改 `cad-core` C++。
- 不删除 `remaining_gaps`。
