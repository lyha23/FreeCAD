# C6-M7-S1 FreeCAD 源码与 PropertyLinkList 边界复核

## 目标

复核 FreeCAD Loft 调用链和 `Sections` 属性边界，证明当前 gap 是 `PropertyLinkList` native-hidden 问题还是 cad-core DTO 缺口。

## 必读源码

- `src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`
- `src/App/PropertyLinks.cpp::PropertyLinkList`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`

## 产物

- 更新 source candidates、input contract、backend gap 分类矩阵。
- 写清 FreeCAD 原文短句、调用顺序、cad-core 落点和不可越界的字段。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'Loft::execute|Sections|getValues|getTopoShape|makeElementLoft|MapperThruSections|PropertyLinkList' src/Mod/Part/App/PartFeatures.cpp src/App/PropertyLinks.cpp src/Mod/Part/App/TopoShapeExpansion.cpp cad-core/src/part/part_loft.cpp cad-core/src/part/topo_shape_expansion.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线
```

## 非目标

- 不改 collector 或 runtime。
- 不决定删除 remaining gap；S1 只提供 source evidence。
