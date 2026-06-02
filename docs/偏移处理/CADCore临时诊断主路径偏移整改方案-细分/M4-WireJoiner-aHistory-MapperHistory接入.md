# M4 WireJoiner aHistory / MapperHistory 接入

## 目标

让 `NamedShape` / `ElementMap` 消费 WireJoiner 产出的正式 history，而不是从 raw/internal 几何关系采样发明 split/generated/deleted。

M4 解决 topo consumer 问题。它依赖 M1-M3 产出可信 producer ledger。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::MapperHistory`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap()`

关键语义：

```text
shape.makeShapeWithElementMap(comp, MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op)
```

## cad-core 落点

- `cad-core/include/cad_core/topo/named_shape.h`
- `cad-core/src/topo/named_shape.cpp`
- `cad-core/include/cad_core/topo/element_map.h`
- `cad-core/src/topo/element_map.cpp`
- `cad-core/src/features/sketch_object.cpp`
- `cad-core/src/geometry/wire_joiner.cpp`

## 当前基线

已完成：

- FaceMaker generated face history 已进入 `NamedShape.history` 的正式消费路径。
- `topo` 只保留 FreeCAD `getInternalElementMap()` 对应的 exact InternalEdge/InternalVertex alias。
- WireJoiner summary 已透传到 `SketchInternalHistoryContext`。
- `element_history_status` 已能标出 `wire_joiner_history:splitter`、`modified`、`generated`、`deleted`、`open_export`。

仍未完成：

- WireJoiner `aHistory` 尚未完整迁移为元素级 source-to-result map。
- 当前 open-export entries 还是 producer evidence，不是完整 `MapperHistory(aHistory)` consumer。
- ShapeFix `makeCleanWire()` history merge、`aHistory->Remove()` 的完整元素级传播仍未完成。

## 必收切片

1. `WireJoinerHistorySummary` 从 count / status 扩展为元素级 mapping source。
2. `SketchInternalHistoryContext` 明确区分：
   - FaceMaker history。
   - WireJoiner splitter history。
   - WireJoiner generated / modified / deleted history。
   - open-export child-wire history。
3. `namedShapeForSketchInternalShape()` 只消费 history context，不从 raw/internal shape 反推 WireJoiner history。
4. `ElementMap` 对 InternalEdge / InternalVertex 的 stable subname 追踪只来自 exact map 或 `MapperHistory`。

## 边界

M4 不生成 WireJoiner result-wire identity。那属于 M3。

M4 不决定哪些 open wires 输出。那属于 M2。

M4 只做 topo consumer：把 producer ledger 转成 `NamedShape.history`、`ElementMap` 和诊断状态。

## 非目标

- 不恢复 `samplesLieOnEdge()`、endpoint sampling、outer wire sampling 这类 history 主路径。
- 不在 response 层按输出顺序重命名 InternalEdge。
- 不用 ReferenceShadow 生成新的 WireJoiner history。

## 验收

完成条件：

- `NamedShape.history` 中每条 WireJoiner generated / modified / deleted / open_export 都能追溯到 WireJoiner producer entry。
- `ElementMap` 不再靠 raw/internal 几何采样合成 split/deleted。
- 一对多 split、terminal deleted、open result-wire 都能在 history detail 中找到 source entry。

建议检查：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg "internalGeneratedFaceHistoryForSketch|internalSplitElementHistoryForSketch|internalDeletedElementHistoryForSketch|samplesLieOnEdge" cad-core/src/topo
```

命中必须是已删除路径、diagnostic 或明确不参与 `NamedShape.history` 主路径。
