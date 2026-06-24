# 【已实现】C6-M7-S1 FreeCAD 源码与 PropertyLinkList 边界复核

## 目标

复核 FreeCAD Loft 调用链和 `Sections` 属性边界，证明当前 gap 是 `PropertyLinkList` native-hidden 问题还是 cad-core DTO 缺口。

## 执行基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=1c197bf648`。
- `git log -1 --oneline=1c197bf648 冻结 C6-M7 S0 live 基线`。
- S1 开始时工作区干净。
- S1 开始时 C6-M7 队列从 `6-25-00-55-C6-M7-S1-FreeCAD源码与PropertyLinkList边界复核.md` 继续。

## 必读源码

- `src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`
- `src/App/PropertyLinks.cpp::PropertyLinkList`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`

## 产物

- 已更新 source candidates、input contract、backend gap classification、scope、blocker、oracle 和 validation 矩阵中的 S1 actual evidence。
- 已在 README / 主线总入口 / 本 step 写清 FreeCAD 原文短句、调用顺序、cad-core 落点和不可越界字段。

## FreeCAD 源码证据

- `src/Mod/Part/App/PartFeatures.cpp::Loft::Loft()` 声明：`ADD_PROPERTY_TYPE(Sections, (nullptr), "Loft", App::Prop_None, "List of sections")`，属性语义是 section object list。
- `src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` 调用顺序：先检查 `Sections.getSize()`，再遍历 `Sections.getValues()`；每个对象通过 `getTopoShape(obj, ShapeOption::ResolveLink | ShapeOption::Transform)` 取 shape；随后调用 `result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)`。
- `src/App/PropertyLinks.cpp::PropertyLinkList::getPyValue()` 只接受 `DocumentObjectPy`：`Base::PyTypeCheck(&item, &DocumentObjectPy::Type)`；`setValues()` 保存 `std::vector<DocumentObject*>`；`getLinks()` 对 `subs` 和 `newStyle` 执行 `(void)`，`getLinksTo()` 对 `subname` 执行 `(void)subname`。这证明 `PropertyLinkList` 不提供 native subname/subelement storage。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()` 使用 `BRepOffsetAPI_ThruSections`，执行 `SetMaxDegree(maxDegree)`、profile `AddVertex/AddWire`、`CheckCompatibility(Standard_True)`、`Build()`，最后通过 `MapperThruSections(aGenerator, profiles)` 进入 `makeShapeWithElementMap(...)`。

## cad-core 落点

- `cad-core/src/part/part_loft.cpp::resolveLoftSections()` 当前读取 `app::readLinks(object, "Sections")`，并使用 `link.object` 查找 `context.shapes` / `context.namedShapes`；发布 metadata 也只记录 section object names。
- `cad-core/src/part/topo_shape_expansion.cpp::makeElementLoftFromSources()` 已使用 `BRepOffsetAPI_ThruSections`，并通过 `namedShapeForThruSectionsHistory(...)` 记录 `part_loft:thru_sections_history`。
- 因此 S1 结论是：当前 remaining gap 的 FreeCAD 侧证据是 `PropertyLinkList` native-hidden subelement storage；cad-core selected subelement DTO 只是 S2 待判定的 request-local product contract non-parity 候选，不是本步要实现或宣布的 parity 缺口。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'Loft::execute|Sections|getValues|getTopoShape|makeElementLoft|MapperThruSections|PropertyLinkList' src/Mod/Part/App/PartFeatures.cpp src/App/PropertyLinks.cpp src/Mod/Part/App/TopoShapeExpansion.cpp cad-core/src/part/part_loft.cpp cad-core/src/part/topo_shape_expansion.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线
```

S1 不跑构建、不跑 focused tests、不运行 FreeCADCmd；这些属于 S3/S5 或 oracle 任务。

## 非目标

- 不改 collector 或 runtime。
- 不决定删除 remaining gap；S1 只提供 source evidence。
- 不改 C++。
- 不创建 fixture。
- 不声明 FreeCAD parity。
- 不做 S2 route decision。
