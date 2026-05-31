# `/cad/recompute` 全量输入输出接口规定

本文固定 cad-web 后端 recompute 的目标接口。它对齐 `opencascade-rs/docs/说明书/5-21-22-46-草图外Feature调用说明.md` 和 `5-19-14-12-草图GraphAPI调用说明.md` 的调用方式：前端每次发送完整 `Objects[]`，后端无状态重算，响应只返回本次请求目标对象的显示和拾取结果。

本接口不是新增一套建模 DSL。持久输入仍沿用 FreeCAD / TopoNaming 风格的 `Objects[]`、`Name`、`ID`、`TypeId`、`Properties` 和 `PropertyLink*`。

## 总原则

1. `Objects[]` 是唯一持久建模源数据。
2. 前端每次调用 `/cad/recompute` 都发送完整 `Objects[]`。
3. `recompute.objs` 决定本次响应返回哪些对象结果。
4. 如果前端需要“全量派生输出”，就把所有对象名都放进 `recompute.objs`。
5. 后端不保存跨请求 session，不持有跨请求 OCCT shape，不依赖上一次 recompute 的输出。
6. `TopoDS_Shape`、mesh、subshape map、`NamedShape`、`ElementMap` 都是本次 recompute 的运行态产物，不作为下一次请求的源数据保存。
7. 响应保持 `results[]` 数组形式。不要再额外包一层 `display` / `selection` / `toponaming`，避免比现有 Graph API 契约复杂。
8. `elementReferenceUpdates` 只是本次 recompute 给前端的写回建议；只有前端应用到 `Objects[]` 后才成为持久输入。
9. 已批准的 BREP 例外只有 `ReferenceShadow.brep`：它只能挂在 `PropertyLinkSub` / `PropertyLinkSubList.SubSet[]` 的 `ReferenceShadow[]` 中，保存被引用单个 subshape 的旧几何快照，用于引用恢复；不能传完整对象 BREP，不能用于显示、拾取、布尔、拉伸或任意 shape 构造。

## 请求

```http
POST /cad/recompute
Content-Type: application/json
```

请求体顶层是完整 document object：

```json
{
  "Objects": [
    {
      "Name": "Sketch",
      "ID": 30,
      "TypeId": "Sketcher::SketchObject",
      "Properties": {
        "Geometry": [],
        "Constraints": [],
        "MakeInternals": true
      }
    },
    {
      "Name": "Pad",
      "ID": 31,
      "TypeId": "PartDesign::Pad",
      "Properties": {
        "Profile": {
          "PropertyType": "App::PropertyLinkSub",
          "value": "Sketch",
          "SubList": []
        },
        "Type": "Length",
        "Length": 10.0
      }
    },
    {
      "Name": "Body",
      "ID": 1,
      "TypeId": "PartDesign::Body",
      "Properties": {
        "Group": {
          "PropertyType": "App::PropertyLinkList",
          "values": ["Sketch", "Pad"]
        },
        "Tip": {
          "PropertyType": "App::PropertyLink",
          "value": "Pad"
        }
      }
    }
  ],
  "recompute": {
    "objs": ["Body"],
    "force": true
  }
}
```

字段规则：

| 字段 | 规则 |
| --- | --- |
| `Objects[]` | 必填，完整对象图。每个对象必须有 `Name`、`ID`、`TypeId`、`Properties`。 |
| `Objects[].Name` | graph 内唯一对象名，其他对象通过它引用。 |
| `Objects[].ID` | 稳定对象 ID。对象参数改变后仍应保持不变，用于拓扑命名 tag。 |
| `Objects[].TypeId` | FreeCAD 类型名，例如 `Sketcher::SketchObject`、`PartDesign::Pad`。 |
| `Objects[].Properties` | FreeCAD 同名属性。不要另造 `featureType`、`operation`、`params` 包装层。 |
| `recompute.objs` | 本次要返回结果的对象名列表。 |
| `recompute.force` | DTO 保留字段。后端仍按本次完整 graph 重新推导依赖和运行态结果。 |

`recompute.objs` 只控制响应里的 `results[]` 返回哪些对象。执行时，后端仍会按依赖关系在请求内补齐上游对象。

如果不传 `recompute.objs` 或传空数组，后端默认返回 `Objects[]` 最后一个对象的结果。

如果前端想拿完整派生输出，应显式传入全部对象名：

```json
{
  "recompute": {
    "objs": ["Sketch", "Pad", "Body"]
  }
}
```

Link 属性字段名遵循 `docs/接口规定/05-30-12-36-Link属性迁移到values-SubSet方案.md`：

| FreeCAD 属性类型 | 正式 JSON 字段 |
| --- | --- |
| `App::PropertyLink` | `value` |
| `App::PropertyLinkList` | `values` |
| `App::PropertyLinkSub` | `value` + `SubList`，可选 `StableSubList`、`ShadowSub`、`ReferenceShadow` |
| `App::PropertyLinkSubList` | `SubSet`，每项为 `value` + `SubList`，可选 `StableSubList`、`ShadowSub`、`ReferenceShadow` |

### `ReferenceShadow` 与 BREP snapshot

`ReferenceShadow` 是和 `SubList` 对齐的引用恢复证据。它不是 FreeCAD 原生字段，而是把 FreeCAD 的运行期旧 subshape cache 转换成无状态接口可携带的恢复数据。

允许位置：

- `App::PropertyLinkSub.ReferenceShadow[]`
- `App::PropertyLinkSubList.SubSet[].ReferenceShadow[]`
- `elementReferenceUpdates[].ReferenceShadow[]`

基本结构：

```json
{
  "PropertyType": "App::PropertyLinkSub",
  "value": "Sketch",
  "SubList": ["InternalFace2"],
  "StableSubList": ["g305:split2;..."],
  "ShadowSub": [
    {
      "newName": "g305:split2;...",
      "oldName": "InternalFace2"
    }
  ],
  "ReferenceShadow": [
    {
      "target": "Sketch",
      "targetId": 30,
      "property": "InternalShape",
      "shapeType": "Face",
      "indexed": "Face2",
      "subname": "InternalFace2",
      "stableSubname": "g305:split2;...",
      "fingerprint": {
        "area": 1200.0,
        "centroid": [30.0, 10.0, 0.0],
        "normal": [0.0, 0.0, 1.0],
        "bboxMin": [20.0, 0.0, 0.0],
        "bboxMax": [40.0, 20.0, 0.0],
        "edgeCount": 4,
        "vertexCount": 4,
        "boundaryStableSubnames": ["g301", "g302", "g305:split2", "g306:split2"],
        "boundaryVertexPoints": [
          [20.0, 0.0, 0.0],
          [40.0, 0.0, 0.0],
          [40.0, 20.0, 0.0],
          [20.0, 20.0, 0.0]
        ]
      },
      "brep": {
        "format": "brep-bin-zstd-base64",
        "byteLength": 812,
        "sha256": "base16-or-base64-digest",
        "data": "..."
      }
    }
  ]
}
```

规则：

1. `ReferenceShadow` 存在时必须是数组，长度应与同级 `SubList` 一致；长度不一致返回 `invalid_link_value`。
2. `ReferenceShadow[].target` 应与同级 `value` 指向同一对象；不一致返回 `invalid_link_value`。
3. `ReferenceShadow[].targetId` 应匹配当前 `Objects[].ID`；不匹配说明 shadow 已过期，应忽略该 shadow 并返回引用诊断，不应拿旧 BREP 去匹配新对象。
4. `ReferenceShadow[].shapeType` 只允许 `Vertex`、`Edge`、`Face`。
5. fingerprint 和 BREP 都必须使用被引用属性 shape 的本地坐标，不能混用世界坐标。
6. `ReferenceShadow[].fingerprint` 是轻量恢复证据；后端可在 stable subname 解析成功后用它检测语义漂移。
7. `ReferenceShadow[].brep` 是已批准的唯一 BREP 载荷。它只能表示被引用的单个旧 subshape，不能是完整对象 shape，也不能被 feature executor 当作 profile / boolean / display 输入。
8. `ReferenceShadow[].brep.format` 当前固定为 `brep-bin-zstd-base64`；后续如果支持其它编码，必须在 capabilities 中显式声明。
9. `ReferenceShadow[].brep.byteLength` 和 `ReferenceShadow[].brep.sha256` 用于体积限制与完整性校验；后端应先做大小检查，再解码 BREP。
10. 后端解码失败、格式不支持、体积超限、digest 不匹配或 shapeType 不匹配时，应忽略该 BREP 并降级到 fingerprint 或返回引用诊断，不应让整个 recompute 崩溃。
11. 前端可以把 `ReferenceShadow` 随 Link 属性保存在 `Objects[]` 中；它是引用恢复辅助数据，不改变“当前 shape 必须由 `DocumentObject graph` 重新计算”的原则。

## 响应

成功响应使用 `results[]` 数组。每个 target 对应一条 result，顺序按 `recompute.objs` 顺序返回。

```json
{
  "results": [
    {
      "object": "Body",
      "mesh": {
        "vertices": [],
        "normals": [],
        "indices": [],
        "faceIds": []
      },
      "subshapes": [
        {
          "id": "face_3",
          "kind": "Face",
          "indexed": "Face3",
          "subname": "Pad.Face3",
          "stableSubname": "Pad.Face6"
        }
      ]
    }
  ],
  "elementReferenceUpdates": [],
  "diagnostics": []
}
```

顶层字段：

| 字段 | 说明 |
| --- | --- |
| `results[]` | 本次目标对象结果数组。 |
| `elementReferenceUpdates[]` | 本次建议前端写回 `Objects[]` 的引用修正。不是后端保存状态。 |
| `diagnostics[]` | 全局诊断，例如重复对象名、缺失对象、循环依赖、业务错误等。 |

`results[]` 必须满足：

1. 每个 target 最多一个 result。
2. `results[]` 顺序按 `recompute.objs` 顺序返回。
3. `results[].object` 必须等于对应 target 对象名。
4. 目标对象没有可显示 shape 时，`mesh` 可以为 `null`。
5. 目标对象没有可拾取子元素时，`subshapes` 使用空数组。
6. 对象级错误通过 `diagnostics[]` 的 `object` 字段定位，不额外添加 result `status` 字段。

## result 字段

| 字段 | 说明 |
| --- | --- |
| `object` | 本条结果对应的对象名。 |
| `mesh` | 当前 target shape 的显示网格。open wire / 空 shape 可能没有 mesh。 |
| `mesh.faceIds` | 网格 face 到 `subshapes[].id` 的映射。 |
| `subshapes` | 可拾取 Face / Edge / Vertex。 |
| `subshapes[].id` | 本次响应内拾取 id，只在本次响应里有效。 |
| `subshapes[].kind` | `Face`、`Edge`、`Vertex`、`Solid`、`Shell`、`Wire` 等。 |
| `subshapes[].indexed` | 当前 OCC indexed subname，例如 `Face1`、`Edge3`。不要长期保存。 |
| `subshapes[].subname` | 当前显示 / 拾取名，例如 `InternalFace1`、`Pad.Face3`。 |
| `subshapes[].stableSubname` | 后续 feature 引用该元素时优先保存的稳定名。前端应当把它当 opaque string；为空字符串表示当前子元素没有可跨重建使用的稳定名。 |

`mesh` 只服务前端显示，不是持久模型。前端不能把它作为下一次 recompute 的输入。

`subshapes` 只服务本次显示结果上的拾取。前端选择 face / edge / vertex 后，应把当前显示 / 拾取名写入后续 `PropertyLinkSub.SubList` 或 `PropertyLinkSubList.SubSet[].SubList`；只有 `stableSubname` 非空时，才把它写入同位置的 `StableSubList`。如果当前名和稳定名暂时相同，两边可以写同一个值；如果稳定名为空，例如当前阶段的 Sketch `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`，不得自行把当前名当稳定名写入。不要自行拼接 `FaceN`、`InternalFaceN`、`;g...` 或 Body 前缀。前端不要传 `FullSubList`。

如果 result 对象是 `PartDesign::Body` 且 `Tip` 指向 solid feature，Body 导出的 `subname` / `stableSubname` 可以带 tip feature 段，例如 `Pad.Face1` 或 `Pad.<stableSubname>`，用于对齐 FreeCAD `Body::getSubObject()` 的 child-object 路径。

## `elementReferenceUpdates`

`elementReferenceUpdates` 是后端在本次 recompute 后给出的 graph 写回建议：

```json
{
  "elementReferenceUpdates": [
    {
      "object": "Pad",
      "property": "Profile",
      "PropertyType": "App::PropertyLinkSub",
      "value": "Sketch",
      "SubList": [";g101;g102;g103;g104.Face1.Face1"],
      "StableSubList": [";g101;g102;g103;g104.Face1.Face1"],
      "ShadowSub": [
        {
          "newName": ";g101;g102;g103;g104.Face1.Face1",
          "oldName": "InternalFace2"
        }
      ],
      "ReferenceShadow": [
        {
          "target": "Sketch",
          "targetId": 30,
          "property": "InternalShape",
          "shapeType": "Face",
          "indexed": "Face2",
          "subname": "InternalFace2",
          "stableSubname": ";g101;g102;g103;g104.Face1.Face1",
          "fingerprint": {},
          "brep": {
            "format": "brep-bin-zstd-base64",
            "byteLength": 812,
            "sha256": "base16-or-base64-digest",
            "data": "..."
          }
        }
      ]
    }
  ]
}
```

规则：

1. `elementReferenceUpdates` 不表示后端保存了状态。
2. 前端可以把建议应用到本地 `Objects[]`，也可以提示用户确认后再应用。
3. 只有被写回 `Objects[]` 的内容，才会成为下一次 recompute 的持久输入；其中 `ReferenceShadow.brep` 也只作为引用恢复证据，而不是建模几何源。
4. 后端不得在 recompute 中隐式修改并持久化前端 graph。

## diagnostics 与 HTTP 状态

CAD 业务问题通过 `diagnostics[]` 表达。只要 FFI 调用成功且返回合法 result JSON，HTTP 状态仍为 `200`。

| 场景 | HTTP | 响应 |
| --- | --- | --- |
| 成功，无业务问题 | `200` | `diagnostics=[]`。 |
| CAD 业务错误，例如 `UNKNOWN_TYPE_ID`、`MISSING_OBJECT`、`PROFILE_SUBNAME_NOT_FOUND` | `200` | `diagnostics[]` 非空。 |
| 请求体不是 JSON object | `400` | `bad_request` diagnostics。 |
| FFI 输入错误，例如空 buffer 或 JSON parse 失败 | `400` | `cad_core_ffi_error` diagnostics。 |
| C++ 异常、FFI decode 异常、内存错误 | `500` | `cad_core_ffi_error` diagnostics。 |

常用 diagnostics 字段：

```json
{
  "code": "PROFILE_SUBNAME_NOT_FOUND",
  "severity": "error",
  "object": "Pad",
  "property": "Profile",
  "target": "Sketch",
  "subname": "InternalFace3",
  "message": "profile subname not found"
}
```

## 与现有文档的关系

### 与草图外 Feature 调用说明的关系

本规范与 `opencascade-rs/docs/说明书/5-21-22-46-草图外Feature调用说明.md` 保持同一响应形态：

```json
{
  "results": [
    {
      "object": "Body",
      "mesh": {},
      "subshapes": []
    }
  ],
  "elementReferenceUpdates": [],
  "diagnostics": []
}
```

本文只把该形态提升为 cad-web 的接口规定，并补充：

1. `PropertyLinkList.values` / `PropertyLinkSubList.SubSet` 的输入字段规则。
2. 如果前端需要完整派生输出，应显式把全部对象名放进 `recompute.objs`。
3. `mesh`、`subshapes`、`stableSubname`、`elementReferenceUpdates` 都不是后端跨请求状态。
4. `ReferenceShadow.brep` 是已批准的旧 subshape snapshot 例外，但只作为 Link 引用恢复证据，不作为显示、拾取或建模输入。

### 与 TopoNaming 持久 graph 的关系

TopoNaming 文档定义的是持久输入 graph。本文定义的是 `/cad/recompute` 的一次性派生输出。

对应关系：

| 层次 | 格式 | 是否持久 |
| --- | --- | --- |
| 持久输入 | `Objects[]` / `Properties` / `PropertyLinkSub` / `PropertyLinkSubList` | 是 |
| 本次显示 | `results[].mesh` | 否 |
| 本次拾取 | `results[].subshapes` | 否 |
| 本次稳定引用候选 | `results[].subshapes[].stableSubname` | 否，除非前端写回 `Objects[]` |
| 写回建议 | `elementReferenceUpdates[]` | 否，除非前端应用到 `Objects[]` |
| 引用恢复证据 | `PropertyLinkSub.ReferenceShadow[]` / `SubSet[].ReferenceShadow[]` | 是，可随 Link 属性保存；其中 `brep` 只允许保存被引用旧 subshape |

### 与旧 cad-web envelope 的关系

旧 cad-web 方案曾使用：

```json
{
  "objects": {},
  "mesh": {},
  "subshapes": {},
  "named_shapes": {},
  "diagnostics": []
}
```

该结构可以作为迁移前背景或兼容层，但不作为新目标契约。新契约以 `results[]` 为准。

映射关系如下：

| 旧字段 | 新字段 |
| --- | --- |
| `mesh[name]` | `results[].mesh` |
| `subshapes[name]` | `results[].subshapes` |
| `named_shapes[name].element_map` | 优先通过 `results[].subshapes[].stableSubname` 和 `elementReferenceUpdates[]` 消费；不作为独立持久模型返回 |
| `diagnostics` | 顶层 `diagnostics[]` |

## 前端使用流程

1. 前端本地维护完整 `Objects[]`。
2. 新建对象、修改参数或删除对象时，只修改本地 `Objects[]`。
3. 每次调用 `/cad/recompute` 都发送完整 `Objects[]`。
4. 前端通过 `recompute.objs` 指定这次要返回哪些对象结果。
5. 前端收到响应后，用 `results[]` 更新对应 target 的显示和拾取缓存。
6. 三维显示消费 `results[].mesh`。
7. 拾取消费 `results[].subshapes`。
8. 保存后续 feature 引用时，优先写入选中项的 `stableSubname`。
9. 收到 `elementReferenceUpdates` 后，前端决定是否应用到本地 `Objects[]`。
10. 下一次 recompute 仍只以新的完整 `Objects[]` 为输入，不回传上一次的 `mesh`、`subshapes` 或 `elementReferenceUpdates`；如果前端应用了 `ReferenceShadow` 写回建议，它作为 Link 属性的一部分随 `Objects[]` 发送。

## 后端实现边界

Web 层职责：

1. 校验请求体是 JSON object。
2. 记录 request id、对象数、target 数、diagnostics 数和耗时。
3. 调用 CAD Core recompute。
4. 将 CAD Core 结果转换为本文定义的响应结构。
5. 不复制 CAD Core 的 feature 级业务校验。

CAD Core 职责：

1. 解析完整 `Objects[]`。
2. 分析 `PropertyLink*` 依赖。
3. 在单次请求内构建 shape、mesh、subshape map、NamedShape / ElementMap 等运行态产物。
4. 导出 `mesh`、`subshapes`、`stableSubname`、`elementReferenceUpdates` 和 diagnostics。
5. 请求结束后释放所有运行态产物。

禁止事项：

1. 不在 Web 层保存 mesh、subshape map、NamedShape 或 ElementMap；Web 层只能按接口透传 `ReferenceShadow.brep`，不能生成、缓存或解释 BREP。
2. 不在 recompute 响应中返回可被前端当作持久模型的 `NamedShape` 对象。
3. 不通过“根据上一次响应查表”的方式恢复引用。
4. 不在后端隐式改写 `Objects[]`。
5. 不把 CAD 业务 diagnostics 映射成 HTTP 500。
6. 不在 `ReferenceShadow.brep` 之外新增任何 BREP 请求或响应字段。
