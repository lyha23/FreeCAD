# C8-M7 S1 FreeCAD 源码与 current 覆盖复核

## 目标

复核 ImportBrep / ImportStep / ImportIges、TopoShape import、ElementMap、PropertyLinks 和 current cad-core import first slice，形成 C8-M7 的 source authority。

## 输入

- FreeCAD：`src/Mod/Part/App/FeaturePartImportBrep.cpp`
- FreeCAD：`src/Mod/Part/App/FeaturePartImportStep.cpp`
- FreeCAD：`src/Mod/Part/App/FeaturePartImportIges.cpp`
- FreeCAD：`src/Mod/Part/App/ImportStep.cpp`
- FreeCAD：`src/Mod/Part/App/ImportIges.cpp`
- FreeCAD：`src/Mod/Part/App/TopoShapeExpansion.cpp`
- FreeCAD：`src/Mod/Part/App/TopoShapeMapper.cpp`
- FreeCAD：`src/App/ElementMap.cpp`
- FreeCAD：`src/App/PropertyLinks.cpp`
- cad-core：`cad-core/src/part/part_import.cpp`
- cad-core：`cad-core/src/part/topo_shape_expansion.cpp`
- cad-core：`cad-core/src/runtime/reference_resolution.cpp`
- cad-core：`cad-core/src/runtime/element_reference_update.cpp`
- tests：`cad-core/tests/test_p6_topology.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`

## 必须复核

- FreeCAD import features 只从 `FileName` 读当前文件；文件不可读是错误路径，不是隐式使用旧后端 cache。
- current cad-core 对 BREP / STEP / IGES 已按当前文件导入，并通过 `namedShapeForImportedShape()` 生成 import ElementMap 和 mapper history。
- current tests 是否已经覆盖 `import_shape_element_map`、owner-qualified alias 和 capability row。
- `ReferenceShadow.brep` 的现有语义是否只作为单 subshape snapshot evidence。

## 必须回写

- `c8m7_import_shape_recovery_source_candidates.tsv`
- `C8M7-BLOCKER-101`
- README 的 S1 结论段。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'FileName|ImportBrep|ImportStep|ImportIges|importStep|importIges|importBrep|ElementMap|ReferenceShadow|ShadowSub' src/Mod/Part/App src/App cad-core/src/part cad-core/src/runtime cad-core/tests/test_p6_topology.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不新增 fixture。
- 不改 capability。
- 不用 current cad-core 输出倒推 FreeCAD expected。
