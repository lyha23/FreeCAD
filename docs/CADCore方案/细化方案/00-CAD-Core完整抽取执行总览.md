# CAD Core 完整抽取执行总览

本目录是 `../00-CAD-Core抽取方案.md` 的执行拆解。文档只记录当前基线、阶段边界、剩余缺口和验收规则，不记录逐次实现过程。

## 当前基线

`cad-core` 已是独立 C++17 / CMake Core，包含 `cad-core-lib`、CLI adapter 和薄 C ABI adapter。输入是 FreeCAD 风格 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`，输出包含对象结果、mesh summary、bbox、volume、subshape map、`named_shapes` 和 diagnostics。

当前 registry 覆盖：

```text
App::Part
App::Origin
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
- P5：Sketcher profile、point raw vertex、construction、Coincident、ExternalGeometry 子集、raw shape / profile face 分离、基础 `InternalShape` 与 internal element map。
- P6：`NamedShape`、`ElementMap`、prism / Body boolean maker history 子集、stable subname 引用更新、split / deleted diagnostics。
- P7：Datum / Origin、Refine 边界、Hole 基础孔、点驱动孔、Tapered、head-cut / drill-point 轮廓、非建模 Threaded tap-drill、ISO metric clearance fit、Fillet / Chamfer、基础 DressUp `SupportTransform` AddSubShape cache、Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform Features 模式；transformed family 已支持基础 Whole shape，LinearPattern / PolarPattern 已支持基础 Sketch axis 引用。
- P8：已启动 Part primitive 基础子集，`Part::Box` / `Part::Cylinder` 可生成 OCCT solid、应用全局 Placement、导出 mesh / subshape / indexed `NamedShape`；Cylinder 已覆盖 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 基础斜拉方向。

## 未完成边界

- Sketcher 完整 solver、BSpline、FaceMakerBuildFace / WireJoiner 账本、复杂 `InternalShape` / `getInternalElementMap()`。
- Topo Naming 完整 MapperHistory、split / merge 旧引用恢复、taper / Refine / ShapeFix / transformed / DressUp 的完整 history。
- PartDesign Hole ModelThread、完整 thread profile / clearance 表、标准件表驱动头部尺寸迁移、链式 DressUp `SupportTransform` ownership、复杂 transformed / pattern ownership。
- Sphere / Cone / Torus 等剩余 Part primitives、Part Boolean、导入导出、Assembly Link / Joint、Worker / WASM / Web adapter。

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
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 已启动 Part primitive 基础子集 |

## 后续队列

1. 补 P6：MapperHistory、taper history、Refine / ShapeFix history、split / merge 旧引用恢复。
2. 补 P5：FaceMaker / WireJoiner 状态机、复杂 internal element map、更多 external geometry 和约束。
3. 补 P7：Hole ModelThread、完整 thread profile / clearance tables、链式 DressUp SupportTransform ownership、复杂 transformed / pattern ownership。
4. 扩展 P8：Sphere / Cone / Torus、Part Boolean、导入导出、Assembly、Worker / WASM / Web adapter。

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
