# C12-M5 SubShapeBinder CopyOnChange Request-Local DTO 准入复审批次方案

## 背景

C12-M4 已把 ProjectOnSurface request-local ledger 产品契约落到 expected / capability / adapter / 接口文档的公开口径里。当前 CADCore12 已没有 ProjectOnSurface 后续队列，live capability 中唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。

这个 gap 不能直接变成 C++ 实现任务。FreeCAD `SubShapeBinder` 的 CopyOnChange full path 依赖临时文档、copied-object 私有账本和 recompute lifecycle；C9-M5 / C10-M4 均确认现有证据只能支持 retained diagnostic。C12-M5 的价值是重新做一轮准入：如果当前 native evidence 或产品边界已有新变化，则把它升级为实现候选；否则明确保留为 diagnostic，避免继续循环讨论。

## 设计原则

| 原则 | 含义 |
| --- | --- |
| request graph 是唯一真实数据 | 后端仍无状态，不保存 temporary document、TopoDS、NamedShape、ElementMap 或 copied-object cache。 |
| FreeCAD source authority 优先 | 先读 `ShapeBinder.cpp`、`Link.cpp`、`Document.cpp` 的真实调用链，再决定 DTO 边界。 |
| App::Link 只能作参考 | `documentObjectUpdates` 词汇可复用，但不能自动证明 SubShapeBinder CopyOnChange supported。 |
| diagnostic 也是产品行为 | 若 evidence / DTO 不成立，继续发布 `copy_on_change_full_temporary_document_cache_not_supported` 是正确出口。 |

## 最小完整语义批次

本包不拆成单个 fixture，因为 `BindCopyOnChange`、`PartialLoad`、temporary document、copied object、support mutation、ElementMap lifecycle 和 `documentObjectUpdates` 是同一 FreeCAD CopyOnChange 调用链。拆成单个 case 容易误把 property 状态或 App::Link transport 当作完整 SubShapeBinder support。

本批次一次性覆盖：

- FreeCAD source authority。
- 旧 C9-M5 / C10-M4 evidence 和当前 capability / tests。
- native evidence 是否需要刷新。
- request-local DTO 字段与禁止字段。
- current mismatch / implementation gate。
- 发布闸门和后续分流。

## 关键问题

1. FreeCAD native 路径现在是否能稳定导出 copied-object identity、dependency order、support rewrite 和 ElementMap lifecycle？
2. 这些 evidence 是否能转成前端可保存的 DocumentObject graph / `documentObjectUpdates`，而不是后端 session cache？
3. `cad-core` 当前 retained diagnostic 是否和批准后的 DTO 产生真实 current mismatch？
4. 如果没有 mismatch，是否应关闭为 no-code retained diagnostic，而不是新增 fallback？

## 交付物

- `README.md`
- `6-30-01-03-C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次总入口.md`
- `工作步骤细分/`
- `矩阵/c12m5_copy_on_change_source_candidates.tsv`
- `矩阵/c12m5_copy_on_change_dto_contract_fields.tsv`
- `矩阵/c12m5_copy_on_change_scope_review_matrix.tsv`
- `矩阵/c12m5_copy_on_change_backend_gap_classification.tsv`
- `矩阵/c12m5_copy_on_change_blocker_queue.tsv`
- `矩阵/c12m5_copy_on_change_non_goal_registry.tsv`
- `矩阵/c12m5_copy_on_change_validation_matrix.tsv`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次 docs/CADCore12.0/README.md
git diff --check
```

