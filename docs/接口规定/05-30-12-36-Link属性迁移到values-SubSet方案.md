# Link 属性迁移到 values / SubSet 方案

本文给出把 cad-web / cad-core 当前 `value` 数组写法迁移到 TopoNaming 文档中 `values` / `SubSet` 写法的方案。

目标不是改变建模语义。输入仍然是 FreeCAD 风格 `DocumentObject graph`，对象仍然是 `Name`、`ID`、`TypeId`、`Properties`。本方案只调整 `PropertyLinkList` 和 `PropertyLinkSubList` 这两类 Link 属性的 JSON 字段名和结构。

## 总结论

建议迁移。

原因：

- `values` 更准确表达 `PropertyLinkList` 是多个对象引用。
- `SubSet` 更准确表达 `PropertyLinkSubList` 是多组 `(对象, SubList)`。
- 这两个名字更接近 FreeCAD `PropertyLinks` 的语义，不会把“单对象 value”和“多对象数组 value”混在一起。
- `SubSet` 让 stable subname 字段可以清晰挂在每一组对象引用下面，而不是塞进一个过度泛化的 `value` 数组。

最终契约：

| FreeCAD 属性类型 | 目标 JSON 字段 |
| --- | --- |
| `App::PropertyLink` | `value` |
| `App::PropertyLinkList` | `values` |
| `App::PropertyLinkSub` | `value` + `SubList` |
| `App::PropertyLinkSubList` | `SubSet` |

## 当前差异

当前 cad-core parser 和 fixtures 主要使用统一 `value` 字段：

```json
{
  "PropertyType": "App::PropertyLinkList",
  "value": ["Sketch", "Pad"]
}
```

```json
{
  "PropertyType": "App::PropertyLinkSubList",
  "value": [
    {
      "PropertyType": "App::PropertyLinkSub",
      "value": "BasePad",
      "SubList": ["Face1"]
    }
  ]
}
```

迁移后改为：

```json
{
  "PropertyType": "App::PropertyLinkList",
  "values": ["Sketch", "Pad"]
}
```

```json
{
  "PropertyType": "App::PropertyLinkSubList",
  "SubSet": [
    {
      "value": "BasePad",
      "SubList": ["Face1"]
    }
  ]
}
```

大白话说，迁移后字段名按含义分开：

- `value`：一个对象。
- `values`：一批对象。
- `SubList`：某个对象里面的 face / edge / vertex。
- `SubSet`：一批“对象 + 这个对象里的 SubList”。

## 目标输入格式

### App::PropertyLink

单对象引用，继续使用 `value`。

```json
{
  "PropertyType": "App::PropertyLink",
  "value": "Pad"
}
```

无引用时：

```json
{
  "PropertyType": "App::PropertyLink",
  "value": null
}
```

### App::PropertyLinkList

多对象引用，使用 `values`。

```json
{
  "PropertyType": "App::PropertyLinkList",
  "values": ["Sketch", "Pad"]
}
```

规则：

- `values` 必须是字符串数组。
- 空数组表示不引用任何对象。
- 不在 `PropertyLinkList` 上使用 `SubList`、`StableSubList`、`FullSubList`。

### App::PropertyLinkSub

单对象 + 子元素引用，继续使用 `value` + `SubList`。

```json
{
  "PropertyType": "App::PropertyLinkSub",
  "value": "Pad",
  "SubList": ["Face3"],
  "StableSubList": ["Pad.Face6"],
  "FullSubList": ["Body.Face3"],
  "ShadowSub": [
    {
      "newName": "Pad.Face6",
      "oldName": "Face3"
    }
  ],
  "ReferenceShadow": [
    {
      "target": "Pad",
      "targetId": 31,
      "property": "Shape",
      "shapeType": "Face",
      "indexed": "Face3",
      "subname": "Face3",
      "stableSubname": "Pad.Face6",
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
```

规则：

- `value` 是被引用对象名，允许为 `null`。
- `SubList` 是当前 indexed subname，例如 `Face3`、`Edge1`。
- `StableSubList` 是稳定 subname，用于后续 recompute 恢复当前 indexed subname。
- `FullSubList` 是带对象上下文的显示/回写路径。
- `ShadowSub` 是 FreeCAD 风格的新旧元素名对，存在时应和 `SubList` 一一对应。
- `ReferenceShadow` 是引用恢复证据，存在时应和 `SubList` 一一对应；其中 `brep` 只允许保存被引用单个旧 subshape 的 BREP snapshot。
- `StableSubList`、`FullSubList`、`ShadowSub`、`ReferenceShadow` 存在时，应和 `SubList` 一一对应；长度不一致时返回 `invalid_link_value`。

### App::PropertyLinkSubList

多组“对象 + 子元素引用”，使用 `SubSet`。

```json
{
  "PropertyType": "App::PropertyLinkSubList",
  "SubSet": [
    {
      "value": "Pad",
      "SubList": ["Face3"],
      "StableSubList": ["Pad.Face6"],
      "FullSubList": ["Body.Face3"],
      "ShadowSub": [{ "newName": "Pad.Face6", "oldName": "Face3" }],
      "ReferenceShadow": [{ "...": "..." }]
    },
    {
      "value": "Sketch",
      "SubList": []
    }
  ]
}
```

规则：

- `SubSet` 必须是数组。
- 每个 `SubSet[]` item 表示一组 `(DocumentObject, SubList)`。
- `SubSet[].value` 是被引用对象名。
- `SubSet[].SubList` 是该对象内部的 sub element 名列表。
- `SubSet[].StableSubList`、`SubSet[].FullSubList`、`SubSet[].ShadowSub` 和 `SubSet[].ReferenceShadow` 只描述同一个 item 下的 sub element，不跨 item。
- `SubSet[]` item 不需要再写 `PropertyType`，因为外层已经声明这是 `App::PropertyLinkSubList`。

## 输出格式影响

本次迁移只改变输入 graph 中 Link 属性的字段名和结构，不改变 recompute 输出里“mesh、subshapes、stableSubname、diagnostics 都是本次派生结果”的语义。

`/cad/recompute` 的目标响应结构以 `docs/接口规定/01-cad-recompute全量输入输出接口.md` 为准，使用 `results[]` 下的扁平 `mesh` / `subshapes` 结构，并通过 `subshapes[].stableSubname` 和 `elementReferenceUpdates[]` 支持稳定引用写回。旧的 `objects` / `mesh` / `subshapes` / `named_shapes` 顶层 envelope 只作为迁移前背景或兼容层，不再作为新目标契约。

本次迁移影响的是：

- 前端保存和发送的 `Objects[].Properties`。
- cad-core fixtures。
- capabilities 里声明的 link property fields。
- 文档中的请求示例。

不影响：

- `results[].mesh` / `results[].subshapes` 的派生输出语义。
- `subshapes[].stableSubname` 和 `elementReferenceUpdates[]` 的稳定引用写回语义。
- diagnostics 的业务语义。
- recompute 的无状态模型。

## 迁移策略

采用一步到位策略，不做向后兼容。

从迁移提交开始，正式请求、fixtures、文档示例、capabilities 和测试全部切到新格式：

- `PropertyLinkList.values` 是唯一正式写法。
- `PropertyLinkSubList.SubSet` 是唯一正式写法。
- `PropertyLinkList.value` 直接判错。
- `PropertyLinkSubList.value` 直接判错。

如果同一个属性同时出现新旧字段，也直接判错，不做转换，不猜优先级。

错误边界：

- Web 层能静态识别旧字段时，直接返回 `400 bad_request`。
- 进入 cad-core 后识别到旧字段或字段类型错误，返回 `invalid_link_value` diagnostics。

这样做的代价是迁移提交必须同时改 parser、fixtures、文档和前端请求生成逻辑。好处是契约只有一套，不存在“客户端到底该发新格式还是旧格式”的灰区。

## 代码落点

### cad-core parser

主要落点：

```text
cad-core/src/document/model.cpp
cad-core/include/cad_core/document/model.h
```

当前 parser 已经把 Link 属性归一化到 `document::Link`：

```cpp
struct Link {
    std::string object;
    std::vector<std::string> subnames;
    std::vector<std::string> stableSubnames;
    std::vector<std::string> fullSubnames;
    std::vector<ShadowSub> shadowSubs;
    std::vector<ReferenceShadow> referenceShadows;
    std::string property;
};
```

`ShadowSub` 和 `ReferenceShadow` 是本次接口扩展后的新增归一化字段。旧的
`value` / `values` / `SubSet` 字段迁移仍只改 JSON 读取入口；接入引用恢复时，再把
`ShadowSub` / `ReferenceShadow` 写入同一个 `document::Link`。

建议拆出三个读取函数：

```text
readLinkObject()       // 读 PropertyLink / PropertyLinkSub
readLinkList()         // 读 PropertyLinkList.values
readLinkSubList()      // 读 PropertyLinkSubList.SubSet
```

`readLinks()` 的目标逻辑：

```text
PropertyLink        -> readLinkObject(value)
PropertyLinkSub     -> readLinkObject(value)
PropertyLinkList    -> readLinkList(values)
PropertyLinkSubList -> readLinkSubList(SubSet)
```

`isMalformedLinkValue()` 的目标校验：

```text
PropertyLink:
  value must be string or null

PropertyLinkSub:
  value must be string or null
  SubList / StableSubList / FullSubList must be string arrays
  ShadowSub must be array when present
  ReferenceShadow must be array when present
  StableSubList / FullSubList / ShadowSub / ReferenceShadow length must match SubList when present

PropertyLinkList:
  values must exist and be string array
  value must not exist

PropertyLinkSubList:
  SubSet must exist and be array
  each item.value must be string or null
  each item.SubList / StableSubList / FullSubList must be string arrays
  each item.ShadowSub must be array when present
  each item.ReferenceShadow must be array when present
  each item.StableSubList / FullSubList / ShadowSub / ReferenceShadow length must match item.SubList when present
  value must not exist
```

### capabilities

落点：

```text
cad-core/src/adapters/c_api/c_api.cpp
```

当前 capabilities 的 `link_property_fields` 应从：

```json
["value", "SubList", "StableSubList", "FullSubList"]
```

改成：

```json
["value", "values", "SubList", "SubSet", "StableSubList", "FullSubList", "ShadowSub", "ReferenceShadow"]
```

更清楚的表达是直接按属性类型返回：

```json
{
  "link_property_shapes": {
    "App::PropertyLink": ["value"],
    "App::PropertyLinkList": ["values"],
    "App::PropertyLinkSub": ["value", "SubList", "StableSubList", "FullSubList", "ShadowSub", "ReferenceShadow"],
    "App::PropertyLinkSubList": ["SubSet"]
  }
}
```

可以保留 `link_property_fields` 作为字段全集展示，但它不能表达旧格式兼容。前端应以 `link_property_shapes` 判断每种 Link 属性的唯一合法结构。

### cad-server HTTP 校验

落点：

```text
crates/cad-server/src/controller/cad.rs
```

当前 Web 层主要做 JSON object 检查，再把 payload 透传给 `CadCore::recompute()`。迁移后仍建议保持 Web 层薄 adapter，不复制完整 FreeCAD 业务校验。

Web 层只做轻校验：

- `PropertyLinkList` 出现 `value` 时，返回 `400 bad_request`。
- `PropertyLinkSubList` 出现 `value` 时，返回 `400 bad_request`。
- 同一属性同时出现 `value` 和 `values`，或同时出现 `value` 和 `SubSet`，返回 `400 bad_request`。

复杂字段类型错误仍可由 cad-core 返回 `invalid_link_value` diagnostics。

## 文档迁移

需要更新：

```text
docs/05-29-00-53-cad-recompute接口功能说明.md
docs/05-30-03-07-cad-web后端接口整合方案.md
docs/CADCore方案/**/*.md
```

核心改动：

- 把 `PropertyLinkList.value` 示例改成 `PropertyLinkList.values`。
- 把 `PropertyLinkSubList.value` 示例改成 `PropertyLinkSubList.SubSet`。
- 删除“Web 入参拒绝 `values` / `SubSet`”的旧结论。
- 改成“只接受 `values` / `SubSet`，旧 `value` 数组直接拒绝”。
- 明确 `PropertyType` 只是 JSON 类型辅助字段，不是 FreeCAD property 的真实字段名。

## Fixtures 迁移

当前 fixtures 数量较多，不建议手工逐个改。

建议写一个一次性脚本，做结构化 JSON 转换：

```text
PropertyType == App::PropertyLinkList:
  if value is array of strings:
    values = value
    delete value

PropertyType == App::PropertyLinkSubList:
  if value is array:
    SubSet = value.map(item => {
      if item is string:
        return { value: item, SubList: [] }
      else:
        copy value/SubList/StableSubList/FullSubList
        drop nested PropertyType
    })
    delete value
```

脚本必须用 JSON parser，不要用字符串替换。

迁移后用 `rg` 做静态检查：

```bash
rg -n '"PropertyType": "App::PropertyLinkList"|"PropertyType": "App::PropertyLinkSubList"|"values"|"SubSet"|"value"' cad-core/fixtures docs
```

还要增加一个专门检查：

```bash
rg -n '"PropertyType": "App::PropertyLinkList"[\s\S]*"value"\s*:' cad-core/fixtures
rg -n '"PropertyType": "App::PropertyLinkSubList"[\s\S]*"value"\s*:' cad-core/fixtures
```

如果 `rg` 不适合跨行匹配，改用 JSON 脚本扫描 AST。

## 测试矩阵

至少补这些测试：

| 层级 | 用例 |
| --- | --- |
| cad-core parse | `PropertyLinkList.values` 成功解析成 dependency links。 |
| cad-core parse | `PropertyLinkSubList.SubSet` 成功解析成 dependency links，保留 `SubList` / `StableSubList` / `FullSubList`。 |
| cad-core parse | `PropertyLinkList.values` 非数组返回 `invalid_link_value`。 |
| cad-core parse | `PropertyLinkSubList.SubSet` 非数组返回 `invalid_link_value`。 |
| cad-core parse | 同一属性同时出现 `value` 和 `values` / `SubSet`，返回错误。 |
| cad-core parse | `PropertyLinkList.value` 直接返回 `invalid_link_value`。 |
| cad-core parse | `PropertyLinkSubList.value` 直接返回 `invalid_link_value`。 |
| HTTP recompute | 使用新格式的 `rect-pad` fixture 正常返回。 |
| HTTP recompute | 使用新格式的 `Body.Group.values` 正常返回 Body 结果。 |
| HTTP recompute | 旧 `PropertyLinkList.value` / `PropertyLinkSubList.value` 被拒绝。 |
| capabilities | 返回 `link_property_shapes`，且 `PropertyLinkList` 声明 `values`，`PropertyLinkSubList` 声明 `SubSet`。 |

## 风险和处理

| 风险 | 处理 |
| --- | --- |
| 旧 fixtures 全部失效 | 同一个迁移提交内批量改 fixtures，并用 targeted 测试兜住。 |
| 前端和后端切换时间不一致 | 不做混用；后端、前端请求生成、fixtures 和文档在同一版本边界切换。 |
| `SubSet` item 中 stable 字段和 `SubList` 对不齐 | cad-core 返回 `invalid_link_value`，不要在 Web 层猜。 |
| 嵌套 item 是否保留 `PropertyType` 产生分歧 | 正式格式不保留；出现嵌套 `PropertyType` 时可返回 `invalid_link_value`，避免两套结构并存。 |
| Web 层校验复制 cad-core 规则 | Web 只做字段冲突和旧格式禁用，业务语义继续放在 cad-core。 |

## 推荐实施顺序

1. 修改 cad-core parser，只读取 `PropertyLinkList.values` 和 `PropertyLinkSubList.SubSet`，旧 `value` 数组直接报错。
2. 增加 parser 单测，证明新格式能读、旧格式和冲突格式都会报错。
3. 更新 capabilities，新增 `link_property_shapes`，声明唯一合法结构。
4. 批量迁移 fixtures 到 `values` / `SubSet`。
5. 更新 `docs/05-29-00-53-cad-recompute接口功能说明.md`、`docs/05-30-03-07-cad-web后端接口整合方案.md` 和 `docs/CADCore方案` 下的示例。
6. 更新前端保存/发送 document JSON 的 Link 属性生成逻辑。
7. 增加 HTTP recompute 新格式测试和旧格式拒绝测试。

## 验收标准

迁移完成后应满足：

- 新请求中 `PropertyLinkList` 只使用 `values`。
- 新请求中 `PropertyLinkSubList` 只使用 `SubSet`。
- `PropertyLink` 和 `PropertyLinkSub` 仍使用 `value`。
- `StableSubList` / `FullSubList` 保留在 `PropertyLinkSub` 或 `SubSet[]` item 内。
- `/cad/recompute` 响应结构按 `01-cad-recompute全量输入输出接口.md` 输出；本迁移不再引入额外响应字段。
- `cad-core/fixtures` 中不再有 list/sublist 的旧 `value` 数组写法。
- capabilities 能告诉前端每种 Link 属性应该使用什么字段。
- 旧格式错误请求能被明确拒绝，不会被静默转换。
