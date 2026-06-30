# C12-M5 SubShapeBinder CopyOnChange Request-Local DTO 准入复审批次总入口

## 包目标

C12-M5 处理当前 live capability 中唯一剩余公开 gap：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。本包不是直接实现 temporary document cache，而是判断是否存在可批准的 request-local CopyOnChange DTO 路线。

## 创建基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=0709323947`。
- `git log -1 --oneline=0709323947 docs: 落地 C12-M4 产品契约公开口径`。
- `git -c core.quotepath=false status --short -uall` 输出为空。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 队列均为空。
- `part_workbench.project_on_surface.status=supported_expected_backed_product_contract`，`remaining_gaps=[]`。
- `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。

## S0 live 冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=84179ae66d`。
- `git log -1 --oneline=84179ae66d docs: 新增 C12-M5 CopyOnChange 准入方案`。
- `git -c core.quotepath=false status --short -uall` 输出为空，dirty boundary 为 `<clean>`。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 队列均为空。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache.status=known_gap_diagnostic`，`route=oracle_blocked`，`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- S0 继承 C9-M5 `no_code_retained_known_gap_release_gate` 与 C10-M4 no-code retained diagnostic / release gate；App::Link `documentObjectUpdates` 仍只是 reference-only DTO 路径。

## 当前结论

1. CopyOnChange 是唯一剩余公开 gap，但当前 route 是 `oracle_blocked`，不是直接 implementation route。
2. C9-M5 / C10-M4 仍是有效前置结论：没有稳定 copied-object expected，没有产品批准 DTO，没有 current mismatch。
3. S1 已复核 FreeCAD source/current coverage：SubShapeBinder full CopyOnChange 依赖 `_tmp_binder`、`_CopiedObjs`、`copyObject()` 和 `recomputeFeature(true)` 的真实 Document 生命周期；App::Link `documentObjectUpdates` 仍只是 reference-only DTO 路径。
4. current `cad-core` 只覆盖 `BindMode=Detached` request-local writeback 子集；`BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续保持 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。
5. C12-M5 后续仍必须验证 native evidence、产品 DTO 和 current mismatch 是否同时成立；不能从 remaining gap 或 App::Link coverage 直接推出代码任务。

## 决策顺序

1. S0 冻结 live baseline、旧结论和 forbidden claims（已完成）。
2. S1 复核 FreeCAD source authority 与 current cad-core coverage（已完成，`C12M5-BLOCKER-101` 已关闭）。
3. S2 判断 native evidence 是否足够，必要时设计 probe，而不是先写实现。
4. S3 冻结产品 DTO 边界：允许字段、禁止字段、前端写回职责。
5. S4 做 current mismatch gate：只有 approved DTO 与 current diagnostic 冲突才进入 implementation candidate。
6. S5 发布出口：implementation package、oracle refresh、DTO rejected 或 retained diagnostic。

## 非目标

- 不实现 persistent backend session。
- 不保存 full copied-object BREP / TopoDS / NamedShape / ElementMap cache。
- 不把 App::Link CopyOnChange coverage 自动推广到 SubShapeBinder。
- 不靠 adapter 或前端 mock 生成 CopyOnChange 语义。
- 不删除现有 diagnostic，除非后续 implementation 已替代它并通过 focused tests。

## 交付物

- `README.md`
- `6-30-01-03-C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次方案.md`
- `工作步骤细分/`
- `矩阵/`

## 验收命令

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次 docs/CADCore12.0/README.md
git diff --check
```
