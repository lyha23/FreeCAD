# PadBody 边引用无法映射到 Pad 修复方案

## 当前结论

前端报错的直接原因是：用户在 Body 显示结果上选中了 `PadBody:Edge7`，但前端准备构造后端 `PartDesign::Revolution.ReferenceAxis` 时，需要把这个显示体子元素重定向到源特征对象 `Pad`。当前 `/cad/recompute` 返回的 subshape 只有裸名：

```json
{
  "id": "PadBody:Edge7",
  "indexed": "Edge7",
  "kind": "Edge",
  "subname": "Edge7",
  "stableSubname": "Edge7"
}
```

这只能证明它是当前结果对象 `PadBody` 的 `Edge7`，不能证明它是 `Pad.Edge7`。前端因此不能安全把它写成：

```json
"ReferenceAxis": {
  "value": "Pad",
  "SubList": ["Edge7"]
}
```

修复方向应放在 `cad-core` 的 Body / NamedShape / response subshape 输出链路：让 Body 显示结果中能唯一追溯到 Tip 特征的裸 `FaceN` / `EdgeN` / `VertexN` 输出带上源特征限定路径，例如：

```json
{
  "id": "PadBody:Edge7",
  "indexed": "Edge7",
  "kind": "Edge",
  "subname": "Pad.Edge7",
  "stableSubname": "Pad.Edge7"
}
```

前端继续保持快速失败，不要把裸 `Edge7` 猜成 `Pad.Edge7`。

## 问题边界

实施仓库：

- `/Users/li/Chili3DProject/FreeCAD/cad-core`

文档落点：

- `/Users/li/Chili3DProject/FreeCAD/docs/BUG修改/`

本轮只处理 `cad-core` 输出契约和可验证用例，不在 `my-chili3d` 增加兼容分支，不绕回旧 wasm / 前端 OCC 路径。

## FreeCAD 调用链依据

FreeCAD 的 Body 显示结果不是重新发明一个独立拓扑空间，而是读取 Tip 特征 shape：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute()`
  - `App::DocumentObject* tip = Tip.getValue();`
  - `tipShape = static_cast<Part::Feature*>(tip)->Shape.getShape();`
  - `Shape.setValue(tipShape);`

Additive / Subtractive 特征通过 `FeatureAddSub::getAddSubShape()` 提供工具 shape：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()`
  - Additive 写 `addShape = AddSubShape.getShape()`
  - Subtractive 写 `subShape = AddSubShape.getShape()`

布尔和 maker 后的命名传播依赖 `TopoShape` 的 element map：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`
  - `mapSubElement(shapes)`
  - 继续用 `MapperMaker::modified/generated` 建立旧元素到新元素的映射

因此 `cad-core` 里 Body 输出的 `subname/stableSubname` 应来自 `NamedShape.elementMap` 和明确的 Body Tip 来源，而不是输出层按 mesh 或三角面顺序猜。

## cad-core 当前链路

当前 `cad-core` 已经有可用的几何和拓扑基础：

- `cad-core/src/part_design/body.cpp`
  - `getBodyTopoShapeAtFeature()` 重放 Body.Group 到 Tip。
  - `executeBody()` 写入：
    - `context.namedShapes[object.name]`
    - `context.shapes[object.name]`
    - `context.mesh[object.name]`
    - `context.subshapes[object.name]`
    - `context.objects[object.name]["tip"]`
    - `context.objects[object.name]["replayed_additive_features"]`
    - `context.objects[object.name]["replayed_replacement_features"]`

- `cad-core/src/runtime/recompute.cpp`
  - `responseSubshapes()` 输出 `id/indexed/subname/stableSubname`。
  - `stableSubnameFor()` 读取 `NamedShape.elementMap`，如果 map 里只有 `Edge7 -> Edge7`，最终仍输出裸 `Edge7`。
  - `currentSubnameForStable()` 只在 `stableSubname` 已有对象前缀时生成 `Pad.Edge7`。

- `cad-core/src/part/shape_exporter.cpp`
  - `edgeSegmentsForShape()` 和 `vertexPointsForShape()` 通过 `TopExp::MapShapes()` 枚举拓扑。
  - 这里负责 mesh 到 `EdgeN` / `VertexN` 的显式桥接，不应承载 Body Tip 语义。

当前断点在 response 命名层：`id/indexed` 已经能把渲染边映射到 subshape，但 subshape 的 `subname/stableSubname` 对 Body 显示结果缺少可回放到 Tip 特征的限定路径。

## 推荐实现

### 1. 不改 mesh 枚举，只改 response subshape 命名

保持这些字段不变：

```json
"id": "PadBody:Edge7",
"indexed": "Edge7"
```

原因：

- `edgeSegments[].id` / `vertexPoints[].id` 依赖 `objectName:indexed` 和 `subshapes[].id` 对齐。
- 这部分已经是前端拾取 token 的桥，不需要改变。

只调整 `responseSubshapes()` 中的 `subname/stableSubname`：

```json
"subname": "Pad.Edge7",
"stableSubname": "Pad.Edge7"
```

这样前端现有 `cadObjectLocalSubname("Pad", "Pad.Edge7")` 就能得到 `Edge7`。

### 2. 增加 Body Tip 直接来源判定

在 `cad-core/src/runtime/recompute.cpp` 或相邻 runtime helper 中增加一个小的命名判定函数，输入为：

- 当前 response object name，例如 `PadBody`
- 当前 `indexed`，例如 `Edge7`
- 当前 `stableSubnameFor()` 结果
- `ComputeContext`

判定为可补 `Tip.` 前缀时，返回 `Pad.Edge7`；否则返回原 stable subname。

建议只在这些条件同时满足时补前缀：

1. 当前对象是 `PartDesign::Body` 的结果。
2. `context.objects[bodyName]["tip"]` 或 `["replay_stopped_at_tip"]` 存在。
3. 当前 stable subname 是裸 `FaceN` / `EdgeN` / `VertexN`，且与 `indexed` 同名。
4. Body 结果能证明是 Tip 特征直接来源：
   - 单个首个 additive 特征，例如 `replayed_additive_features == ["Pad"]`，且没有 subtractive / replacement / refine 叠加；或
   - 单个 replacement Tip，例如 DressUp / Transform 已经作为完整替换特征输出，且它本身是前端要重定向的源对象。
5. Body 没有因为非 identity placement 丢弃 `bodyNamedShape`；如果 `getBodyTopoShapeAtFeature()` 已因 placement 把 `bodyNamedShape` 置空，应先保持裸名，避免把变换后的 Body 边错绑到未变换的 Tip 局部坐标。

不满足上述条件时，不补前缀，继续让前端按不可映射处理。

### 3. 已有 elementMap 结果优先

不要覆盖已有明确来源：

- `Sketch.Edge1`
- `Pad.Face6`
- `Pocket.Face5`
- `草图 11:54:45 PM.Edge4`
- 其它已经带点号的 stable path

规则应是：

```text
如果 stableSubnameFor(indexed, namedShape) 已经返回带对象路径的值，直接使用。
只有 stableSubname == indexed 这种裸名，且 Body Tip 来源唯一时，才补 Tip 前缀。
```

这样不会破坏已有 sketch internal、external geometry、stable history、deleted/split diagnostic 链路。

### 4. Body executor 输出必要的来源元数据

如果 runtime response 层仅靠 `context.objects[bodyName]` 不能精确判断 Body 来源，优先在 `BodyTopoShapeResult` / `executeBody()` 输出更明确的内部字段，例如：

```json
"direct_tip_subshape_owner": "Pad"
```

字段只作为 `cad-core` response 命名辅助，不要求前端消费。它的含义必须严格：

- 该 Body 当前 shape 的裸 `FaceN/EdgeN/VertexN` 可以按同一 index 追溯到这个 Tip 对象。
- 一旦发生 fuse/cut/refine/non-identity placement，不能设置该字段，除非 `NamedShape.elementMap` 已经给出唯一 source path。

这比在 `responseSubshapes()` 里硬读 `tip` 更安全，也更容易写测试。

### 5. 单测和 fixture 覆盖

本轮最小完整语义批次建议覆盖三类：

1. 简单 `Sketch -> Pad -> Body`
   - 断言 Body result 的 `EdgeN` / `FaceN` / `VertexN` 至少对裸 direct-tip 元素输出 `Pad.EdgeN` / `Pad.FaceN` / `Pad.VertexN`。
   - 断言 `id` 仍是 `PadBody:EdgeN`，`indexed` 仍是 `EdgeN`。

2. `Sketch -> Pad -> Body -> Revolve 以 Body 边为轴`
   - 构造和前端等价的请求：Revolve Profile 来自 Body face，ReferenceAxis 应可由 `Pad.EdgeN` 回落成 `Pad` 的 `EdgeN`。
   - 断言不再出现“所选旋转轴无法映射到后端特征子形状”对应的后端输入缺口。

3. 布尔或多特征 Body
   - 选择一个已有 Pad + Pocket / Pad + Fillet 类 fixture。
   - 断言没有唯一来源的裸新边不被强行补成 `Tip.EdgeN`。
   - 已经有 `Pad.FaceN` / `Pocket.FaceN` 这类 stable history 的项继续保持原值。

候选测试文件：

- `cad-core/tests/test_adapters.py`
  - 当前已经检查 `edgeSegments[].id` 与 `subshapes[].id` 对齐，适合补 response-level contract。
- `cad-core/tests/test_p6_topology.py`
  - 当前已有 `Pad.Face6`、`Pocket.Face5` 等 stable history 断言，适合防回归。
- 必要时新增小 fixture 到 `cad-core/fixtures/c51m1/` 或当前 PartDesign 分组目录，避免把用例塞进无关阶段。

## 非目标

- 不在前端把裸 `Edge7` 猜成 `Pad.Edge7`。
- 不改变 `edgeSegments` / `vertexPoints` 的 id 对齐规则。
- 不从 mesh 三角面、线段顺序、点坐标反推 topology identity。
- 不把所有 Body 裸 `EdgeN` 无条件改成 `Tip.EdgeN`。
- 不为这一个报错新增旧 wasm / ShapeFactory / HTTP 单操作兼容层。
- 不把 ambiguous split / deleted / boolean-generated 元素包装成稳定来源。

## 验收标准

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_adapters.py -k edge
python3 -m unittest tests/test_p6_topology.py -k stable
git diff --check
```

如果本地 `unittest -k` 不可用，则改用对应测试类或具体测试方法名运行。

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_mvp.py tests/test_adapters.py tests/test_p6_topology.py
```

重型收口只在准备合并或修改了 `topo_shape.cpp` 主历史传播逻辑时执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest discover -s tests
```

最终可接受输出：

- `PadBody:Edge7` 仍作为显示对象 token。
- 对可唯一追溯到 Tip 的 Body direct-tip 边，`subname/stableSubname` 带 `Pad.` 前缀。
- 前端可把 `PadBody:Edge7` 安全重定向为后端 `Pad.Edge7` 输入。
- 多特征、布尔、split/deleted、非 identity placement 场景不新增错误稳定名。
