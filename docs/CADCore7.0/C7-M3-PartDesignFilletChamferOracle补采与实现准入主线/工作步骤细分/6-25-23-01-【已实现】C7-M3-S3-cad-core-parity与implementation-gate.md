# 【已实现】C7-M3 S3 cad-core parity 与 implementation gate

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

1. 已记录 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=ac831f3ba7`（`ac831f3ba7 文档：完成 C7-M3 S2 oracle expected 固化`），开始时 `git -c core.quotepath=false status --short -uall` 无输出。
2. 已刷新队列：S3 为当前 pending，S4/S5 在后。
3. 已从当前 `cad-core/tests/test_p7_features.py` 读取真实 test helpers，并新增最小 focused tests：`test_c7m3_fillet_oracle_rows_match_expected`、`test_c7m3_chamfer_flip_direction_oracle_rows_match_expected`、`test_c7m3_reference_shadow_recovery_oracle_remains_blocked`。
4. 已对 5 个 S2 Fillet/Chamfer FreeCADCmd expected-backed fixtures 运行当前 cad-core parity，全部匹配 bbox、volume、topology_counts。
5. 已将 `dressup-reference-shadow-base-recovery` 裁为 `oracle_blocked`；该 expected 是 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`，不能作为 supported 或 backend gap。
6. 已更新 `backend_gate_matrix`、`blocker_queue`、`oracle_scope`、`oracle_plan`、`source_authority`、`validation_matrix` 以及本包 README/方案/总入口。
7. 已确认没有 `backend_gap_requires_implementation`，S4 不改 C++，只做 no-code publication/docs sync。

## S3 route

| row | fixture | route |
| --- | --- | --- |
| `C7M3-SCOPE-101` | `p7/fillet-pad-multi-edge` | `already_closed_expected_backed` |
| `C7M3-SCOPE-101` | `p7/fillet-pad-use-all-edges` | `already_closed_expected_backed` |
| `C7M3-SCOPE-102` | `p7/chamfer-pad-edge-flip-true` | `already_closed_expected_backed` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-two-distances-edge-flip-true` | `already_closed_expected_backed` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-distance-angle-edge-flip-true` | `already_closed_expected_backed` |
| `C7M3-SCOPE-103` | `c3m5/dressup-reference-shadow-base-recovery` | `oracle_blocked` |

## S4 gate

- Code gate：closed；没有 `backend_gap_requires_implementation`。
- S4 可改范围：docs/matrix/capability publication sync。
- S4 禁止范围：不改 `cad-core/src/part_design/feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_dress_up.cpp`、runtime、topo、adapter；不把 `oracle_blocked` 发布成 supported。

## 非目标

- 不做 C++ 实现。
- 不扩大到 S2 未采 oracle 的场景。
- 不把 oracle blocked 写成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_fillet_oracle_rows_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_chamfer_flip_direction_oracle_rows_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_reference_shadow_recovery_oracle_remains_blocked

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
