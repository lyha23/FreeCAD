# 【已实现】C10-M4 S1 FreeCAD 源码与 current 覆盖候选矩阵

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
- C9-M5 CopyOnChange native probe historical evidence（脚本已移除）
- C8-M2 CopyOnChange native probe historical evidence（脚本已移除）
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

## 执行结果

- S1 起点：`pwd=/home/user/Chili3DProject/FreeCAD`，HEAD=`708053014b`（`708053014b docs: 完成 C10-M4 S0 live 基线冻结`），起点工作区干净。
- 已复核 FreeCAD source authority：`ShapeBinder.cpp/.h` 的 `PartialLoad`、`BindCopyOnChange`、`setupCopyOnChange()`、`checkCopyOnChange()`、`checkPropertyStatus()`；`Link.cpp/.h` 的 `LinkBaseExtension::setupCopyOnChange()`、`checkCopyOnChange()`、`makeCopyOnChange()`。
- 已复核 current cad-core coverage：`feature_shape_binder.cpp/.h` 仅保留 SubShapeBinder CopyOnChange / PartialLoad retained diagnostic；`copy_on_change.cpp/.h` 与 `app/link.cpp` 只作为 App::Link request-local `documentObjectUpdates` DTO reference path。
- 已复核 probe history：`collect_c8m1_shapebinder_expected.py`、`C8-M2 CopyOnChange native probe（已移除）`、`C9-M5 CopyOnChange native probe（已移除）` 均保留 property-state evidence 与 full temporary-document cache blocker。
- 已更新 `c10m4_copy_on_change_dto_source_candidates.tsv` 的 live path / line / symbol / evidence / cad-core landing，并把 stale `test_p7_features.py` landing 改为 `test_c8_shapebinder.py` 与 `test_p8_features.py` 的实际 coverage。
- 已更新 `c10m4_copy_on_change_dto_scope_review_matrix.tsv`：S1 相关 scope 标记为 source reviewed，但 S3/S4/S5 evidence / DTO / non-goal gate 仍未关闭。
- 已关闭 `c10m4_copy_on_change_dto_blocker_queue.tsv` 中 `C10M4-BLOCKER-101`。
- 已更新包 README 的 S1 结论；App::Link `documentObjectUpdates` 明确只是 DTO reference path，不自动等同于 SubShapeBinder supported。
- 本步骤未采 oracle，未改 C++，未升级 `copy_on_change_full_temporary_document_cache`、full temporary-document cache、backend session 或 persistent copied object 为 supported。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "setupCopyOnChange|checkCopyOnChange|BindCopyOnChange|PartialLoad|makeCopyOnChange|documentObjectUpdates|copy_on_change_full_temporary_document_cache" src/Mod/PartDesign/App/ShapeBinder.cpp src/Mod/PartDesign/App/ShapeBinder.h src/App/Link.cpp src/App/Link.h cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/src/app/copy_on_change.cpp cad-core/include/cad_core/app/copy_on_change.h cad-core/src/runtime/capability_contract.cpp cad-core/tools/collect_c8m1_shapebinder_expected.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
