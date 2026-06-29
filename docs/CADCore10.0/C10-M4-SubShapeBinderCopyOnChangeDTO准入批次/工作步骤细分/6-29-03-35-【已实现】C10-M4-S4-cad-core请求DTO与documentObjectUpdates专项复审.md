# C10-M4 S4 cad-core 请求 DTO 与 documentObjectUpdates 专项复审

## 目标

在 S3 证据基础上复核 current cad-core 是否已有足够 request-local DTO / `documentObjectUpdates` 能力承接 SubShapeBinder CopyOnChange。S4 主要做 current comparison 和 DTO boundary，默认不改 C++。

## 必做动作

1. 复核 `cad-core/src/app/copy_on_change.cpp` 与 `cad-core/include/cad_core/app/copy_on_change.h` 的 App::Link DTO 输出是否可复用，哪些字段不能直接套到 SubShapeBinder。
2. 复核 `cad-core/src/part_design/feature_shape_binder.cpp` 当前 `BindCopyOnChange` / `PartialLoad` diagnostic 与 `documentObjectUpdates` 行为。
3. 若 S3 有 native expected，比较 current output，确认是否存在 mismatch。
4. 若产品批准 request-local DTO 子集，写清 DTO input / output / writeback / diagnostics；否则保持 `product_decision_needed`。
5. 更新 scope / blocker / backend-gap matrix；只有 evidence + product decision + mismatch 同时成立，才打开 `backend_gap_candidate`。
6. 通过验收后重命名本文件为 `【已实现】`。

## DTO 边界必须回答

- 前端请求里携带的是 copied object snapshot、copy intent、还是 source mutation evidence。
- cad-core 返回的是 `documentObjectUpdates`、diagnostic、还是两者都有。
- recompute 后前端如何把 copied object 写回 DocumentObject graph。
- 哪些 FreeCAD temporary document / cache 行为继续保持 unsupported。

## S4 执行结果

- 起点基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=cc713e621f`（`cc713e621f docs: 完成 C10-M4 S3 native evidence 复审`），`git -c core.quotepath=false status --short -uall` 无输出。
- App::Link DTO reference path：`cad-core/src/app/copy_on_change.cpp` 从 request graph 里的 `LinkCopyOnChange`、`LinkedObject`、`LinkCopyOnChangeSource`、`LinkCopyOnChangeGroup`、`LinkCopyOnChangeTouched` 与 dependencies 计算 group/copied object/link writeback；输出的 `documentObjectUpdates` 包含 `action=create|update`、`reason=copy_on_change_group_sync|copy_on_change_deep_copy_lifecycle`、`object/objectId/typeId`、`owner/sourceObject/group`、`properties`、`dependencyRewrite` 和 `historyPreserve`。`cad-core/src/app/link.cpp` 只把该 helper 的 diagnostics 与 updates 合并进 `ComputeContext`，这是 App::Link 参考通道。
- App::Link tests/capability 覆盖：`cad-core/tests/test_p8_features.py` 覆盖 deep copy、subtree relink/history preserve、touched tracking，并把 updates 应用回稳定 graph 后验证下一次请求不再产生同类 updates；`cad-core/tests/test_adapters.py` 和 `cad-core/src/runtime/capability_contract.cpp` 声明 `documentObjectUpdates` channel、CopyOnChange writeback properties 和 request-local boundary。
- SubShapeBinder current path：`cad-core/src/part_design/feature_shape_binder.cpp` 对 `BindMode=Detached` 只产生清空 `Support` 的 request-local writeback；对 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 只产生 `copy_on_change_full_temporary_document_cache_not_supported` retained diagnostic 和 metadata，不产生 copied-object `documentObjectUpdates`。
- DTO 边界四问：当前 App::Link 请求携带的是前端已持久化 copied graph snapshot 加 link CopyOnChange properties/touched state，不是 SubShapeBinder `_CopiedObjs` snapshot、`copyObject` intent 或 source mutation evidence；App::Link 可返回 `documentObjectUpdates`，缺 source 时也可返回 diagnostic，SubShapeBinder CopyOnChange/PartialLoad 当前只返回 retained diagnostic；前端写回方式是应用 App::Link updates 到 DocumentObject graph 后作为下一次请求输入，SubShapeBinder 暂无 copied-object writeback target；FreeCAD `_tmp_binder`、`_CopiedObjs`、`copyObject`、`recomputeFeature(true)`、temporary document/cache、backend session 和 copied-object ElementMap lifecycle 仍 unsupported。
- 结论：S3 没有 request-local copied-object expected，S4 不能比较出 current mismatch，也不能进入 S6 C++ 实现。`C10M4-BLOCKER-401` 关闭为 `product_decision_needed` / `current_retained_diagnostic` / `release_gate`；`backend_gap_candidate` 不打开，S5/S6 保持 pending。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "documentObjectUpdates|CopyOnChange|BindCopyOnChange|PartialLoad|copy_on_change_full_temporary_document_cache_not_supported" cad-core/src/app/copy_on_change.cpp cad-core/include/cad_core/app/copy_on_change.h cad-core/src/app/link.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/tests/test_p7_features.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/src/runtime/capability_contract.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```

## 最终验收结果

- S4 `rg`：已运行，退出码 0，命中 App::Link `documentObjectUpdates`、CopyOnChange DTO reference path、SubShapeBinder `BindCopyOnChange` / `PartialLoad` retained diagnostic、tests 和 capability publication。
- queue：已运行，S4 被跳过，下一项为 S5。
- TSV 列数：已运行，退出码 0。
- trailing whitespace：已运行，退出码 1，表示无匹配。
- `git diff --check`：已运行，退出码 0。
