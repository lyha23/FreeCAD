# C12-M5 SubShapeBinder CopyOnChange Request-Local DTO 准入复审批次

C12-M5 承接 C12-M4 公开口径迁移完成后的 live capability 状态。当前 CADCore12 队列均为空，`part_workbench.project_on_surface.remaining_gaps=[]`，唯一仍公开存在的 capability gap 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。

本包不直接实现 FreeCAD 的 temporary document cache，也不把 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 改写成 supported。C9-M5 与 C10-M4 已多次复审并保持该项为 `known_gap_diagnostic` / `oracle_blocked`。C12-M5 的目标是做一次新的准入复审：只有在 FreeCAD native copied-object evidence、产品批准的 request-local DTO 边界和 current `cad-core` mismatch 同时成立时，才打开后续 C++ implementation gate；否则继续保留结构化 diagnostic。

## Live 基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线 HEAD：`0709323947`（`0709323947 docs: 落地 C12-M4 产品契约公开口径`）。
- 创建前工作区：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 `工作步骤细分` 队列均只输出表头。
- `cad-core/build/cad-core capabilities` 显示 `known_gaps=[]`，`part_workbench.project_on_surface.remaining_gaps=[]`，`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S0 执行 HEAD：`84179ae66d`（`84179ae66d docs: 新增 C12-M5 CopyOnChange 准入方案`）。
- S0 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 `工作步骤细分` 队列均只输出表头。
- 当前 capability 仍只有 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- 当前 known gap 为 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- S0 已冻结 forbidden claims：不把 remaining gap 直接写成 full support；不声明 backend session、persistent temporary document、cross-request BREP / TopoDS / NamedShape / ElementMap cache；不把 App::Link CopyOnChange `documentObjectUpdates` 自动等同为 SubShapeBinder CopyOnChange supported。

## S1 source/current 覆盖

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S1 执行 HEAD：`de93b702b0`（`de93b702b0 docs: 完成 C12-M5 S0 live 基线冻结`）。
- S1 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- FreeCAD `SubShapeBinder::execute()` 先 `setupCopyOnChange()` 再按 `BindMode=Synchronized` 调用 `update(UpdateForced)`；`setupCopyOnChange()` 只在 `BindCopyOnChange!=Disabled` 且单一 `Support` 时接入 `LinkBaseExtension::setupCopyOnChange()`。
- FreeCAD `checkCopyOnChange()` 在 Enabled、单一 Support、非 transaction 且 dynamic CopyOnChange property 变化时把 `BindCopyOnChange` 推进到 Mutated。
- FreeCAD Mutated 主路径在 `update()` 中创建或复用 `_CopiedObjs`：缺少有效 copied object 时新建临时文档 `_tmp_binder`，调用 `tmpDoc->copyObject({obj}, true, true)`，先 `recomputeFeature(true)` 生成正确 geometry element map，再复制属性并按需再次 `recomputeFeature(true)`，最后写 `_CopiedLink`。
- `Document::copyObject()` 依赖 dependency list 与 `exportObjects()` / `MergeDocuments::importObjects()` 的真实 Document 生命周期；这说明 SubShapeBinder CopyOnChange 的完整 copied-object lifecycle 不能仅靠 request-local scalar field 推出 supported。
- App::Link `documentObjectUpdates` 是 reference-only：current `cad-core` 已有 App::Link CopyOnChange create/update/delete 建议，但该路径不等同于 SubShapeBinder `_tmp_binder` / `_CopiedObjs` 支持。
- current `cad-core` 只确认 `BindMode=Detached` 的 request-local `Support` clear writeback 子集；`BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续发布 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic，capability 仍是 `known_gap_diagnostic` / `oracle_blocked`。
- `C12M5-BLOCKER-101` 已关闭；S1 不打开 implementation candidate，后续只进入 S2 native evidence / probe 准入复核。

## 继承口径

- C9-M5 裁决：CopyOnChange full temporary-document copied-object lifecycle 依赖 `_tmp_binder`、`_CopiedObjs`、`copyObject()`、`recomputeFeature(true)` 和 `_CopiedLink`，S6 关闭为 `no_code_retained_known_gap_release_gate`。
- C10-M4 裁决：App::Link `documentObjectUpdates` 是 DTO 参考路径，但不等同于 SubShapeBinder CopyOnChange 支持；S6 保持 `known_gap_diagnostic` / `oracle_blocked`。
- 当前 `cad-core/src/part_design/feature_shape_binder.cpp` 对 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续发布 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。

## 包目标

1. 冻结当前 live capability 和旧复审结论，避免把 retained gap 误写成实现任务。
2. 复核 FreeCAD `SubShapeBinder::setupCopyOnChange()` / `checkCopyOnChange()` / `update()`、`LinkBaseExtension::setupCopyOnChange()` 和 `Document::copyObject()` 的 source authority。
3. 判断是否能采到稳定、可序列化、完全 request-local 的 copied-object evidence。
4. 决定产品 DTO 是否允许前端 graph 承载 copied-object snapshot、copy intent、mutation delta 或 `documentObjectUpdates`。
5. 在 S4/S5 发布实现分流：`implementation_candidate` 或 `no_code_retained_diagnostic`。

## 非目标

- 不实现 backend session、persistent temporary document 或跨请求 cache。
- 不保存 BREP、TopoDS_Shape、NamedShape、ElementMap 或完整 copied-object graph 作为后端状态。
- 不把 App::Link CopyOnChange group lifecycle 直接等同为 SubShapeBinder CopyOnChange supported。
- 不通过 adapter 修补、frontend mock、fixture 名称、输出顺序或几何猜测伪造 copied-object ownership。
- 不扩大 `ReferenceShadow.brep` 的 single-subshape evidence 例外为 full object BREP persistence。

## 工作步骤

- S0：live 基线与继承口径冻结（已完成）。
- S1：FreeCAD 源码与 current 覆盖矩阵（已完成）。
- S2：native evidence 刷新与 probe 准入。
- S3：request-local DTO 产品边界冻结。
- S4：current mismatch 与实现候选闸门。
- S5：发布闸门与后续分流。

## 出口

- `implementation_candidate`：只有 native evidence、产品 DTO 和 current mismatch 同时成立时发布，并另开 C++ implementation package。
- `oracle_refresh_required`：source authority 明确但 evidence 不够，需要先补 probe / collector。
- `dto_rejected_known_gap_retained`：产品边界拒绝 request-local DTO，继续保留 diagnostic。
- `no_code_retained_diagnostic`：没有实现候选，capability 继续发布 `known_gap_diagnostic` / `oracle_blocked`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
```
