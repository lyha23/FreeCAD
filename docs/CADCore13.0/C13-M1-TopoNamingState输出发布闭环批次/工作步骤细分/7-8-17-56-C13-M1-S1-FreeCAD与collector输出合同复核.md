# C13-M1 S1 FreeCAD 与 collector 输出合同复核

## 目标

明确 `topoNamingState` 中哪些字段来自 FreeCAD 语义，哪些只是 collector expected schema，避免 runtime 代码照 fixture 字符串反推。

## 必读文件

- S0 输出
- `src/App/ElementMap.cpp`
- `src/App/PropertyLinks.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`

## 操作

1. 记录 FreeCAD `ElementMap` / `TopoShape::makeShapeWithElementMap()` / `PropertyLinkBase::_updateElementReference()` 的关键语义。
2. 对照 collector 的 `topo_state_object_payload()`，把字段分为：
   - C13-M1 必须实现。
   - 可继承或可占位。
   - 后续 mapped-name parity。
3. 抽样 p2/p5/c4m6 expected，确认最小完整语义批次。
4. 更新 source / contract / non-goal matrix。

## 关闭条件

- `c13m1_topo_state_source_matrix.tsv` 有明确 source authority。
- `c13m1_topo_state_contract_matrix.tsv` 标出 `required_now` / `followup_mapped_name`。
- 不再存在“直接复制 expected mappedName”这类实现路线。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```
