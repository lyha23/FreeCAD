# C12-M8 SubShapeBinder CopyOnChange Native Copied Graph DTO 准入解锁批次

C12-M8 用于重新打开 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 的准入链路，但不是直接 C++ implementation 包。

C12-M5 已关闭为 `no_code_retained_diagnostic`：旧 native evidence 只能证明 FreeCAD property / session 状态，不能证明 copied-object graph、dependency order、support rewrite 和 ElementMap lifecycle 可以稳定转成 request-local DTO。C12-M8 的变化点是把解锁条件拆清楚：先证明 FreeCAD 原生 CopyOnChange 能导出完整 copied graph evidence，再裁决哪些字段允许进入前端持久化的 DocumentObject graph / `documentObjectUpdates`，最后才看 current `cad-core` retained diagnostic 是否形成真实 mismatch。

## 当前基线

- 创建基线：`HEAD=1d03274e9e`（`1d03274e9e docs: 发布 C12-M7 S5 产品诊断契约出口`）。
- 创建时 worktree clean，C12-M1..M7 工作步骤队列均为空。
- live capability 仍为 `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`。
- live capability 仍发布 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- known gap 仍为 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- `link_transaction.copy_on_change_writeback_contract` 已支持 persisted CopyOnChange copied graph 的 `documentObjectUpdates` transport，但它只证明 App::Link 侧 transport 词汇存在，不自动证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` lifecycle 可用。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S0 执行 HEAD：`fd9810dc23`（`fd9810dc23 文档：新增 C12-M8 CopyOnChange 解锁方案`）。
- S0 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 `<clean>`；未发现非本任务 dirty work。
- C12-M1..M7 `工作步骤细分` 队列均只输出表头。
- `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache.status=known_gap_diagnostic`，`route=oracle_blocked`，`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- delete condition：只有 FreeCADCmd 暴露不依赖 persistent backend session 的 stable request-local CopyOnChange copied-object evidence 后，才能替换该 diagnostic。
- reopen condition：只有产品批准由更强 native oracle 支撑的 request-local CopyOnChange DTO 后，才重新打开实现判断。
- C12-M5 `no_code_retained_diagnostic` 继续有效：S2=`native_evidence_retained_blocker`，S3=`dto_rejected_known_gap_retained`，S4=`no_current_mismatch_retained_diagnostic`。
- C12-M7 后续分流口径继续有效：Groove UpTo 已发布 `product_diagnostic_contract_published`，只有同一 FreeCAD / LibPack / OCCT oracle baseline 证明 native success 且 current mismatch 时才另开 geometry implementation candidate；本包不重开 Groove 或 RuledSurface。

## S1 source / current 复核

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=79602c1072`（`79602c1072 fix: 修复 Body replay 默认 RefineModel 语义`），起点 worktree clean。
- FreeCAD `SubShapeBinder::setupCopyOnChange()` 入口条件已复核：`BindCopyOnChange != Disabled` 且 `Support.getSubListValues().size()==1`；否则清理动态 CopyOnChange property 并返回。
- FreeCAD `SubShapeBinder::update()` 的 `BindCopyOnChange=Mutated` 路径已复核：新建 temporary `_tmp_binder`，调用 `copyObject({obj}, true, true)`，填充 `_CopiedObjs`，先后执行 `recomputeFeature(true)`，并用 `_CopiedLink` 指向 copied object 的 subvalues。
- `PartialLoad` 是可持久化输入和 `canLoadPartial()` / `Support.setAllowPartial()` 边界；`Cache_*` 是 dynamic matrix cache 的 hit/update/cleanup 优化边界，不能默认写成后端 persistent semantic state。
- current `cad-core` 仍只支持 `BindMode` request-local 子集；`BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续发布 `copy_on_change_full_temporary_document_cache_not_supported` retained diagnostic。
- App::Link CopyOnChange `documentObjectUpdates` transport 已记录为 reference-only：它提供 group sync、copy object、dependency rewrite 和 link writeback 词汇，但不证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` lifecycle 已支持。
- `C12M8-SRC-001..008`、`C12M8-BLOCKER-101`、`C12M8-BLOCKER-102` 已关闭；S2 probe 必须证明 baseline、mode matrix、single support gate、temporary binder lifecycle、copied object identities、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle 和 `Cache_*` 边界。

## 解锁条件

只有以下三项同时成立，C12-M8 才允许产出后续 implementation package：

1. Native copied graph evidence ready：FreeCADCmd artifact 能稳定暴露 `_CopiedObjs`、`_CopiedLink`、copied dependency order、support rewrite、`recomputeFeature(true)` 后状态和 ElementMap / NamedShape lifecycle。
2. Request-local DTO approved：批准字段只表达前端可保存的 DocumentObject graph / `documentObjectUpdates`，不引入 backend session、temporary document、TopoDS、BREP、persistent NamedShape / ElementMap cache。
3. Current mismatch confirmed：当前 `cad-core` retained diagnostic 与批准 DTO 之间存在真实 mismatch，且 mismatch 不能通过文档 wording 解释。

任何一项不成立，最终出口都应继续保留 `known_gap_diagnostic` / `oracle_blocked`，不改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source。

## FreeCAD / CAD Core 依据

- `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()`：`BindCopyOnChange` 与 `support.size()==1` 是 CopyOnChange lifecycle 入口条件。
- `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()`：`BindCopyOnChange=Mutated` 路径使用 `_tmp_binder`、`copyObject()`、`_CopiedObjs`、`recomputeFeature(true)` 和 `_CopiedLink`。
- `src/Mod/PartDesign/App/ShapeBinder.h`：定义 `PartialLoad`、`BindCopyOnChange`、`_CopiedLink`、`_CopiedObjs`。
- `src/App/Document.cpp::Document::copyObject()` 与 `Document::recomputeFeature()`：copied-object graph 与 recompute lifecycle 的 FreeCAD authority。
- `src/App/Link.cpp::LinkBaseExtension::*CopyOnChange*`：App::Link transport / `documentObjectUpdates` 参考词汇，不能单独当作 SubShapeBinder 支持证据。
- `cad-core/src/part_design/feature_shape_binder.cpp` 与 `cad-core/tests/test_c8_shapebinder.py`：当前 retained diagnostic 与 BindMode request-local 子集。
- `cad-core/src/app/copy_on_change.cpp` 与 `cad-core/include/cad_core/app/copy_on_change.h`：已存在 App::Link CopyOnChange writeback vocabulary。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py`：公开 capability / adapter assertion 权威。

## 工作步骤

- S0：live 基线与 C12-M5/C12-M7 继承口径冻结（已完成）。
- S1：FreeCAD source、current coverage 和 existing transport evidence 复核（已完成）。
- S2：native copied graph probe schema 与 evidence gate。
- S3：request-local DTO 产品边界裁决。
- S4：current mismatch 与 implementation candidate gate。
- S5：implementation package authorization / no-code retained decision。
- S6：发布闸门、README 更新和后续分流。

## 入口

- 总入口：`6-30-21-24-C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次总入口.md`
- 方案：`6-30-21-24-C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次 docs/CADCore12.0/README.md
git diff --check
```
