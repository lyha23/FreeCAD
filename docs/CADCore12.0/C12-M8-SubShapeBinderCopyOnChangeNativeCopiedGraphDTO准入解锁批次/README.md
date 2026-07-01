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
- C12-M5 `no_code_retained_diagnostic` 继续有效：S2=`native_evidence_retained_blocker`，S3 继承 retained path；本包已将 S3 精化为 `dto_not_reviewed_due_to_native_blocker`。
- C12-M7 后续分流口径继续有效：Groove UpTo 已发布 `product_diagnostic_contract_published`，只有同一 FreeCAD / LibPack / OCCT oracle baseline 证明 native success 且 current mismatch 时才另开 geometry implementation candidate；本包不重开 Groove 或 RuledSurface。

## S1 source / current 复核

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=79602c1072`（`79602c1072 fix: 修复 Body replay 默认 RefineModel 语义`），起点 worktree clean。
- FreeCAD `SubShapeBinder::setupCopyOnChange()` 入口条件已复核：`BindCopyOnChange != Disabled` 且 `Support.getSubListValues().size()==1`；否则清理动态 CopyOnChange property 并返回。
- FreeCAD `SubShapeBinder::update()` 的 `BindCopyOnChange=Mutated` 路径已复核：新建 temporary `_tmp_binder`，调用 `copyObject({obj}, true, true)`，填充 `_CopiedObjs`，先后执行 `recomputeFeature(true)`，并用 `_CopiedLink` 指向 copied object 的 subvalues。
- `PartialLoad` 是可持久化输入和 `canLoadPartial()` / `Support.setAllowPartial()` 边界；`Cache_*` 是 dynamic matrix cache 的 hit/update/cleanup 优化边界，不能默认写成后端 persistent semantic state。
- current `cad-core` 仍只支持 `BindMode` request-local 子集；`BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续发布 `copy_on_change_full_temporary_document_cache_not_supported` retained diagnostic。
- App::Link CopyOnChange `documentObjectUpdates` transport 已记录为 reference-only：它提供 group sync、copy object、dependency rewrite 和 link writeback 词汇，但不证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` lifecycle 已支持。
- `C12M8-SRC-001..008`、`C12M8-BLOCKER-101`、`C12M8-BLOCKER-102` 已关闭；S2 probe 必须证明 baseline、mode matrix、single support gate、temporary binder lifecycle、copied object identities、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle 和 `Cache_*` 边界。

## S2 native evidence gate

- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=f5ed6d2397`（`f5ed6d2397 文档：关闭 C12-M8 S1 源码复核`），起点 worktree clean。
- S2 执行前队列确认：S2 是第一条 pending，后续为 S3-S6。
- 本机 FreeCADCmd 可运行；本轮只写入 `docs/temp/`，不刷新 checked-in expected。
- FreeCAD / OCCT baseline：`freecadcmd=/Users/li/.cargo/bin/freecadcmd`，FreeCAD `1.2.0 revision 20260519`，OCCT `7.8.1`。
- raw native artifact：`docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-probe.raw.c9m5.freecad.json`。
- C12-M8 gate artifact：`docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json`，schema 为 `c12m8.subshapebinder-copy-on-change-native-copied-graph.v1`。
- S2 裁决：`native_evidence_retained_blocker`。旧 C9M5 probe 能证明 Python-visible property / session 状态、`_tmp_binder` document name 和部分 `_CopiedLink` value，但不能证明 stable copied-object graph evidence。
- 核心缺口仍是 `_CopiedObjs` identity、copyObject dependency order、support rewrite map、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle 和 request-local serializability。
- `Cache_*` 已分类为 transform matrix cache hit/update/cleanup optimization；backend persistent `Cache_*` 继续禁止。
- `C12M8-PROBE-001..010` 均已写 observed_status / decision / artifact_or_note；`C12M8-BLOCKER-201` 已关闭为 `closed_s2_retained_blocker`。
- S3/S4 必须继承 S2 retained blocker，不得把 property 状态、label、bbox、shape count、temporary document name 或 `_CopiedLink` target 单独当作 implementation approval 证据。

## S3 request-local DTO 边界

- S3 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=557c5be617`（`557c5be617 文档：关闭 C12-M8 S2 native evidence gate`），起点 worktree clean。
- S3 执行前队列确认：S3 是第一条 pending，后续为 S4-S6。
- S3 裁决：`dto_not_reviewed_due_to_native_blocker`。由于 S2 没有输出 `native_copied_graph_evidence_ready`，本步不批准完整 CopyOnChange request-local DTO，也不允许 S4/S5 进入 implementation approval。
- `C12M8-DTO-001..004` copied object create、property writeback、link rewrite、support sublist rewrite 均是未来可能允许的 frontend-persisted graph / `documentObjectUpdates` 方向，但本轮因 S2 缺 stable copied graph evidence 裁决为 `deferred`。
- `C12M8-DTO-005` 只批准为 input-only `BindCopyOnChange` request graph 字段；`C12M8-DTO-006` `PartialLoad=True` 仍因 native blocker 裁决为 `deferred`。
- `C12M8-DTO-007..012` temporary document handle、native pointer、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache、post-request `_tmp_binder` / `_CopiedObjs` session state、backend `Cache_*` 均裁决为 `rejected`。
- S3 的 mesh / subshape map 禁止项只约束 CopyOnChange copied graph DTO，不改变普通 recompute 运行态响应合同；`results[].mesh.edgeSegments` 应继续覆盖可拾取拓扑边，包括 open wire mesh 的边段。
- `C12M8-BLOCKER-301` 已关闭为 `closed_s3_dto_not_reviewed_due_to_native_blocker`；backend gap classification 已同步为 native blocker 下的 DTO retained/deferred 状态。

## S4 current mismatch gate

- S4 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=6279752006`（`6279752006 文档：关闭 C12-M8 S3 DTO 边界裁决`），起点 worktree clean。
- S4 执行前队列确认：S4 是第一条 pending，后续为 S5-S6。
- S4 裁决：`no_current_mismatch_retained_diagnostic`。S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，没有 approved CopyOnChange request-local DTO，因此同一 request-local graph comparison 不成立。
- current `cad-core` 对 `BindCopyOnChange=Enabled` / `Mutated` 与 `PartialLoad=True` 保留 `copy_on_change_full_temporary_document_cache_not_supported`，capability 继续发布 `known_gap_diagnostic` / `oracle_blocked`；该 retained diagnostic 与现有证据一致，没有 C++ implementation mismatch。
- `C12M8-BLOCKER-401` 已关闭为 `closed_s4_no_current_mismatch`；`C12M8-CAT-001` 已裁决为 `retained_diagnostic/no_current_mismatch`。
- S4 focused tests 未运行，记录为 `not_run_blocked_by_s2_s3_gate`；S6 最多做 publication wording check，不得把 App::Link transport、property/session 状态、label、bbox、shape count、`_tmp_binder` document name 或 `_CopiedLink` target 当作 SubShapeBinder success/mismatch 证据。

## S5 implementation authorization

- S5 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=02ace35b4d`（`02ace35b4d 文档：关闭 C12-M8 S4 mismatch gate`），起点 worktree clean。
- S5 执行前队列确认：S5 是第一条 pending，后续为 S6。
- S5 裁决：`no_code_retained_diagnostic`。S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，三项 implementation gate 均未满足。
- S5 不创建 C12-M9 implementation package，不授权 C++ work。
- 继续保留 `copy_on_change_full_temporary_document_cache_not_supported`、`known_gap_diagnostic`、`oracle_blocked` 与 `remaining_gaps=[copy_on_change_full_temporary_document_cache]`。
- delete condition：只有 FreeCADCmd 稳定暴露 `_CopiedObjs` identity、copyObject dependency order、support rewrite map、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle，且这些证据能转成前端持久化 graph / `documentObjectUpdates` 后，才可替换 diagnostic。
- reopen condition：更强 native copied graph artifact 必须先重开并通过 S2，再重新执行 S3 DTO approval 与 S4 current mismatch gate。

## S6 publication gate

- S6 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7e5befaa50`（`7e5befaa50 docs: 关闭 C12-M8 S5 no-code 裁决`），起点 worktree clean。
- S6 发布结果：C12-M8 最终发布为 `no_code_retained_diagnostic`，没有 implementation package；C12-M8 队列关闭后只输出表头。
- 最终事实固定为：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`。
- 本包不创建 C12-M9 package，不授权 C++ work，不修改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 继续保留；known gap 继续是 `known_gap_diagnostic` / `oracle_blocked`，diagnostic 继续是 `copy_on_change_full_temporary_document_cache_not_supported`。
- final delete condition：只有 FreeCADCmd 稳定暴露 `_CopiedObjs` identity、copyObject dependency order、support rewrite map、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle，且这些证据能转成前端持久化 graph / `documentObjectUpdates` 后，才可替换 retained diagnostic。
- final reopen condition：更强 native copied graph artifact 必须先重开并通过 S2，再重新执行 S3 DTO approval 与 S4 current mismatch gate；不得从 App::Link transport、property/session 状态或 publication wording 直接重开 implementation。

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
- S2：native copied graph probe schema 与 evidence gate（已完成，`native_evidence_retained_blocker`）。
- S3：request-local DTO 产品边界裁决（已完成，`dto_not_reviewed_due_to_native_blocker`）。
- S4：current mismatch 与 implementation candidate gate（已完成，`no_current_mismatch_retained_diagnostic`）。
- S5：implementation package authorization / no-code retained decision（已完成，`no_code_retained_diagnostic`）。
- S6：发布闸门、README 更新和后续分流（已完成，`no_code_retained_diagnostic` published）。

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
