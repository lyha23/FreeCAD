# RevolutionBody 裸 Face 无法映射修复方案

## 当前结论

这是后端 `cad-core` 的 Body Tip 子形状导出契约问题，不应在前端把裸 `FaceN` 猜成 `Revolution.FaceN`。

前端日志中的关键数据是：

```json
{
  "profileRef": {
    "objectName": "RevolutionBody",
    "indexed": "Face8",
    "subname": "Face8",
    "stableSubname": "Face8"
  },
  "sourceChain": {
    "sourceObjectName": "Revolution",
    "baseKind": "pad",
    "bodyName": "RevolutionBody",
    "dressUps": [
      {
        "kind": "face-revolve",
        "objectName": "Revolution",
        "baseObjectName": "Pad"
      }
    ]
  },
  "targetObjectName": "Revolution"
}
```

用户点中的是 `RevolutionBody:Face8`。这个面在 Body 显示结果里存在，但后端只返回了裸的：

```json
{
  "id": "RevolutionBody:Face8",
  "indexed": "Face8",
  "kind": "Face",
  "subname": "Face8",
  "stableSubname": "Face8"
}
```

这只能说明它是当前 Body result 的 `Face8`，不能证明它是当前 Tip 特征 `Revolution` 的 `Face8`。前端准备构造后续 Extrude / Revolve / 其它后续特征时，需要把 Body 显示面重定向到真实后端特征对象：

```json
{
  "objectName": "Revolution",
  "indexed": "Face8",
  "subname": "Face8",
  "stableSubname": "Face8"
}
```

前端只能从后端返回的 `Revolution.Face8` 这种 Tip child path 中剥掉 `Revolution.` 得到上述引用。当前数据没有这个路径，所以前端快速失败是合理行为。

## 问题边界

实施仓库：

- `/Users/li/Chili3DProject/FreeCAD/cad-core`

文档落点：

- `/Users/li/Chili3DProject/FreeCAD/docs/BUG修改/`

如果当前前端运行时实际调用的是 `/Users/li/Chili3DProject/cad-web-background`，则同一契约需要同步到该仓库的 `cad-core` 与 Rust HTTP adapter。

本问题不在 `/Users/li/Chili3DProject/my-chili3d` 里新增兼容分支；前端现有行为应保持：没有后端 Tip child path 时，视为不可 retarget，而不是猜测。

## 契约期望

对 `Sketch -> Pad -> face-revolve Revolution -> Body` 这条链，`RevolutionBody` 的 response subshape 应同时表达两层身份：

- `id`：仍锚定 Body 本次显示结果，例如 `RevolutionBody:Face8`。
- `indexed`：仍是 Body 本次 OCC 枚举名，例如 `Face8`。
- `subname`：必须是 Body 可解析的当前 Tip child path，例如 `Revolution.Face8`。
- `stableSubname`：必须是同一个 Tip 路径下的稳定引用，例如 `Revolution.Face8` 或 `Revolution.Pad.Face5`。

也就是说，`id/indexed` 负责渲染拾取对齐；`subname/stableSubname` 负责后续 `PropertyLinkSub` 可引用路径。不要把这两组语义混成裸 `FaceN`。

对由 `Revolution` 新生成的面，期望至少返回：

```json
{
  "id": "RevolutionBody:Face8",
  "indexed": "Face8",
  "kind": "Face",
  "subname": "Revolution.Face8",
  "stableSubname": "Revolution.Face8"
}
```

对从 `Pad` 继承并经过 `Revolution` Tip 暴露的面，期望返回当前 Tip 路径加稳定来源，例如：

```json
{
  "id": "RevolutionBody:Face2",
  "indexed": "Face2",
  "kind": "Face",
  "subname": "Revolution.Face2",
  "stableSubname": "Revolution.Pad.Face5"
}
```

前端随后执行：

```text
cadObjectLocalSubname("Revolution", "Revolution.Face8") -> "Face8"
cadObjectLocalSubname("Revolution", "Revolution.Pad.Face5") -> "Pad.Face5"
```

这样后续 feature 才能安全写入：

```json
{
  "value": "Revolution",
  "SubList": ["Face8"],
  "StableSubList": ["Face8"]
}
```

或：

```json
{
  "value": "Revolution",
  "SubList": ["Face2"],
  "StableSubList": ["Pad.Face5"]
}
```

## 当前前端为何失败

`my-chili3d` 的 face profile 重定向链路大致是：

1. 用户点中 `RevolutionBody:Face8`。
2. 前端从 runtime node 得到 `profileRef.objectName = "RevolutionBody"`。
3. 前端根据 feature 来源解析出当前可重建 Tip 为 `Revolution`。
4. 前端尝试从 `subname/stableSubname` 中剥离 `Revolution.`。
5. 当前数据只有裸 `Face8`，剥不出 `Revolution` 局部子形状。
6. 前端报错：所选实体面无法映射到后端特征子形状。

这一步不能改成猜测。原因：

- `Face8` 是 Body 本次结果枚举名，不是跨重建稳定身份。
- 同一个 Body 链路里可能同时存在 `Pad.FaceN`、`Revolution.FaceN`、新生成面和 split/deleted 面。
- 如果前端把裸 `Face8` 猜成 `Revolution.Face8`，会把后端 TopoNaming 缺失隐藏成错误的后续特征引用。
- 后续 `elementReferenceUpdates` 和 diagnostic 将无法判断是稳定引用恢复、分裂、删除还是误绑。

## 推荐修复步骤

### 1. 先补最小复现 fixture

新增或复用一个请求，包含：

1. `SketchObject`
2. `PartDesign::Pad`
3. 基于 Pad 面的 `PartDesign::Revolution`
4. `PartDesign::Body` target 指向 `Revolution`

验收 response：

```text
results[].object == "RevolutionBody"
subshapes[] 中存在当前可点选的 Face8 或等价面
id == "RevolutionBody:Face8"
indexed == "Face8"
subname 以 "Revolution." 开头
stableSubname 以 "Revolution." 开头
```

如果该面是 `Revolution` 新生成面，允许：

```text
stableSubname == "Revolution.Face8"
```

如果该面来自 `Pad` 稳定历史，应返回：

```text
stableSubname == "Revolution.Pad.FaceN"
```

同时断言没有 `kind=Face` 但 `stableSubname` 指向 `.EdgeN` 或 `.VertexN` 的输出。

### 2. 修正 Body response 的 Tip child path

优先检查这些落点：

- `cad-core/src/part_design/body.cpp`
  - Body 执行结果里是否保留当前 Tip object name。
  - `executeBody()` 输出的 `context.objects[bodyName]["tip"]` 是否足够判断当前 Tip。
  - `getBodyTopoShapeAtFeature()` 是否在 replay 到 `Revolution` 后仍保留可用于 response 命名的 `NamedShape`。
- `cad-core/src/runtime/recompute.cpp`
  - `responseSubshapes()`。
  - `stableSubnameFor()`。
  - `currentSubnameForStable()` 或同类 path 生成逻辑。
- `cad-core/src/part/*` 与 `cad-core/src/topo/*`
  - `NamedShape` / `ElementMap` 是否能表达 `Revolution.Face8` 与 `Revolution.Pad.Face5`。

目标规则：

```text
当前 response object 是 PartDesign::Body
当前 Body Tip 是 Revolution
当前输出 subshape 是 Tip shape 上的可枚举元素
response subname 使用 Revolution.<current local subname>
response stableSubname 使用 Revolution.<stable path inside Tip>
```

如果某个元素是 `Revolution` 新生成的面，且没有更深的来源，`stableSubname` 可以是 `Revolution.FaceN`。不要为了追溯而强行写成 `Pad.FaceN`。

### 3. 已有稳定来源优先，但必须挂在当前 Tip 路径下

不要覆盖已有明确来源，但 Body result 上对后续特征可引用的路径仍应以当前 Tip 开头。

建议规则：

```text
如果 stableSubnameFor(indexed, namedShape) 返回裸 FaceN:
  在 Body Tip 唯一且可确认时输出 Revolution.FaceN

如果 stableSubnameFor(indexed, namedShape) 返回 Pad.Face5:
  当前 Body Tip 是 Revolution 时输出 Revolution.Pad.Face5

如果 stableSubnameFor(indexed, namedShape) 已经返回 Revolution.Pad.Face5:
  原样保留

如果 split/deleted/ambiguous 无法唯一恢复:
  返回 diagnostic 或保持不可引用，不要猜测
```

这能让前端基于 `targetObjectName = "Revolution"` 做一致 retarget，而不是在同一个 `RevolutionBody` 结果里有的面归 `Pad`、有的面归 `Revolution`、有的面是裸 `FaceN`。

### 4. stableSubname 查询必须按拓扑 kind 隔离

修复或增加保护：

- `kind=Face` 只能接受 `FaceN` 或以 `.FaceN` 结尾的 stable path。
- `kind=Edge` 只能接受 `EdgeN` 或以 `.EdgeN` 结尾的 stable path。
- `kind=Vertex` 只能接受 `VertexN` 或以 `.VertexN` 结尾的 stable path。

如果 `ElementMap` 查到跨 kind 结果，应视为映射不可用或返回 diagnostic，不能直接写入 `stableSubname`。

这一步不能靠前端字符串过滤解决。应在 `ElementMap` / `NamedShape` 查询阶段按 `TopAbs_ShapeEnum` 或等价 kind 过滤候选。

### 5. 保持 mesh 拾取 token 不变

不要改这些字段的语义：

```json
{
  "id": "RevolutionBody:Face8",
  "indexed": "Face8"
}
```

`mesh.faceIds`、`edgeSegments`、`vertexPoints` 与 `subshapes[].id` 的对齐依赖 Body result 当前枚举。当前问题在 `subname/stableSubname` 的可引用路径，不在 mesh token。

### 6. 同步 Rust adapter 契约测试

如果 Rust adapter 只是 JSON-through，应保持它不裁剪这些字段：

- `results[].subshapes[].subname`
- `results[].subshapes[].stableSubname`
- `elementReferenceUpdates`
- `diagnostics`

候选测试文件：

- `cad-core/tests/test_adapters.py`
  - 适合补 response-level contract。
- `cad-core/tests/test_p6_topology.py`
  - 适合防止 stable history 回归。
- `crates/cad-server/src/controller/cad.rs`
  - 适合补 HTTP response 保留字段断言。

## 非目标

- 不在前端把裸 `Face8` 猜成 `Revolution.Face8`。
- 不改变 `mesh.faceIds` / `edgeSegments` / `vertexPoints` 的 id 对齐规则。
- 不从 mesh 三角面、线段顺序、点坐标反推 topology identity。
- 不把所有 Body 裸 `FaceN` 无条件改成当前 Tip 前缀；必须有明确 Body Tip 和 shape 来源。
- 不为这一个报错新增旧 wasm / ShapeFactory / HTTP 单操作兼容层。
- 不把 split / deleted / ambiguous 元素包装成稳定来源。

## 验收标准

本轮修复完成后应满足：

1. `RevolutionBody` 上当前 Tip 为 `Revolution` 的可引用面不再输出裸 `FaceN`。
2. `RevolutionBody:Face8` 这类新生成面至少输出 `subname = "Revolution.Face8"`、`stableSubname = "Revolution.Face8"`。
3. 继承自 `Pad` 的面在 Body result 上输出可从 `Revolution` 剥离的路径，例如 `Revolution.Pad.Face5`。
4. `id/indexed` 不变，前端拾取仍能通过 `RevolutionBody:Face8` 对齐 mesh。
5. 前端不新增猜测分支；原有 retarget 逻辑在后端返回正确路径后自然通过。
6. 对 split / deleted / ambiguous 的元素，后端返回 diagnostic 或不可引用状态，而不是输出伪稳定路径。
