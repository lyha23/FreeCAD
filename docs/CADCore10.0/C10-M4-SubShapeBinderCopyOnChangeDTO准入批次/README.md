# C10-M4 SubShapeBinder CopyOnChange DTO 准入批次

## 当前定位

C10-M4 承接 C10-M3 队列关闭后的 live capability 剩余项：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。这个剩余项目前仍是 `known_gap_diagnostic` / `oracle_blocked`，不是直接实现 full temporary-document cache 的许可。

本包目标是做 SubShapeBinder `BindCopyOnChange` 的 request-local DTO 准入：先证明 FreeCAD native copied-object lifecycle 能稳定转成无状态请求证据，再决定是否把 cad-core 现有 `documentObjectUpdates` 机制扩到 SubShapeBinder。若证据或产品边界不足，S6 只发布 no-code retained diagnostic / release gate。

## S0 live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- S0 起点 HEAD：`cd8cd95fa2`（`cd8cd95fa2 docs: 新增 C10-M4 CopyOnChange DTO 准入方案`）。
- S0 起点 dirty state：`git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列状态：C10-M1 / C10-M2 / C10-M3 队列为空；C10-M4 在 S0 执行前下一步为 `6-29-03-31-C10-M4-S0-live基线与声明口径冻结.md`，S0 完成后仅 S1-S6 待执行。
- retained gap：`cad-core/src/runtime/capability_contract.cpp` 仍发布 `copy_on_change_full_temporary_document_cache` 为 `known_gap_diagnostic` / `oracle_blocked`，diagnostic 为 `copy_on_change_full_temporary_document_cache_not_supported`，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- S0 冻结的 forbidden claims：不声明 full temporary-document cache supported；不声明 backend session、persistent copied object、cross-request BREP / TopoDS / NamedShape / ElementMap cache supported；不把 App::Link CopyOnChange current DTO 自动等同为 SubShapeBinder CopyOnChange supported。

## S1 source authority 与 current coverage

- S1 起点 HEAD：`708053014b`（`708053014b docs: 完成 C10-M4 S0 live 基线冻结`），起点工作区干净。
- FreeCAD authority：`src/Mod/PartDesign/App/ShapeBinder.cpp/.h` 仍是 SubShapeBinder `PartialLoad`、`BindCopyOnChange`、`setupCopyOnChange()`、`checkCopyOnChange()` 和 `Support.setAllowPartial()` 的主依据；`src/App/Link.cpp/.h` 仍是共享 CopyOnChange setup/check/make lifecycle 依据。
- current cad-core coverage：`cad-core/src/part_design/feature_shape_binder.cpp` 只读取 `BindCopyOnChange` / `PartialLoad` 并发布 `copy_on_change_full_temporary_document_cache_not_supported` retained diagnostic；`cad-core/tests/test_c8_shapebinder.py` 和 capability contract 保持该 gap。
- DTO reference path：`cad-core/src/app/copy_on_change.cpp`、`cad-core/src/app/link.cpp`、`cad-core/tests/test_p8_features.py` 和 `cad-core/tests/test_adapters.py` 证明 App::Link `documentObjectUpdates` transport 存在，但它只是 request-local DTO 参考路径，不自动等同于 SubShapeBinder CopyOnChange supported。
- Probe history：S1 复核了 `collect_c8m1_shapebinder_expected.py`、`probe_c8m2_subshapebinder_copyonchange.py` 和 `probe_c9m5_subshapebinder_copyonchange.py`；这些工具覆盖 Disabled / Enabled / Mutated / PartialLoad property states，同时仍把 full temporary-document copied-object cache 记录为 `oracle_blocked` 或 `native_oracle_blocked`。
- `C10M4-BLOCKER-101` 已按 S1 关闭；S3/S4/S5 仍必须分别处理 native evidence、DTO boundary 和 no-session non-goal，不得升级 unsupported/full temporary-document cache 为 supported。

## S3 native evidence 复审结果

- S3 起点 HEAD：`ed483b6c34`（`ed483b6c34 docs: 完成 C10-M4 S2 范围准入矩阵`），起点工作区干净。
- 已复核 `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`、`cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`、`cad-core/tools/collect_c8m1_shapebinder_expected.py`、`cad-core/fixtures/c8m1` 和 `cad-core/fixtures/c8m2`；未修改 probe 或 expected。
- 既有 FreeCADCmd expected 基线为 FreeCAD `1.2.0 revision 20260519`。证据能区分 `BindCopyOnChange=Disabled/Enabled/Mutated`、`PartialLoad=True`、动态 CopyOnChange property 写入和 `_tmp_binder` 文档名，但这仍是 Python-visible property/session evidence。
- S3 未采到可序列化为 request-local DTO 的 copied-object evidence：`_CopiedObjs` 不可访问，`_tmp_binder`/`copyObject` 依赖顺序和 `recomputeFeature(true)` ElementMap lifecycle 仍不可观测，`_CopiedLink` 可见值也不是前端 graph writeback target。
- `C10M4-SCOPE-101` 已关闭为 `diagnostic_retained`，`C10M4-SCOPE-102` 已关闭为 `notCollected`，`C10M4-BLOCKER-301` 已关闭为 `closed_s3_notCollected`。在当前证据下，S4/S6 不能进入 C++ 实现；`backend_gap_candidate` 仍只能由未来 S3 native expected + S4 DTO approval + S5 stateless clearance 共同产生。

## S4 DTO / documentObjectUpdates 复审结果

- S4 起点 HEAD：`cc713e621f`（`cc713e621f docs: 完成 C10-M4 S3 native evidence 复审`），起点工作区干净。
- App::Link 路径已复核：`cad-core/src/app/copy_on_change.cpp` 和 `cad-core/src/app/link.cpp` 使用当前 request graph 中的 `LinkCopyOnChange`、`LinkedObject`、`LinkCopyOnChangeSource`、`LinkCopyOnChangeGroup`、`LinkCopyOnChangeTouched` 与依赖图生成 `documentObjectUpdates`，覆盖 group create/update、copied object create/update、dependency rewrite、history preserve 与 link property writeback；`cad-core/tests/test_p8_features.py` 会把这些 updates 应用回下一次请求 graph，`cad-core/tests/test_adapters.py` 和 capability contract 也声明这是 App::Link `documentObjectUpdates` transport。
- DTO 边界四问结论：当前 App::Link 请求携带的是前端已经持久化在 DocumentObject graph 里的 copied-object snapshot 加 CopyOnChange link properties/touched state，不是 SubShapeBinder 的 `_CopiedObjs` snapshot、`copyObject` intent 或 source mutation evidence；App::Link 可返回 `documentObjectUpdates`，缺 source 时也可返回 diagnostic；前端写回方式是把 create/update properties 应用到 DocumentObject graph 后再发下一次请求；SubShapeBinder 的 `_tmp_binder`、`_CopiedObjs`、`copyObject`、`recomputeFeature(true)`、temporary document/cache 和 copied-object ElementMap lifecycle 仍 unsupported。
- SubShapeBinder 当前路径已复核：`cad-core/src/part_design/feature_shape_binder.cpp` 对 `BindMode=Detached` 只返回清空 `Support` 的 request-local `documentObjectUpdates`；对 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 只发布 `copy_on_change_full_temporary_document_cache_not_supported` retained diagnostic 和 metadata，不产生 copied-object `documentObjectUpdates`。
- 因 S3 未采到 request-local copied-object expected，S4 不能比较出 current mismatch，也不能进入 S6 C++ 实现。`C10M4-BLOCKER-401` 关闭为 `product_decision_needed` / `current_retained_diagnostic` / `release_gate`；S5/S6 仍保持 pending。

## 入口

- 总入口：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次总入口.md`
- 方案：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

- S0：live 基线与声明口径冻结。
- S1：FreeCAD 源码与 current coverage 候选矩阵复核（已关闭 source authority blocker）。
- S2：范围准入与 blocker 矩阵。
- S3：CopyOnChange native probe 与 DTO evidence 专项复审（已关闭为 `diagnostic_retained` / `notCollected`，无 C++ route）。
- S4：cad-core 请求 DTO 与 `documentObjectUpdates` 专项复审。
- S5：临时文档禁用与 non-goal 边界专项复审。
- S6：Oracle 实现与发布闸门。

## 当前禁止声明

- 不实现 backend session、persistent temporary document 或 cross-request cache。
- 不把 BREP、TopoDS_Shape、NamedShape、ElementMap 或完整 copied-object graph 保存为后端状态。
- 不把 FreeCAD GUI / task panel / user prompt 生命周期写入 cad-core。
- 不把 App::Link 的完整 CopyOnChange group lifecycle 当成 SubShapeBinder supported，除非 S3-S5 证明 request-local DTO 子集成立。
- 不用 fixture 名称、几何猜测、输出修剪或 adapter repair 伪造 CopyOnChange 语义。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
