# CAD Core 抽取方案

本文是 CAD Core 抽取总纲。目标是从本地 FreeCAD 源码中抽出一个独立、无状态、可重建、可验证的参数化几何内核，而不是复刻 FreeCAD 的 GUI、Workbench 或 Web 服务。

## 架构边界

```text
调用方 / 前端
  保存和编辑完整 DocumentObject graph
  发起 recompute 请求
  消费 mesh、subshape、stable subname 和 diagnostics

CAD Core
  document / graph / runtime / features / geometry / topo
  只根据本次请求中的 DocumentObject graph 计算结果
  不保存跨请求 shape、BREP、mesh、NamedShape 或 ElementMap

Adapters
  CLI / C ABI / 测试 harness / 后续 Web、Worker、WASM 桥接
  只做协议转换，不承载 FreeCAD 业务语义
```

持久数据只有 FreeCAD 风格 `DocumentObject graph`。BREP、`TopoDS_Shape`、mesh、`NamedShape`、`ElementMap`、subshape map 都是一次 recompute 的运行产物，请求结束后丢弃。前后端协议不把 BREP 当成长期状态传递。

CAD Core 不依赖 Qt、`src/Gui`、Workbench、ViewProvider、TaskPanel、Web route、用户会话、数据库或前端缓存。

## 当前实现基线

`cad-core/` 已是独立 C++17 / CMake 工程：

- `cad-core-lib`：核心库。
- `cad-core`：CLI adapter。
- `cad_core_ffi`：薄 C ABI adapter。
- `document/`：解析 `Objects[]`、`Name`、`ID`、`TypeId`、`Properties`，并归一化常用 typed property 和 `PropertyLink*`。
- `graph/`：从 document 层 dependency links 生成 recompute plan，提供缺失链接和循环依赖 diagnostics。
- `runtime/`：提供 `ComputeContext`、diagnostics、feature registry、recompute loop 和单次请求内 shape / named shape 账本。
- `features/`：承接 Sketcher、PartDesign、Datum、DressUp、Transformed family 的 executor。
- `geometry/`：封装 OCCT 构造、placement、mesh、bbox、volume。
- `topo/`：承接 subshape map、`ElementMap`、`NamedShape` 和当前 maker history 子集。

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

## 模块职责

| 模块 | 职责 | FreeCAD 对齐 |
| --- | --- | --- |
| `document/` | 中立输入模型、属性值、链接解析、property diagnostics | `src/App/Document*`、`Property*.cpp`、`PropertyLinks.cpp` |
| `graph/` | dependency edge、拓扑排序、循环诊断、目标选择 | `DocumentObject::getOutList()` / `getInList()` |
| `runtime/` | feature registry、compute context、diagnostics、recompute 调度 | `Document::recompute()`、`DocumentObject::execute()` |
| `features/` | FreeCAD App / Mod 业务语义 executor | `src/Mod/Sketcher/App`、`src/Mod/PartDesign/App` |
| `geometry/` | OCCT 构造、FaceMaker / WireJoiner / ShapeFix 落点、mesh、bbox、volume | `src/Mod/Part/App` |
| `topo/` | `NamedShape`、`ElementMap`、stable subname、MapperHistory、引用恢复 | `PropertyTopoShape*`、`TopoShape*`、`TopoShapeMapper*` |
| `adapters/` | CLI、C ABI、后续 Web / Worker / WASM 协议桥接 | 不承载建模语义 |

依赖方向固定：

```text
adapters -> runtime -> features -> geometry
                    -> graph
                    -> document
features -> topo
geometry -> topo
```

`document/` 不依赖 OCCT，`graph/` 不生成几何，adapter 不修正业务结果。FaceMaker / WireJoiner / ShapeFix 的账本放 `geometry/`，`NamedShape` / `ElementMap` / MapperHistory 放 `topo/`。

## 输入输出模型

输入采用 FreeCAD 风格对象图：

```json
{
  "Objects": [
    {
      "Name": "Body",
      "ID": 1,
      "TypeId": "PartDesign::Body",
      "Properties": {
        "Group": {
          "PropertyType": "App::PropertyLinkList",
          "value": ["Sketch", "Pad"]
        },
        "Tip": {
          "PropertyType": "App::PropertyLink",
          "value": "Pad"
        }
      }
    }
  ],
  "recompute": {
    "objs": ["Body"]
  }
}
```

`Objects[]` 是唯一建模源数据，`recompute.objs` 只选择本次目标。不要新增与 FreeCAD 平行的 `featureType`、`operation`、`params` 数据树。

输出只暴露显示、拾取和诊断需要的运行结果：

```text
objects[name]
  status
  bbox / volume / mesh summary
  feature-specific result fields

subshapes[name]
  FaceN / EdgeN / VertexN / Internal*

named_shapes[name]
  indexed elements
  element_map
  history

diagnostics[]
  level / code / message / object / property / stage / target / subname
```

前端拾取应通过返回的 subshape / stable subname 回写 `PropertyLinkSub` 或 `PropertyLinkSubList`，不得自己按几何顺序猜测拓扑命名。

## 已落地能力

P0-P2 已形成可运行底座：文档解析、diagnostics、registry、Sketch + Body + Pad、FeatureBase / FeatureAddSub / Pocket、Body fuse / cut 主链。

P3a/P3b 已把 shared `FeatureExtrude` 扩到常用终止和方向语义：`Length`、`ThroughAll`、`UpToFace`、单目标 `UpToShape`、`Two sides`、`Symmetric`、`Length2`、第一侧 / 第二侧 UpTo、`UpToFirst`、`UpToLast`、Pad / Pocket taper 几何、custom direction、ReferenceAxis sketch / edge / DatumLine 子集和 Placement。

P4 已把 property / link / placement 收敛到 document 层：常用 typed scalar、Link / LinkList / LinkSub / LinkSubList、Part / Body / Sketch placement、Datum support、graph edge 和结构化 diagnostics。

P5 已接入 Sketcher solver-facing 子集：line / arc / circle / ellipse / 基础 BSpline profile、Sketch point raw vertex、construction geometry 忽略、Coincident 端点合并、多闭合 wire 基础 face-with-holes / island、open wire raw shape 与 profile face 分离、`InternalShape` 的基础导出、`InternalEdgeN` / `InternalVertexN` 外部引用、Datum / Edge / Vertex external geometry 投影，以及 unsupported geometry / constraint diagnostics。

P6 已建立 topo 主路径骨架：`topo/named_shape`、`topo/element_map`、indexed `NamedShape`、identity / source-preserved / history-derived `ElementMap`、merge history 账本、prism maker history 子集、taper `BRepOffsetAPI_ThruSections` generated history 与多侧 / 内环组合透传、`makeElementXorFromSources`、`BRepBuilderAPI_RefineModel` partial history、Body boolean maker history、stable subname 引用更新，以及 split / deleted / unsupported stable subname diagnostics；deleted / split terminal history 已能跨后续 maker 保持诊断语义。

P7 已覆盖常用 PartDesign 生态子集：CoordinateSystem、Origin、Pad standalone refine、Body AddSub final-result refine（Pad / Pocket / Hole）、Fillet / Chamfer / Transformed family replacement refine 的 FreeCAD RefineModel maker 子集、Hole 平底圆柱盲孔 / 通孔 / 点驱动孔 / Tapered / Counterbore / Countersink / Counterdrill / Angled drill point、非建模 Threaded ISO metric / ISO metric fine / UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre（TapDrill 表和 FreeCAD fallback 公式）、thread clearance 的 ISO metric 表 / UTS 表 / 非 ISO fallback、Fillet / Chamfer 基础 DressUp 与基础 `SupportTransform` AddSubShape cache、Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform Features 模式；transformed family 已支持 `Whole shape` 基础路径，LinearPattern / PolarPattern 已支持 Sketch `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN` 基础引用。

P8 已覆盖 FreeCAD `Part::Primitive` 与基础 Box primitive 集合：`Part::Box`、`Part::Cylinder`、`Part::Prism`、`Part::Sphere`、`Part::Ellipsoid`、`Part::Cone`、`Part::Torus`、`Part::Wedge`、`Part::Helix`、`Part::Spiral`、`Part::Vertex`、`Part::Line`、`Part::Ellipse`、`Part::Plane` 和 `Part::RegularPolygon` 走 `features/part.cpp` executor，按 FreeCAD primitive 属性构造 OCCT solid / vertex / edge / face / wire，套用全局 Placement，导出 mesh、subshape map 和 indexed `NamedShape`；Cylinder / Prism 已覆盖 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 基础斜拉方向，Ellipsoid 按 FreeCAD sphere + `BRepBuilderAPI_GTransform` 路径缩放，Torus 按 FreeCAD `TopoShape::makeTorus()` 的圆面旋转路径构造，Helix / Spiral 按 FreeCAD `TopoShape::makeSpiralHelix()` 的 surface-of-revolution + 2D segment 路径构造 wire。P8 也已接入常用 Part Boolean：`Part::Fuse`、`Part::Cut`、`Part::Common`、`Part::Section`、`Part::MultiFuse`、`Part::MultiCommon`、`Part::XOR` / `Part::FeatureXOR`、`Part::BooleanFragments` / `Part::FeatureBooleanFragments` 走 `features/part_boolean.cpp`；Section 读取 `Base` / `Tool` 和 `Approximation`，走 `topo::makeElementSectionFromSources()` 的 maker-history 主路径；XOR 对齐 BOPTools `FeatureXOR`，当前以 typed alias 方式读取 `Objects`，走 `topo::makeElementXorFromSources()` 的 Fuse / Common / Cut 主路径；BooleanFragments 对齐 BOPTools `FeatureBooleanFragments`，当前以 typed alias 方式读取 `Objects`、`Mode=Standard`、`Mode=Split` 的 solid-safe / wire aggregate / CompSolid aggregate 子集、`Mode=CompSolid` 和非负 `Tolerance`，走 `topo::makeElementGeneralFuseFromSources()` 的 generalFuse maker-history 主路径；Split 已迁移 FreeCAD `GeneralFuseResult.makeSplitPieces()` 的 Wire / CompSolid 分支，CompSolid 按 FreeCAD `ShapeMerge.mergeSolids(..., bool_compsolid=True)` 将 solid pieces 按共享面分组为 compound of compsolids。

## 未完成边界

仍未完成的核心边界：

- `FeatureExtrude`：多 face / shell `UpToShape`、非平面终止面、完整 attachment/support/subname 恢复。
- Sketcher：完整约束求解、BSpline solver/control-point 语义、ExternalGeometry face / arc edge / defining profile、完整 FaceMakerBuildFace / WireJoiner 账本和复杂 `getInternalElementMap()`。
- Topo Naming：完整 MapperHistory 消费、split 旧引用恢复、merge history 到完整 MapperHistory 的收敛、ShapeFix / transformed / DressUp 的完整命名传播，以及 RefineModel partial history 向完整 MapperHistory 生命周期收敛。
- PartDesign：Hole ModelThread、标准件表驱动头部尺寸迁移、复杂 Fillet / Chamfer 参数、链式 DressUp `SupportTransform` ownership、复杂 transformed ownership。
- P8：BooleanFragments Shell aggregate Split、文件导入导出、Assembly Link / Joint、Worker / WASM / Web adapter 产品化。

这些缺口必须保持显式 diagnostics 或 `known_gap`，不能用 fixture 特判、输出端修剪、几何类型排序或 source edge 猜测伪装完成。

## 实施顺序

后续推进优先级：

1. P6 topo 主路径补强：完整 MapperHistory 生命周期、split / merge 旧引用恢复、ShapeFix history，以及 RefineModel / taper partial history 到正式 MapperHistory 的收敛。
2. P5 Sketcher 补强：FaceMaker / WireJoiner 账本、复杂 internal element map、更多 external geometry 和约束。
3. P7 PartDesign 补强：Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp SupportTransform ownership、复杂 transformed / pattern ownership。
4. P8 后置能力：继续补 BooleanFragments Shell aggregate Split、导入导出、Assembly、Worker / WASM / Web adapter。

除非前置 topo naming 和 property/link 边界已经覆盖对应引用场景，不应继续扩大高层 executor 的 fixture 特判。

## 验收规则

每个新增能力必须同时满足：

- 以本地 FreeCAD `src/` 的 App / Mod 源码为语义依据。
- 有 executor 或明确 diagnostics。
- 有 fixture 或语义单测覆盖成功路径和关键失败路径。
- 正常 fixture diagnostics 为空，错误 fixture diagnostics code 稳定。
- `PropertyLinkSub`、stable subname、placement、Body Tip、NamedShape / ElementMap 的影响范围明确。
- 文档记录当前边界、FreeCAD 依据、cad-core 落点和剩余缺口。

常用验证：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

文档类修改至少执行：

```bash
git diff --check
```
