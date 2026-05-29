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
| P5 | Sketcher profile、基础 BSpline、point raw vertex、construction、Coincident、多闭合 wire、ExternalGeometry 子集、基础 `InternalShape` 与 internal element map 已接入 |
| P6 | `NamedShape` / `ElementMap` 主路径、prism / Body boolean maker history、merge history、stable subname 引用更新、split / deleted diagnostics 已接入 |
| P7 | Datum / Origin、RefineModel 子集、Hole 常用孔、Fillet / Chamfer、DressUp cache、Transformed family 基础路径已接入 |
| P8 | Part primitive、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、常用 Part Boolean、基础 Link / LinkSub 单/多 SubList compound / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementCount collapsed / materialized ShowElement child claim / Assembly display 已接入 |

当前 P8 只固定可用于显示、拾取和单次 recompute 的基础能力：导入 shape 仍使用 indexed `NamedShape`；Part Boolean 已消费 boolean / section / generalFuse maker-history 子集；Link display 已保留源对象 alias retag，并支持 `LinkedObject.SubList` 单/多 subshape compound、显式 `ElementList` group、LinkGroup source alias subshape、`ElementCount` 折叠数组和同文档已物化 `ShowElement=true` 子 `LinkElement` 认领，但不等价于完整 FreeCAD Link 账本；Assembly display 不包含 Joint / solver。

## 未完成边界

- Sketcher 完整 solver、BSpline solver/control-point 语义、FaceMakerBuildFace / WireJoiner 账本、复杂 `InternalShape` / `getInternalElementMap()`。
- Topo Naming 完整 MapperHistory、split 旧引用恢复、merge history 收敛、RefineModel partial history 收敛、ShapeFix / transformed / DressUp 的完整 history。
- PartDesign Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp `SupportTransform` ownership、复杂 transformed / pattern ownership。
- Assembly Joint / solver、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、cross-document postfix retag 和多层复杂 LinkSub 链。

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
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已覆盖基础 primitive、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、基础 Link / LinkSub 单/多 SubList compound / LinkGroup source-alias subshape / LinkElement / LinkGroup / ElementCount collapsed / materialized ShowElement child claim / Assembly display 与常用 Part Boolean 子集 |

## 后续队列

1. 补 P6：完整 MapperHistory 生命周期、ShapeFix history、split / merge 旧引用恢复，并把 RefineModel / taper partial history 收敛到正式 MapperHistory。
2. 补 P5：FaceMaker / WireJoiner 状态机、复杂 internal element map、更多 external geometry 和约束。
3. 补 P7：Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp SupportTransform ownership、复杂 transformed / pattern ownership。
4. 扩展 P8：Assembly Joint / solver、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 自动创建 / 删除生命周期、cross-document postfix retag 和多层复杂 LinkSub 链。

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
