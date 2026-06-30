# C12-M5 S1 FreeCAD 源码与 current 覆盖矩阵【已实现】

## 目标

复核 FreeCAD SubShapeBinder CopyOnChange 调用链和当前 `cad-core` 覆盖，明确哪些是 source authority、哪些只是 App::Link DTO 参考，关闭 source authority blocker。

## 必读文件

- `../矩阵/c12m5_copy_on_change_source_candidates.tsv`
- `../矩阵/c12m5_copy_on_change_dto_contract_fields.tsv`
- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.h`
- `src/App/Link.cpp`
- `src/App/Document.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/runtime/capability_contract.cpp`

## 操作

1. 记录 FreeCAD `setupCopyOnChange()`、`checkCopyOnChange()`、`update()`、`copyObject()` 的真实调用顺序和关键字段。
2. 标出 current `cad-core` 中 retained diagnostic、BindMode Detached update、App::Link `documentObjectUpdates` 的边界。
3. 更新 source_candidates 和 dto_contract_fields 中 S1 行。
4. 若 source path 或 current landing 不存在，保留 blocker，不进入 S2 probe。

## S1 live 基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=de93b702b0`。
- `git log -1 --oneline=de93b702b0 docs: 完成 C12-M5 S0 live 基线冻结`。
- `git -c core.quotepath=false status --short -uall` 无输出，dirty boundary 为 `<clean>`；未发现非本任务 dirty work。
- S1 开始前队列显示本文件为首个 pending，S2-S5 仍 pending。

## S1 源码与 current 结论

- FreeCAD 主链：`SubShapeBinder::execute()` 先调用 `setupCopyOnChange()`，`BindMode=Synchronized` 时再 `update(UpdateForced)`；`onChanged(Support)` 会 `clearCopiedObjects()`、`setupCopyOnChange()` 并触发 `update()`，`onChanged(BindCopyOnChange)` 只刷新 copy-on-change property 同步，普通 dynamic copy-on-change property 变化走 `checkCopyOnChange()`。
- `setupCopyOnChange()` 只有 `BindCopyOnChange!=Disabled` 且 `Support` 恰好一个对象时才调用 `App::LinkBaseExtension::setupCopyOnChange()`；Enabled 连接源属性同步，源对象普通属性变化会清空 `_CopiedObjs`。
- `checkCopyOnChange()` 只在 `BindCopyOnChange=Enabled`、单一 `Support`、非 transaction 且属性是 CopyOnChange dynamic property 时比较 linked property；一旦 binder 属性与 linked 属性不同，就把 `BindCopyOnChange` 写成 `Mutated`。
- `update()` 在 `BindCopyOnChange=Mutated` 且单一 `Support` 时进入 full temporary document lifecycle：复用 `_CopiedObjs`，否则创建临时文档 `_tmp_binder`，执行 `tmpDoc->copyObject({obj}, true, true)`，把返回对象写入 `_CopiedObjs`，先 `recomputeFeature(true)` 生成 element map，再复制属性并按需再次 `recomputeFeature(true)`，最后写 `_CopiedLink` 并用 copied object 参与后续 shape 构造。
- `Document::copyObject()` 通过 dependency list、`exportObjects()` / `MergeDocuments::importObjects()` 复制对象，并在非 `returnAll` 时按输入对象顺序返回；`Document::recomputeFeature(feature, true)` 会走 recursive recompute。该生命周期依赖真实 FreeCAD document / temporary document，不是 request-local DTO 字段本身。
- App::Link 链路只作为 reference-only DTO 参考：FreeCAD `LinkBaseExtension` 同样使用 `copyObject()` 和 CopyOnChange group/source/touched 生命周期；current `cad-core/src/app/copy_on_change.cpp` / `link.cpp` 把 App::Link lifecycle 变成 `documentObjectUpdates` 建议，但这不能证明 `PartDesign::SubShapeBinder` 的 `_tmp_binder` / `_CopiedObjs` 生命周期已 supported。
- current `cad-core/src/part_design/feature_shape_binder.cpp` 只覆盖 `BindMode=Detached` 的 request-local `Support` clear writeback 子集；当 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 时继续发布 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。
- `cad-core/src/runtime/capability_contract.cpp` 仍发布 `copy_on_change_full_temporary_document_cache` 为 `known_gap_diagnostic` / `oracle_blocked`，delete condition 仍要求 FreeCADCmd 暴露不依赖 persistent backend session 的 stable request-local copied-object evidence。
- 因此 `C12M5-BLOCKER-101` 关闭为 source/current coverage verified；S1 不打开 implementation candidate，S2 只能继续复核 native evidence / probe 准入。

## 非目标

- 不从 App::Link coverage 推导 SubShapeBinder supported。
- 不实现 CopyOnChange。
- 不新增 fixture。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'setupCopyOnChange|checkCopyOnChange|_CopiedObjs|_tmp_binder|copyObject|recomputeFeature|copy_on_change_full_temporary_document_cache_not_supported' src/Mod/PartDesign/App/ShapeBinder.cpp src/App/Link.cpp src/App/Document.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/src/runtime/capability_contract.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```
