# C7-M3 S3 cad-core parity 与 implementation gate

## 目标

用当前 cad-core 对 S2 oracle fixtures 做 focused parity，并裁决每个 row 是否已经 expected-backed、需要实现、oracle blocked 或 non-goal。S3 是 C7-M3 唯一 implementation gate。

## 必读

- S2 完成后的 fixtures、expected 和本包矩阵
- `cad-core/src/part_design/feature_fillet.cpp`
- `cad-core/src/part_design/feature_chamfer.cpp`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/topo/`
- `cad-core/tests/test_p7_features.py`

## 裁决类型

- `already_closed_expected_backed`：新 oracle 与当前 cad-core parity 一致。
- `backend_gap_requires_implementation`：新 oracle 证明 cad-core active gap，S4 可实现。
- `oracle_blocked`：S2 没有可信 FreeCAD oracle，不能实现或发布 supported。
- `diagnostic_non_goal`：超出本包后端边界。

## 动作

1. 记录 live baseline 和队列状态。
2. 从当前 `cad-core/tests/test_p7_features.py` 读取真实 test names，补 focused test plan。
3. 对 S2 fixtures 运行 focused parity 或新增最小 focused tests。
4. 更新 `backend_gate_matrix`、`blocker_queue`、`validation_matrix`。
5. 若产生 `backend_gap_requires_implementation`，写清 S4 可改文件、FreeCAD 依据、test/fixture 范围和禁止事项。
6. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S4。

## 非目标

- 不做 C++ 实现。
- 不扩大到 S2 未采 oracle 的场景。
- 不把 oracle blocked 写成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
rg -n 'already_closed_expected_backed|backend_gap_requires_implementation|oracle_blocked|diagnostic_non_goal' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
git diff --check
```

## 通过条件

- 每个 S2 row 有明确 gate route。
- Code edit gate 状态明确。
- 队列推进到 S4。
