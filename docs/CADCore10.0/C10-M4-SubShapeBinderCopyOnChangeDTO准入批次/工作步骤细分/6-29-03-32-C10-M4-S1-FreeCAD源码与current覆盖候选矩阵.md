# C10-M4 S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 SubShapeBinder CopyOnChange 的 FreeCAD source authority 和 current cad-core coverage。S1 只更新 source candidate / scope matrix，不采 oracle，不改 C++。

## 必读路径

- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.h`
- `src/App/Link.cpp`
- `src/App/Link.h`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/include/cad_core/part_design/feature_shape_binder.h`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/include/cad_core/app/copy_on_change.h`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`
- `cad-core/tools/collect_c8m1_shapebinder_expected.py`

## 必做动作

1. 用 `rg` 复核 `setupCopyOnChange`、`checkCopyOnChange`、`BindCopyOnChange`、`PartialLoad`、`makeCopyOnChange`、`documentObjectUpdates` 的 live path 和符号。
2. 更新 `c10m4_copy_on_change_dto_source_candidates.tsv` 的 line/path/symbol/evidence；不要保留 stale 路径。
3. 标出 App::Link request-local DTO 与 SubShapeBinder gap 的边界差异。
4. 更新 `C10M4-BLOCKER-101`、相关 `scope_id` 的 owner step。
5. 通过验收后重命名本文件为 `【已实现】`。

## 输出要求

- 每个 source row 必须有 FreeCAD 文件、函数名和 cad-core landing。
- 若发现 cad-core 已有 supported 行，必须同时给出 fixture / test / capability evidence；否则不得升级状态。
- 若发现 source moved，只更新矩阵和 README，不做行为判断。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "setupCopyOnChange|checkCopyOnChange|BindCopyOnChange|PartialLoad|makeCopyOnChange|documentObjectUpdates|copy_on_change_full_temporary_document_cache" src/Mod/PartDesign/App/ShapeBinder.cpp src/Mod/PartDesign/App/ShapeBinder.h src/App/Link.cpp src/App/Link.h cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/src/app/copy_on_change.cpp cad-core/include/cad_core/app/copy_on_change.h cad-core/src/runtime/capability_contract.cpp cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py cad-core/tools/collect_c8m1_shapebinder_expected.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
