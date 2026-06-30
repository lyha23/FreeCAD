# C12-M10 S1 source 与 native oracle schema 复核

## 目标

复核 FreeCAD source、current `cad-core` retained diagnostic、C12-M8 raw artifact 缺口和 App::Link reference vocabulary，固定 C12-M10 native copied graph oracle schema。

## 必读文件

- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.h`
- `src/App/Document.cpp`
- `src/App/Link.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/include/cad_core/app/copy_on_change.h`
- `../矩阵/c12m10_copy_on_change_source_candidates.tsv`
- `../矩阵/c12m10_copy_on_change_probe_matrix.tsv`
- `../矩阵/c12m10_copy_on_change_blocker_queue.tsv`

## 操作

1. 复核 `setupCopyOnChange()`、`checkCopyOnChange()`、`update()`、`_CopiedObjs`、`_CopiedLink`、`copyObject()`、`recomputeFeature()` 和 App::Link CopyOnChange transport source。
2. 从 C12-M8 旧 artifact 缺口出发，固定 C12-M10 native artifact schema：baseline、mode matrix、support gate、temporary binder lifecycle、copied identities、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle、`PartialLoad`、`Cache_*` boundary。
3. 更新 source / probe / blocker / validation 矩阵。
4. 将本 S1 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M10-SRC-001..008` 有 source/current evidence。
- `C12M10-PROBE-001..011` 有 schema requirement。
- `C12M10-BLOCKER-101` 关闭。

## 非目标

- 不运行 FreeCADCmd。
- 不采 native oracle。
- 不改 production code、fixtures、expected、tests、adapters 或 capability source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
