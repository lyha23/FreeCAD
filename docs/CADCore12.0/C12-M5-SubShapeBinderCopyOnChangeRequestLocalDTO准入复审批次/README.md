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

## S2 native evidence / probe 准入

- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S2 执行 HEAD：`b38b0647d6`（`b38b0647d6 docs: 完成 C12-M5 S1 source 覆盖复核`）。
- S2 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- S2 已复核 `C9-M5 CopyOnChange native probe（已移除）`、`C8-M2 CopyOnChange native probe（已移除）`、`collect_c8m1_shapebinder_expected.py`、C9-M5 expected 和 C9/C10 README；未修改 probe、expected、fixture、C++、tests 或 adapter。
- C9-M5 probe schema 仍覆盖 `BindCopyOnChange=Disabled/Enabled/Mutated`、`PartialLoad=True`、dynamic CopyOnChange property、mutation-triggered Mutated、`_tmp_binder` document name、`_CopiedLink` 和 Python-visible property state。
- checked-in expected 基线仍是 FreeCAD `1.2.0 revision 20260519`，route 为 `native_evidence_collected_with_known_gap_blocker`；可观察字段仍限于 property/session evidence、`_tmp_binder` document name 和 `_CopiedLink` 单值。
- `_CopiedObjs` 仍不可通过 Python API 观察；`copyObject()` dependency order、copied support rewrite 的完整 graph、`recomputeFeature(true)` internal ElementMap lifecycle 仍不能导出为稳定 request-local DTO。
- 因此 `C12M5-BLOCKER-201` 关闭为 `native_evidence_retained_blocker`；S2 不设计新 probe schema，不采新 oracle，不打开 C++ implementation candidate。S3 只能继续做 DTO 产品边界裁决。

## S3 request-local DTO 产品边界

- S3 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S3 执行 HEAD：`239a866ad9`（`239a866ad9 docs: 完成 C12-M5 S2 native 证据复核`）。
- S3 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- 允许持久化的 graph 字段只包括 `Objects[]`、`Name`、`ID`、`TypeId`、`Properties`、`BindCopyOnChange`、`PartialLoad`、`Support` / `App::PropertyLinkSub`、`SubList`、`StableSubList`、`FullSubList`、source id/name、support subname、diagnostic，以及被前端应用后才成为下一次输入的 `documentObjectUpdates`。
- `copy_intent` 与 `mutation_delta` 只能作为未来 request-local DTO 设计词汇；S2 没有 stable copied-object graph evidence 时，不能证明前端可把 SubShapeBinder copied-object lifecycle 完整写回 `DocumentObject graph`。
- 禁止字段保持为 temporary document、native object pointer、TopoDS / full object BREP、post-request `NamedShape` / `ElementMap` cache、`_CopiedObjs` private vector、`_tmp_binder` session state、adapter / frontend mock 和 output guessing。
- App::Link `documentObjectUpdates` 仍是 reference-only；`ReferenceShadow.brep` 仍只允许单个旧 subshape snapshot，不扩展成 full object BREP。
- S3 裁决为 `dto_rejected_known_gap_retained`，`C12M5-BLOCKER-301` 已关闭；S4 只能在 DTO 已拒绝前提下做 current mismatch gate，不打开 implementation candidate。

## S4 current mismatch / implementation candidate gate

- S4 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S4 执行 HEAD：`f7c1e83960`（`f7c1e83960 docs: 完成 C12-M5 S3 DTO 边界冻结`）。
- S4 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- S2 不是 `native_evidence_ready`，而是 `native_evidence_retained_blocker`；S3 不是 `dto_approved_for_mismatch_gate`，而是 `dto_rejected_known_gap_retained`。
- current `feature_shape_binder.cpp` 仍在 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 时发布 `copy_on_change_full_temporary_document_cache_not_supported`。
- current capability 仍是 `known_gap_diagnostic` / `oracle_blocked`，focused tests 仍覆盖 retained diagnostic 和 capability wording。
- 因此没有 approved DTO 与 current retained diagnostic 的冲突，S4 关闭为 `no_current_mismatch_retained_diagnostic`，不创建 implementation package。

## S5 发布闸门

- S5 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S5 执行 HEAD：`c1f02e3d7e`（`c1f02e3d7e docs: 完成 C12-M5 S4 实现候选闸门`）。
- S5 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- S5 逐行复核 source、scope、DTO、backend classification、blocker、non-goal 和 validation 矩阵后确认：S2=`native_evidence_retained_blocker`，S3=`dto_rejected_known_gap_retained`，S4=`no_current_mismatch_retained_diagnostic`。
- C12-M5 最终出口为 `no_code_retained_diagnostic`。不创建 implementation package，不刷新 oracle，不改 C++、fixtures、expected、tests 或 adapter。
- capability 继续保留 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- known gap 继续保留 `status=known_gap_diagnostic`、`route=oracle_blocked`、diagnostic=`copy_on_change_full_temporary_document_cache_not_supported`。
- 后续只在同时满足 stable native copied-object graph evidence、产品批准 request-local DTO、current `cad-core` mismatch 时重开；future reopen condition 不写成 current supported。

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
- S2：native evidence 刷新与 probe 准入（已完成）。
- S3：request-local DTO 产品边界冻结（已完成，`dto_rejected_known_gap_retained`）。
- S4：current mismatch 与实现候选闸门（已完成，`no_current_mismatch_retained_diagnostic`）。
- S5：发布闸门与后续分流（已完成，`no_code_retained_diagnostic`）。

## 出口

- `implementation_candidate`：只有 native evidence、产品 DTO 和 current mismatch 同时成立时发布，并另开 C++ implementation package。
- `oracle_refresh_required`：source authority 明确但 evidence 不够，需要先补 probe / collector。
- `dto_rejected_known_gap_retained`：产品边界拒绝 request-local DTO，继续保留 diagnostic。
- `no_code_retained_diagnostic`：C12-M5 最终出口；没有实现候选，capability 继续发布 `known_gap_diagnostic` / `oracle_blocked`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
```
