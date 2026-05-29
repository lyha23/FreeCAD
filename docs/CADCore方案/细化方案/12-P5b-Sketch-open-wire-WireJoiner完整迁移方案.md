# P5b：Sketch open-wire / WireJoiner 完整迁移方案

## 结论

`cad-core` 的 Sketch 内部面生成不能停留在当前 closed-wire baseline。目标必须迁移 FreeCAD 完整路径：

```text
SketchObject::buildInternals()
  -> TopoShape::makeElementFace(..., "Part::FaceMakerBuildFace")
  -> FaceMakerBuildFace / FaceMaker::postBuild()
  -> WireJoiner::getOpenWires()
  -> InternalShape + InternalFace/InternalEdge/InternalVertex + internal element map
```

这条路径是支持“外矩形 + 中间分隔线 -> 两个可选 profile 面”的关键。当前 `cad-core/src/features/sketch_object.cpp` 只把 closed wires 送进 `geometry::makeFaceWithHolesFromClosedWires()`，能处理多个已经闭合的 loop，但不能把开放分隔线参与到面域划分里。

## 当前 cad-core 缺口

当前 `cad-core` 的 Sketch 流程位于：

```text
/Users/admin/Chili3DProject/重构Chili/cad-core/src/features/sketch_object.cpp
```

关键现状：

- `buildRawSketchShape()` 先尝试 `makeClosedWiresFromEdges()`，要求边能被拆成闭合 wire。
- `buildOptionalProfileFace()` 只接受闭合 wires、circle、ellipse，然后调用 `makeFaceWithHolesFromClosedWires()`。
- `internalShape = *profileFace` 只是 P5 baseline，并非 FreeCAD 完整 `buildInternals()`。
- 源码注释已标明缺口：`SketchObject::buildInternals() -> FaceMakerBuildFace + WireJoiner::getOpenWires()` 尚未迁移。

直接后果：

```text
外矩形四条边 + 中间一条分隔线
```

会失败或得不到两个面，因为中间线不是闭合 wire。只有把左右区域显式输入成两个闭合 loop，当前实现才可能得到 `InternalFace1` / `InternalFace2`。这不符合 Sketcher 交互语义。

## FreeCAD 依据

### SketchObject 主入口

本地 FreeCAD 源码：

```text
/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
```

关键语义：

- `SketchObject::execute()` 先把草图几何构造成 raw `Shape`。
- 设置 `Shape` 前先设置 `InternalShape`，因为外部引用可能指向 internal subshape。
- `SketchObject::buildInternals(const Part::TopoShape& edges)` 是内部面生成入口。
- `getInternalElementMap()` 从 `InternalShape` 回查 raw `Shape`，建立 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN` 映射。

`buildInternals()` 的核心流程：

```text
result = result.makeElementFace(
  edges.getSubTopoShapes(TopAbs_WIRE),
  "",
  "Part::FaceMakerBuildFace",
  nullptr
)

WireJoiner joiner;
joiner.setTightBound(true);
joiner.setMergeEdges(true);
joiner.addShape(edges);
joiner.getOpenWires(openWires, "SKF");

if openWires is null:
  return result
if result is null:
  return openWires
return result.makeElementCompound({result, openWires})
```

这里的语义不是“只要闭合 wire 才能生成面”。`FaceMakerBuildFace` 负责把线网切分、生成内部面；`WireJoiner` 负责把未被闭合面消费的开放片段作为 internal open wires 输出。

### FaceMaker / history

本地 FreeCAD 源码：

```text
/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
```

关键语义：

- `FaceMaker::Build()` 调用具体 maker 的 `Build_Essence()`，之后统一 `postBuild()`。
- `FaceMaker::postBuild()` 会把结果写入 `TopoShape`，并处理 source shape 到结果 face/edge 的命名映射。
- `FaceMakerBuildFace` 会修改/切分输入边，`postBuild()` 需要通过 maker/splitter history 把结果追溯回原始草图元素。

迁移时不能只复刻几何形状，必须保留 history / element-map 语义。否则即使面数量对了，`InternalEdgeN`、`InternalVertexN` 和后续稳定引用仍会漂移。

### WireJoiner

本地 FreeCAD 源码：

```text
/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.h
/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
```

关键语义：

- `WireJoiner::getOpenWires(TopoShape& shape, const char* op = "", bool noOriginal = true)` 是 `SketchObject::buildInternals()` 使用的开放片段输出 API。
- `WireJoiner::getResultWires()` 是另一类闭合结果 wire API，不是 Sketch internal shape 主路径。
- `WireJoinerP::EdgeInfo` 记录每条边的状态、切分、归属、`wireInfo` / `wireInfo2`、`iteration` 等。
- `WireJoinerP::build()` 会构造 `openWireCompound`，开放片段输出不是调试数据，而是 `InternalShape` 的一部分。
- `getOpenWires(noOriginal=true)` 会在输出时过滤 original source edge，保留真正需要暴露的 open fragments。

迁移时不要用“结果侧删边”“按几何猜测归属”“只看 final face 使用了哪些边”替代 `WireJoinerP` 的状态机。

## 目标行为

### 矩形 + 中间分隔线

输入：

```text
外矩形 4 条边
中间一条贯穿分隔线
```

期望：

- `Sketch.status = ok`
- `Sketch.profile_ready = true`
- `Sketch.internal_shape = occt_internal_shape`
- `Sketch.internal_face_count = 2`
- `subshapes.Sketch` 包含 `InternalFace1`、`InternalFace2`
- 两个 internal face 都能被前端 hover / select，并转换成独立 `SketchProfileRef`
- 中间分隔线对应的 internal edge / vertex 映射必须可追踪到原始草图元素或其 split history

### 开放线段

输入：

```text
只有开放线段，无法形成闭合面
```

期望：

- raw `Shape` 保留开放线。
- `profile_ready = false`
- `InternalShape` 可包含 WireJoiner 输出的 open wires，具体是否为空以 FreeCAD oracle 为准。
- Pad / Pocket 仍应通过 `open_profile` 失败，不能把 open wire 伪造成 face。

### 混合线网

输入：

```text
闭合外轮廓 + 开放切割线 / 相交线 / 多个内部切割线
```

期望：

- `FaceMakerBuildFace` 负责生成可闭合的内部面。
- `WireJoiner::getOpenWires()` 负责追加剩余开放片段。
- 输出的 `InternalFaceN`、`InternalEdgeN`、`InternalVertexN` 必须来自同一套 history / element map，不允许用前端或 executor 侧临时编号拼接。

## cad-core 落点

建议新增或扩展：

| 模块 | 目标 |
| --- | --- |
| `features/sketch_object.cpp` | 将 `buildOptionalProfileFace()` baseline 替换为 `buildSketchInternalsLikeFreeCAD()` |
| `geometry/face_maker_build_face.*` | 迁移 `Part::FaceMakerBuildFace` 的线网构面和 split history |
| `geometry/wire_joiner.*` | 迁移 `WireJoinerP` 状态机、open-wire compound、history |
| `topo/element_map.*` | 消费 FaceMaker / WireJoiner history，生成 internal element map |
| `topo/named_shape.*` | 支持 InternalFace/InternalEdge/InternalVertex 的稳定引用恢复 |
| `tests/fixtures/p5*` | 增加 FreeCAD oracle 对齐 fixture |

`runtime::ShapeValue` 已经有 `profileShape` 和 `internalShape` 字段，可以继续作为承载点；但 `profileShape` 不应再只等同于 closed-wire face，`internalShape` 也不能再简单赋值为 `profileFace`。

## 实施步骤

### 1. 固定 FreeCAD oracle

先在 FreeCAD checkout 里建立 fixture oracle，覆盖：

- 外矩形 + 中间一条贯穿线：2 faces。
- 外矩形 + 两条贯穿线：3 faces 或对应 FreeCAD 实际结果。
- 外矩形 + T 型开放线：face 与 open wire 的组合结果。
- 外矩形 + 十字分隔线：4 faces。
- 开放多段线：无 profile face，检查 open-wire 输出。
- 近切线 / 重合 / 相交边界：记录 diagnostics 或 known gap。

每个 fixture 记录：

- `InternalShape` 是否为空。
- `InternalShape.Faces/Edges/Vertexes` 数量。
- `InternalShape.ElementMap` / reverse map。
- `Shape` 与 `InternalShape` 的 `InternalEdgeN <-> EdgeN`、`InternalVertexN <-> VertexN` 映射。

### 2. 迁移 FaceMakerBuildFace

不要把 `geometry::makeFaceWithHolesFromClosedWires()` 继续扩展成大杂烩。它只适合 closed-wire baseline。

应新增专门模块承接 FreeCAD `FaceMakerBuildFace`：

- 接收 raw sketch wires / edges。
- 调用 OCCT 构面和 split 流程。
- 保存 pre-split history、splitter history、source shape 映射。
- 返回 result faces / compound。

### 3. 迁移 WireJoinerP 账本

按 FreeCAD `WireJoinerP` 的数据结构迁移，不要用结果后处理替代：

- `EdgeInfo`
- `WireInfo`
- `wireInfo` / `wireInfo2`
- `iteration` / `iteration2`
- `superEdge`
- `openWireCompound`
- `getOpenWires(noOriginal=true)` 的 original filtering

开放片段必须从 WireJoiner 状态机自然输出，而不是在 `sketch_object.cpp` 里猜哪些边没被 face 用掉。

### 4. 统一 InternalShape 发布

`SketchObject` executor 的目标结构：

```text
rawShape = buildRawSketchShape(...)
internalFaceShape = faceMakerBuildFace(rawShape / wires)
openWires = wireJoinerGetOpenWires(rawShape / wires)
internalShape = compound(internalFaceShape, openWires)
profileShape = internalFaceShape 可用于 Pad/Pocket 的 face/compound 子集
```

如果 `FaceMakerBuildFace` 失败但 WireJoiner 仍有 open wires，应按 FreeCAD 结果决定 `InternalShape` 是否只含 open wires。不要为了让 Pad 成功把 open wires 升级成 face。

### 5. element map / stable subname

迁移后必须同步解决命名：

- `InternalFaceN` 用 FaceMaker 输出的 face 子形状编号。
- `InternalEdgeN` / `InternalVertexN` 从 internal shape 子形状导出。
- `internal_element_map` 不能只靠几何相等一次性查找；涉及 split / merged / deleted 时要消费 FaceMaker / WireJoiner history。
- 对无法稳定恢复的 split/deleted 场景，输出明确 diagnostic，而不是发明假映射。

## 接口输出要求

`/cad/recompute` 或 CLI recompute 对 Sketch 至少输出：

```json
{
  "objects": {
    "Sketch": {
      "status": "ok",
      "shape": "occt_sketch_shape",
      "profile": "occt_face|occt_compound|none",
      "profile_ready": true,
      "internal_shape": "occt_internal_shape",
      "internal_face_count": 2,
      "internal_edge_count": 7,
      "internal_vertex_count": 6,
      "internal_element_map": {}
    }
  },
  "subshapes": {
    "Sketch": {
      "InternalFace1": {},
      "InternalFace2": {},
      "InternalEdge1": {},
      "InternalVertex1": {}
    }
  },
  "diagnostics": []
}
```

前端后续可以把 `InternalFaceN` 作为正式 sketch profile token。不要返回只在前端本地有效的 polygon id。

## 禁止事项

- 禁止把“外矩形 + 中间线”预处理成两个闭合矩形来假装支持。
- 禁止让前端 `ClosedPolygonDetector` 作为正式 profile region 来源。
- 禁止在 `sketch_object.cpp` 里按“哪些边没被 face 用过”手工拼 open wire 输出。
- 禁止用 mesh 三角面反推长期 profile identity。
- 禁止把 open profile 静默兜底成可 Pad 的 face。
- 禁止只做 face 数量对齐而忽略 `InternalEdgeN` / `InternalVertexN` / history 映射。

## 验收标准

最小验收：

- 自然输入“外矩形 4 边 + 中间 1 条贯穿分隔线”返回 2 个 `InternalFaceN`。
- 对同一 fixture，FreeCAD oracle 与 `cad-core` 的 `InternalShape` face / edge / vertex 数量一致。
- `subshapes.Sketch` 包含可拾取的 `InternalFace1`、`InternalFace2`。
- Pad / Pocket 引用其中一个 `InternalFaceN` 时，只拉伸对应半边区域。
- 开放线 fixture 不产生假 `profile_ready=true`。
- 所有失败路径输出明确 diagnostics，不继续下游几何计算。

扩展验收：

- 十字分隔矩形可形成 4 个 internal faces。
- T 型开放线保留 FreeCAD 一致的 face / open-wire 组合结果。
- 重合、相交、近切线场景有 FreeCAD oracle 对齐或明确 known gap。
- `internal_element_map` 在 split/merged 场景中不靠几何猜测，必须由 history 传播或明确诊断。

## 与 P5 / P6 的关系

P5 当前文档中的 closed-wire face-with-holes 只是基础能力，不能代表完整 Sketch internal face。

P5b 负责补齐 Sketch internal geometry：

```text
FaceMakerBuildFace geometry
WireJoiner open-wire output
InternalShape 发布
InternalFace/InternalEdge/InternalVertex 导出
```

P6 负责把这些结果接入 topo naming：

```text
NamedShape
ElementMap
MapperHistory
stable subname resolve
split / deleted diagnostics
```

两者必须联动推进。只迁几何不迁 history，会导致前端能看到两个面，但后续编辑后引用漂移；只迁 naming 不迁 WireJoiner，也无法得到 FreeCAD 一致的 internal shape。
