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
App::Origin
Assembly::AssemblyLink
Assembly::AssemblyObject
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
| P3a/P3b | shared `FeatureExtrude` 已覆盖主要长度、终止、双侧、方向、placement 和 taper 几何子集 |
| P4 | typed property、`PropertyLink*`、placement、GeoFeatureGroup 基础、graph edge 和结构化 diagnostics 已接入 |
| P5 | Sketcher profile、基础 BSpline、point raw vertex、construction、Coincident、已满足 line / line-end-pair Horizontal / Vertical orientation constraint、Parallel whole-line relation constraint、Perpendicular whole-line / line-circle-arc midpoint / point-wise / point-point-line relation constraint、Tangent whole-geometry direct / point-wise relation constraint、PointOnObject point-on-curve constraint、Symmetric point-point relation constraint、Block fixed-geometry constraint、Angle whole-line / point-wise datum constraint、Equal line-length / circle-radius constraint、Distance / Radius / Diameter datum constraint 和 line endpoint fixed coordinate datum、多闭合 wire、ExternalGeometry edge / vertex / planar face boundary / normal-face 单线投影 / whole-shape Face / Edge 展开 / ExternalTypes section 子集、`FaceMakerBuildFace` bounded split 子集、`WireJoiner::getOpenWires(noOriginal=true)` 原始 source edge 过滤与 open-edge split fragment 子集、基础 `InternalShape` 与 internal element map 已接入 |
| P6 | `NamedShape` / `ElementMap` 主路径、prism / Body boolean maker history、RefineModel + GenericShapeMapper history、AddSubShape slot 级 NamedShape ownership、merge history、stable subname 引用更新、同类唯一 split target 自动恢复、split / deleted diagnostics、Link retag 和 transformed copy terminal history 传播已接入 |
| P7 | Datum / Origin、RefineModel 主路径子集、Hole 常用孔、HoleCutType `Resources/Hole/*.json` 表加载生命周期、ThreadDepth / DIN76、ModelThread pipe-shell 实体螺纹几何和 thread clearance 入参归一化、Fillet / Chamfer、DressUp cache 与链式 `SupportTransform`、Transformed family 消费 AddSubShape slot ownership、Whole shape / Features 的 BaseFeature / Body 前缀 support、refined prefix support、subtractive-only original 和 multi-original Add/Sub replay 的基础路径与 terminal history diagnostics 已接入 |
| P8 | Part primitive、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、常用 Part Boolean、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / Assembly display 已接入 |

当前 P8 只固定可用于显示、拾取和单次 recompute 的基础能力：导入 shape 仍使用 indexed `NamedShape`；Part Boolean 已消费 boolean / section / generalFuse maker-history 子集；Link display 已保留源对象 alias retag，并支持 `LinkedObject.SubList` 单/多 subshape compound、对象名和 `$Label` 前缀形式的嵌套 LinkSub 路由、显式 `ElementList` group、LinkGroup source alias subshape、ElementList 数字下标 / `$Label` LinkSub、`ElementCount` 折叠数组数字下标 / linked-target alias LinkSub，并保持 LinkSub 可引用隐藏元素、同文档已物化 `ShowElement=true` 子 `LinkElement` 认领、已物化子元素 owner `LinkedObject` / `LinkTransform` / `PlacementList` / `ScaleList` 继承、缺失 `ShowElement=true` 子元素的请求内合成、`FullSubList` 精确 alias retag、`;:I` array index / `;:X` external-tag 的请求内 mapped postfix alias，以及上游 split / deleted terminal history 经 Link retag 后继续用于后续 stable subname diagnostics。请求内合成和继承计算沿用 `Owner_iN` 命名、`_LinkOwner`、`PlacementList` / `ScaleList`、默认 placement 和 `VisibilityList`，但不会创建对象或写回 `DocumentObject graph`。这些能力不等价于完整 FreeCAD Link 账本；Assembly display 不包含 Joint / solver。

## 未完成边界

- Sketcher 完整 solver、BSpline solver/control-point 语义、完整 FaceMakerBuildFace / WireJoinerP 账本、history-driven `InternalShape` / `getInternalElementMap()`。
- Topo Naming 完整 MapperHistory、复杂 split 自动旧引用恢复、merge history 收敛、ShapeFix / transformed / DressUp 的完整 maker history，以及 taper partial history 收敛。
- PartDesign transformed / pattern 完整 MapperHistory 与更复杂 ownership。
- Assembly Joint / solver、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

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
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已覆盖基础 primitive、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、基础 Link / LinkSub 单/多 SubList compound / LinkSub 对象名和 Label 前缀嵌套路由 / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementList 和 ElementCount 可见性独立 LinkSub / 已物化 ShowElement 子 LinkElement 认领与 owner target / transform-list 继承 / 缺失 ShowElement 子元素请求内合成 / `FullSubList` 精确 alias retag / LinkSub mapped postfix alias / Link retag terminal history 传播 / Assembly display 与常用 Part Boolean 子集 |

## 后续队列

1. 补 P6：完整 MapperHistory 生命周期、ShapeFix history、复杂 split 自动旧引用恢复、merge history 收敛，并把 taper partial history 收敛到正式 MapperHistory。
2. 补 P5：FaceMaker / WireJoiner 状态机、复杂 internal element map、更多 external geometry 和约束。
3. 补 P7：transformed / pattern 完整 MapperHistory 与更复杂 ownership。
4. 扩展 P8：Assembly Joint / solver、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

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
