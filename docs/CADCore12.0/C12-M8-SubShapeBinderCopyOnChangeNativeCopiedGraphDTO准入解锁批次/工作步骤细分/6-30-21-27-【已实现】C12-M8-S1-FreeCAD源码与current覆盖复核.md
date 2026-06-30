# C12-M8 S1 FreeCAD 源码与 current 覆盖复核【已实现】

## 目标

复核 FreeCAD `SubShapeBinder` CopyOnChange source chain、当前 `cad-core` retained diagnostic、App::Link CopyOnChange transport 和 C12-M5 retained blocker，确定 S2 probe 必须证明的 copied graph evidence。

## 必读文件

- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.h`
- `src/App/Document.cpp`
- `src/App/Link.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `../矩阵/c12m8_copy_on_change_source_candidates.tsv`
- `../矩阵/c12m8_copy_on_change_native_graph_probe_matrix.tsv`

## 操作

1. 记录 `SubShapeBinder::setupCopyOnChange()` 的入口条件：`BindCopyOnChange`、`support.size()==1`、CopyOnChange properties。
2. 记录 `SubShapeBinder::update()` 在 Mutated 路径中如何使用 `_tmp_binder`、`copyObject()`、`_CopiedObjs`、`recomputeFeature(true)` 和 `_CopiedLink`。
3. 复核 `PartialLoad` 与 `Cache_*` 的语义边界，避免把 performance cache 误写成 stateless 必需状态。
4. 复核 current `cad-core` coverage：BindMode request-local 子集、CopyOnChange retained diagnostic、App::Link CopyOnChange transport。
5. 更新 source_candidates、scope_review、native_graph_probe_matrix 和 blocker_queue。

## 关闭条件

- `C12M8-SRC-001..008` 均有 source / current landing / next action。
- `C12M8-BLOCKER-101` 关闭：source authority 完整。
- `C12M8-BLOCKER-102` 关闭：App::Link transport 被标为 reference-only，不等同 SubShapeBinder support。
- S2 probe 必填字段已补齐。

## S1 复核结果

- 本轮 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=79602c1072`，`git log -1 --oneline=79602c1072 fix: 修复 Body replay 默认 RefineModel 语义`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- `SubShapeBinder::setupCopyOnChange()` 入口条件已记录：`BindCopyOnChange.getValue() == 0` 或 support 数量不是 1 时退出；单 support 时通过 `App::LinkBaseExtension::setupCopyOnChange()` 复制动态 CopyOnChange property 监控。
- `SubShapeBinder::update()` Mutated 路径已记录：复用或清理 `_CopiedObjs`，新建 temporary `_tmp_binder`，执行 `copyObject({obj}, true, true)`，反向保存 returned objects 到 `_CopiedObjs`，在属性复制前后执行 `recomputeFeature(true)`，最后用 `_CopiedLink` 记录 copied object 和 subvalues。
- `PartialLoad` 已记录为 `canLoadPartial()` 与 `Support.setAllowPartial()` 边界；`Cache_*` 已记录为 matrix cache hit/update/cleanup 边界，S2 只能把它作为 boundary review，不能默认升级为 persistent backend semantic state。
- current `cad-core/src/part_design/feature_shape_binder.cpp` 已复核：`BindMode=Detached` 只发 request-local `Support` clear `documentObjectUpdates`；`BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 继续发 `copy_on_change_full_temporary_document_cache_not_supported`。
- `cad-core/src/app/copy_on_change.cpp` 与 `copy_on_change.h` 已复核为 App::Link persisted copied graph transport vocabulary，包含 group sync、copied object create/update、dependencyRewrite、historyPreserve 和 link writeback；该证据被标为 reference-only，不等同于 SubShapeBinder `_tmp_binder` / `_CopiedObjs` support。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_c8_shapebinder.py` 已复核：capability 仍发布 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`、`known_gap_diagnostic`、`oracle_blocked` 和 retained diagnostic。
- 已更新 `source_candidates`、`scope_review`、`native_graph_probe_matrix` 和 `blocker_queue`：`C12M8-SRC-001..008`、`C12M8-BLOCKER-101`、`C12M8-BLOCKER-102` 已关闭。
- S2 probe 必须证明：FreeCAD / OCCT baseline、mode matrix、single support gate、temporary binder lifecycle、copied object identities、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle 和 `Cache_*` 边界。

## 非目标

- 不运行 FreeCADCmd。
- 不刷新 expected。
- 不改 production code。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
