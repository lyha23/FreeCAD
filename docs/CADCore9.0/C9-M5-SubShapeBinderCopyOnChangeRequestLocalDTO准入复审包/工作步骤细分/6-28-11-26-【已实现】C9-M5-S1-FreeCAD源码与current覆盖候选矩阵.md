# 【已实现】C9-M5-S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 CopyOnChange source authority 和 current cad-core coverage，把可进入 S2/S3/S4/S5 的候选写入 source candidates。S1 只做源码和 current 状态审计，不采 oracle、不改 C++。

## 执行基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`4f2416c8dd`
- `git log -1 --oneline`：`4f2416c8dd docs: 关闭 C9-M5 S0 基线冻结`
- `git -c core.quotepath=false status --short -uall`：无输出，工作区干净。

## FreeCAD 依据

| 语义 | 源码入口 | S1 要确认的事实 |
| --- | --- | --- |
| setup | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` | 单 support、`BindCopyOnChange`、dynamic property setup、source change clear cache。 |
| mutated update | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` | `_tmp_binder`、`copyObject()`、`_CopiedObjs`、`recomputeFeature(true)`、`_CopiedLink`。 |
| App Link 对照 | `src/App/Link.cpp::LinkBaseExtension::syncCopyOnChange()` | CopyOnChangeGroup、UUID matching、property paste、dependency order。 |
| owned copy | `src/App/Link.cpp::LinkBaseExtension::makeCopyOnChange()` | `getOnChangeCopyObjects()`、`Document::copyObject()`、owned link transition。 |
| document copy | `src/App/Document.cpp::Document::copyObject()`、`Document::recomputeFeature()` | full cache 依赖的 document object copy 与 recompute。 |

## current cad-core 依据

- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_diagnostics.py`
- `cad-core/tests/test_adapters.py`

## S1 审计结论

- `SubShapeBinder::setupCopyOnChange()` 只在 `BindCopyOnChange != Disabled` 且单 `Support` 时设置 CopyOnChange 动态属性；source 非输出属性变化会清空 `_CopiedObjs` cache。
- `SubShapeBinder::update()` 的 `Mutated` full path 仍依赖 `"_tmp_binder"` temporary document、`copyObject({obj}, true, true)`、`_CopiedObjs`、`recomputeFeature(true)` 和 `_CopiedLink`，不是仅靠 property-state 可表达的 DTO。
- `LinkBaseExtension::syncCopyOnChange()` / `makeCopyOnChange()` 提供 App Link 对照：hidden `CopyOnChangeGroup`、UUID matching、dependency-order copy、property paste、link replacement 和 owned transition，但它不自动证明 SubShapeBinder full cache supported。
- `Document::copyObject()` / `recomputeFeature()` 说明 full copied-object lifecycle 依赖文档对象 copy/export/import、dependency order 和 recompute side effect，不应在 S1 被写成无状态 backend support。
- current `cad-core` SubShapeBinder 仍只对 Enabled / Mutated / PartialLoad 发布 `copy_on_change_full_temporary_document_cache_not_supported` 和 `copy_on_change_boundary=known_gap_full_temporary_document_cache`。
- `cad-core/src/app/copy_on_change.cpp` 只能作为 request-local `documentObjectUpdates` 词汇对照；capability/tests 仍发布 `known_gap_diagnostic` / `oracle_blocked` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。

## 必须回写的矩阵行

- `C9M5-SRC-101` 到 `C9M5-SRC-205`
- `C9M5-SCOPE-101` 到 `C9M5-SCOPE-104`
- `C9M5-BLOCKER-101`

回写状态：

- `c9m5_copyonchange_source_candidates.tsv`：`C9M5-SRC-101..205` 已补入 FreeCAD / cad-core 行段、语义边界和下一步。
- `c9m5_copyonchange_scope_review_matrix.tsv`：`C9M5-SCOPE-101..104` 当前保持为 `native_oracle_required`、`known_gap_retained`、`backend_gap_candidate`、`release_gate`，没有提升为 supported 或 `backend_gap_requires_implementation`。
- `c9m5_copyonchange_blocker_queue.tsv`：`C9M5-BLOCKER-101` 已关闭为 `closed_S1`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'setupCopyOnChange|checkCopyOnChange|BindCopyOnChange|_CopiedObjs|_CopiedLink|_tmp_binder|copyObject|recomputeFeature' src/Mod/PartDesign/App/ShapeBinder.cpp src/App/Link.cpp src/App/Document.cpp
rg -n 'copy_on_change_full_temporary|sub_shape_binder|BindCopyOnChange|copy_on_change_boundary|known_gaps|remaining_gaps' cad-core/src/part_design/feature_shape_binder.cpp cad-core/src/app/copy_on_change.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_c8_shapebinder.py cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
git diff --check
```

验收标准：

- `source_candidates.tsv` 每条 source 都有 FreeCAD 或 cad-core 落点。
- `scope_review_matrix.tsv` 没有把 current known gap 提升为 supported。
- S1 文档记录当前 cad-core diagnostic boundary，不新增 fixture、expected、tests 或 C++。

## 非目标

- 不运行 FreeCADCmd。
- 不改 `cad-core` 业务代码。
