# C12-M9 S5 implementation package authorization 裁决

## 目标

汇总 S1-S4 证据，决定 C12-M9 是否授权后续 implementation package、oracle / product-contract package，或关闭为 no-code backlog gate。

## 必读文件

- `../README.md`
- `../7-1-01-23-C12-M9-CADCoreImplementationCandidate再盘点批次总入口.md`
- `../矩阵/c12m9_candidate_backend_gap_classification.tsv`
- `../矩阵/c12m9_candidate_blocker_queue.tsv`
- `../矩阵/c12m9_candidate_validation_matrix.tsv`

## 操作

1. 核对是否存在 stable expected / product contract。
2. 核对 request-local boundary 是否批准。
3. 核对 current mismatch 是否确认。
4. 三项均成立时输出 `implementation_package_authorized` 并写后续包最小 scope。
5. 若缺 expected 或 product decision，输出 `oracle_or_product_contract_package_required`。
6. 若没有候选，输出 `no_code_backlog_gate`。

## 关闭条件

- `C12M9-BLOCKER-501` 关闭。
- `C12M9-CAT-006` 写最终分类。
- README / 总入口记录 S5 出口。

## 非目标

- 不直接修改 C++。
- 不刷新 expected。
- 不删除已发布 known gap 或 product diagnostic contract。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
git diff --check
```
