# 【已实现】C10-M3-S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 C10-M3 source candidate seed，把 FreeCAD authority、current cad-core coverage、测试入口和能力文档落点都改成当前仓库可追溯路径。S1 不采 oracle，不升级 supported，不创建 backend gap。

## FreeCAD 依据

| 轴 | 路径 | 必查符号 |
| --- | --- | --- |
| Link reference update | `src/App/PropertyLinks.cpp` | `PropertyLinkBase::updateElementReferences()`、`PropertyLinkBase::_updateElementReference()` |
| ShadowSub getSubValues | `src/App/PropertyLinks.cpp` | `PropertyLinkSub::getSubValues(bool)`、`PropertyLinkSubList::getSubValues(bool)`、`afterRestore()` |
| Element resolve | `src/App/GeoFeature.cpp` | `GeoFeature::resolveElement()`、ElementMap version update path |
| Shape restore | `src/Mod/Part/App/PropertyTopoShape.cpp` | `PropertyPartShape::getElementMapVersion()`、`PropertyPartShape::afterRestore()` |
| Mapper history | `src/Mod/Part/App/TopoShape.cpp`、`src/Mod/Part/App/TopoShapeMapper.cpp` | `makeShapeWithElementMap`、`MapperHistory`、ElementMap propagation |

## current cad-core 扫描轴

- `cad-core/src/app/property_links.cpp`
- `cad-core/include/cad_core/app/property_links.h`
- `cad-core/src/part/topo_shape_reference.cpp`
- `cad-core/include/cad_core/part/topo_shape_reference.h`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/app/element_map.cpp`
- `cad-core/include/cad_core/runtime/element_reference_update.h`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 必须回写的矩阵行

- `C10M3-SRC-101` 到 `C10M3-SRC-104`：确认 FreeCAD path、line anchor、symbol 和 source evidence。
- `C10M3-SRC-201` 到 `C10M3-SRC-204`：确认 current cad-core path、symbol、focused test 和 capability landing。
- `C10M3-BLOCKER-101`：S1 完成后改为 `closed_s1`。
- 若路径不存在，不能静默删除；必须写 replacement path、source risk 或 reopen condition。

## 推荐命令

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "updateElementReferences|_updateElementReference|getSubValues|afterRestore|resolveElement|checkElementMapVersion|getElementMapVersion|makeShapeWithElementMap|MapperHistory" src/App/PropertyLinks.cpp src/App/GeoFeature.cpp src/Mod/Part/App/PropertyTopoShape.cpp src/Mod/Part/App/TopoShape.cpp src/Mod/Part/App/TopoShapeMapper.cpp
rg -n "ReferenceShadow|ShadowSub|StableSubList|recoverReferenceShadowSubshape|resolveElementReference|elementReferenceUpdates|unsupportedReferenceShadowBrepReason|element_history_status" cad-core/src/app cad-core/src/part cad-core/src/runtime cad-core/include/cad_core cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py
```

## 验收标准

- `c10m3_reference_shadow_recovery_source_candidates.tsv` 每一行都有真实 FreeCAD path 或 current cad-core landing。
- `C10M3-BLOCKER-101` 关闭为 `closed_s1`。
- `scope_review_matrix.tsv` 仍不出现未经 S3-S5 证明的 `supported` / `backend_gap_requires_implementation` 结论。
- S1 未创建 `cad-core/fixtures/c10m3` expected，未修改 C++。

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/矩阵/*.tsv
git diff --check
```

验收通过后，S1 文件才能重命名为 `6-29-01-10-【已实现】C10-M3-S1-FreeCAD源码与current覆盖候选矩阵.md`。

## S1 收口结果

- `c10m3_reference_shadow_recovery_source_candidates.tsv` 已把 `C10M3-SRC-101..104` 刷新到 live FreeCAD path、line anchor、symbol 和 source evidence。
- `C10M3-SRC-201..204` 已补 current cad-core parser、ReferenceShadow recovery、ElementMap diagnostics、focused tests 与 capability landing；这些 landing 只作为 current 覆盖候选，不声明 supported。
- `C10M3-SRC-104` 保留 required rg 对 `TopoShape.cpp` / `TopoShapeMapper.cpp` 的扫描结论，并记录 `TopoShapeExpansion.cpp` 为 `makeShapeWithElementMap` / `MapperHistory` 的 replacement implementation path；若实现再次迁移，S4 需按 reopen condition 复核。
- `C10M3-BLOCKER-101=closed_s1`；S1 未运行 FreeCADCmd、未采 oracle、未修改 `cad-core/src`、tests、fixtures 或 capability。

## 非目标

- 不运行 FreeCADCmd。
- 不采 native oracle。
- 不用 current output 反推 FreeCAD 语义。
- 不把候选源码行升级为 supported 功能声明。
