# CAD Core 完整抽取执行总览

本目录是 `../00-CAD-Core抽取方案.md` 的执行拆解。文档只记录当前基线、阶段边界、剩余缺口和验收规则，不记录逐次实现过程。

## 当前基线

`cad-core` 已是独立 C++17 / CMake Core，包含 `cad-core-lib`、CLI adapter 和薄 C ABI adapter。输入是 FreeCAD 风格 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`，输出包含对象结果、mesh summary、bbox、volume、subshape map、`named_shapes` 和 diagnostics。

当前 registry 覆盖：

```text
App::Part
App::Origin
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

已经形成的主链：

- P0-P2：document / graph / runtime 底座，Sketch + Body + Pad，FeatureBase / Pocket / Body fuse-cut。
- P3a/P3b：shared `FeatureExtrude` 支持主要长度、终止、双侧、方向、placement 和 taper 几何子集。
- P4：typed property、`PropertyLink*`、placement、GeoFeatureGroup 基础、graph edge 和结构化 diagnostics。
- P5：Sketcher profile、基础 BSpline edge、point raw vertex、construction、Coincident、多闭合 wire 基础 face-with-holes / island、ExternalGeometry 子集、raw shape / profile face 分离、基础 `InternalShape` 与 internal element map。
- P6：`NamedShape`、`ElementMap`、prism / Body boolean maker history 子集、merge history 账本、taper `BRepOffsetAPI_ThruSections` generated history 与多侧 / 内环组合透传、RefineModel partial history、stable subname 引用更新、split / deleted diagnostics；deleted / split terminal history 已能跨后续 maker 保持诊断语义。
- P7：Datum / Origin、Pad standalone refine、Body AddSub final-result refine（Pad / Pocket / Hole）、Fillet / Chamfer / Transformed family replacement refine 的 RefineModel maker 子集、Hole 基础孔、点驱动孔、Tapered、head-cut / drill-point 轮廓、非建模 Threaded ISO metric / ISO metric fine / UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre（TapDrill 表和 FreeCAD fallback 公式）、thread clearance 的 ISO metric 表 / UTS 表 / 非 ISO fallback、Fillet / Chamfer、基础 DressUp `SupportTransform` AddSubShape cache、Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform Features 模式；transformed family 已支持基础 Whole shape，LinearPattern / PolarPattern 已支持基础 Sketch axis 引用。
- P8：已覆盖 FreeCAD `Part::Primitive` 与基础 Box primitive 集合，`Part::Box` / `Part::Cylinder` / `Part::Prism` / `Part::Sphere` / `Part::Ellipsoid` / `Part::Cone` / `Part::Torus` / `Part::Wedge` 可生成 OCCT solid，`Part::Vertex` / `Part::Line` / `Part::Ellipse` / `Part::Plane` / `Part::RegularPolygon` / `Part::Helix` / `Part::Spiral` 可生成 OCCT vertex / edge / face / wire；这些结果应用全局 Placement，并导出 mesh / subshape / indexed `NamedShape`；Cylinder / Prism 已覆盖 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 基础斜拉方向，Ellipsoid 按 FreeCAD sphere + `BRepBuilderAPI_GTransform` 路径缩放，Torus 按 FreeCAD `TopoShape::makeTorus()` 的圆面旋转路径构造，Helix / Spiral 按 FreeCAD `TopoShape::makeSpiralHelix()` 的 surface-of-revolution + 2D segment 路径构造 wire；`Part::Fuse` / `Part::Cut` / `Part::Common` / `Part::Section` / `Part::MultiFuse` / `Part::MultiCommon` 已注册 executor，读取 `Base` / `Tool` 或 `Shapes` 链接，走 OCCT boolean maker history，并接入当前 RefineModel 子集；`Part::Section` 支持 `Approximation` 并输出 section edges compound；`Part::XOR` / `Part::FeatureXOR` 以 BOPTools `FeatureXOR` typed alias 方式接入，读取 `Objects`，走 `makeElementXorFromSources()` 的 Fuse / Common / Cut 主路径；`Part::BooleanFragments` / `Part::FeatureBooleanFragments` 以 BOPTools `FeatureBooleanFragments` typed alias 方式接入，读取 `Objects`、`Mode=Standard`、`Mode=Split` 的 solid-safe / wire aggregate / CompSolid aggregate 子集、`Mode=CompSolid` 和非负 `Tolerance`，走 `makeElementGeneralFuseFromSources()` 的 generalFuse maker-history 主路径；Split 已迁移 FreeCAD `GeneralFuseResult.makeSplitPieces()` 的 Wire / CompSolid 分支，CompSolid 按 FreeCAD `ShapeMerge.mergeSolids(..., bool_compsolid=True)` 将 solid pieces 按共享面分组为 compound of compsolids；`MultiCommon` 默认按 FreeCAD `CommonOfAllShapes` 逐步求交，并保留 `CommonOfFirstAndRest` 兼容行为。

## 未完成边界

- Sketcher 完整 solver、BSpline solver/control-point 语义、FaceMakerBuildFace / WireJoiner 账本、复杂 `InternalShape` / `getInternalElementMap()`。
- Topo Naming 完整 MapperHistory、split 旧引用恢复、merge history 收敛、RefineModel partial history 收敛、ShapeFix / transformed / DressUp 的完整 history。
- PartDesign Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp `SupportTransform` ownership、复杂 transformed / pattern ownership。
- BooleanFragments Shell aggregate Split、导入导出、Assembly Link / Joint、Worker / WASM / Web adapter。

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
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已覆盖基础 primitive 与常用 Part Boolean 子集 |

## 后续队列

1. 补 P6：完整 MapperHistory 生命周期、ShapeFix history、split / merge 旧引用恢复，并把 RefineModel / taper partial history 收敛到正式 MapperHistory。
2. 补 P5：FaceMaker / WireJoiner 状态机、复杂 internal element map、更多 external geometry 和约束。
3. 补 P7：Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp SupportTransform ownership、复杂 transformed / pattern ownership。
4. 扩展 P8：BooleanFragments Shell aggregate Split、导入导出、Assembly、Worker / WASM / Web adapter。

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
