# CAD Core 抽取方案

本文不讨论 Rust 框架，也不讨论 Web 框架。目标是从 FreeCAD/Chili3D 需要的建模链路里抽出一个无 UI、无 Qt、可被任意外壳调用的 CAD Core。

最终边界应是：

```text
CAD Core
  文档对象图 / 属性 / 依赖分析 / recompute / 几何执行 / topo naming / mesh 导出

Adapter
  CLI / FFI / Web / 桌面进程 / Worker

UI 或调用方
  Chili3D 前端 / 自动化脚本 / 测试工具 / 其他程序
```

CAD Core 不依赖 Qt，不知道 Web 路由，不知道用户系统，不保存 GUI 状态。它只接收 FreeCAD 风格的 `DocumentObject graph`，执行建模语义，然后返回几何结果和诊断。

## 当前结论

当前 `/cad-core` 已经不是 Python 原型，而是一个独立 C++/CMake Core：`cad-core-lib` 承载建模逻辑，`cad-core` 是 CLI adapter，`cad_core_ffi` 是 C ABI adapter。几何侧已经真实调用 OCCT，覆盖 wire / face / prism、mesh、bbox、volume、subshape 遍历。

已跑通的 MVP 边界很小，但是真 CAD 链路：

```text
Objects[]
  -> Sketcher::SketchObject
  -> PartDesign::Pad
  -> PartDesign::Body
  -> OCCT mesh / bbox / volume / subshape map / diagnostics
```

还没有实现完整 PartDesign Body 生态。当前 registry 只注册：

```text
Sketcher::SketchObject
PartDesign::Body
PartDesign::Pad
```

下一步不应该先扩 Web / WASM / Worker，也不应该在 `Pad` executor 里继续堆 Body 逻辑。应该先把 PartDesign 主链拆出来：

```text
FeatureBase / BaseFeature
  -> FeatureAddSub 的 add_shape / sub_shape 双通道
  -> Pad 和 Pocket 共用 FeatureExtrude
  -> Pocket Length subtractive
  -> Pattern / Mirror / MultiTransform
```

## 要抽出来的不是后端框架

如果目标是“CAD 后端”，真正应该抽的是下面这些能力：

- 文档对象模型：对象名、类型、属性、链接、Placement。
- 属性系统：普通值、枚举、长度、角度、布尔、对象链接、子元素链接。
- 依赖图：对象之间的输入输出关系、拓扑排序、循环依赖诊断。
- recompute 执行：按顺序调用对象 executor，失败时保留诊断。
- 几何执行：Sketch、Body、Pad、Pocket、Fillet、Chamfer 等特征的真实建模。
- topo naming：面、边、顶点的稳定引用、旧引用更新、新 subshape 导出。
- 结果导出：mesh、BREP、subshape map、diagnostics。

不应该抽进 CAD Core：

- Qt event loop。
- `src/Gui`。
- ViewProvider。
- TaskPanel。
- Workbench。
- Selection、TreeView、PropertyEditor。
- Web route、Session、JWT、数据库、上传目录。
- 跨请求或跨调用方共享的临时 shape 状态。

这些都可以作为外壳或 UI 功能存在，但不能成为 CAD Core 的依赖。

## 核心接口

CAD Core 第一版只需要暴露一组稳定 API。具体语言可以是 C++、Rust、C ABI、WASM 或进程协议，但能力边界不要变：

```text
load_document(document_graph) -> DocumentHandle
set_property(document, object_name, property_name, value) -> Result
recompute(document, objs) -> RecomputeResult
export_mesh(document, object_name) -> MeshResult
export_brep(document, object_name) -> BrepResult
export_subshape_map(document, object_name) -> SubshapeMap
diagnostics(document) -> Diagnostics
free_document(document)
```

如果先做进程级 MVP，可以先不用稳定 ABI，直接做 CLI：

```bash
cad-core recompute input.json --output output.json
cad-core export-mesh input.json --object Pad --output pad.mesh.json
cad-core export-brep input.json --object Pad --output pad.brep
```

如果需要给多语言调用，第二步再包 C ABI：

```c
CadDocument* cad_load_json(const char* json);
CadResult* cad_recompute(CadDocument* doc, const char* objs_json);
const char* cad_result_json(CadResult* result);
void cad_free_result(CadResult* result);
void cad_free_document(CadDocument* doc);
```

Web、桌面、脚本都应该只调用这些接口。这样 CAD Core 不会被某个框架锁死。

## DocumentObject Graph 输入模型

第一步先固定输入模型。不要直接暴露 FreeCAD 内部 C++ 对象，也不要直接暴露某个前端状态树。持久输入采用 `docs/TopoNaming` 中收敛后的 FreeCAD 风格对象图：

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
```

当前不新增上层 `Document.Name` / `Document.Version` 包装，也不新增并行 `edges`、`featureType`、`operation`、`params`。对象语义由 `TypeId` 和 `Properties` 中的 FreeCAD 同名属性决定。

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

`Objects[]` 是可保存的建模源数据；`recompute.objs` 只是本次 API 要计算和返回的目标，不进入持久层。`Placement`、`Label`、`Visibility` 这类数据如果要保存，也应作为 `Properties` 中的 FreeCAD 同名属性出现，而不是对象顶层的独立字段。

`PropertyType` 是 JSON 序列化辅助字段，不是 FreeCAD 属性名。实现如果能从 schema 推断属性类型，可以不保存它；但 `PropertyLink` / `PropertyLinkList` / `PropertyLinkSub` / `PropertyLinkSubList` 的目标对象、子元素列表和 graph edge 语义必须沿用 FreeCAD。

当前 MVP parser 已固定 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`，并拒绝旧的 lowercase schema。链接解析仍是最小版：只识别 `App::PropertyLinkSub` 以及它组成的数组。进入 P2 前要补齐 `PropertyLink`、`PropertyLinkList`、`PropertyLinkSubList` 的归一化解析，避免 Body / FeatureAddSub / Pattern 继续依赖临时 JSON 形态。

## 内部模块拆分

建议 CAD Core 内部按下面拆：

```text
cad-core/
  document/
    document
    document_object
    property
    property_link
    placement

  graph/
    dependency_analyzer
    recompute_plan
    cycle_detector

  runtime/
    compute_context
    diagnostics
    feature_registry
    feature_executor

  features/
    sketch_object
    body
    feature_base
    feature_add_sub
    feature_extrude
    pad
    pocket
    hole
    fillet
    chamfer
    pattern
    mirror
    scaled
    datum

  geometry/
    kernel
    occt_adapter
    mesh_exporter
    brep_exporter

  topo/
    named_shape
    element_reference
    stable_subname
    subshape_map

  adapters/
    cli
    ffi
    web_optional
```

关键原则：

- `features/*` 可以调用 `geometry/*`，但不能依赖 UI。
- `document/*` 不直接调用 OCCT，只描述对象和属性。
- `graph/*` 只做依赖分析，不生成几何。
- `runtime/compute_context` 只活在一次 recompute 内。
- `topo/*` 是 CAD Core 的一等模块，不能作为导出时的临时补丁。
- `adapters/*` 可以依赖 CAD Core，CAD Core 不能反向依赖 adapters。

## FreeCAD 语义来源

迁移时只看 FreeCAD 的 App/Mod 层语义，避开 Gui 层。

| 能力 | FreeCAD 源码位置 | 迁移内容 |
| --- | --- | --- |
| 文档对象创建和重算 | `src/App/Document.cpp`：`Document::addObject()`、`Document::recompute()`、`Document::_recomputeFeature()` | 对象注册、recompute 入口、重算顺序、错误传播。 |
| 对象执行外壳 | `src/App/DocumentObject.cpp`：`DocumentObject::touch()`、`mustExecute()`、`recompute()`、`execute()` | touched 状态、是否需要执行、通用 execute 调用链。 |
| 依赖关系 | `src/App/DocumentObject.cpp` / `src/App/DocumentObject.h`：`getOutList()`、`getInList()` | 对象链接如何形成 DAG。 |
| 链接属性 | `src/App/PropertyLinks.cpp` / `src/App/PropertyLinks.h` | `PropertyLink`、`PropertyLinkSub`、`PropertyLinkSubList` 的目标对象和子元素引用。 |
| Placement 和坐标系 | `src/App/GeoFeature.cpp`、`src/App/GeoFeatureGroupExtension.cpp` | 局部 Placement、全局 Placement、Body/Part 内坐标传递。 |
| topo naming 更新 | `src/App/GeoFeature.cpp`、`src/App/PropertyLinks.cpp`、`src/Mod/Part/App/PartFeature.cpp` | `updateElementReference()`、`resolveElement()`、`registerElementCache()`。 |
| 草图执行 | `src/Mod/Sketcher/App/SketchObject.cpp` | `SketchObject::execute()`、`buildShape()`、`InternalShape`、`buildInternals()`。 |
| 草图内部引用 | `src/Mod/Sketcher/App/SketchObject.h` / `.cpp` | `InternalShape`、`getInternalElementMap()`、内部边面映射。 |
| Body 特征链 | `src/Mod/PartDesign/App/Body.cpp` / `.h` | `Body::addObject()`、`Body::execute()`、`BaseFeature`、Tip、Group 顺序。 |
| PartDesign 基类 | `src/Mod/PartDesign/App/FeatureBase.cpp` / `.h` | `BaseFeature`、前序 solid、Body 内局部放置和 suppressed 行为。 |
| 加料/减料通道 | `src/Mod/PartDesign/App/FeatureAddSub.cpp` / `.h` | `getAddSubShape()`、additive / subtractive 结果如何交给 Body 做 Fuse/Cut。 |
| 共享拉伸逻辑 | `src/Mod/PartDesign/App/FeatureExtrude.cpp` / `.h` | Profile、方向、Length、SideType、UpTo* 的通用计算。 |
| Pad/Pocket | `src/Mod/PartDesign/App/FeaturePad.cpp`、`FeaturePocket.cpp` | Pad 走 additive，Pocket 走 subtractive；二者不应复制拉伸实现。 |
| Refine | `src/Mod/PartDesign/App/FeatureRefine.cpp` / `.h` | `Refine` 属性和 refine 失败处理策略。 |
| Fillet/Chamfer | `src/Mod/PartDesign/App/FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDressUp.cpp` | dress-up 特征如何读取 Base、子边引用和 BaseFeature。 |
| Pattern/Mirror/Scale | `src/Mod/PartDesign/App/FeatureTransformed.cpp` 及相关子类 | 变换特征的源特征、变换列表和 refine。 |

如果一个功能同时有 `App` 和 `Gui` 实现，以 `App` / `Mod/*/App` 为准。`Gui` 只用于理解交互入口，不作为 CAD Core 依赖。

## 执行模型

一次 recompute 的运行过程应固定成：

```text
input document
  -> parse Objects[].Properties
  -> validate TypeId
  -> build dependency graph
  -> build recompute plan
  -> create ComputeContext
  -> execute feature executors in order
  -> update element references
  -> export result
  -> drop ComputeContext
```

`ComputeContext` 里面可以保存：

- 当前请求内的 object Name -> shape。
- 当前请求内的 object Name -> NamedShape。
- 当前请求内的 subshape map。
- 当前请求内的 diagnostics。

`ComputeContext` 里面不应该保存：

- 用户信息。
- UI selection。
- ViewProvider。
- 长生命周期 cache。
- 下次请求还要复用的裸 `TopoDS_Shape`。

如果后面要做缓存，必须先引入 document revision / object revision / property hash。否则 topo naming 和引用更新会变得不可控。

## FeatureRegistry

CAD Core 应该有一个明确白名单：

```text
TypeId -> FeatureExecutor
```

当前 MVP 只注册这些类型：

```text
Sketcher::SketchObject
PartDesign::Body
PartDesign::Pad
```

P2 先补 PartDesign 主链，而不是一次性铺开全部 workbench：

```text
features/feature_base
features/feature_add_sub
features/feature_extrude
PartDesign::Pocket
```

P3 再扩常用 Body 生态：

```text
PartDesign::Hole
PartDesign::Fillet
PartDesign::Chamfer
PartDesign::LinearPattern
PartDesign::Mirrored
PartDesign::PolarPattern
PartDesign::MultiTransform
PartDesign::Scaled
Part::Feature
PartDesign::Plane
PartDesign::Line
App::Plane
App::Line
App::Point
App::LocalCoordinateSystem
App::Origin
Part::DatumPlane
Part::DatumLine
Part::DatumPoint
Part::LocalCoordinateSystem
```

规则：

- 支持的 `TypeId` 必须进入明确 executor。
- 不支持的 `TypeId` 必须返回 diagnostics。
- 不允许未知类型降级成空 shape。
- 不允许悄悄跳过失败对象继续生成看似成功的结果。
- 每新增一个 executor，必须补最小 fixture。

## MVP 顺序

### P0：抽出 Core 壳

目标：能在没有 Qt 和 Web 的情况下执行一次空 recompute。

交付：

- FreeCAD 风格 `Objects[]` / `DocumentObject` / `Properties`。
- `Diagnostics`。
- `FeatureRegistry`。
- `recompute(document, objs)` 空链路。
- CLI 入口。
- JSON 输入输出样例。

验收：

```bash
cad-core recompute empty.json --output result.json
```

能返回稳定 JSON，未知类型有 diagnostics。

### P1：跑通 Sketch + Body + Pad

目标：第一条真正的 CAD 建模链闭环。

交付：

- `Sketcher::SketchObject` 最小执行。
- `PartDesign::Body` 特征链。
- `PartDesign::Pad` 拉伸。
- mesh 导出。
- subshape map 导出。

验收：

```text
input:
  Sketch + Body + Pad

output:
  Pad mesh
  Pad subshape map
  diagnostics empty
```

### P2：补 PartDesign 主链

目标：先让 Body 的加料/减料链成立，再扩更多特征。

交付：

- `FeatureBase` / `BaseFeature`。
- `FeatureAddSub` 的 add_shape / sub_shape 双通道。
- `FeatureExtrude` 共享拉伸逻辑。
- `Pocket` 的 `Type=Length` 减料。
- Pad 继续走同一套 `FeatureExtrude`，不能复制第二套拉伸。

验收：

- `Sketch + Body + Pad + Pocket` 能生成最终 Body solid。
- Pocket 之前的 Pad 作为 base solid，Pocket 只输出 subtractive shape。
- Body 按顺序做 Fuse / Cut，Tip 指向最终 shape。
- 每个失败路径有 diagnostics，不能生成假成功结果。

### P3：扩 Body 生态和 topo naming

目标：覆盖常用参数化建模，并让已有引用在重算后尽量稳定。

交付：

- Hole。
- Fillet。
- Chamfer。
- Datum Plane/Line/Point。
- Refine。
- `PropertyLinkSub` / `PropertyLinkSubList` 更新。
- stable subname 导出。
- InternalShape 引用支持。
- LinearPattern。
- Mirrored。
- PolarPattern。
- MultiTransform。
- Scaled。

验收：

- 每个特征至少一个最小 fixture。
- 每个 fixture 有 FreeCAD 对照结果。
- 失败时 diagnostics 能指出对象、属性、引用值。
- 修改 Pad 长度后，Fillet/Chamfer 引用不应无声丢失。
- 草图内部边面引用能导出和回读。
- 旧引用无法恢复时必须产生 diagnostics。

## 不同外壳怎么接

CAD Core 做完后，可以有多个 adapter。

CLI：

```text
最适合调试、fixture、CI。
```

FFI：

```text
适合给其他语言、桌面程序、Node/WASM host 调用。
```

Web：

```text
适合远程服务和前端调用，但只是 adapter，不是 CAD Core 本身。
```

Worker：

```text
适合把耗时 recompute 放进独立进程，主程序只传 JSON/BREP/mesh。
```

同一个 input document，不管从 CLI、FFI 还是 Web 调，结果应该一致。

## 验收标准

CAD Core MVP 完成标准：

- 不需要 Qt 就能运行。
- 不需要 FreeCAD GUI 就能运行。
- 不需要 Web 框架就能运行。
- 可以从 JSON 或等价协议构建 FreeCAD 风格 `DocumentObject graph`。
- 可以按依赖顺序 recompute。
- 可以跑通 `Sketch + Body + Pad`。
- 可以导出 mesh。
- 可以导出 subshape map。
- 可以报告未知类型、缺失链接、循环依赖、执行失败。
- 单次 recompute 结束后释放临时 shape 上下文。
- 同一 fixture 在 CLI 和任意其他 adapter 下结果一致。

## 估时

按“从现有 FreeCAD 语义抽 Core，不绑定 Rust/Web 框架”估算：

| 阶段 | 目标 | 估时 |
| --- | --- | --- |
| P0 Core 壳 | `DocumentObject graph` 输入、recompute 空链路、CLI、diagnostics | 1 到 2 周 |
| P1 第一条建模链 | Sketch + Body + Pad、mesh、subshape map | 3 到 6 周 |
| P2 PartDesign 主链 | FeatureBase、FeatureAddSub、FeatureExtrude、Pocket Length | 3 到 6 周 |
| P3 Body 生态和稳定引用 | Hole、Fillet、Chamfer、Datum、Refine、topo naming、Pattern/Mirror/MultiTransform | 2 到 4 个月 |

P0 / P1 是当前已有基线；上表保留原始阶段量级，后续剩余工作从 P2 开始计算。  
如果只要求“先有一个可调用的无 Qt CAD Core MVP”，当前 P0 + P1 已经有 C++/OCCT 基线。  
如果要求 Body 的加料/减料链能继续承载常用 PartDesign feature，下一步必须做到 P2。  
如果还要求引用稳定、编辑后不丢边面，必须继续做 P3。

## 结论

正确抽法不是把 FreeCAD 或现有 Web 服务整体搬出来，而是抽出一条干净的 CAD Core：

```text
Document model
  -> dependency graph
  -> recompute runtime
  -> feature executors
  -> geometry kernel adapter
  -> topo naming
  -> result exporters
```

当前已落地的第一条路线是：

```text
JSON input
  -> cad-core CLI
  -> Sketch + Body + Pad recompute
  -> mesh/subshape/diagnostics output
```

下一条路线应先补 PartDesign Core 主链：

```text
FeatureBase
  -> FeatureAddSub
  -> shared FeatureExtrude
  -> Pocket subtractive
```

等这条链稳定后，再扩 Web、WASM、桌面进程或 Worker。外壳可以换，CAD Core 不能被外壳绑死。
