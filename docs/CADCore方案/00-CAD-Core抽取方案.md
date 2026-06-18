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
- `geometry/`：封装 OCCT 构造、placement、mesh、bbox、volume 和请求内 shape 的文件导出。
- `topo/`：承接 subshape map、`ElementMap`、`NamedShape` 和当前 maker history 子集。

当前 registry 覆盖：

```text
App::Part
App::Link
App::LinkElement
App::LinkGroup
App::DocumentObjectGroup
App::DocumentObjectGroupPython
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
Part::Extrusion
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
| `geometry/` | OCCT 构造、FaceMaker / WireJoiner / ShapeFix 落点、mesh、bbox、volume、BREP / STEP / STL 文件写出 | `src/Mod/Part/App` |
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

## 阶段状态

| 阶段 | 当前实现 | 仍需补强 |
| --- | --- | --- |
| P0-P2 | 文档解析、diagnostics、registry、Sketch / Body / Pad、FeatureBase / FeatureAddSub / Pocket、Body fuse / cut 主链可运行 | 保持底座稳定，不把后续语义塞回 adapter 或 graph |
| P3a/P3b | shared `FeatureExtrude` 支持常用长度、UpTo、双侧、方向、placement、Pad / Pocket taper 几何子集 | 多 face / shell UpTo、非平面终止、完整 attachment / support / subname 恢复 |
| P4 | typed property、`PropertyLink*`、placement、Datum support、graph edge 和结构化 diagnostics 已进入 document / runtime 主路径 | Sketch 专有属性、复杂 GeoFeatureGroup、Origin / AttachEngine 完整坐标传递 |
| P5 | Sketcher solver-facing 子集、open/raw shape 与 profile face 分离、基础 `InternalShape` / internal element、ExternalGeometry 子集 | 完整 `ExternalGeometryExtension` 状态机、FaceMakerBuildFace / WireJoiner 账本、复杂 `getInternalElementMap()` |
| P6 | `NamedShape`、`ElementMap`、prism / Body boolean maker history、merge history、stable subname 引用更新、split / deleted diagnostics 已成主路径骨架 | 完整 MapperHistory 生命周期、split 旧引用恢复、ExternalGeometry 引用恢复链路、ShapeFix / Refine / transformed / DressUp history 收敛 |
| P7 | Datum / Origin、RefineModel 子集、Hole 常用孔、Fillet / Chamfer、DressUp cache、Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 基础路径 | Hole ModelThread、标准件表驱动头部尺寸、链式 SupportTransform ownership、复杂 transformed / pattern ownership |
| P8 | Part primitives、BREP / STEP / IGES / STL 导入、BREP / STEP / STL CLI 导出、常用 Part Boolean、基础 Link / LinkSub / LinkGroup / LinkElement display、ElementCount 折叠数组、ShowElement 请求内子元素合成与 `documentObjectUpdates` 建议、`PropertyXLink*` / `FullSubList` / mapped postfix alias、`App::DocumentObjectGroup` plain group 展开、Assembly display、Joint 输入元数据、硬依赖 real Ondsel adapter 与 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local placement writeback | 完整 Joint placement / constraint、剩余 JointType、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、完整 FreeCAD Link 账本与持久写回事务、完整 cross-document 文档哈希 / postfix 生命周期 |

P8 当前实现边界：

- Part primitive：Box、Cylinder、Prism、Sphere、Ellipsoid、Cone、Torus、Wedge、Helix、Spiral、Vertex、Line、Ellipse、Plane、RegularPolygon 落在 `features/part.cpp`，输出请求内 OCCT shape、mesh、subshape map 和 indexed `NamedShape`。
- Import / Export：`Part::ImportBrep`、`Part::ImportStep`、`Part::ImportIges`、`Mesh::Import` STL 子集可读入请求内 shape；CLI 可把已计算对象写出为 BREP / STEP / STL，导出是 adapter 文件副作用，不进入 graph 或默认 JSON 状态。
- Part Boolean：Fuse、Cut、Common、Section、MultiFuse、MultiCommon、XOR、BooleanFragments 落在 `features/part_boolean.cpp`，消费 `topo` maker-history 子集；BooleanFragments 覆盖 Standard、solid-safe Split、wire / shell / CompSolid aggregate Split 和 CompSolid mode。
- Link / Assembly display：`features/link.cpp` 支持 `App::Link`、`App::LinkElement`、`App::LinkGroup`、`App::DocumentObjectGroup`、`Assembly::AssemblyLink`、`Assembly::AssemblyObject`、`Assembly::JointGroup` 和 Assembly `App::FeaturePython` Joint / GroundedJoint 输入元数据。已支持 `LinkedObject`、`LinkTransform`、`LinkPlacement` / `Placement`、基础 `Scale` / `ScaleVector`、`LinkedObject.SubList` 单 subshape 与多 subshape compound、LinkSub 对象名 / `$Label` 前缀路由、LinkGroup source alias subshape、显式 `ElementList` group、`ElementCount` 折叠数组、ShowElement 已物化子元素认领、缺失子元素请求内合成、owner / child sync、toggle-off 收回与删除建议、`PlacementList` / `ScaleList` / `VisibilityList`、`PropertyXLink*`、`FullSubList`、mapped postfix alias、源对象 alias retag、Link retag 后 terminal / merge history 传播，以及 plain group children 请求内 display 展开。`documentObjectUpdates` 只作为前端更新 `DocumentObject graph` 的建议返回，CAD Core 不在后端持久化或隐式改写请求 graph。
- Link / Assembly 未完成：完整 FreeCAD Link 账本、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期、更复杂多层 LinkSub 链、完整 Joint placement / constraint 与剩余 JointType 尚未迁移。

## 未完成边界

仍未完成的核心边界：

- `FeatureExtrude`：多 face / shell `UpToShape`、非平面终止面、完整 attachment/support/subname 恢复。
- Sketcher：完整约束求解、BSpline solver/control-point 语义、`ExternalGeometryExtension` 的 Defining / Frozen / Detached / Missing / Sync 状态机、ExternalGeometry face / arc edge / defining profile、完整 FaceMakerBuildFace / WireJoiner history 消费和复杂 `getInternalElementMap()`。
- Topo Naming：完整 MapperHistory 消费、split 旧引用恢复、merge history 到完整 MapperHistory 的收敛、ExternalGeometry 旧引用恢复链路、ShapeFix / transformed / DressUp 的完整命名传播，以及 RefineModel partial history 向完整 MapperHistory 生命周期收敛。
- PartDesign：Hole ModelThread、标准件表驱动头部尺寸迁移、复杂 Fillet / Chamfer 参数、链式 DressUp `SupportTransform` ownership、复杂 transformed ownership。
- P8：Assembly solver 剩余 JointType 与完整 Joint placement / constraint、Worker / WASM / Web adapter 产品化、导入 shape 完整 ElementMap、完整 FreeCAD Link 账本、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

这些缺口必须保持显式 diagnostics 或 `known_gap`，不能用 fixture 特判、输出端修剪、几何类型排序或 source edge 猜测伪装完成。

## 下一阶段重点

P5 Sketcher 外部几何与 P6 topo naming 联合主线已收口：完整 `ExternalGeometryExtension` 状态机、完整 MapperHistory、FaceMaker / WireJoiner history 消费和复杂旧引用恢复已按主线落地。细化方案见 `细化方案/13-【已实现】ExternalGeometry-TopoNaming下一阶段主线.md`。

这条主线的正确顺序是先补 `topo` 中的 MapperHistory / ElementMap 生命周期，再让 `geometry` 中的 FaceMaker / WireJoiner / ShapeFix 产出可消费 history，然后在 `runtime` / `document` 中统一旧引用解析与 `elementReferenceUpdates`，最后由 `features/sketch_object.*` 表达 FreeCAD `SketchObjectExternal` 的外部几何状态机。不得把 ExternalGeometry 恢复、InternalShape 命名或 split fragment 归属继续放在 sketch executor、adapter 或输出层靠几何猜测完成。

## 实施顺序

后续推进优先级：

1. P5/P6 联合主线：已按 `细化方案/13-【已实现】ExternalGeometry-TopoNaming下一阶段主线.md` 收口完整 MapperHistory、FaceMaker / WireJoiner history、ExternalGeometryExtension 状态机和复杂引用恢复；后续只在阶段发布时复跑验收和回写台账。
2. P6 余量收敛：ShapeFix history、RefineModel / taper partial history 到正式 MapperHistory 的收敛，以及 transformed / DressUp history 的完整传播。
3. P7 PartDesign 补强：Hole ModelThread、标准件表驱动头部尺寸迁移、链式 DressUp SupportTransform ownership、复杂 transformed / pattern ownership。
4. P8 后置能力：继续补 Assembly solver 剩余 JointType 与完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、完整 FreeCAD Link 账本、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期和更复杂多层 LinkSub 链。

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
