# 【已实现】C9-M5-S3 native CopyOnChange 生命周期 probe 复审

## 目标

复跑并增强 SubShapeBinder CopyOnChange native evidence。S3 只回答“FreeCAD 是否能导出稳定 request-local DTO 证据”，不决定产品边界，不直接改 cad-core C++。

## 执行基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`11eab67134`
- `git log -1 --oneline`：`11eab67134 docs: 关闭 C9-M5 S2 范围准入矩阵`
- `git -c core.quotepath=false status --short -uall`：无输出，工作区干净。
- 队列首项是本 S3 文件；S4/S5/S6 仍 pending。

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

## native probe 产物

- Probe：`cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`
- Source fixture：`cad-core/fixtures/c9m5/subshape-binder-copyonchange-lifecycle-probe.json`
- Native evidence：`cad-core/fixtures/c9m5/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`
- FreeCADCmd：`/home/user/.local/bin/freecadcmd`
- Native version：`freecad_version=1.2.0 revision 20260519`，`freecad_revision=20260519`
- Evidence route：`native_evidence_collected_with_known_gap_blocker`

## S3 结论

| 观察轴 | S3 结果 | 关键证据 | 边界 |
| --- | --- | --- | --- |
| property-state | `native_property_state_collected` | Disabled / Enabled / Mutated Python-visible `BindCopyOnChange`；动态 `C9M5CopyOnChangeValue` 可从 source 复制到 binder；写入该动态属性后 Enabled 进入 Mutated | 只能证明 property-state 和 mutation trigger，不证明 full cache supported |
| temporary document | `native_lifecycle_evidence_collected_with_blocker` | 初次 recompute 和 mutation recompute 后 `FreeCAD.listDocuments()` 均可见 `_tmp_binder`；Mutated path 暴露 `_CopiedLink` | `_tmp_binder` 是 FreeCAD temporary document lifecycle 证据，不是 cad-core 可持久化状态 |
| copied-object mapping | `not_serializable` | explicit Mutated 初始 `_CopiedLink` 指向 `SupportBox`，mutation recompute 后 `_CopiedLink` 指向 copied `SupportBox001`；`_CopiedObjs` Python API 仍不可见 | copied-object private vector、copyObject dependency order 和 copied support mapping 不能导出稳定 request-local DTO |
| recompute lifecycle | `oracle_blocked` | `recomputeFeature(true)` 相关 lifecycle 只以 `_tmp_binder` / `_CopiedLink` 和 shape summary 可见 | internal ElementMap / copied-object recompute lifecycle 仍不可序列化为 request-local fact |

C9-M5 复采比 C8-M2 更明确地隔离到了专属 fixture，但语义结论没有改变：FreeCAD native evidence 能证明 property-state、temporary document 和 hidden link observable；不能证明 `_CopiedObjs/_tmp_binder/copyObject/recomputeFeature(true)` full temporary-document cache 可转成稳定 request-local DTO。

因此 S3 不打开 C++ implementation gate：

- `C9M5-SCOPE-101` 关闭为 `native_property_state_collected`。
- `C9M5-SCOPE-102` 保持 `known_gap_retained_after_S3`。
- `C9M5-SCOPE-103` 保持 `needs_more_native_evidence`，不得升级为 implementation。
- `C9M5-BLOCKER-301` 关闭为 evidence collected with retained blocker。
- `C9M5-CAT-101` 保持 full temporary-document cache known gap。
- `C9M5-CAT-102` 只能进入 S4 deferred/rejected DTO review，不能直接交 S5 code gate。

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
- 验收通过后，本文件重命名为 `6-28-11-28-【已实现】C9-M5-S3-native-CopyOnChange生命周期probe复审.md`，队列下一项应为 S4。

## 非目标

- 不做产品决策。
- 不修改 `feature_shape_binder.cpp` 或 `copy_on_change.cpp`。
