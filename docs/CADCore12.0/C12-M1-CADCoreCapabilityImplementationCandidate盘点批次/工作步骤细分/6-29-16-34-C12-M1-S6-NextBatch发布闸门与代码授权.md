# C12-M1 S6 NextBatch 发布闸门与代码授权

## 目标

消费 S0-S5 结论，发布 CADCore12 下一步：选择一个明确 implementation package，或发布 no-code backlog gate。S6 是 C12-M1 唯一能授权下一轮 C++ 的步骤。

## 输入

- S0-S5 已完成文件。
- `矩阵/c12m1_capability_candidate_source_candidates.tsv`
- `矩阵/c12m1_capability_candidate_scope_review_matrix.tsv`
- `矩阵/c12m1_capability_candidate_blocker_queue.tsv`
- `矩阵/c12m1_capability_candidate_non_goal_registry.tsv`
- `矩阵/c12m1_capability_candidate_backend_gap_classification.tsv`
- `矩阵/c12m1_capability_candidate_validation_matrix.tsv`
- live `cad-core/cad-core capabilities` snapshot。

## 下一轮代码落点

S6 如果发现 `implementation_candidate` 或 `backendGap`，必须新增本节的最终表格，且至少包含：

| blocker / scope IDs | C++ landing files | FreeCAD source authority | focused tests / fixtures | success criteria | prohibited shortcut paths |
| --- | --- | --- | --- | --- | --- |
| 待 S6 填写 | 待 S6 填写 | 待 S6 填写 | 待 S6 填写 | 待 S6 填写 | 禁止 fixture-name branches、geometry-type guessing、bbox/area/order matching、adapter-layer business logic、cross-request shape/BREP state。 |

如果没有 implementation row，本节必须写成 `本轮无代码落点`，并列出保留原因。

## 发布闸门规则

- `C12M1-CAT-005` 有真实 implementation row：创建下一包名称、C++ landing、fixtures/tests、validation 和 non-goal。
- 只有 product decision 缺失：发布 `product_decision_needed`，不要写 C++。
- 只有 native oracle 缺失：发布 `oracle_blocked`，不要写 C++。
- 只有 historical/narrowed evidence：发布 `diagnostic_retained` 或 `no_code_retained_non_parity`。
- 没有候选：发布 `no_code_backlog_gate`，并建议下一步应先做产品决策或 native oracle probe，而非实现。

## 必须回写的矩阵行

- `C12M1-SCOPE-401`
- `C12M1-BLOCKER-601`
- `C12M1-CAT-004`
- `C12M1-CAT-005`
- `C12M1-VAL-601..606`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n 'implementation_candidate|no_code_backlog_gate|oracle_blocked|product_decision_needed|diagnostic_retained|backendGap' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次
rg -n '[ \t]$' docs/CADCore12.0
git diff --check
```

通过条件：

- 所有 S0-S6 文件均验证后重命名为 `【已实现】`。
- goal 队列为空。
- `backend_gap_classification.tsv` 只有一个最终 next action。
- 如果打开代码 gate，S6 已写清下一包 code landing 和 focused tests。
- 如果没有代码 gate，S6 已写清 retained/no-code 原因和 reopen condition。
- 验证后将本文件重命名为 `6-29-16-34-【已实现】C12-M1-S6-NextBatch发布闸门与代码授权.md`，并更新工作步骤索引与 `docs/CADCore12.0/README.md`。

## 非目标

- S6 不直接实现 C++。
- S6 不绕过 S0-S5 证据开代码。
- S6 不把 CopyOnChange、C11 helper notCollected 或 representative fallback 强行转成 implementation row。
