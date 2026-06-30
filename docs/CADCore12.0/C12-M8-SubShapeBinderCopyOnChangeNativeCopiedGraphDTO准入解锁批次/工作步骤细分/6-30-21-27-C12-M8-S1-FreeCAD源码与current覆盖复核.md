# C12-M8 S1 FreeCAD 源码与 current 覆盖复核

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
