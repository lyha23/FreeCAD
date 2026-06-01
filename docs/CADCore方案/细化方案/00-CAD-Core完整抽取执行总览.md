# CAD Core 完整抽取执行总览

本目录是 `../00-CAD-Core抽取方案.md` 的执行拆解。文档只记录当前基线、阶段边界、剩余缺口和验收规则，不记录逐次实现过程。

## 当前基线

`cad-core` 已是独立 C++17 / CMake Core，包含 `cad-core-lib`、CLI adapter 和薄 C ABI adapter。输入是 FreeCAD 风格 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`，输出包含对象结果、mesh summary、bbox、volume、subshape map、`named_shapes` 和 diagnostics；CLI 可额外把本次 recompute 的指定对象写出为 BREP / STEP / STL 文件，但文件内容不进入持久 graph 或默认响应状态。

当前 registry 覆盖：

```text
App::Part
App::Link
App::LinkElement
App::LinkGroup
App::FeaturePython
App::Origin
Assembly::AssemblyLink
Assembly::AssemblyObject
Assembly::JointGroup
Mesh::Import
Part::BooleanFragments
Part::Box
Part::Common
Part::Cone
Part::Cut
Part::Cylinder
Part::Ellipse
Part::Ellipsoid
Part::FeatureBooleanFragments
Part::FeatureXOR
Part::Fuse
Part::Helix
Part::ImportBrep
Part::ImportIges
Part::ImportStep
Part::Line
Part::MultiCommon
Part::MultiFuse
Part::Plane
Part::Prism
Part::RegularPolygon
Part::Section
Part::Sphere
Part::Spiral
Part::Torus
Part::Vertex
Part::Wedge
Part::XOR
Sketcher::SketchObject
PartDesign::Body
PartDesign::CoordinateSystem
PartDesign::Plane
PartDesign::Line
PartDesign::Point
PartDesign::FeatureBase
PartDesign::Pad
PartDesign::Pocket
PartDesign::Hole
PartDesign::Fillet
PartDesign::Chamfer
PartDesign::Mirrored
PartDesign::LinearPattern
PartDesign::PolarPattern
PartDesign::Scaled
PartDesign::MultiTransform
```

阶段基线：

| 阶段 | 当前状态 |
| --- | --- |
| P0-P2 | document / graph / runtime 底座、Sketch + Body + Pad、FeatureBase / Pocket / Body fuse-cut 主链稳定 |
| P3a/P3b | shared `FeatureExtrude` 已覆盖主要长度、终止、双侧、方向、placement 和 taper 几何子集；taper 对象级 partial history 状态已暴露 |
| P4 | typed property、`PropertyLink*`、placement、GeoFeatureGroup 基础、graph edge 和结构化 diagnostics 已接入 |
| P5 | Sketcher profile、基础 BSpline、point raw vertex、construction、Coincident、已满足 line / line-end-pair Horizontal / Vertical orientation constraint、Parallel whole-line relation constraint、Perpendicular whole-line / line-circle-arc midpoint / point-wise / point-point-line relation constraint、Tangent whole-geometry direct / point-wise relation constraint、PointOnObject point-on-curve constraint、Symmetric point-point relation constraint、Block fixed-geometry constraint、Angle whole-line / point-wise datum constraint、Equal line-length / circle-radius constraint、Distance / Radius / Diameter datum constraint 和 line endpoint fixed coordinate datum、多闭合 wire、closed-wire hole 的 profileShape / InternalShape 分离、ExternalGeometry edge / vertex / planar face boundary / normal-face 单线投影 / whole-shape Face / Edge 展开 / ExternalTypes section 子集、`FaceMakerBuildFace` bounded split 子集和 self-intersecting edge pre-split terminal history、`WireJoiner::getOpenWires(noOriginal=true)` 原始 source edge 过滤、open-edge split fragment、closed-source result-fragment ownership、InternalFace outer-boundary generated history 和 InternalShape terminal split / deleted history 子集、基础 `InternalShape` 与 internal element map 已接入 |
| P6 | `NamedShape` / `ElementMap` 主路径、prism / Body boolean maker history、Part::Extrusion prism / taper maker history、taper `topo_naming_history=history_partial:taper` 对象级验收、RefineModel + GenericShapeMapper history、AddSubShape slot 级 NamedShape ownership、merge history 与 Link retag 后续传播、Sketch InternalShape `InternalFaceN` generated-from-outer-boundary-edge history、self-intersecting raw edge pre-split 到多个 `InternalEdgeN` 的 terminal split history、one-source-to-many `InternalEdgeN` split history 和 one-source-to-zero `EdgeN/VertexN` deleted history、stable subname 引用更新、ReferenceShadow 经 ElementMap 刷新普通 Shape LinkSub、UpToFace stable-subname native geometry expected、Sketch ExternalGeometry direct indexed / source-prefixed / Body profile-source oldName / split collapsed point subset native geometry expected、同类唯一 split target 自动恢复、split / deleted diagnostics、Link retag 和 transformed copy terminal history 传播已接入 |
| P7 | Datum / Origin、RefineModel 主路径子集、Hole 常用孔、HoleCutType `Resources/Hole/*.json` 表加载生命周期、ThreadDepth / DIN76、ModelThread pipe-shell 实体螺纹几何和 thread clearance 入参归一化、Hole support-backed native expected collector 生命周期、blind / through-all / point / tapered / drill-point / head-cut / threaded standard heads / threaded dynamic DIN7984 / ISO2009 / ThreadDepth / clearance / ModelThread metric pipe-shell native oracle、Fillet / Chamfer Body-member native expected collector、Mirrored basic native expected collector 与链式 DressUp SupportTransform geometry oracle、LinearPattern basic native expected collector、PolarPattern basic native expected collector 与 Body-prefix Whole shape oracle、Scaled basic native expected collector、MultiTransform basic native expected collector、DressUp cache 与链式 `SupportTransform`、Transformed family 消费 AddSubShape slot ownership、Whole shape / Features 的 BaseFeature / Body 前缀 support、refined prefix support、subtractive-only original 和 multi-original Add/Sub replay 的基础路径与 terminal history diagnostics 已接入 |
| P8 | Part primitive、Ellipsoid / Torus / ImportBrep / ImportStep / Mesh::Import STL / Section native expected collector 与 FreeCAD `optimalBoundingBox()` bbox 口径、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、常用 Part Boolean、BOPTools FeatureXOR / FeatureBooleanFragments native expected collector、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / 多层 `$Label` 与 ElementList child linked-target `$Label` LinkSub source-alias retag / hidden link 解析 / LinkElement native expected collector / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / LinkGroup ElementList / VisibilityList native expected collector / 数字子元素 linked-target prefix LinkSub / ShowElement 数字子元素 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / ShowElement owner Shape 与 child ScaleList 边界 / ShowElement LinkElement create / claim / update / delete `documentObjectUpdates` / ShowElement=true 显式 ElementList owner ElementCount / VisibilityList 和 child `_LinkOwner` / `LinkedObject` / `LinkTransform` sync `documentObjectUpdates` / ShowElement true->false owner list 收回与 owned child delete `documentObjectUpdates` / ShowElement=false ElementCount owner list resize sync `documentObjectUpdates` / `FullSubList` parser / C ABI `PropertyXLink*` capabilities / `PropertyXLinkList` values/SubSet parser / elementReferenceUpdates / 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal 与 merge history 传播 / Assembly display / JointGroup 与 App::FeaturePython Joint 输入元数据已接入，并纳入 expected collector 验收 |

当前 P8 只固定可用于显示、拾取和单次 recompute 的基础能力：Part primitive、ImportBrep / ImportStep 和 Section bbox oracle 按 FreeCAD `optimalBoundingBox()` / OCCT `AddOptimal` 的 tighter bbox，不使用 UI/display 的松 `Shape.BoundBox`；Mesh::Import STL oracle 按 FreeCAD `Mesh` 属性的点、边、面、体积和 bbox 统计固定；导入 shape 或 section edge 在不同 OCCT runtime 下出现 tolerance 外扩时，用显式 `bbox_delta` 固定验收边界；导入 shape 仍使用 indexed `NamedShape`；Part Boolean 已消费 boolean / section / generalFuse maker-history 子集，XOR / BooleanFragments expected 已走 FreeCADCmd BOPTools `makeXOR()` / `makeBooleanFragments()` native proxy oracle；Link display 已保留源对象 alias retag，并支持 `LinkedObject.SubList` 单/多 subshape compound、`LinkPlacement` / `Placement` alias、`ScaleVector` 优先于 scalar `Scale` 的 FreeCAD 缩放语义、对象名和 `$Label` 前缀形式的嵌套 LinkSub 路由、多层 `$Label` 与 ElementList child linked-target `$Label` LinkSub source-alias retag、LinkElement native expected collector、显式 `ElementList` group、LinkGroup source alias subshape、LinkGroup ElementList / VisibilityList native expected collector、ElementList 数字下标 / `$Label` LinkSub、`ElementCount` 折叠数组数字下标 / linked-target alias LinkSub、`1.Box.Face1` 这类数字子元素 linked-target prefix LinkSub，并保持 LinkSub 可引用隐藏元素、同文档已物化 `ShowElement=true` 子 `LinkElement` 认领、已物化子元素 owner `LinkedObject` / `LinkTransform` / `PlacementList` / `ScaleList` 继承、ShowElement owner Shape 与 child Shape 的 ScaleList native 边界、缺失 `ShowElement=true` 子元素的请求内合成，以及 `ShowElement=true` 子元素 `1.Face1` 这类数字下标 LinkSub source-alias retag、`FullSubList` parser / C ABI `PropertyXLink*` capabilities / `PropertyXLinkList` values/SubSet parser / elementReferenceUpdates / 精确 alias retag、`;:I` array index / `;:X` external-tag 的请求内 mapped postfix alias，以及上游 split / deleted terminal history 和 merge history 经 Link retag 后继续用于后续诊断与 source ledger 追踪。document 层已识别 `PropertyXLink` / `PropertyXLinkSub` / `PropertyXLinkSubList` / `PropertyXLinkList` 及 `PropertyLinkHidden` / `PropertyLinkListHidden` / `PropertyLinkSubHidden` / `PropertyLinkSubListHidden` / `PropertyXLinkSubHidden`，executor 可读取这些 hidden link，但 graph 不把它们当依赖边；C ABI capabilities 同步暴露上述 XLink 输入 shape，`PropertyXLinkList` 按 FreeCAD object-only values 和 sub-element SubSet 双形态解析，避免 Web adapter 只能依赖 parser 隐式支持。`ShowElement=true` 请求内合成和继承计算沿用 `Owner_iN` 命名、`_LinkOwner`、`PlacementList`、child `ScaleList`、默认 placement 和 `VisibilityList`；`documentObjectUpdates` 已输出缺失子元素 create、空 owner / owner=0 子元素 claim、已物化子元素属性 update、超出 `ElementCount` 的 owned child delete，显式 ElementList 与 owner `ElementCount` / `VisibilityList` 不一致时的 owner sync 建议，显式 ElementList child `_LinkOwner` / `LinkedObject` / `LinkTransform` sync 建议且 `CopyOnChangeOwned` child 保留自有 `LinkedObject`，`ShowElement` true->false 时 child placement / scale 收回到 owner list 并删除 owned child，以及 `ShowElement=false` ElementCount owner `PlacementList` / `ScaleList` / `VisibilityList` resize sync 建议，供前端更新 `DocumentObject graph`，CAD Core 不在后端持久化或隐式改写请求 graph；`VisibilityList` native collector 走 `setElementVisible()`，`_LinkOwner` native collector 映射到 FreeCAD `DocumentObject::getID()`，LinkGroup native collector 走 `LinkBaseExtension::setLink()`，ownerless fixture `LinkElement` 只在 collector 内用 native `App::Link` 代理避免 FreeCADCmd 崩溃；直接 `$Label` 指向实体对象的 LinkSub collector 会去掉当前 target 自身 Label token 以采集 native 几何，目标为 `App::Link` 的多层 LinkSub 仍保持 source-equivalent 预期，不伪装成 native oracle。这些能力不等价于完整 FreeCAD Link 账本；Assembly 已能显示 component group，并把 `Assembly::JointGroup` 下的 `App::FeaturePython` Joint / GroundedJoint 输入输出为 `solve=not_migrated` 元数据，collector 通过 FreeCAD `newObject()` / `JointObject` 初始化固定该输入边界，但不执行 OndselSolver。

P8 Link display 已新增 `App::DocumentObjectGroup` plain group 请求内 child-cache-style 展开：`App::DocumentObjectGroup` 作为轻量容器 executor 保留 `Group` children；`App::Link.LinkedObject -> App::DocumentObjectGroup` 和显式 `ElementList` 内的 plain group 都按 FreeCAD `linkedPlainGroup()` / `updateGroup()` 递归读取 `Group` children 并组合为本次请求内 display compound，同时保留 flattened index、object-name、group path 和 `$Label` LinkSub alias。当前只覆盖请求内 display、shape 聚合和 nested child subshape picking，不声明完整 `_ChildCache` 持久生命周期、copy-on-change / linked-owner 矩阵或完整 Link 写回事务。

## 未完成边界

- Sketcher 完整 solver、BSpline solver/control-point 语义、完整 FaceMakerBuildFace / WireJoinerP 账本、history-driven `InternalShape` / `getInternalElementMap()`。
- Topo Naming 完整 MapperHistory、复杂 split 自动旧引用恢复、ShapeFix / transformed / DressUp 的完整 maker history，以及 taper partial history 收敛；merge history 已有 Body boolean 与 Link retag 传播回归，但仍要纳入完整 MapperHistory 生命周期。
- PartDesign transformed / pattern 完整 MapperHistory 与更复杂 ownership。
- Assembly 求解器、完整 Joint placement / constraint 求解、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

## 阶段索引

| 阶段 | 文档 | 状态 |
| --- | --- | --- |
| P0 | `01-P0-Core壳.md` | 已形成冻结底座 |
| P1 | `02-P1-Sketch-Body-Pad闭环.md` | 已形成基础闭环 |
| 接口 | `03-接口与验收样例.md` | 持续维护验收口径 |
| P2 | `04-P2-FeatureBase-FeatureAddSub-Pocket.md` | 已形成 PartDesign 主链 |
| P3a | `05-P3a-FeatureExtrude-UpTo终止语义.md` | 已覆盖核心 UpTo 子集 |
| P3b | `06-P3b-FeatureExtrude双侧方向与Placement.md` | 已覆盖常用双侧 / 方向 / placement 子集 |
| P4 | `07-P4-Document-Property-Placement完整化.md` | 已接入 property / link / placement 主路径 |
| P5 | `08-P5-Sketcher核心与内部元素.md` | 已接入 solver-facing 子集 |
| P6 | `09-P6-TopoNaming主路径.md` | 主路径骨架已落地，完整 MapperHistory 待补 |
| P7 | `10-P7-PartDesign常用生态.md` | 常用生态基础子集已落地 |
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已覆盖基础 primitive、Ellipsoid / Torus / ImportBrep / ImportStep / Mesh::Import STL / Section native expected collector 与 FreeCAD `optimalBoundingBox()` bbox 口径、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、BOPTools FeatureXOR / FeatureBooleanFragments native expected collector、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / 多层 `$Label` 与 ElementList child linked-target `$Label` LinkSub source-alias retag / hidden link 解析 / LinkElement native expected collector / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / LinkGroup ElementList / VisibilityList native expected collector / 数字子元素 linked-target prefix LinkSub / ShowElement 数字子元素 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / ShowElement owner Shape 与 child ScaleList 边界 / ShowElement LinkElement create / claim / update / delete `documentObjectUpdates` / ShowElement=true 显式 ElementList owner ElementCount / VisibilityList 和 child `_LinkOwner` / `LinkedObject` / `LinkTransform` sync `documentObjectUpdates` / ShowElement true->false owner list 收回与 owned child delete `documentObjectUpdates` / ShowElement=false ElementCount owner list resize sync `documentObjectUpdates` / `FullSubList` parser / C ABI `PropertyXLink*` capabilities / `PropertyXLinkList` values/SubSet parser / elementReferenceUpdates / 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal 与 merge history 传播 / Assembly display、JointGroup / App::FeaturePython Joint 输入元数据 expected collector 与常用 Part Boolean 子集 |

## 后续队列

1. 补 P6：完整 MapperHistory 生命周期、ShapeFix history、复杂 split 自动旧引用恢复，并把已记录 / 已跨 Link retag 传播的 merge history 与 taper partial history 收敛到正式 MapperHistory。
2. 补 P5：FaceMaker / WireJoiner 状态机、复杂 internal element map、更多 external geometry 和约束。
3. 补 P7：transformed / pattern 完整 MapperHistory 与更复杂 ownership。
4. 扩展 P8：Assembly 求解器与完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

## 全局规则

- 本地 `src/` 是语义来源；不确定行为先读 FreeCAD App / Mod 源码。
- `DocumentObject graph` 是唯一持久源数据。
- adapter 只做协议转换。
- 不按 fixture 名称、几何类型排序或输出端修剪替代 FreeCAD 通用流程。
- 不支持的 `TypeId`、属性、LinkSub、subshape 或构造路径必须返回结构化 diagnostics。

## 验收命令

功能修改优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

文档修改至少执行：

```bash
git diff --check
```
