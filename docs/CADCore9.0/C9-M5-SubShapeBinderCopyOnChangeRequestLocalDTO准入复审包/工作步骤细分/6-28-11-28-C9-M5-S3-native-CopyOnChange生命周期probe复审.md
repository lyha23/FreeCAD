# C9-M5-S3 native CopyOnChange 生命周期 probe 复审

## 目标

复跑并增强 SubShapeBinder CopyOnChange native evidence。S3 只回答“FreeCAD 是否能导出稳定 request-local DTO 证据”，不决定产品边界，不直接改 cad-core C++。

## FreeCAD 依据

- `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()`
- `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()`
- `src/App/Link.cpp::LinkBaseExtension::syncCopyOnChange()`
- `src/App/Document.cpp::Document::copyObject()`
- `src/App/DocumentObject.cpp::DocumentObject::recomputeFeature()`

## probe 范围

| probe 轴 | 必须观察 | 允许结论 |
| --- | --- | --- |
| property-state | Disabled / Enabled / Mutated / PartialLoad / dynamic CopyOnChange properties | `native_property_state_collected` 或 `probe_failed`。 |
| temporary document | `"_tmp_binder"` document、copied object name、`_CopiedLink` subvalues | `native_lifecycle_evidence_collected` 或 `oracle_blocked`。 |
| copied-object mapping | source object、support subname、copied support、mutated property delta | `dto_evidence_candidate` 或 `not_serializable`。 |
| recompute lifecycle | `recomputeFeature(true)` 前后 shape / element map 是否可导出为 request-local fact | `dto_evidence_candidate` 或 `oracle_blocked`。 |

## 必须回写的矩阵行

- `C9M5-SCOPE-101`
- `C9M5-SCOPE-102`
- `C9M5-SCOPE-103`
- `C9M5-BLOCKER-301`
- `C9M5-CAT-101`
- `C9M5-CAT-102`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-ORACLE|copyonchange|_tmp_binder|_CopiedLink|BindCopyOnChange|PartialLoad' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 cad-core/tools cad-core/fixtures/c8m2
```

若新增 C9-M5 probe，验收还必须包含：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C9M5|copyonchange|_tmp_binder|_CopiedLink|BindCopyOnChange|PartialLoad|freecad_version' cad-core/tools cad-core/fixtures/c9m5 docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
git diff --check
```

验收标准：

- S3 必须明确记录 FreeCADCmd / native probe 版本或环境阻断原因。
- 若没有稳定 DTO evidence，`C9M5-SCOPE-103` 必须保持 `known_gap_retained` 或 `needs_more_native_evidence`。
- S3 不允许把 `_tmp_binder` 或 `_CopiedObjs` 本身写成 cad-core 可持久化状态。

## 非目标

- 不做产品决策。
- 不修改 `feature_shape_binder.cpp` 或 `copy_on_change.cpp`。
