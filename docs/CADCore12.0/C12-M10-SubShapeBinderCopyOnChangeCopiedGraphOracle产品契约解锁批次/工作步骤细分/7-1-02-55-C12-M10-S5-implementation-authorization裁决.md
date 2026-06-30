# C12-M10 S5 implementation authorization 裁决

## 目标

汇总 S1-S4 证据，决定是否授权后续 implementation package，或关闭为 product-contract follow-up / oracle retained / no-code retained diagnostic。

## 必读文件

- `../README.md`
- `../7-1-02-48-C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次总入口.md`
- `../矩阵/c12m10_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m10_copy_on_change_blocker_queue.tsv`
- `../矩阵/c12m10_copy_on_change_validation_matrix.tsv`

## 操作

1. 核对 S2 native copied graph evidence 是否 ready。
2. 核对 S3 DTO / product contract 是否 approved。
3. 核对 S4 current mismatch 是否 confirmed。
4. 三项均成立时输出 `implementation_package_authorized`，并写后续 implementation package 最小完整语义批次。
5. 若缺 product decision，输出 `product_contract_package_required`。
6. 若缺 native evidence，输出 `oracle_blocked_retained`。
7. 若无 mismatch，输出 `no_code_retained_diagnostic`。
8. 将本 S5 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M10-BLOCKER-501` 关闭。
- `C12M10-CAT-004..005` 写最终分类。
- README / 总入口记录 S5 出口。

## 非目标

- 不直接修改 C++。
- 不刷新 expected。
- 不删除已发布 known gap，除非 S5 明确授权后续 implementation package。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
