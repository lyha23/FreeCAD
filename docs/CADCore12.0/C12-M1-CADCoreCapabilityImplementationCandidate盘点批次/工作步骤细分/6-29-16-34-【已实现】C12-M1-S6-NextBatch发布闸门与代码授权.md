# 【已实现】C12-M1 S6 NextBatch 发布闸门与代码授权

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

## S6 最终结论

S6 起点确认：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=c14ece91e7
git log -1 --oneline=c14ece91e7 docs: 完成 C12-M1 S5 历史证据复审
git -c core.quotepath=false status --short -uall=<clean>
```

`cad-core/cad-core capabilities > /tmp/c12-s6-capabilities.json` 的 live snapshot 与 S0-S5 结论一致：唯一 active `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，状态为 retained known gap / `oracle_blocked`；Assembly marker/writeback 已是 expected-backed current-covered request-local subset；Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 均没有 stable expected/current mismatch。

因此 C12-M1 最终 next action 是 `no_code_backlog_gate`。本轮无代码落点，不授权 C++、fixtures、expected、oracle 采集、capability wording 或 adapter/test 改动。`C12M1-CAT-005` 继续保持 `none_s2_placeholder`，截至 S6 没有 implementation candidate。

## 下一轮代码落点

本轮无代码落点。

| blocker / scope IDs | C++ landing files | FreeCAD source authority | focused tests / fixtures | success criteria | prohibited shortcut paths |
| --- | --- | --- | --- | --- | --- |
| `C12M1-SCOPE-401`;`C12M1-BLOCKER-601` | 本轮无代码落点 | S0-S5 已消费所有 FreeCAD source authority；没有新 source/current mismatch 组合 | 不新增 fixtures/tests/expected | 发布 `no_code_backlog_gate`，队列清空 | 禁止 fixture-name branches、geometry-type guessing、bbox/area/order matching、adapter-layer business logic、cross-request shape/BREP state。 |

## retained / no-code 原因与重开条件

| family | S6 裁决 | retained / no-code 原因 | reopen condition |
| --- | --- | --- | --- |
| SubShapeBinder CopyOnChange | `oracle_blocked` / retained known gap | S3 确认没有 stable native copied-object expected；没有产品批准的 SubShapeBinder request-local DTO；current cad-core 仍匹配 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。 | 同时具备 stable native copied-object expected、产品批准 request-local DTO、current cad-core mismatch。 |
| Assembly representative / marker / writeback | `product_decision_needed` retained boundary，但本轮不授权代码 | S4 确认 representative solver 只是 `available=false` fallback metadata；marker/writeback 已有 expected-backed current-covered request-local subset；没有产品批准的新 representative subset 和 expected/current mismatch。 | 产品批准 request-local subset，补 expected/current mismatch，并保持 full solver、persistent solver state、cross-request session 为 non-goal。 |
| Sweep / Filling / GeomPlate / Loft / ProjectOnSurface | `diagnostic_retained` / no-code retained historical evidence | S5 确认 historical `notCollected`、native-hidden、probe-only 或 product-contract non-parity 不能证明 backend gap；五类 Part Workbench 行均无 stable native/request-local expected 加 current mismatch。 | 对具体 Part Workbench 行取得 stable native/request-local expected，并证明 current cad-core mismatch；不得从 crash、timeout、TypeError、`notCollected` 或 helper wrapper lifecycle 直接开代码。 |
| `implementation_candidate` placeholder | `none_s2_placeholder` retained | S0-S5 没有任何行同时满足 source authority、stable expected、current mismatch、request-local 边界和 focused tests。 | 未来矩阵新增真实 scope row 后由新的 release gate 重新授权。 |

## S6 回写结果

- `C12M1-SCOPE-401` 关闭为 `closed_s6_no_code_backlog_gate`。
- `C12M1-BLOCKER-601` 关闭为 `closed_s6_no_code_backlog_gate_published`。
- `C12M1-CAT-004` 关闭为 `closed_s6_no_code_backlog_gate`。
- `C12M1-CAT-005` 关闭为 `closed_s6_no_implementation_candidate`，`scope_ids` 仍为 `none_s2_placeholder`。
- `C12M1-VAL-601..606` 记录 S6 docs-only 验收：队列为空、TSV 字段一致、无 trailing whitespace、`git diff --check` 通过；capability tests 与 build 因未改代码不运行。

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
