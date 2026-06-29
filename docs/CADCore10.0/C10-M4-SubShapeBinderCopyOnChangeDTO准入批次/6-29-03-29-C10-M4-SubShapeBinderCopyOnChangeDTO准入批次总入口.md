# C10-M4 SubShapeBinder CopyOnChange DTO 准入批次总入口

## 目标

把 live capability 中唯一仍非空的 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 拆成可执行准入批次。C10-M4 不直接实现 FreeCAD 的 full temporary-document cache，而是判断是否存在一个被 FreeCAD native evidence 支撑、被产品边界批准、并能在 cad-core 无状态架构里表达的 request-local CopyOnChange DTO。

## 基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- seed HEAD：`3c21f08005`
- S0 live HEAD：`cd8cd95fa2`（`cd8cd95fa2 docs: 新增 C10-M4 CopyOnChange DTO 准入方案`）。
- S0 起始 dirty state：工作区干净。
- 上游闭环：C10-M1 / C10-M2 / C10-M3 队列为空。
- live retained gap：`copy_on_change_full_temporary_document_cache_not_supported`。
- 当前包状态：S0-S6 已完成；工作步骤索引已实现；C10-M4 队列已关闭。

## 关键判断

S6 只有在以下条件同时成立时才能进入 C++ 实现：

1. FreeCAD native probe 能稳定暴露 copied-object evidence，不要求 backend 持有 session。
2. 产品批准 request-local DTO 子集，并明确前端 graph writeback 语义。
3. current cad-core 与该 native expected 存在可复现 mismatch。
4. 实现落点保持在 `part_design` / `app` / `runtime` / capability / tests，不把业务逻辑塞到 adapter 输出修剪。

任一条件不成立，S6 发布 no-code retained diagnostic / release gate。本轮 S3 copied-object evidence 为 `notCollected`，S4 DTO 边界为 reference-only / `product_decision_needed` / `current_retained_diagnostic`，S5 no-session non-goal 已关闭且没有 backend gap candidate；S6 因此只发布 docs-only no-code retained diagnostic / release gate。

## 发布闸门结果

- S6 执行起点 HEAD：`376e3dba31`（`docs: 完成 C10-M4 S5 non-goal 边界复审`），起点工作区干净。
- 当前矩阵没有任何 `route` / `current_status` / `status` 列被分类为 `backend_gap_candidate`；实现条件未成立。
- 未修改 `cad-core/src`、tests、fixtures、expected、probe、collector 或 capability；未运行 build / focused tests。
- `cad-core/src/runtime/capability_contract.cpp` 继续保留 `copy_on_change_full_temporary_document_cache` 的 `known_gap_diagnostic` / `oracle_blocked` / `remaining_gaps` 口径，unsupported `BindCopyOnChange` / `PartialLoad` 继续输出结构化 diagnostic。
- `C10M4-SCOPE-401=release_closed`，`C10M4-BLOCKER-601=closed_s6`，`C10M4-CAT-105=release_closed`；队列为空。

## 文件结构

```text
C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/
  README.md
  6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次总入口.md
  6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次方案.md
  工作步骤细分/
  矩阵/
```

## 矩阵

- `c10m4_copy_on_change_dto_source_candidates.tsv`
- `c10m4_copy_on_change_dto_scope_review_matrix.tsv`
- `c10m4_copy_on_change_dto_blocker_queue.tsv`
- `c10m4_copy_on_change_dto_non_goal_registry.tsv`
- `c10m4_copy_on_change_dto_backend_gap_classification.tsv`
- `c10m4_copy_on_change_dto_validation_matrix.tsv`

## 通用验收

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
