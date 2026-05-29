# CAD Core 抽取方案

本文是 CAD Core 抽取总纲。目标不是做一个只会跑 `Sketch + Pad` 的 MVP，而是从本地 FreeCAD 源码树中抽出一个可独立运行、可重建、可验证、可被任意外壳调用的参数化几何内核。

CAD Core 的长期边界是：

```text
CAD Core
  DocumentObject graph / Properties / links / dependency graph
  recompute runtime / feature executors / OCCT geometry
  NamedShape / ElementMap / topo naming / result mapping
  mesh / subshape / stable subname / diagnostics export

Adapters
  CLI / C ABI / test harness / optional Web / optional Worker / optional WASM host

UI 或调用方
  Chili3D 前端 / 自动化脚本 / 测试工具 / 桌面壳 / 远程服务
```

CAD Core 不依赖 Qt、`src/Gui`、Workbench、ViewProvider、TaskPanel、Web route、session、数据库或前端状态。它只接收 FreeCAD 风格的 `DocumentObject graph` 和本次 recompute 请求，计算后返回前端显示、拾取和诊断所需结果。

## 总目标

完整 CAD Core 指的是把 FreeCAD 参数化建模链路中和几何重建直接相关的 App / Mod 语义抽出来，而不是把 FreeCAD 整个应用搬出来。

必须抽出的能力：

- 文档对象模型：`Document`、`DocumentObject`、对象名、`TypeId`、属性容器、状态、错误。
- 属性系统：数值、枚举、布尔、长度、角度、向量、矩阵、Placement、Link、LinkSub、LinkList、LinkSubList。
- 链接和引用：对象引用、子元素引用、引用更新、循环引用诊断。
- 依赖分析：由属性链接生成依赖图，按目标对象生成 recompute plan。
- recompute runtime：按 FreeCAD 风格调用 executor，管理单次请求内的 shape、NamedShape、ElementMap 和 diagnostics。
- Sketcher 核心：草图几何、约束输入、profile 构造、外部引用、`InternalShape`、内部元素映射。
- PartDesign 主链：Body、BaseFeature、FeatureBase、FeatureAddSub、FeatureExtrude、Pad、Pocket。
- PartDesign 常用生态：Datum、Hole、DressUp、Fillet、Chamfer、Refine、Pattern、Mirror、MultiTransform、Scaled。
- Part / OCCT 几何层：TopoShape、FaceMaker、WireJoiner、ShapeFix、布尔、拉伸、变换、mesh、bbox、volume。
- 拓扑命名：`NamedShape`、`ElementMap`、stable subname、旧引用恢复、MapperHistory、内部面边点命名。
- 结果导出：mesh、subshape map、完整 subname、stable subname、拾取索引、diagnostics。
- oracle 和 fixture：用 FreeCAD 当前行为固化 expected，形成分阶段验收集。

不抽进 CAD Core：

- Qt event loop、`src/Gui`、ViewProvider、TaskPanel、Workbench UI。
- Selection、TreeView、PropertyEditor、命令面板、资源浏览器。
- Web route、JWT、用户、权限、数据库、上传目录、业务会话。
- 跨请求共享的裸 `TopoDS_Shape`、BREP、mesh 或 GUI cache。
- 只服务某个前端交互的临时字段、fixture 名称特判或输出端修补规则。

`DocumentObject graph` 是唯一可持久保存的建模源数据。BREP、shape、mesh、NamedShape、ElementMap、subshape map 都是一次 recompute 的运行产物，请求结束后应丢弃。前后端协议默认不传 BREP；如果后续需要 BREP，只能作为离线调试或文件导入导出的 adapter 能力，不进入持久文档模型。

## 当前基线

当前 `/cad-core` 已经是独立 C++17 / CMake 工程，不再是 Python 原型：

- `cad-core-lib`：核心库。
- `cad-core`：CLI adapter。
- `cad_core_ffi`：薄 C ABI adapter。
- `document/`：已能解析 FreeCAD 风格 `Objects[]`、`Name`、`ID`、`TypeId`、`Properties`。
- `graph/`：已有最小 recompute plan。
- `runtime/`：已有 diagnostics、feature registry、compute context、recompute loop。
- `features/`：已有 `Sketcher::SketchObject`、`PartDesign::Body`、`PartDesign::FeatureBase`、`PartDesign::Pad`、`PartDesign::Pocket` 和 shared `FeatureExtrude` 最小链。
- `geometry/`：已真实调用 OCCT 生成 wire / face / prism，并导出 mesh、bbox、volume。
- `topo/`：已有最小 subshape map。

已跑通的基线链路：

```text
Objects[]
  -> Sketcher::SketchObject
  -> PartDesign::Pad
  -> PartDesign::FeatureBase
  -> PartDesign::Pocket
  -> PartDesign::Body
  -> OCCT mesh / bbox / volume / subshape map / diagnostics
```

这个基线证明方向正确，但还不是完整 CAD Core。主要缺口是：

- `PropertyLink` / `PropertyLinkList` / `PropertyLinkSubList` 还没有完整归一化成统一 graph edge 和 subname 引用。
- `FeatureExtrude` 仍以 `Length` 最小路径为主，`ThroughAll`、`UpToFace`、`UpToShape`、双侧、对称、taper、custom direction、attachment/support 尚未完整。
- Body 生态还没有完整覆盖 transformed family、Hole、DressUp、Refine、Datum。
- `NamedShape`、`ElementMap`、MapperHistory、旧引用恢复还没有成为正式主路径。
- Sketcher 还没有完整迁移外部几何、约束求解、内部面边点映射和 solver-facing 状态。
- FaceMaker、WireJoiner、ShapeFix 等内部账本还没有形成和 FreeCAD 对齐的通用模块。

## 分层架构

CAD Core 内部按 FreeCAD App / Mod 语义分层，不按通用 Web 后端分层重命名：

```text
cad-core/
  include/cad_core/
    document/
    graph/
    runtime/
    features/
    geometry/
    topo/
    adapters/

  src/
    document/
    graph/
    runtime/
    features/
    geometry/
    topo/
    adapters/
```

模块职责：

| 模块 | 职责 | FreeCAD 对齐 |
| --- | --- | --- |
| `document/` | 中立 `Document`、`DocumentObject`、属性值、链接属性、JSON 解析 | `src/App/Document*`、`Property*.cpp` |
| `graph/` | 依赖图、拓扑排序、循环诊断、目标选择 | `DocumentObject::getOutList()` / `getInList()` |
| `runtime/` | `ComputeContext`、diagnostics、feature registry、recompute loop | `Document::recompute()`、`DocumentObject::execute()` |
| `features/` | Sketcher / PartDesign executor 和 FreeCAD 同名业务语义 | `src/Mod/Sketcher/App`、`src/Mod/PartDesign/App` |
| `geometry/` | OCCT shape 构造、FaceMaker、WireJoiner、ShapeFix、mesh、bbox、volume | `src/Mod/Part/App` |
| `topo/` | `NamedShape`、`ElementMap`、stable subname、MapperHistory、引用更新 | `PropertyTopoShape*`、`TopoShape*`、`TopoShapeMapper*` |
| `adapters/` | CLI、C ABI、测试协议、可选 Web/WASM/Worker 桥接 | 只做协议转换 |

依赖方向固定：

```text
adapters -> runtime -> features -> geometry
                    -> graph
                    -> document
features -> topo
geometry -> topo

document 不依赖 OCCT
graph 不生成几何
topo 不依赖 adapter
CAD Core 不反向依赖外壳
```

`features/*` 可以表达 FreeCAD 业务调用顺序，但不能把几何内核推理、split history 合成、face 排序或 topo naming 输出修正塞进单个 executor。FaceMaker / WireJoiner / ShapeFix 的账本放 `geometry/`；NamedShape / ElementMap / MapperHistory 放 `topo/`。

## 输入输出模型

持久输入只保存 FreeCAD 风格 `DocumentObject graph`：

```text
Document
  Objects[]
    Name
    ID
    TypeId
    Properties
      FreeCAD 同名属性
      PropertyLink* 属性形成 graph edge

runtime request
  recompute.objs[]
  export options
```

不新增并行 `edges`、`featureType`、`operation`、`params`。对象语义由 `TypeId` 和 FreeCAD 同名 `Properties` 决定。

示例：

```json
{
  "Objects": [
    {
      "Name": "Body",
      "ID": 20,
      "TypeId": "PartDesign::Body",
      "Properties": {
        "Group": [
          {
            "PropertyType": "App::PropertyLinkSub",
            "value": "Sketch",
            "SubList": []
          },
          {
            "PropertyType": "App::PropertyLinkSub",
            "value": "Pad",
            "SubList": []
          }
        ],
        "Tip": {
          "PropertyType": "App::PropertyLinkSub",
          "value": "Pad",
          "SubList": []
        }
      }
    },
    {
      "Name": "Sketch",
      "ID": 30,
      "TypeId": "Sketcher::SketchObject",
      "Properties": {
        "Placement": {
          "PropertyType": "App::PropertyPlacement",
          "Base": [0, 0, 0],
          "Rotation": [0, 0, 0, 1]
        },
        "Geometry": [],
        "Constraints": []
      }
    },
    {
      "Name": "Pad",
      "ID": 31,
      "TypeId": "PartDesign::Pad",
      "Properties": {
        "Profile": {
          "PropertyType": "App::PropertyLinkSub",
          "value": "Sketch",
          "SubList": []
        },
        "SideType": "One side",
        "Type": "Length",
        "Length": 10.0,
        "Reversed": false
      }
    }
  ],
  "recompute": {
    "objs": ["Body"]
  }
}
```

`Objects[]` 是建模源数据；`recompute.objs` 只是本次 API 的计算目标。`Placement`、`Label`、`Visibility` 等信息如果要保存，也放在 `Properties` 中，属性名沿用 FreeCAD。

返回结果只暴露运行产物：

```text
Result
  objects[]
    name
    typeId
    status
    mesh
    bbox
    volume
    subshapes[]
      id
      subname
      stableSubname
      kind
      source
  diagnostics[]
```

前端拾取应从返回的 mesh triangle / face id / subshape 表回到 `PropertyLinkSub`，不要自己推断拓扑命名。

## FreeCAD 语义来源

迁移只以本仓库 `src/` 的 App / Mod 层为语义依据，`Gui` 只用于理解交互入口。

| 能力 | FreeCAD 源码位置 | 迁移内容 |
| --- | --- | --- |
| 文档对象创建和重算 | `src/App/Document.cpp`、`src/App/DocumentObject.cpp` | 对象注册、touch、mustExecute、execute、错误传播。 |
| 属性和链接 | `src/App/Property*.cpp`、`src/App/PropertyLinks.cpp` | 属性类型、链接目标、子元素引用、引用更新。 |
| Placement 和坐标系 | `src/App/GeoFeature.cpp`、`src/App/GeoFeatureGroupExtension.cpp` | 局部 / 全局 Placement、容器坐标传递。 |
| Part 形状封装 | `src/Mod/Part/App/PartFeature.cpp`、`BodyBase.cpp` | Shape 属性、几何对象基类、结果映射。 |
| TopoShape 和拓扑命名 | `src/Mod/Part/App/PropertyTopoShape.cpp`、`TopoShape.cpp`、`TopoShapeExpansion.cpp`、`TopoShapeMapper.cpp` | `NamedShape`、ElementMap、MapperHistory、稳定命名。 |
| FaceMaker / WireJoiner | `src/Mod/Part/App/FaceMaker*.cpp`、`WireJoiner.cpp` | 内部面、open wire、split history、构面后处理。 |
| Sketcher | `src/Mod/Sketcher/App/SketchObject*.cpp`、`Sketch.cpp` | 草图执行、几何、约束、外部引用、`InternalShape`。 |
| Body | `src/Mod/PartDesign/App/Body.cpp` | Group、Tip、BaseFeature、Body 内顺序、最终 Shape。 |
| FeatureBase | `src/Mod/PartDesign/App/FeatureBase.cpp` | 前序 solid、Body 内放置、suppressed / base feature 语义。 |
| FeatureAddSub | `src/Mod/PartDesign/App/FeatureAddSub.cpp` | additive / subtractive 通道、Body Fuse / Cut。 |
| FeatureExtrude | `src/Mod/PartDesign/App/FeatureExtrude.cpp` | 方向、长度、双侧、终止方式、UpTo*、taper。 |
| Pad / Pocket | `src/Mod/PartDesign/App/FeaturePad.cpp`、`FeaturePocket.cpp` | Pad 加料、Pocket 减料，共享拉伸主逻辑。 |
| DressUp / Hole / Refine | `FeatureDressUp.cpp`、`FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureHole.cpp`、`FeatureRefine.cpp` | 子元素引用、孔、倒角、圆角、refine 策略。 |
| Transformed family | `FeatureTransformed.cpp` 及 Pattern / Mirror / MultiTransform 子类 | 源特征、变换矩阵、阵列结果、历史映射。 |
| Assembly 后续 | `src/Mod/Assembly/App` | Link、Joint、装配依赖和求解，后置阶段。 |

新增 public API、核心语义类型、executor、mapper/history 规则时，相邻 C++ 注释必须写明 FreeCAD 源文件、类/函数和支撑字段或短句。

## Recompute 执行模型

一次 recompute 固定为无状态流程：

```text
input document graph
  -> parse Objects[].Properties
  -> normalize PropertyLink*
  -> validate TypeId and required properties
  -> build dependency graph
  -> build recompute plan
  -> create ComputeContext
  -> execute feature executors
  -> record NamedShape / ElementMap / history
  -> update element references
  -> export mesh / subshape / stable subname / diagnostics
  -> drop ComputeContext
```

`ComputeContext` 只保存本次请求内的数据：

- object name -> raw shape。
- object name -> NamedShape。
- object name -> ElementMap / mapper history。
- object name -> mesh / bbox / volume / subshape map。
- diagnostics。

`ComputeContext` 不保存：

- 用户、会话、权限。
- UI selection、ViewProvider、TreeView 状态。
- 下次请求还要继续用的裸 `TopoDS_Shape`。
- 持久 BREP、mesh、NamedShape 或 ElementMap。

如果未来要做缓存，必须先引入 document revision、object revision、property hash 和 topo naming 失效规则。缓存不能改变“请求输入是唯一真实数据”的边界。

## FeatureRegistry

CAD Core 使用明确白名单：

```text
TypeId -> FeatureExecutor
```

规则：

- 支持的 `TypeId` 必须注册明确 executor。
- 不支持的 `TypeId` 必须返回 diagnostics。
- 未知类型不得降级成空 shape。
- 执行失败不得静默跳过并生成假成功结果。
- 每新增一个 executor，必须补对应 fixture 或语义单测。

当前已注册或已有基线：

```text
Sketcher::SketchObject
PartDesign::Body
PartDesign::FeatureBase
PartDesign::Pad
PartDesign::Pocket
```

完整抽取过程中需要逐步进入 registry 的类型：

```text
PartDesign::FeatureAddSub
PartDesign::Hole
PartDesign::Fillet
PartDesign::Chamfer
PartDesign::LinearPattern
PartDesign::PolarPattern
PartDesign::Mirrored
PartDesign::MultiTransform
PartDesign::Scaled
PartDesign::Plane
PartDesign::Line
PartDesign::Point
PartDesign::CoordinateSystem
Part::Feature
Part::DatumPlane
Part::DatumLine
Part::DatumPoint
App::Part
App::Origin
App::Plane
App::Line
App::Point
App::LocalCoordinateSystem
```

Assembly、Link、Joint 和更广泛 workbench 作为后续阶段，不阻塞 CAD Core 第一轮完整 PartDesign 抽取。

## 阶段路线

本节是总路线；可执行拆解见 `docs/CADCore方案/细化方案`：

| 阶段 | 细化文档 |
| --- | --- |
| 总览 | `细化方案/00-CAD-Core完整抽取执行总览.md` |
| P0 | `细化方案/01-P0-Core壳.md` |
| P1 | `细化方案/02-P1-Sketch-Body-Pad闭环.md` |
| 接口与验收 | `细化方案/03-接口与验收样例.md` |
| P2 | `细化方案/04-P2-FeatureBase-FeatureAddSub-Pocket.md` |
| P3a | `细化方案/05-P3a-FeatureExtrude-UpTo终止语义.md` |
| P3b | `细化方案/06-P3b-FeatureExtrude双侧方向与Placement.md` |
| P4 | `细化方案/07-P4-Document-Property-Placement完整化.md` |
| P5 | `细化方案/08-P5-Sketcher核心与内部元素.md` |
| P6 | `细化方案/09-P6-TopoNaming主路径.md` |
| P7 | `细化方案/10-P7-PartDesign常用生态.md` |
| P8 | `细化方案/11-P8-Part导入导出与Assembly后续.md` |

### P0：Core 壳

状态：已落地。

目标：在无 Qt、无 Web 的情况下加载文档、识别对象、构造空 recompute 链路并输出 diagnostics。

交付：

- FreeCAD 风格 `Objects[]` / `DocumentObject` / `Properties`。
- `Diagnostics`。
- `FeatureRegistry`。
- `recompute(document, objs)` 空链路。
- CLI 输入输出。

### P1：Sketch + Body + Pad 闭环

状态：已落地。

目标：跑通第一条真实 CAD 建模链。

交付：

- `Sketcher::SketchObject` 最小 profile。
- `PartDesign::Body` 顺序和 Tip。
- `PartDesign::Pad` 长度拉伸。
- mesh、bbox、volume、subshape map。

### P2：PartDesign 主链

状态：已有最小基线。

目标：让 Body 的加料 / 减料通道成立，避免 Pad / Pocket 各写一套几何。

交付：

- `FeatureBase` / `BaseFeature`。
- `FeatureAddSub` 的 `add_shape` / `sub_shape` 双通道。
- shared `FeatureExtrude`。
- `Pocket Type=Length`。
- Body 顺序 Fuse / Cut。

冻结规则：

- Pad / Pocket 继续共享 `FeatureExtrude`。
- Body 负责最终组合，单个 feature 不直接伪造最终 Body。
- 不把裸 shape 或 BREP 写入持久 JSON。

### P3：FeatureExtrude 完整终止语义

状态：下一条主线。

目标：把 shared `FeatureExtrude` 从 `Length only` 扩到 FreeCAD 风格终止语义，服务 Pad / Pocket / 后续 transformed family。

交付：

- `ThroughAll`。
- `UpToFace`。
- `UpToShape`。
- `FaceN` / stable subname 解析到本次 `ComputeContext` 内目标 shape。
- `Two sides`。
- `Symmetric`。
- `Length2` / `Type2`。
- taper / taper angle。
- custom direction。
- reverse / midplane / offset 组合。
- Pad / Pocket 全部继续走 shared `FeatureExtrude`。

验收：

- P0 / P1 / P2 fixture 不回退。
- Pad / Pocket UpTo fixtures 具备 FreeCAD oracle。
- 不用 bbox 替代 `UpToFace` 的真实目标面。
- 失败路径能指出对象、属性和引用值。

### P4：Document / Property / Placement 完整化

目标：把 executor 对临时 JSON 形态的依赖收敛到统一属性模型，让后续特征只面对 FreeCAD 语义。

交付：

- `PropertyLink`、`PropertyLinkList`、`PropertyLinkSub`、`PropertyLinkSubList` 统一解析。
- LinkSub 的 object name、subname、完整 subname、stable subname 表达。
- `PropertyPlacement`、`PropertyVector`、`PropertyAngle`、`PropertyLength`、`PropertyEnumeration` 等常用属性类型。
- Body / Part / Origin / Datum 的坐标系传递。
- missing link、bad subname、cycle、unsupported property 的结构化 diagnostics。

验收：

- graph edge 全部来自属性系统，不再由 executor 重新猜输入对象。
- Body、FeatureBase、DressUp、Pattern 能读取同一套 LinkSub 数据结构。

### P5：Sketcher 核心和内部元素

目标：让草图不只是简单闭合 profile，而能承载 FreeCAD 风格草图引用和内部元素。

交付：

- Sketch geometry 类型矩阵：line、arc、circle、ellipse、bspline、construction geometry。
- Constraints 输入模型和最小 solver-facing 状态。
- 外部几何引用。
- `SketchObject::buildShape()` / `buildInternals()` 对齐。
- `InternalShape`、`InternalFaceN`、`InternalEdgeN`、`InternalVertexN`。
- `getInternalElementMap()` 等价映射。
- open wire / closed wire / 多 profile / internal face 的 diagnostics。

验收：

- 不从 fixture 输出倒推几何排序。
- FaceMaker / WireJoiner 的几何结果和历史映射由 `geometry/` / `topo/` 承担。
- open profile 与 `InternalShape` 为空的语义要和 FreeCAD 一致，不把原始 sketch 边误判为丢失。

### P6：Topo Naming 主路径

目标：把稳定引用从导出补丁升级为 CAD Core 的正式账本。

交付：

- `NamedShape`。
- `ElementMap`。
- `MapperHistory`。
- `TopoShape::makeElement*` 等命名传播 helper。
- face / edge / vertex 的 source trace。
- split / merge / generated / modified / deleted 历史。
- `PropertyLinkSub` / `PropertyLinkSubList` 引用更新。
- InternalShape 命名和外部引用恢复。

验收：

- 修改 Pad 长度后，Fillet / Chamfer / Sketch external reference 不无声丢失。
- source edge 一对多 fragment 映射有稳定历史。
- 如果几何等价但 `InternalFaceN` 顺序不同，归类为命名顺序差异，不算硬失败。
- face/edge/vertex 数量、几何内容、稳定 subname 丢失仍算失败。

### P7：PartDesign 常用生态

目标：覆盖前端参数化建模最常用的 Body 特征。

交付顺序：

```text
Datum Plane / Line / Point / CoordinateSystem
  -> Refine
  -> Hole
  -> Fillet / Chamfer
  -> LinearPattern / PolarPattern / Mirrored
  -> MultiTransform / Scaled
```

规则：

- DressUp 特征必须通过 LinkSub 读取 base edge / face。
- Transformed family 必须复用 source feature 的 NamedShape / ElementMap 历史。
- Refine 不得长期停留在输出端 fallback；要迁移 FreeCAD maker / object chain。
- 每个 feature 至少有一个成功 fixture、一个错误 fixture、一个引用稳定性验收项。

### P8：Part / 导入导出 / Assembly 后续

目标：在 PartDesign 主体稳定后，扩到更宽的 CAD Core 能力。

候选范围：

- Part primitives 和 Boolean。
- Shape import/export adapter。
- 更完整的 `Part::Feature`。
- Assembly Link / Joint / recompute 关系。
- Worker / WASM / Web adapter 产品化。

规则：

- 这些能力不能倒逼 Core 破坏无状态边界。
- 文件导入导出可以作为 adapter 或 geometry service，但持久建模源仍是 `DocumentObject graph`。
- Assembly 进入前，Document / Link / topo naming 必须先稳定。

## 验收体系

完整抽取不能只靠“当前 fixture 通过”。需要四层验收：

### 1. Core 单元测试

- 属性解析和归一化。
- graph edge 和 cycle diagnostics。
- recompute plan。
- diagnostics code。
- stable subname parser。
- ElementMap / MapperHistory helper。

### 2. Fixture parity

每个 fixture 都应包含：

- 输入 document graph。
- FreeCAD oracle 输出。
- cad-core 输出。
- 成功 / 失败状态。
- mesh / bbox / volume / subshape 数量。
- diagnostics code。
- topo naming 差异分类。

fixture 分层：

```text
fixtures/mvp   P0/P1/P2 frozen baseline
fixtures/p3    FeatureExtrude / Placement / PropertyLink
fixtures/p4    Sketch internals / topo naming
fixtures/p5    DressUp / Hole / Pattern / Refine
fixtures/p6    Assembly / broad Part support
```

### 3. FreeCAD oracle

涉及语义不确定时，先读 FreeCAD 源码，再用本机 `FreeCADCmd` 采集 oracle。Codex sandbox 内如果 Qt/FreeCAD 进程报处理器或 Qt feature 错误，只说明 sandbox 不适合启动该进程，不代表本机 FreeCAD 不可用。

oracle 更新规则：

- 只有证明采集脚本或 expected 错了，才改 expected。
- 不能按 cad-core 当前输出倒推 expected。
- 命名顺序差异和几何差异必须分开记录。

### 4. Adapter 一致性

同一份 input document 通过 CLI、C ABI、测试 harness 或未来 Web adapter 调用，结果结构应一致。adapter 只允许做协议转换，不允许补业务逻辑。

常用验证命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

如果 `build/` 不存在：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake -S . -B build
cmake --build build
python3 -m unittest tests/test_mvp.py
```

## 当前优先队列

现在不应该先做 Web / WASM / Worker 产品化，也不应该先铺开 Pattern / Mirror。优先队列是：

1. 冻结 P0 / P1 / P2 回归面，确保现有 `rect-pad`、`rect-pad-pocket` 和 diagnostics fixtures 稳定。
2. 补 `PropertyLink*` 归一化，让 graph / executor 共享同一套链接语义。
3. 做 P3 `FeatureExtrude` 终止语义：`ThroughAll -> UpToFace -> UpToShape`。
4. 补 Pad / Pocket UpTo oracle 和 fixtures。
5. 迁移 Placement / Body 局部坐标 / custom direction。
6. 把 `NamedShape` / `ElementMap` / MapperHistory 提升为主路径。
7. 再进入 Sketcher internals、Datum、Refine、Hole、Fillet、Chamfer。
8. 最后扩 transformed family、Assembly 和外壳 adapter 产品化。

这个顺序的原因是：Pattern / Mirror / Fillet / Chamfer 都依赖已有特征的终止语义、LinkSub、subshape 稳定引用和历史映射。如果这些底座不先补齐，后续 feature 会复制不完整逻辑，最终返工。

## 完整完成定义

第一轮完整 CAD Core 抽取完成，应同时满足：

- CAD Core 可以在没有 Qt、GUI、Workbench、Web 框架的环境下独立构建和运行。
- 输入模型是 FreeCAD 风格 `DocumentObject graph`，而不是前端临时状态树。
- `DocumentObject graph` 是唯一持久源数据，shape / BREP / mesh / NamedShape / ElementMap 不跨请求持久保存。
- property/link/placement/graph/recompute 形成通用底座，executor 不再各自解析临时 JSON。
- Sketcher、PartDesign Body 主链和常用 Body feature 能按 FreeCAD 语义重建。
- Topo naming 是正式模块，不是导出层补丁。
- 每个已支持 `TypeId` 有明确 executor、fixture、diagnostics 和 FreeCAD 依据。
- 未支持 `TypeId`、缺失链接、坏 subname、构造失败、循环依赖都有结构化 diagnostics。
- 同一 fixture 在 CLI、C ABI 和未来 adapter 下结果一致。
- 文档中能追溯每个核心语义对应的 FreeCAD 源文件和 cad-core 落点。

## 结论

正确抽法不是“先做一个后端服务”，也不是“把 FreeCAD 整个应用塞进库里”。正确边界是：

```text
FreeCAD App / Mod 语义
  -> DocumentObject graph
  -> property and link system
  -> dependency graph
  -> recompute runtime
  -> feature executors
  -> OCCT geometry modules
  -> NamedShape / ElementMap / topo naming
  -> mesh / subshape / diagnostics result
```

当前 P0 / P1 / P2 已经给出可运行底座。后续要把它发展成完整 CAD Core，必须先补 `PropertyLink*`、`FeatureExtrude`、Placement 和 topo naming，再扩 Sketcher internals、Datum、Hole、DressUp、Pattern、Mirror、Assembly。外壳可以晚一点接；CAD Core 的语义边界必须先稳住。
