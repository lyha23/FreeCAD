# C12-M5 S3 request-local DTO 产品边界冻结【已实现】

## 目标

在 S1/S2 evidence 基础上，决定 SubShapeBinder CopyOnChange 是否有产品可接受的 request-local DTO。没有 approved DTO 时，必须保留 known gap diagnostic。

## 必读文件

- `../矩阵/c12m5_copy_on_change_dto_contract_fields.tsv`
- `../矩阵/c12m5_copy_on_change_non_goal_registry.tsv`
- `docs/接口规定/01-cad-recompute全量输入输出接口.md`
- `docs/CADCore方案/细化方案/03-接口与验收样例.md`
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/tests/test_p8_features.py`

## 操作

1. 列出允许 DTO 字段：request graph 可持久化字段、copy intent、source id/name、support subname、mutation delta、diagnostic、`documentObjectUpdates`。
2. 列出禁止字段：temporary document、native object pointer、TopoDS / BREP full object、post-request NamedShape / ElementMap cache、`_CopiedObjs` private vector。
3. 判断前端是否能用该 DTO 更新 DocumentObject graph，再作为下一次请求唯一真实数据。
4. 发布 `dto_approved_for_mismatch_gate` 或 `dto_rejected_known_gap_retained`。

## S3 live 基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=239a866ad9`。
- `git log -1 --oneline=239a866ad9 docs: 完成 C12-M5 S2 native 证据复核`。
- `git -c core.quotepath=false status --short -uall` 无输出，dirty boundary 为 `<clean>`；未发现非本任务 dirty work。
- S3 开始前队列首项为本文件，S4-S5 仍 pending。

## S3 DTO 产品边界

允许字段只按 request-local / graph-writeback 语义成立，不构成 SubShapeBinder CopyOnChange support：

- request graph 可持久化字段：`Objects[].Name`、`Objects[].ID`、`TypeId`、`Properties`、`BindCopyOnChange`、`PartialLoad`、`Support` / `App::PropertyLinkSub`、`SubList`、`StableSubList`、`FullSubList` 和仅限单 subshape 引用恢复的 `ReferenceShadow`。
- copy intent：只能作为未来产品 DTO 的显式意图字段；当前不得用它暗示 FreeCAD `_tmp_binder`、`_CopiedObjs`、`copyObject()` 或 `recomputeFeature(true)` 生命周期已经可重建。
- source id/name 与 support subname：对象 `Name` / `ID`、source object name、`Support.SubList` / `StableSubList` / `FullSubList` 可以序列化并随 graph 保存，但只能定位引用源，不能表达 copied-object graph。
- mutation delta：只有当前端能把完整对象级变更应用到 `Objects[]`，并让下一次 recompute 只依赖新 graph 时才可接受；S2 缺少 stable copied-object graph evidence，因此本轮不批准 SubShapeBinder CopyOnChange mutation delta。
- diagnostic：继续允许并保留 `copy_on_change_full_temporary_document_cache_not_supported`。
- `documentObjectUpdates`：仍是 App::Link / Assembly 的 reference-only graph 写回建议；它证明前端可以应用某些对象级更新，但不能证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` / copied support rewrite / ElementMap lifecycle 已 supported。

禁止字段和禁止路线：

- temporary document、`_tmp_binder` session state 或 backend persistent session。
- native object pointer、FreeCAD native object handle、Python session object。
- TopoDS / full object BREP 作为请求模型输入；`ReferenceShadow.brep` 仍只允许单个旧 subshape snapshot，不能扩展为完整 copied object BREP。
- post-request `NamedShape` / `ElementMap` cache、mesh、subshape map 或任意上次响应查表状态。
- `_CopiedObjs` private vector、`_CopiedLink` 单值 session evidence 或 `_tmp_binder` document name 作为 DTO。
- adapter repair、frontend mock、output guessing、fixture name / bbox / output order 推断 copied-object ownership。

## S3 关闭结论

- 裁决为 `dto_rejected_known_gap_retained`。
- 前端目前不能把 SubShapeBinder CopyOnChange DTO 写回 `DocumentObject graph` 并让下一次请求成为唯一真实数据；缺口仍是稳定 copied-object graph、dependency order、copied support rewrite 和 `recomputeFeature(true)` ElementMap lifecycle 证据。
- App::Link `documentObjectUpdates` 只保留 reference-only 形态，不升级为 SubShapeBinder CopyOnChange support evidence。
- `C12M5-BLOCKER-301` 关闭；S4 只能在 DTO 已拒绝的前提下做 current mismatch gate，不打开 implementation candidate。

## 非目标

- 不要求后端持久化 copied-object cache。
- 不把 `ReferenceShadow.brep` 扩展成 full object BREP transport。
- 不靠 adapter 层修复 DTO 缺失。
- 不打开 S4 implementation candidate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'documentObjectUpdates|ReferenceShadow|LinkCopyOnChange|CopyOnChangeOwned|temporary document|full object BREP|copy_on_change_full_temporary_document_cache' docs/接口规定 docs/CADCore方案 cad-core/src/app cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```
