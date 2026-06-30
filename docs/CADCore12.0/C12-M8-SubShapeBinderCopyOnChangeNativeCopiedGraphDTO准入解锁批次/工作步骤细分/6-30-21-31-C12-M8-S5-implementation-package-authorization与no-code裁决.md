# C12-M8 S5 implementation package authorization 与 no-code 裁决

## 目标

汇总 S2/S3/S4 的三个闸门，给出最终实现授权裁决：另开 implementation package，还是保留 no-code retained diagnostic。

## 必读文件

- `../README.md`
- `../6-30-21-24-C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次总入口.md`
- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m8_copy_on_change_blocker_queue.tsv`
- `../矩阵/c12m8_copy_on_change_validation_matrix.tsv`
- `cad-core/src/runtime/capability_contract.cpp`

## 操作

1. 逐项核对 S2 是否为 `native_copied_graph_evidence_ready`。
2. 逐项核对 S3 是否为 `dto_approved_for_request_local_graph`。
3. 逐项核对 S4 是否为 `current_mismatch_confirmed`。
4. 三项均成立时，输出 `implementation_package_authorized`，并写出后续 C12-M9 或等价包的最小 scope。
5. 任一项不成立时，输出 `no_code_retained_diagnostic`，保留 known gap、diagnostic、delete condition 和 reopen condition。

## 关闭条件

- `C12M8-BLOCKER-501` 关闭：implementation gate 已裁决。
- `C12M8-CAT-001..004` 均有最终分类。
- README / 总入口记录 S5 出口。

## 非目标

- 不在 S5 修改 C++。
- 不刷新 expected。
- 不删除 `copy_on_change_full_temporary_document_cache_not_supported`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
