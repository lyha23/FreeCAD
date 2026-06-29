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
- 当前包状态：S0 live 基线已冻结；S1-S6 待执行；工作步骤索引已实现。

## 关键判断

S6 只有在以下条件同时成立时才能进入 C++ 实现：

1. FreeCAD native probe 能稳定暴露 copied-object evidence，不要求 backend 持有 session。
2. 产品批准 request-local DTO 子集，并明确前端 graph writeback 语义。
3. current cad-core 与该 native expected 存在可复现 mismatch。
4. 实现落点保持在 `part_design` / `app` / `runtime` / capability / tests，不把业务逻辑塞到 adapter 输出修剪。

任一条件不成立，S6 发布 no-code retained diagnostic / release gate。

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
