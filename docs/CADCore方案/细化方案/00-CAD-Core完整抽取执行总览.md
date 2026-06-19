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
| P3a/P3b | shared `FeatureExtrude` 已覆盖主要长度、终止、双侧、方向、placement 和 taper 几何子集；taper ThruSections maker history 已由 P6 发布为 `covered_full` |
| P4 | typed property、`PropertyLink*`、placement、GeoFeatureGroup 基础、graph edge 和结构化 diagnostics 已接入 |
| P5 | Sketcher profile、基础约束与 datum constraint 子集、ExternalGeometry 子集、closed / open / split internal geometry、BSpline InternalShape oracle、`FaceMakerBuildFace` bounded split 子集、pre-split / splitter history summary、WireJoiner EdgeInfo / WireInfo 边级账本子集、ordered `WireInfo::vertices` / `iteration2` 标记、branch-search candidate inside/outside、`newWire` seed、splitWire / done lifecycle 与 `exhaustTightBound()` 二次 owner 诊断账本、bounded tight-bound primary / secondary owner slot 汇总、基础 `InternalShape`、internal element map 与 terminal split / deleted history 已接入 |
| P6 | `NamedShape` / `ElementMap` 主路径、Sketch InternalShape FaceMaker history context 与通用 `element_history_status` expected / C ABI capabilities 验收、prism / Body boolean / Part::Extrusion maker history 子集、RefineModel `Modified()` / `IsDeleted()` + GenericShapeMapper history、AddSubShape slot ownership、stable subname 引用更新、ReferenceShadow 恢复、split / deleted diagnostics、Link retag 和 transformed copy terminal history 传播已接入 |
| P7 | Datum / Origin、RefineModel 主路径子集、Hole 常用孔与资源表、ModelThread pipe-shell 子集、Fillet / Chamfer、DressUp cache、Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 基础路径、Whole shape / Features support、Add/Sub replay 与 terminal history diagnostics 已接入 |
| P8 | Part primitives、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、常用 Part Boolean、基础 Link / LinkSub / LinkGroup / LinkElement display、ShowElement 请求内子元素合成与 `documentObjectUpdates` 建议、`PropertyXLink*` / `FullSubList` / mapped postfix alias、`App::DocumentObjectGroup` plain group 展开、Assembly display、Joint 输入元数据、硬依赖 real Ondsel adapter 与 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local placement writeback 已接入 |

当前 P8 只固定可用于显示、拾取和单次 recompute 的基础能力。Part primitive、ImportBrep / ImportStep / Mesh::Import STL 和 Section native oracle 已按 FreeCAD `optimalBoundingBox()` / OCCT `AddOptimal` 口径冻结，导入 shape 或 section edge 的 runtime tolerance 差异用显式 `bbox_delta` 标明；导入 shape 仍使用 indexed `NamedShape`。Part Boolean 已消费 boolean / section / generalFuse maker-history 子集，XOR / BooleanFragments expected 走 FreeCADCmd BOPTools native proxy oracle。Part Workbench surface 当前发布 source-backed `Part::Loft` first batch、source-backed `Part::Sweep` / PipeShell first batch、source-backed `Part::FilledFace` helper first batch，以及 source-backed `Part::GeomPlateSurface` geometry helper first batch：`Part::FilledFace` 是 cad-core 对 FreeCAD `Part.makeFilledFace()` 的 helper 请求类型，不是原生 FreeCAD DocumentObject；`Part::GeomPlateSurface` 是 cad-core 对 FreeCAD `Part.GeomPlate.BuildPlateSurface()` 的 geometry backend helper 请求类型，不是 GUI 或 fake FreeCAD `Part::GeomPlate` DocumentObject。GeomPlate 首批只发布 3D G0 curve constraints、3D point constraints、默认 / 显式 build params、`GeomPlate_MakeApprox` face、source evidence、surface metadata 和 invalid diagnostics。`Part::Sweep.Linearize=true`、advanced PipeShell wrapper、auxiliary spine、located profile、support / trihedron / binormal、Filling 的 `surface` / `supports` / `orders` / constraints / non-default params、compound optional case、GeomPlate initial surface / G1 curve-on-surface / projected 2D curve / `Part.PlateSurface(Curves=...)`、Hole `ModelThread` 内部 PipeShell 和 full Part surface family 仍不是已支持目标。

Link display 已覆盖源对象 alias retag、单/多 subshape compound、Link placement / scale、对象名和 `$Label` 前缀路由、显式 `ElementList`、`ElementCount` 折叠数组、ShowElement 已物化子元素认领、缺失子元素请求内合成、owner / child sync、toggle-off 收回与删除建议、hidden link 解析、`PropertyXLink*` / `FullSubList` / mapped postfix alias、Link retag 后 terminal / merge history 传播，以及 plain group children 请求内 display 展开。`documentObjectUpdates` 只作为前端更新 `DocumentObject graph` 的建议返回，CAD Core 不在后端持久化或隐式改写请求 graph。这些能力不等价于完整 FreeCAD Link 账本。Assembly 已能显示 component group，并把 `Assembly::JointGroup` 下的 `App::FeaturePython` Joint / GroundedJoint 输入输出为 request-local solver DTO；CAD Core 构建时硬依赖 bundled OndselSolver，只发布 real Ondsel adapter。`cad-core/fixtures/c3m6/expected` 已采集并验收 10 个 FreeCADCmd native solver placement expected，覆盖 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local 子集、object-level JCS placement、Distance / multi-component writeback、invalid grounded 和 ungrounded 行为；当前无 SCOPE-006 known-gap expected。

P8 Link display 已新增 `App::DocumentObjectGroup` plain group 请求内 child-cache-style 展开：`App::DocumentObjectGroup` 作为轻量容器 executor 保留 `Group` children；`App::Link.LinkedObject -> App::DocumentObjectGroup` 和显式 `ElementList` 内的 plain group 都按 FreeCAD `linkedPlainGroup()` / `updateGroup()` 递归读取 `Group` children 并组合为本次请求内 display compound，同时保留 flattened index、object-name、group path 和 `$Label` LinkSub alias。当前只覆盖请求内 display、shape 聚合和 nested child subshape picking，不声明完整 `_ChildCache` 持久生命周期、copy-on-change / linked-owner 矩阵或完整 Link 写回事务。

## 未完成边界

- Sketcher 完整 solver、BSpline solver/control-point 语义、完整 `ExternalGeometryExtension` 状态机、WireJoiner EdgeInfo / WireInfo 的 `findTightBoundSplitWire()` / `findTightBoundUpdateVertices()` / `exhaustTightBound()` 已有 splitWire / done / secondary-owner lifecycle 诊断账本，但尚未替代 bounded ownership classifier，也尚未执行 `exhaustTightBoundUpdateWire()` 搜索主路径；真实 `openWireCompound` history 过滤、FaceMaker / WireJoiner history summary 到正式 `NamedShape` / `ElementMap` 的完整 MapperHistory 消费仍待迁移；FaceMaker summary 当前进入 `Sketch.InternalShape` 的 `NamedShape` 元数据和通用 `element_history_status`。
- Topo Naming 完整 MapperHistory 发布边界仍按 maker 分阶段复核；P6 MakerHistory S3-S5 已确认 ShapeFix、DressUp / Refine / transformed 和 taper 当前 focused scope 为 supported，复杂 split / deleted 旧引用恢复仍保留为 `notCollected`，只有专项复核证明当前 cad-core 与 FreeCAD authority 或 checked-in expected 不匹配时才转为 `backendGap`；merge history 已有 Body boolean 与 Link retag 传播回归，但仍要纳入完整 MapperHistory 生命周期。
- PartDesign transformed / pattern 完整 MapperHistory 与更复杂 ownership。
- 完整 Joint placement / constraint 求解、剩余 JointType、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

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
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已覆盖 Part primitives、导入导出、常用 Part Boolean、基础 Link / LinkSub / LinkGroup / LinkElement display、ShowElement 请求内生命周期建议、XLink / FullSubList / mapped alias、plain group 展开、Assembly display、Joint 输入元数据、硬依赖 real Ondsel adapter 与 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle stateless placement writeback |
| P5/P6 联合主线 | `13-【已实现】ExternalGeometry-TopoNaming下一阶段主线.md` | 已实现：ExternalGeometryExtension 状态机、MapperHistory、FaceMaker / WireJoiner history 消费和复杂引用恢复已按 P5P6 主线收口 |

## 后续队列

1. P5/P6 联合主线已按 `13-【已实现】ExternalGeometry-TopoNaming下一阶段主线.md` 收口；后续只在阶段发布时复跑验收和回写台账。
2. 补 P6 MakerHistory 余量：S0-S6 已完成 ShapeFix、RefineModel、taper、transformed / DressUp 的 capability / tests / expected 与正式文档一致性复核；当前无 C++ backendGap，后续只在复杂 split / deleted oracle 证明 mismatch 时重新打开实现任务。
3. 补 P7：transformed / pattern 完整 MapperHistory 与更复杂 ownership。
4. 扩展 P8：完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期、更复杂多层 LinkSub 链，以及 Part surface 的 `Linearize` / advanced PipeShell wrapper / Filling support-order-constraint / GeomPlate initial/G1/projected/PlateSurface curves / full surface family。

## 全局规则

- 本地 `src/` 是语义来源；不确定行为先读 FreeCAD App / Mod 源码。
- `DocumentObject graph` 是唯一持久源数据。
- adapter 只做协议转换。
- 不按 fixture 名称、几何类型排序或输出端修剪替代 FreeCAD 通用流程。
- 不支持的 `TypeId`、属性、LinkSub、subshape 或构造路径必须返回结构化 diagnostics。

## 验收命令

功能修改优先执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

文档修改至少执行：

```bash
git diff --check
```
