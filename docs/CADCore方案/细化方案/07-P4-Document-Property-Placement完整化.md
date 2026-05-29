# P4：Document / Property / Placement 完整化

P4 的目标是把 executor 对临时 JSON 形态的依赖收敛到统一 Document / Property / Link / Placement 模型。完成后 graph、runtime、features 都从同一套属性语义读数据，而不是各 executor 自己猜输入对象和 subname。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| Document / DocumentObject | `src/App/Document.cpp`、`src/App/DocumentObject.cpp` |
| 属性系统 | `src/App/Property*.cpp`、`src/App/PropertyContainer*.cpp` |
| Link 属性 | `src/App/PropertyLinks.cpp`、`src/App/PropertyLinks.h` |
| Placement | `src/App/GeoFeature.cpp`、`src/App/GeoFeatureGroupExtension.cpp` |
| Body / Part 坐标 | `src/Mod/PartDesign/App/Body.cpp`、`src/Mod/Part/App/BodyBase.cpp` |

## 当前问题

- FeatureExtrude 常用 scalar / enum / bool / vector 属性和 Sketch / Body / FeatureBase / Part / Datum 主路径的 Placement 已接入 document typed getter；Sketch `Geometry` / `Constraints` 这类草图专有输入仍保留 raw JSON 读取路径，等待 P5 专门收敛。
- Link / LinkSub 已有 document 层规范化结构，但 full subname / stable subname 还只是 schema 落点，尚未接入 P6 的 `NamedShape` / `ElementMap` 主路径。
- Placement 已覆盖 Sketch / Body / FeatureBase 基础路径、`App::Part -> Body` 的 parent GeoFeatureGroup 传递、Sketch attached to DatumPlane 的基础坐标引用，以及 DatumLine / DatumPoint 的基础 shape placement；完整 Origin / AttachEngine 和复杂 Datum attachment 还不完整。
- diagnostics 已有 `invalid_link_value`、`invalid_property_type`、`invalid_placement`，parse / graph / runtime 关键路径已输出 `stage`，链接和子形状错误已输出 `target` / `subname`；专门的 `unresolved_subname` code 仍待 P6 topo naming 主路径再拆。

## 目标模型

```text
Document
  objects: Name -> DocumentObject

DocumentObject
  name
  id
  typeId
  properties: name -> PropertyValue

PropertyValue
  scalar / enum / bool / length / angle / vector / placement
  link / link_list / link_sub / link_sub_list

LinkSub
  target object name
  subnames[]
  stable subnames[]
  full subnames[]
  resolved runtime target
```

## 当前落地状态

- `cad-core/include/cad_core/document/model.h` 已固定 `PropertyKind`、`PropertyValue`、`Link`，`DocumentObject` 同时保留 raw `properties` 和规范化 `propertyValues` / `dependencyLinks`。
- `cad-core/src/document/model.cpp` 在 parse 阶段识别 Bool、Integer、Float/Length/Angle、String、Enumeration、Vector、Placement、Link、LinkList、LinkSub、LinkSubList；`PropertyType` 只用于选择规范化类型，不作为业务属性名。
- document 层已提供 `readBool`、`readNumber`、`readString`、`readVector3`、`readPlacement`，兼容 raw JSON 和 typed wrapper；FeatureExtrude 的 `Type`、`SideType`、`Length*`、`Reversed`、`UseCustomVector`、`Direction`、`AlongSketchNormal` 已切到这些 getter，runtime global placement、Sketch placement、FeatureBase placement 已切到 `readPlacement`。
- `PropertyLink` / `PropertyLinkList` / `PropertyLinkSub` / `PropertyLinkSubList` 统一归一为 `Link{object, subnames, stableSubnames, fullSubnames, property}`，旧 fixture 中 `Group` raw array 仍通过 document 层兼容。
- `cad-core/src/graph/recompute_plan.cpp` 只读取 `DocumentObject::dependencyLinks`，不再递归解析 feature raw JSON。
- Body、FeatureBase、FeatureExtrude 的 Profile、BaseFeature、Tip、Group、UpToFace、UpToShape、ReferenceAxis 已切到 `document::readLink/readLinks(object, property)`。
- `Document::parentGroupByObject` 已记录 `App::Part` / `PartDesign::Body` 的 GeoFeatureGroup membership；runtime 预先计算 `globalPlacements = parent group placement * object placement`。
- Body 输出已使用 runtime global placement；`App::Part` 作为薄 GeoFeatureGroup container executor 接入 registry，并在单一子 solid 场景下把 child solid 暴露为前端显示输出。
- `PartDesign::Plane` / `PartDesign::Line` / `PartDesign::Point` 已作为基础 Datum executor 接入 registry；Sketch 的 `AttachmentSupport` / `Support` 可消费 DatumPlane global placement，用于 datum plane 上的 profile 构造；FeatureExtrude `ReferenceAxis` 可直接使用 DatumLine 方向，DatumPoint 可消费 parent Part placement。
- diagnostics JSON 已兼容输出 `stage`、`target`、`subname`；当前覆盖 parse 阶段 invalid placement、graph 阶段 missing target、runtime 阶段 LinkSub 缺失子元素。
- `fixtures/p4` 已覆盖 LinkList、LinkSubList、missing target、cycle、invalid link value、invalid placement、invalid typed property、typed scalar Pad、Part-local Body placement、Sketch placement Pocket、DatumPlane support、DatumLine ReferenceAxis、DatumPoint parent Part placement；当前测试命令 `python3 -m unittest tests/test_mvp.py` 为 34 tests OK。

## Step 32：属性 schema 和解析

目标：

- 固定 `PropertyValue` 内部结构。
- 支持常用 FreeCAD 属性类型：Length、Angle、Bool、Integer、Float、String、Enumeration、Vector、Placement、Link、LinkList、LinkSub、LinkSubList。
- `PropertyType` 只是 JSON 序列化辅助字段，不是业务属性名。

验收：

- 错误类型返回 `unsupported_property` 或更精确 diagnostics。
- executor 不直接依赖某个 JSON 包装形态。

当前 FeatureExtrude 常用输入和 Placement 主路径已不依赖 raw 包装形态；其它 executor 后续迁移时应直接使用 document getter。

## Step 33：Link / LinkSub 统一 graph edge

目标：

- 所有 `PropertyLink*` 都由 document 层归一成 graph edge。
- graph 层只读取归一后的 dependency，不解析 feature 属性。
- runtime 能通过同一 LinkSub 数据解析 `FaceN`、stable subname、内部元素。

fixtures：

```text
fixtures/p4/
  body-link-list.json
  feature-link-sub-list.json
  missing-link-target.json
  cycle-link-sub.json
  invalid-link-value.json
```

验收：

- Body、FeatureBase、FeatureExtrude、DressUp、Pattern 读取同一套 LinkSub 数据结构。
- cycle diagnostics 发生在 graph 阶段，不拖到 feature executor。

当前已完成 Body、FeatureBase、FeatureExtrude；DressUp、Pattern 仍归入 P7 对应 executor 迁移时接入同一入口。

## Step 34：Placement 和坐标系

目标：

- 支持 `PropertyPlacement`。
- 支持 object-local、Body-local、Part-local placement 传递。
- 支持 Origin / Datum 的基础坐标引用。
- 坐标变换进入几何构造和 subshape map，不在输出端补偏移。

fixtures：

```text
fixtures/p4/
  body-placement-pad.json
  part-placement-body.json
  sketch-placement-pocket.json
  datum-plane-support.json
  datum-line-reference-axis.json
  datum-point-part-placement.json
```

验收：

- bbox、mesh、subshape map 使用同一坐标语义。
- 修改 placement 后依赖对象 recompute 顺序正确。

当前已落地 `part-placement-body.json`、`sketch-placement-pocket.json`、`datum-plane-support.json`、`datum-line-reference-axis.json`、`datum-point-part-placement.json`。`part-placement-body` 以 `App::Part` 为 recompute target，graph 先执行 Body，再由 Part container 暴露同一全局 placement 后的 child solid；Body / Part bbox、mesh summary bbox、volume 保持一致。`datum-plane-support` 覆盖 Sketch 的 `AttachmentSupport` 指向 `PartDesign::Plane`，Pad / Body bbox 和 mesh summary 使用 DatumPlane placement 后的坐标。`datum-line-reference-axis` 覆盖 FreeCAD `PartDesign::Line::getDirection()` 风格的 DatumLine 方向输入；`datum-point-part-placement` 覆盖 DatumPoint 自身 placement 与 parent Part placement 的组合。

## Step 35：Diagnostics 收敛

目标：

- diagnostics 包含 object、property、subname、target、stage。
- parse / graph / runtime / geometry / topo 阶段 code 可区分。
- 错误 fixture 不依赖自然语言全文。

建议新增 code：

```text
invalid_property_type
invalid_link_value
invalid_placement
unresolved_subname
unsupported_property_combination
```

当前已新增并覆盖 `invalid_link_value`、`invalid_property_type`、`invalid_placement`；`Diagnostic` 结构已兼容 `stage`、`target`、`subname` 并在 document parse、graph dependency、FeatureExtrude LinkSub / ReferenceAxis、Body / FeatureBase 链接失败路径输出。`unresolved_subname` 暂不单独新增，P6 接入 stable/full subname 和 `NamedShape` / `ElementMap` 后再从 `invalid_subshape` 中拆分。

## 完成定义

P4 完成需要同时满足：

- graph edge 全部来自 document/property 层。
- 已迁移 executor 不再各自解析 LinkSub JSON；后续新增 DressUp / Pattern / Datum 等 executor 时必须直接使用 document 层入口。
- Placement 已贯穿 sketch、feature、Body、Part 的当前主路径，并覆盖基础 DatumPlane / DatumLine / DatumPoint support；完整 Origin / AttachEngine 和复杂 Datum attachment 仍待补齐。
- P0-P3b fixture 不回退。
- P5/P6 所需的 external reference 和 topo naming 输入字段已经稳定。
