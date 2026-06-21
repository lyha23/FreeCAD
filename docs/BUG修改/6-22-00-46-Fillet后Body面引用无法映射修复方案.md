# Fillet 后 Body 面引用无法映射修复方案

## 当前结论

这是后端 `cad-web-background` / `cad-core` 的 Body Tip 子形状导出问题，不应在前端把数据结构补成兼容分支。

前端日志中的关键数据是：

```json
{
  "profileRef": {
    "objectName": "PadBody",
    "indexed": "Face5",
    "subname": "Pad.Face5",
    "stableSubname": "Pad.Face6"
  },
  "targetObjectName": "Fillet"
}
```

这份引用只能说明用户点中的是 `PadBody` 当前显示结果里的 `Face5`，并且它追溯到 `Pad.Face6`。它没有表达这个面在当前 Tip 特征 `Fillet` 上的局部名字。前端要构造后续 `PropertyLinkSub` 时，需要得到：

```json
{
  "objectName": "Fillet",
  "indexed": "Face5",
  "subname": "Face5",
  "stableSubname": "Pad.Face6"
}
```

因此后端 Body result 的 `subshapes[]` 应返回可剥离的 Tip child path，例如：

```json
{
  "id": "PadBody:Face5",
  "indexed": "Face5",
  "kind": "Face",
  "subname": "Fillet.Face5",
  "stableSubname": "Fillet.Pad.Face6"
}
```

前端再从 `Fillet.Face5` / `Fillet.Pad.Face6` 中剥掉 `Fillet.`，得到后续 feature 可写入的 `SubList=["Face5"]`、`StableSubList=["Pad.Face6"]`。

如果后端只返回 `Pad.Face5` / `Pad.Face6`，前端无法安全判断这个 Body face 是否就是 `Fillet.Face5`；强行猜会把 TopoNaming 错误隐藏到前端。

## 额外异常信号

同一批 `sourceSubshapes` 里还有 kind 串型：

```json
{
  "indexed": "Face3",
  "kind": "Face",
  "stableSubname": "Pad.Edge7",
  "subname": "Pad.Face3"
}
```

`kind=Face` 的稳定名不应指向 `Edge`。这说明问题不只是缺少 `Fillet.` 前缀，也可能存在 `NamedShape` / `ElementMap` 查询或 response stable name 回填时没有按拓扑类型隔离，导致 Face / Edge / Vertex 稳定引用串型。

## 问题边界

运行时问题仓库：

- `/Users/li/Chili3DProject/cad-web-background`

语义来源与方案记录仓库：

- `/Users/li/Chili3DProject/FreeCAD`

本轮修复应优先落在当前前端实际调用的几何服务实现中，也就是 `cad-web-background/cad-core` 及其 Rust HTTP adapter；若 `FreeCAD/cad-core` 是当前同步主线，则相同契约需要同步到该仓库。

不建议在 `/Users/li/Chili3DProject/my-chili3d` 中新增兼容分支。前端现有行为应保持：只有后端返回的 `subname/stableSubname` 明确包含可剥离的目标对象路径时，才重定向引用；否则快速失败并提示重新选择。

## 契约期望

Body target 的 response 字段应分工清楚：

- `id`：仍锚定 Body 本次结果，例如 `PadBody:Face5`。
- `indexed`：仍是 Body 本次结果枚举名，例如 `Face5`。
- `subname`：用于从 Body result 回指当前 Tip 子元素，应带当前 Tip 对象路径，例如 `Fillet.Face5`。
- `stableSubname`：用于从当前 Tip 子元素继续追溯到更稳定的来源，应带当前 Tip 路径，例如 `Fillet.Pad.Face6`。

也就是说，`id/indexed` 保持渲染拾取对齐；`subname/stableSubname` 承担后续 `PropertyLinkSub` 可引用路径。不要把这两组语义混成裸 `FaceN` 或只带旧 base feature 的 `Pad.FaceN`。

对 `Sketch -> Pad -> Body`，直接 Tip 是 `Pad`，可以返回：

```json
{
  "id": "PadBody:Face1",
  "indexed": "Face1",
  "subname": "Pad.Face1",
  "stableSubname": "Pad.Face1"
}
```

对 `Sketch -> Pad -> Fillet -> Body`，当前 Tip 是 `Fillet`，即使 stable 来源追溯到 `Pad`，也应返回：

```json
{
  "id": "PadBody:Face5",
  "indexed": "Face5",
  "subname": "Fillet.Face5",
  "stableSubname": "Fillet.Pad.Face6"
}
```

## FreeCAD 语义依据

FreeCAD 的 `PartDesign::Body` 结果不是独立拓扑空间，而是通过 Tip feature 暴露可解析的 child path。Body 的 sub object 解析语义要求后续引用能明确回到真实 Tip feature。

需要对照的 FreeCAD 源码入口：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
  - `Body::getSubObject()` / Body Tip 子对象路径解析。
  - `Body::execute()` / Body shape 来自 Tip。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp`
  - DressUp feature 的 base feature 与输出 shape 关系。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
  - `TopoShape::makeShapeWithElementMap()` / element map 传播。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
  - modified / generated / deleted 拓扑历史映射。

迁移到 `cad-core` 时，Body response 的 `subname/stableSubname` 应由 Tip child path 和 `NamedShape.elementMap` 共同决定，而不是在输出层按 `FaceN`、mesh 顺序或前端目标对象名猜测。

## 当前前端为何失败

`my-chili3d` 的重定向逻辑大致是：

1. 用户点中 `PadBody:Face5`。
2. 前端拿到 `profileRef.objectName = "PadBody"`。
3. 前端根据 feature 来源知道后续应引用 `targetObjectName = "Fillet"`。
4. 前端尝试从后端 subshape path 中剥离 `Fillet.` 前缀。
5. 当前数据只有 `Pad.Face5` / `Pad.Face6`，剥不出 `Fillet` 局部面。
6. 前端快速失败，提示所选实体面无法映射到后端特征子形状。

这个失败是合理的。前端如果在第 5 步把 `Pad.Face5` 猜成 `Fillet.Face5`，会破坏以下场景：

- Fillet / Chamfer 新生成的面和边没有对应 `Pad.FaceN`。
- Split / deleted / generated 元素需要 diagnostic，而不是猜回旧面。
- 多个 base feature 或 transform chain 下同一个 `FaceN` 可能不唯一。
- Face stable name 串到 Edge 时，前端猜测会继续放大错误。

## 推荐修复步骤

### 1. 先补最小复现 fixture

新增或复用一个请求，包含：

1. `SketchObject`
2. `PartDesign::Pad`
3. `PartDesign::Fillet`
4. `PartDesign::Body` target 指向 Fillet

验收 response：

```text
results[].object == "PadBody"
subshapes[] 中存在 Face5 或等价面
id == "PadBody:Face5"
indexed == "Face5"
subname 以 "Fillet." 开头
stableSubname 以 "Fillet." 开头，且剥掉 Fillet 后仍能得到 Pad.* 或当前 Tip 稳定路径
```

同时断言没有 `kind=Face` 但 `stableSubname` 指向 `.EdgeN` 或 `.VertexN` 的输出。

### 2. 修正 Body response 的 Tip child path

优先检查这些落点：

- `cad-core/src/part_design/body.cpp`
  - Body 执行结果里是否保留当前 Tip object name。
  - DressUp / transformed / replacement chain 是否能明确当前 Tip。
- `cad-core/src/runtime/recompute.cpp`
  - `responseSubshapes()`。
  - `stableSubnameFor()`。
  - `currentSubnameForStable()` 或同类 path 生成逻辑。
- `cad-core/src/topo/*`
  - `NamedShape` / `ElementMap` 是否能表达 `Fillet.Face5 -> Pad.Face6`。

目标不是给所有 Body subshape 无条件加 Tip 前缀，而是满足：

```text
Body result 当前 Tip 明确为 Fillet
当前输出 subshape 是 Tip shape 上的可枚举子元素
response subname 使用 Fillet.<current local subname>
response stableSubname 使用 Fillet.<stable path inside Tip>
```

如果某个元素是布尔、dress-up 或 split 后新生成的元素，且没有更稳定的 base 来源，允许 stable path 停留在 `Fillet.FaceN`；不要强行写成 `Pad.FaceN`。

### 3. stableSubname 查询必须按拓扑 kind 隔离

修复或加保护：

- `kind=Face` 只能接受 `FaceN` 或以 `.FaceN` 结尾的 stable path。
- `kind=Edge` 只能接受 `EdgeN` 或以 `.EdgeN` 结尾的 stable path。
- `kind=Vertex` 只能接受 `VertexN` 或以 `.VertexN` 结尾的 stable path。

如果 `ElementMap` 查到跨 kind 结果，应视为映射不可用或返回 diagnostic，不能直接写入 `stableSubname`。

这一步要避免简单字符串后处理掩盖问题。更好的方式是在 `ElementMap` / `NamedShape` 查询阶段按 `TopAbs_ShapeEnum` 或等价 kind 过滤候选。

### 4. 保持 mesh 拾取 token 不变

不要改这些契约：

```json
{
  "id": "PadBody:Face5",
  "indexed": "Face5"
}
```

`edgeSegments` / `vertexPoints` / face groups 依赖 `id` 和 `indexed` 对齐渲染结果。当前问题在 `subname/stableSubname` 的可引用路径，不在 mesh token 的枚举方式。

### 5. 同步 Rust adapter 契约测试

如果当前 HTTP response 由 `crates/cad-server` 转发或校验，应补充 adapter 层测试，确保：

- `results[].subshapes[].subname` 不被 adapter 改回裸名。
- `results[].subshapes[].stableSubname` 保留 `Fillet.Pad.Face6` 这类多段路径。
- `diagnostics[]` 仍用于业务失败，不把 subshape contract 错误静默吞掉。

## 非目标

- 不在前端把 `Pad.Face5` 猜成 `Fillet.Face5`。
- 不把 `PadBody` 的所有裸 `FaceN` 无条件改成当前 Tip 的 `FaceN`。
- 不改变 `id` / `indexed` / `edgeSegments` / `vertexPoints` 的拾取对齐规则。
- 不从 mesh 面片、bbox、面积、几何顺序反推 stable subname。
- 不把 Face / Edge / Vertex 的 stable name 串型结果当作可兼容输入。
- 不恢复旧 wasm / ShapeFactory / 单操作 OCC 路径。

## 验收标准

本轮短跑：

```bash
cd /Users/li/Chili3DProject/cad-web-background/cad-core
cmake --build build
python3 -m unittest tests/test_adapters.py tests/test_p6_topology.py
```

如果本轮修改落在 `/Users/li/Chili3DProject/FreeCAD/cad-core`，则对应执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_adapters.py tests/test_p6_topology.py
```

必须新增或调整 focused case，断言：

1. `Sketch -> Pad -> Fillet -> Body` 的 Body result subshape 带 `Fillet.` Tip path。
2. 剥掉 `Fillet.` 后，后续 feature 可写 `Profile.value="Fillet"`、`SubList=["Face5"]`、`StableSubList=["Pad.Face6"]`。
3. `kind=Face` 的 `stableSubname` 不再出现 `Pad.EdgeN`。
4. 既有 `Sketch -> Pad -> Body` 的 `Pad.FaceN` / `Pad.EdgeN` 输出不回退。
5. 布尔、split、deleted、ambiguous 元素没有被强行猜成旧 base feature 路径。

阶段回归：

```bash
cd /Users/li/Chili3DProject/cad-web-background/cad-core
python3 -m unittest tests/test_mvp.py tests/test_p5_sketch.py tests/test_p6_topology.py
```

文档完成后不用执行上述命令；实施代码修复时再按实际落点选择对应命令。
