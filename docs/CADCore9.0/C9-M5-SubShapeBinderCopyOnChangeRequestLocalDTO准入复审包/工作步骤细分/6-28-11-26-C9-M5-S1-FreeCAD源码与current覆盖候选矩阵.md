# C9-M5-S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 CopyOnChange source authority 和 current cad-core coverage，把可进入 S2/S3/S4/S5 的候选写入 source candidates。S1 只做源码和 current 状态审计，不采 oracle、不改 C++。

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

## 必须回写的矩阵行

- `C9M5-SRC-101` 到 `C9M5-SRC-205`
- `C9M5-SCOPE-101` 到 `C9M5-SCOPE-104`
- `C9M5-BLOCKER-101`

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
