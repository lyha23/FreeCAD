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

## 入口

- 总入口：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次总入口.md`
- 方案：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

- S0：live 基线与声明口径冻结。
- S1：FreeCAD 源码与 current coverage 候选矩阵复核（已关闭 source authority blocker）。
- S2：范围准入与 blocker 矩阵。
- S3：CopyOnChange native probe 与 DTO evidence 专项复审。
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
