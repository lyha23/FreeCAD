# M0 fallback 闸门

## 目标

先冻结偏移扩散。任何会影响 `InternalShape`、`NamedShape`、`ElementMap`、profile selection、subshapes 输出的 fallback，都必须有 FreeCAD 依据、明确边界和删除条件。

M0 不是普通功能实现。它的作用是防止后续 M1-M4 还没完成时，又在输出端补新的 fixture 形态规则。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::Build_Essence()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::MapperHistory`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap()`

关键判断：FreeCAD 有对应账本或 policy 的 fallback 可以迁移；只有 fixture 形态、endpoint、boundary、face count、raw/internal geometry sampling 支撑的 fallback 不能进入主路径。

## cad-core 落点

- `cad-core/src/geometry/wire_joiner.cpp`
- `cad-core/src/geometry/sketch_internal_builder.cpp`
- `cad-core/src/geometry/face_maker.cpp`
- `cad-core/src/topo/named_shape.cpp`
- `cad-core/src/topo/element_map.cpp`
- `cad-core/src/features/sketch_object.cpp`

## 当前基线

已从主路径删除或降级的偏移：

- `classifyBoundedFaceOwnership()` 不再改写 WireJoiner final export state。
- `copiedResultWireGraphProbeForSketchInternals()` 已删除。
- `getOpenWires()` 不再使用 endpoint-touch / boundary-touch / same-coordinate 过滤。
- FaceMakerBuildFace 不再用 face count / topology split 切换最终 topology。
- raw/internal 几何合成 split/generated/deleted history 不再写入 `NamedShape.history` 主路径。

仍允许暂存的临时桥：

- `generatedOpenExportShapeForSketchInternals()`：只允许作为 generated result-wire identity 迁移前的过渡来源；当前 producer reason 只能作为诊断分类，不能反向决定 `InternalShape`、`NamedShape.history` 或 `ElementMap`。
- `purgeAsOriginalOpenEdge`：只允许作为 original open edge 与 split/result edge 的临时 identity bridge；当前赋值只能来自 unsplit `EdgeInfo` 与 source vertex identity，不能恢复 midpoint、face count、boundary-touch 或 same-coordinate gate。

## 本轮闸门审计

审计结论：当前 `cad-core/src` / `cad-core/include` 未发现未登记的 fixture 名称分支，也未发现新增 midpoint、endpoint-touch、boundary-touch、same-coordinate、face count、partial overlap 主路径判断。现存命中按以下边界处理：

- `generatedOpenExportShapeForSketchInternals()` 及其 `consumed_open_cutter_graph`、`partial_junction_open_cutter`、`closed_wire_cycle`、`partial_shared_closed_wire` producer reason 属于已登记临时桥的诊断拆分；删除条件仍是 M3 用真实 result-wire identity 替换 generated open-export 来源。
- `purgeAsOriginalOpenEdge` 属于已登记 identity bridge；删除条件仍是 M1/M2 补齐 `sourceEdgeArray` / `VertexInfo` / `openWireCompound` child-wire ownership。
- `samplesLieOnEdge()` 仅用于 `SketchObject::getInternalElementMap()` 对应的 exact edge alias / `CheckGeometry` 复核；不得扩展为 generated / split / deleted history 合成入口。
- `reference_matcher.cpp` 的 edge sample 只服务批准的 `ReferenceShadow` 单 subshape 恢复通道，不进入建模输入或完整 BREP 状态。
- `face_maker.cpp` 的 `samplePoint()` 和 `boundedFaceCount` 只服务 FaceMaker 内部 face/hole 分类与 metadata；不得重新用 face count 选择 WireJoiner 或 Sketch InternalShape 主路径。
- `sketch_object.cpp` 的 midpoint 命中属于 Sketcher constraint 解析，不参与 WireJoiner ownership、open export 或 topo history。

## 必收切片

1. 所有临时桥必须在文档和相邻实现里有删除条件。
2. 新增诊断字段只能写 result metadata，不得反向影响 shape 或 history。
3. `wire_joiner_ledger` 可以继续扩展，但不能变成新的业务决策入口。
4. fixture 修复不能新增 fixture 名称分支、几何形态分支或 response 端重命名。

## 非目标

- 不在 M0 内删除所有临时桥。
- 不在 M0 内补完整 `aHistory`。
- 不用 M0 判断 fixture 是否最终 parity，只判断有没有新增偏移。

## 验收

代码搜索必须能解释每个命中：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg "boundary-touch|same-coordinate|topologyWasSplit|samplesLieOnEdge|generatedOpenExportShapeForSketchInternals|purgeAsOriginalOpenEdge" cad-core/src cad-core/include
```

通过标准：

- 命中要么是已登记临时桥，要么是 diagnostic / low-level helper，要么是已完成清理的历史注释。
- 不出现新的 fixture 名称分支。
- 不出现新的 midpoint、endpoint、face count、partial overlap 主路径判断。
