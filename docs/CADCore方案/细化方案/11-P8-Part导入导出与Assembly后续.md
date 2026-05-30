# P8：Part、导入导出与 Assembly 后续

P8 已覆盖 FreeCAD `Part::Primitive`、`Part::ImportBrep`、`Part::ImportStep`、`Part::ImportIges`、`Mesh::Import` 的 STL 子集、CLI BREP / STEP / STL 导出、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / Assembly display 与常用 Part Boolean 子集。Assembly Joint / solver、完整 Link 账本和产品化 adapter 仍保持后置。

## 当前基线

| 边界 | 当前实现 | 输出 / 命名 |
| --- | --- | --- |
| Part primitives | `Part::Box`、`Cylinder`、`Prism`、`RegularPolygon`、`Sphere`、`Ellipsoid`、`Cone`、`Torus`、`Wedge`、`Vertex`、`Line`、`Ellipse`、`Plane`、`Helix`、`Spiral` 已接入 | mesh、subshape map、bbox、volume、kernel metadata、indexed `NamedShape` |
| Import | `Part::ImportBrep`、`Part::ImportStep`、`Part::ImportIges`、`Mesh::Import` STL 子集按 `FileName` 读取请求内 shape | 导入内容只参与本次 recompute，不写入响应状态；当前仍使用 indexed `NamedShape` |
| Export | CLI `--export-object` / `--export-format` / `--export-file` 支持 BREP / STEP / STL 写出 | 文件写出是 adapter 副作用，不进入 `DocumentObject graph` |
| Part Boolean | Fuse、Cut、Common、Section、MultiFuse、MultiCommon、XOR、BooleanFragments 已接入 | boolean / section / generalFuse maker-history 子集进入 `topo::NamedShape` |
| Link display | document 层识别 `PropertyXLink*`；`App::Link`、`App::LinkElement`、`App::LinkGroup`、LinkSub 对象名和 Label 前缀嵌套路由、LinkGroup source alias subshape、带 `ElementList` 的 `App::Link`、ElementList / `ElementCount` 可见性独立 LinkSub、同文档已物化 `ShowElement=true` 子 `LinkElement` 认领与 owner target / transform-list 继承、缺失子元素请求内合成、`FullSubList` 精确 alias retag、mapped postfix alias 和 terminal history 传播已接入基础 display | 支持源对象 alias retag；支持单/多 `LinkedObject.SubList` subshape compound、对象名和 `$Label` 前缀形式的嵌套 LinkSub、显式 `ElementList` group、`PlacementList` / `ScaleList`、默认 element placement、`VisibilityList`、ElementList 数字下标与 Label alias、折叠数组数字下标与 linked-target alias LinkSub，并保持 LinkSub 可引用隐藏元素、完整 subname 精确 alias、`;:I` array index、`;:X` external-tag 请求内 mapped alias，以及上游 split / deleted stable history；缺失子元素只在本次响应中合成，不写回 graph |
| Assembly display | `Assembly::AssemblyLink` 代理 component shape；`Assembly::AssemblyObject` 按 `Group` 输出 grouped display shape | 输出标记 `solve=not_migrated`，不包含 Joint / solver |

Part Boolean 当前边界：

- 二元 boolean：`Base` / `Tool` 输入走 OCCT maker history 主路径，`Refine=true` 时复用当前 RefineModel 子集。
- `Part::Section`：输出 section edges compound，并保留 section maker history。
- `Part::MultiFuse` / `Part::MultiCommon`：按 `Shapes` 链接列表读取请求内 shape；`MultiFuse` 可展开单 compound 输入，`MultiCommon` 以 `CommonOfAllShapes` 为默认语义。
- `Part::XOR` / `Part::FeatureXOR`：作为 BOPTools `FeatureXOR` alias，走 Fuse / Common / Cut 组合主路径，当前只支持 `Tolerance=0`。
- `Part::BooleanFragments` / `Part::FeatureBooleanFragments`：支持 `Mode=Standard`、`Mode=Split` 的 solid-safe / wire aggregate / shell aggregate / CompSolid aggregate 子集、`Mode=CompSolid` 和非负 `Tolerance`。

Link / Assembly 当前边界：

- `App::Link` 支持 `LinkedObject`、`LinkTransform`、`LinkPlacement` / `Placement`、基础 `Scale` / `ScaleVector`，并可解析 `LinkedObject.SubList` 中单个或多个 current / stable subname；多个 subshape 会在本次请求内组合为 compound，并保留源对象 alias retag。
- `App::LinkElement` 复用 linked-object、transform、placement 和 scale 语义，输出带源对象 alias retag 的元素 shape。
- `App::LinkGroup` 和带 `ElementList` 的 `App::Link` 聚合请求内元素 shape，支持 group placement、`VisibilityList`、元素对象名、数字下标和 `$Label` LinkSub alias；`VisibilityList` 只影响 display，不阻断 LinkSub 解析。
- `App::Link` 的 `ElementCount` 折叠数组路径支持 `PlacementList`、`ScaleList`、`VisibilityList`、link placement / scale 和源对象 alias retag；`1.Face1` 这类数字下标 LinkSub 会映射到对应折叠元素，且 `VisibilityList=false` 不阻断 LinkSub 解析，`Box.Face1` / `$PrettyBox.Face1` 这类 linked target 前缀会按 FreeCAD `getElementIndex()` 语义重定向到第 0 个折叠元素。
- `ShowElement=true` 已支持同文档中 `Owner_iN` 形式、`_LinkOwner` 匹配或空 owner 的已物化 `LinkElement` 子对象认领与聚合；若已物化子元素缺少自己的 `LinkedObject`，会按 owner `App::Link` 继承 `LinkedObject` 和 `LinkTransform` 并补请求内依赖边；若缺少自身 placement / scale，会消费 owner `PlacementList` / `ScaleList`，其中 `ScaleList[i].x` 按 FreeCAD `LinkElement.Scale` 作为 uniform scale 使用。
- `ShowElement=true` 缺失 `Owner_iN` 子对象时，会按 FreeCAD `LinkBaseExtension::update()` 的命名、`PlacementList` / `ScaleList`、默认 placement 和 `VisibilityList` 规则合成请求内 display 元素；合成元素只出现在本次响应的 group shape、metadata 和 `NamedShape` source alias 中，不创建对象、不写回 `DocumentObject graph`。
- `PropertyXLinkSub.FullSubList` 已作为精确 alias 写入 Link subshape 的 `NamedShape.element_map`，用于保留请求里已经提供的完整 subname；`ElementCount` 折叠数组的非零 index 已写入 `;:I{idx}` mapped postfix alias，显式外部 full subname 已写入 `;:X;...` external-tag mapped postfix alias；Link retag 会传播上游 split / deleted terminal history，避免后续 stable subname 诊断降级为 unsupported；`BoxLink.Box.Face1` 和 `$PrettyBox.Face1` 这类对象名 / Label 前缀 LinkSub 会先按 FreeCAD `getSubObject()` 路由剥离当前 linked object token，再沿源对象 `NamedShape` alias 解析；完整 cross-document 文档哈希生命周期和更复杂多层 LinkSub 链尚未迁移。
- `ShowElement=true` 自动创建 / 删除 `LinkElement` 的完整生命周期尚未迁移。

`fixtures/p8` 覆盖 Link full-shape、Link transform、Link scale、Link SubList 单 face / multi face compound、LinkSub 对象名和 Label 前缀嵌套路由、`FullSubList` 精确 alias retag 与 mapped postfix alias、Link retag 后 split / deleted terminal history 诊断、LinkGroup source alias subshape、LinkElement、LinkGroup ElementList / VisibilityList、Link ElementList 和 ElementCount 可见性独立 LinkSub、已物化 ShowElement 子 LinkElement 认领、已物化子元素 owner target / transform-list 继承、缺失 ShowElement 子元素请求内合成、Link missing target、Link invalid subshape、Assembly basic display、Part primitive、Part import / export round-trip、Part Boolean、XOR 和 BooleanFragments Standard / Split / CompSolid mode。

## 范围分层

| 层级 | 范围 |
| --- | --- |
| 当前主路径 | Part primitives、常用 Part Boolean、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 / Assembly display |
| 当前不扩张 | 导入 shape 完整 ElementMap、完整 FreeCAD Link 账本、Assembly Joint / solver、Worker / WASM / Web service bridge |
| 不属于 P8 | 跨请求缓存、BREP 持久状态、前端状态同步协议、GUI / ViewProvider / Workbench 行为 |

## 边界

- 文件导入导出可以处理 BREP，但 BREP 不进入持久 `DocumentObject graph` 的默认状态模型；导出是 adapter 对本次计算 shape 的文件写出，不是前后端状态同步协议。
- Web / Worker / WASM 只做 adapter，不改变 CAD Core 无状态边界。
- Assembly 不应绕过 topo naming；Joint 的 subname 和 placement 仍需要稳定引用模型。
- 当前 Part primitive 和导入 shape 仍使用 indexed `NamedShape`；基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成已有 source alias retag；Part Boolean 已消费 boolean / section / generalFuse maker history，当前 aggregate Split partial history 向完整 MapperHistory 收敛，导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、完整 cross-document 文档哈希生命周期和更复杂多层 LinkSub 链仍属于 P6/P8 后续工作。

## 前置条件

- P6 MapperHistory、split / merge 旧引用恢复和 ShapeFix / Refine history 足够稳定。
- P5 Sketcher external geometry 和 internal element map 能支撑常用引用。
- P7 Body 生态不再依赖高层 fixture 特判。
- CLI / C ABI 对同一 fixture 的核心结果一致。

## 规划落点

| 能力 | cad-core 落点 |
| --- | --- |
| Part primitives | 当前落在 `features/part.cpp`，后续复杂 primitive 可拆到 `geometry/primitives.*` |
| Part Boolean | `features/part_boolean.*` + `topo/named_shape.*` |
| Import / Export | `Part::ImportBrep` / `Part::ImportStep` / `Part::ImportIges` 落在 `features/part.cpp`，`Mesh::Import` STL 子集落在 `features/mesh.cpp`；BREP / STEP / STL 文件导出落在 `geometry/shape_exporter.*`，由 CLI adapter 触发 |
| Assembly Link / Joint | 基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 display 落在 `features/link.*`、`document/` 已识别 `PropertyXLink*`；Joint / solver 后续落 `features/assembly_*`、`graph/` |
| Worker / WASM / Web | adapter 层 |

## 剩余缺口

- 导入 shape 的完整 ElementMap，以及 `ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、完整 cross-document 文档哈希生命周期和更复杂多层 LinkSub 链尚未迁移；当前 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 display 已保留源对象 alias，但不覆盖完整 FreeCAD Link 账本。
- Assembly Joint、复杂 placement chain 和装配求解未迁移。
- Worker / WASM / Web service bridge 未产品化。

## 验收

- 每个 Part / Assembly `TypeId` 有明确 executor 或 diagnostics。
- 文件导入导出不污染无状态核心边界。
- Link / Joint placement 和 stable subname 不靠前端猜测。
- Worker / WASM / Web adapter 与 CLI / C ABI 复用同一 core recompute。
