# 【已实现】C8-M2-S2 CopyOnChange DTO 准入与 oracle 候选矩阵

## 目标

把 S1 source / current coverage 复核转成 CopyOnChange DTO 准入、native oracle 候选、下游同步合同、blocker route、non-goal 和 implementation gate。S2 不采 oracle，不改 C++。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=73a5acf8a8`（`73a5acf8a8 docs: 完成 C8-M2 S1 源码与能力复核`）
- S2 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S2 文件；后续 pending 为 S3-S6。

## 分类规则

| route | 使用条件 | 后续 |
| --- | --- | --- |
| `already_supported` | C8-M1 已 expected-backed 支持 | S4/S5 同步发布 |
| `sync_required` | 下游需要消费 C8-M1 合同 | S4 写同步方案 |
| `oracle_candidate` | FreeCAD source 明确且 native probe 可能观察 | S3 采集 |
| `known_gap_retained` | 当前 known_gap 证据仍成立 | S5/S6 发布边界 |
| `backend_gap_candidate` | source 明确但缺 native expected | S3 后由 S6 裁决 |
| `diagnostic_non_goal` | GUI/session/persistent cache/Rust 下游 | S5 发布边界 |

## S2 分类结论

| 分类 | 本轮结论 | 后续边界 |
| --- | --- | --- |
| C8-M1 capability 下游同步 | `C8M2-SYNC-101` / `C8M2-SCOPE-101` 为 `sync_required`，只同步 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython`、covered、status 和 remaining_gaps | S4 写下游消费合同；不改 FreeCAD C++ |
| C8-M1 fixtures / expected / diagnostics 下游同步 | `C8M2-SYNC-102..103` 为 `sync_required`，同步 12 个 C8-M1 input、12 个 expected、`copy_on_change_full_temporary_document_cache_not_supported`、`known_gap_diagnostic` 和 `oracle_blocked` | S4/S5 发布合同；不得写成 supported |
| CopyOnChange Disabled / Enabled / Mutated property-state probe | `C8M2-ORACLE-101` 为 `oracle_candidate`，进入 S3 native probe | S3 只采证据；S6 才能裁决是否有 request-local DTO |
| PartialLoad allow-partial probe | `C8M2-ORACLE-102` 为 `oracle_candidate`，进入 S3 native probe | S3 观察 Python-visible state 与 allow-partial blocker |
| full temporary-document copied-object cache | `C8M2-ORACLE-103` / `C8M2-BG-301` 为 `known_gap_retained` | 不因 C8-M1 SubShapeBinder 主路径 supported 而关闭 lifecycle blocker |
| request-local DTO product decision | `C8M2-BG-101` 为 `backend_gap_candidate`，不是 implementation gate | 只有 S3 native evidence 加 product approval 后，S6 才能打开 C++ 实现 |
| GUI/session/persistent cache/Rust 下游 | `C8M2-BG-401` / `C8M2-SCOPE-301` / `C8M2-NG-008` 为 `diagnostic_non_goal` | S5 只发布边界；Rust 另开下游包 |

## 必须纳入同一轮的候选

- C8-M1 ShapeBinder / SubShapeBinder capability 下游同步。
- C8-M1 fixtures / expected / diagnostics 下游同步。
- CopyOnChange Disabled / Enabled / Mutated property-state probe。
- PartialLoad allow-partial native observability probe。
- Full temporary-document copied-object cache blocker。
- Request-local DTO product decision。

## 必须回写的矩阵行

- `C8M2-ORACLE-101..103`
- `C8M2-SYNC-101..103`
- `C8M2-BG-101..201`
- `C8M2-BLOCKER-201`

S2 已回写上述矩阵行，并同步补齐 `C8M2-BG-301` / `C8M2-BG-401` 以保持 `known_gap_retained` 与 `diagnostic_non_goal` 分类一致。`C8M2-BLOCKER-201` 关闭为 `closed_S2_dto_oracle_sync_and_gate_routes_classified_without_closing_S3_S6`；S3/S4/S5/S6 blocker 仍保持 pending。

## 矩阵回写

- `c8m2_copyonchange_oracle_plan.tsv`：`C8M2-ORACLE-101..103` 分别进入 property-state、PartialLoad allow-partial 和 full temporary-document cache blocker 候选；`C8M2-SYNC-101..103` 写成下游消费 C8-M1 capability / diagnostics / fixtures 的 `sync_required` 合同。
- `c8m2_copyonchange_scope_review_matrix.tsv`：把 C8-M1 capability、diagnostics、fixtures 保持为 `sync_required`；CopyOnChange DTO 保持 `backend_gap_candidate`；PartialLoad 保持 `oracle_candidate`；full temporary cache 保持 `known_gap_retained`；GUI/session/persistent cache/Rust 下游保持 `diagnostic_non_goal`。
- `c8m2_copyonchange_backend_gap_classification.tsv`：新增 `s2_route` 列，明确 `backend_gap_candidate` 只是候选，不是 S6 implementation gate。
- `c8m2_copyonchange_blocker_queue.tsv`：仅关闭 `C8M2-BLOCKER-201`；不关闭 S3/S4/S5/S6 blocker。
- `c8m2_copyonchange_non_goal_registry.tsv`：新增 S2 禁止项，确认 S2 不采 oracle、不新增 fixture/expected/tests/collector、不改 C++ 或 Rust。
- `c8m2_copyonchange_validation_matrix.tsv`：同步 S2 oracle-plan 验收说明。

## 下一步

进入 S3：只做 native CopyOnChange 生命周期探针与 blocker 证据。S3 可以采 native evidence 或 blocker evidence，但不能把 `backend_gap_candidate` 直接当成 C++ 实现闸门；S4/S5 继续承接下游同步和 capability 发布边界；S6 才能根据 S3 证据做 implementation/no-code 裁决。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-ORACLE|C8M2-SYNC|sync_required|oracle_candidate|known_gap_retained|backend_gap_candidate|diagnostic_non_goal|CopyOnChange|PartialLoad' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 docs/CADCore8.0/README.md
git diff --check
```

验收通过后，将本文件重命名为 `6-26-22-23-【已实现】C8-M2-S2-CopyOnChangeDTO准入与oracle候选矩阵.md`。

## 非目标

- 不把 `backend_gap_candidate` 当作 S6 implementation gate。
- 不因 C8-M1 已支持 SubShapeBinder 主路径就关闭 CopyOnChange lifecycle blocker。
- 不采 FreeCAD oracle。
- 不新增 fixture / expected / tests / collector。
- 不修改 C++ 或 Rust。
- 不关闭 S3/S4/S5/S6 blocker。
