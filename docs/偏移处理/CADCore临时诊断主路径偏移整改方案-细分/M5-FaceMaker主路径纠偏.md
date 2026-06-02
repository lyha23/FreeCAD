# M5 FaceMaker 主路径纠偏

## 目标

保护已经完成的 FaceMaker 主路径整改，防止后续 WireJoiner 或 topo 改动又把 face count、shared boundary、topology split 选择逻辑带回主路径。

M5 主要是已完成 milestone 的回归守护。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::Build_Essence()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::postBuild()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::MapperHistory`

关键字段：

- `myShapesToReturn`
- `myPreSplitHistory`
- `mySplitter`
- `MapperMaker(mySplitter)`
- `MapperHistory(myPreSplitHistory)`

## cad-core 落点

- `cad-core/src/geometry/face_maker.cpp`
- `cad-core/include/cad_core/geometry/face_maker.h`
- `cad-core/src/geometry/sketch_internal_builder.cpp`
- `cad-core/src/topo/named_shape.cpp`

## 当前基线

已完成：

- `makeFacesFromClosedWiresAndSplitEdgesDetailed()` 直接以 BuilderFace edge-network result 作为 InternalShape / history runtime。
- `makeFaceWithHolesFromClosedWiresImpl()` 只保留 profile / holes helper 职责。
- FaceMaker 不再用 face count、shared boundary 或 `topologyWasSplit()` 选择最终 topology。
- FaceMaker generated face history 已进入 `NamedShape.history` 正式消费路径。
- single closed wire、无 split、bounded face 与 profile face 一一等价时，保留 source-wire face，避免破坏 `InternalEdge1 -> Edge1` 类 FreeCAD 可见顺序。
- terminal split/deleted history 已从 FaceMaker runtime 进入 `NamedShape.history`。

## 必收切片

后续如果改 FaceMaker，只允许沿以下方向补：

1. 补 FreeCAD BuilderFace / FaceMaker history 字段。
2. 补 `myShapesToReturn` 等价产物。
3. 补 `myPreSplitHistory` / splitter history consumer。
4. 补针对 FreeCAD face maker mode 的明确分支。

## 边界

FaceMaker 负责 face 生成与 FaceMaker history。

FaceMaker 不负责：

- WireJoiner open result-wire 导出。
- WireJoiner generated result-wire identity。
- topo 层 ElementMap 最终传播。
- Profile selection 的业务兜底。

## 非目标

- 不恢复 `topologyWasSplit()` 主路径选择。
- 不用 face count 比较决定最终 shape。
- 不把 WireJoiner open edge 泄漏问题归到 FaceMaker。

## 验收

回归标准：

- overlapping closed wires、hole/island、T-junction open splitter case 仍可由 FaceMaker history 解释。
- `NamedShape.history` 中 FaceMaker generated/split/deleted 不来自 raw/internal 几何采样。
- WireJoiner fixture 失败时，不先改 FaceMaker face count 规则。

建议检查：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg "topologyWasSplit|face count|shared boundary" cad-core/src/geometry/face_maker.cpp cad-core/src/geometry/sketch_internal_builder.cpp
```
