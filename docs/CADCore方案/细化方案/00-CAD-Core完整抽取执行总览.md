# CAD Core 完整抽取执行总览

本目录是 `../00-CAD-Core抽取方案.md` 的执行拆解。这里不再只服务 MVP，而是按完整 CAD Core 抽取目标组织：从当前已落地的 P0/P1/P2/P3a/P3b 基线和 P4 Document / Link 初始主路径继续推进到完整 Placement、Sketcher、Topo Naming、PartDesign 常用生态和后续 Part / Assembly 能力。

## 当前基线

当前 `/cad-core` 已经落地为独立 C++17 / CMake Core，并真实接入 OCCT：

- `cad-core-lib` 是 Core 库。
- `cad-core` 是 CLI adapter。
- `cad_core_ffi` 是薄 C ABI adapter。
- 输入采用 FreeCAD 风格 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`。
- registry 当前覆盖 `App::Part`、`Sketcher::SketchObject`、`PartDesign::Body`、`PartDesign::Plane`、`PartDesign::Line`、`PartDesign::Point`、`PartDesign::FeatureBase`、`PartDesign::Pad`、`PartDesign::Pocket`。
- `DocumentObject` 已保留 raw `properties` 并新增规范化 `PropertyValue` / `dependencyLinks`；`PropertyLink` / `PropertyLinkList` / `PropertyLinkSub` / `PropertyLinkSubList` 已在 document 层归一成 recompute dependency edge。
- graph 已切换为只消费 document 层 `dependencyLinks`；Body、FeatureBase、FeatureExtrude 的主要 Link / LinkSub 读取已切到 `document::readLink/readLinks(object, property)`。
- document typed getter 已覆盖 Bool、Number/Length/Angle、String/Enumeration、Vector、Placement；FeatureExtrude 常用参数读取、runtime global placement、Sketch placement 和 FeatureBase placement 已兼容 raw JSON 和 typed wrapper。
- diagnostics JSON 已兼容 `stage`、`target`、`subname`，parse / graph / runtime 的关键链接和子形状错误可定位阶段、目标对象和子元素名。
- `App::Part` / `PartDesign::Body` 的 GeoFeatureGroup membership 已形成 `parentGroupByObject`；runtime 计算 parent group placement * object placement，Body / Part 输出的 bbox、mesh summary bbox、volume 使用同一全局坐标。
- `PartDesign::Plane` / `PartDesign::Line` / `PartDesign::Point` 已作为基础 Datum executor 接入；Sketch 的 `AttachmentSupport` / `Support` 可使用 DatumPlane global placement 构造 profile，`ReferenceAxis` 可直接使用 DatumLine 方向，DatumPoint 可消费 parent Part placement。
- P1 的 `rect-pad.json` 能生成 OCCT mesh、bbox、volume、subshape map。
- P2 的 `rect-pad-pocket.json` 能生成 Pocket 后的 Body mesh、bbox、volume、subshape map。
- P3a 已把 Pad / Pocket 共享 `FeatureExtrude` 从 `Length` 扩到 `ThroughAll`、`UpToFace`、单目标 `UpToShape`。
- `PropertyLinkSub` 的 `FaceN` 可解析到当前 recompute 内目标 solid 的真实 face；非法 subshape、非 face subshape、缺失目标都有稳定 diagnostics。
- `fixtures/p3a` 已覆盖 Pocket `ThroughAll` / `UpToFace` / `UpToShape` 正常 case、错误 diagnostics，以及至少一个 Pad `UpToFace` case。
- P3b 已把 shared `FeatureExtrude` 扩到 `SideType=Two sides / Symmetric`、`Length2`、第一侧 / 第二侧 `UpToFace` / `UpToShape`、Pad / Pocket taper、Pad / Pocket custom direction、ReferenceAxis sketch / EdgeN / DatumLine 基础子集，以及显式 Sketch / Body / FeatureBase Placement。
- `fixtures/p3b` 已覆盖 27 个输入，其中 21 个成功 fixture 有 FreeCAD oracle；P3b 成功 fixture 校验 bbox、volume、topology counts，错误 fixture 校验稳定 diagnostics code。
- `fixtures/p4` 已覆盖 LinkList、LinkSubList、missing target、cycle dependency、`invalid_link_value`、`invalid_property_type`、`invalid_placement`、typed scalar Pad、Part-local Body placement、Sketch placement Pocket、DatumPlane support、DatumLine ReferenceAxis 和 DatumPoint parent Part placement；当前 unittest 为 34 tests OK。
- P5 已开始迁移 Sketcher solver-facing 子集：`Sketcher::SketchObject` 可消费 FreeCAD 风格 `Coincident(Type=1)` 端点约束做 endpoint merge，并用合并后的端点连通性构造 line / circle-arc / ellipse-arc 闭合 wire；单个非 construction `Circle` / `Ellipse` 可作为闭合 profile，construction line / arc / circle / ellipse 不参与 profile 构面。`ExternalGeometry` 已支持 `App::PropertyLinkSubList` 到 DatumLine / `EdgeN` straight-line edge 的 transient construction projection、DatumPoint / `VertexN` 的 transient construction point projection、circle `EdgeN` 平行 / 垂直 / 倾斜投影，以及完整 ellipse `EdgeN` 平行 / 倾斜投影成 transient construction ellipse 或退化 construction line；其它 constraints 和未迁移 external geometry 返回稳定 diagnostics。当前 unittest 为 45 tests OK。
- taper / boolean 的 `NamedShape` / `ElementMap` / MapperHistory 尚未进入 topo 主路径；P3b taper fixture 显式标记 `known_gap:taper_history`，归入 P6。

这些能力证明 CAD Core 已有可运行底座、P3b FeatureExtrude 主矩阵和 P4 Link graph 初始主路径，但还没有完成完整 FreeCAD 语义抽取。

## 未完成边界

当前仍缺：

- `FeatureExtrude` 的后续边界：`UpToFirst`、`UpToLast`、多 face / shell `UpToShape`、stable/full subname 语义、外部 Part feature axis ownership、attachment/support/subname 恢复，以及 taper / boolean 的 topo history 传播。
- 完整 Document / Property / Placement：Sketch 专有 Geometry 输入收敛、完整 object-local inverse placement、更完整 GeoFeatureGroup 边界、Origin / AttachEngine 和复杂 Datum attachment 坐标传递。
- 完整 Sketcher：bspline 等剩余几何、ExternalGeometry face / 非平行 circle/ellipse arc edge / defining profile、内部 face/edge/vertex、完整 solver-facing 状态、`InternalShape` / `getInternalElementMap()`。
- 完整 Topo Naming：`NamedShape`、`ElementMap`、MapperHistory、旧引用恢复和 stable subname 主路径。
- PartDesign 常用生态：Datum、Refine、Hole、Fillet、Chamfer、Pattern、Mirror、MultiTransform、Scaled。
- Part / 导入导出 / Assembly 后续能力。

## 细化文件

| 阶段 | 文档 | 目标 |
| --- | --- | --- |
| 总览 | `00-CAD-Core完整抽取执行总览.md` | 记录当前基线、阶段索引、全局规则和完成定义。 |
| P0 | `01-P0-Core壳.md` | 固定 DocumentObject graph、diagnostics、registry、空 recompute 和 CLI。 |
| P1 | `02-P1-Sketch-Body-Pad闭环.md` | 跑通 Sketch + Body + Pad 第一条真实建模链。 |
| 接口 | `03-接口与验收样例.md` | 固定 CLI / C ABI / fixture / diagnostics / oracle 的验收口径。 |
| P2 | `04-P2-FeatureBase-FeatureAddSub-Pocket.md` | 冻结 Body 加料 / 减料主链和 shared `FeatureExtrude` 最小链。 |
| P3a | `05-P3a-FeatureExtrude-UpTo终止语义.md` | 落地 `ThroughAll`、`UpToFace`、单目标 `UpToShape`。 |
| P3b | `06-P3b-FeatureExtrude双侧方向与Placement.md` | 补 `Two sides`、`Symmetric`、`Length2`、taper、custom direction、placement。 |
| P4 | `07-P4-Document-Property-Placement完整化.md` | 收敛属性系统、链接系统、坐标系和 graph edge。 |
| P5 | `08-P5-Sketcher核心与内部元素.md` | 迁移 Sketcher 外部引用、内部元素和 `InternalShape`。 |
| P6 | `09-P6-TopoNaming主路径.md` | 把 `NamedShape` / `ElementMap` / MapperHistory 提升为正式主路径。 |
| P7 | `10-P7-PartDesign常用生态.md` | 扩 Datum、Refine、Hole、Fillet、Chamfer、Pattern、Mirror、MultiTransform。 |
| P8 | `11-P8-Part导入导出与Assembly后续.md` | 扩 Part、文件导入导出、Assembly、Worker / WASM / Web adapter 产品化。 |

## 执行顺序

后续推进顺序固定为：

```text
P0/P1/P2/P3a/P3b frozen baseline
  -> P4 Document / Property / Placement 完整化（当前在 Document / Link 主路径）
  -> P5 Sketcher 核心和内部元素
  -> P6 Topo Naming 主路径
  -> P7 PartDesign 常用生态
  -> P8 Part / 导入导出 / Assembly / adapter 产品化
```

不要先接 Web / WASM / Worker 产品化，也不要在 `FeatureExtrude`、LinkSub、Placement 和 topo naming 稳定前铺开 Pattern / Mirror / Fillet / Chamfer。否则后续特征会复制不完整的 source feature 语义，最终返工。

## 全局规则

- 本地 `/Users/li/Chili3DProject/重构Chili/FreeCAD/src` 是语义来源；不确定行为先读 FreeCAD App / Mod 源码，再决定 cad-core 落点。
- `DocumentObject graph` 是唯一持久源数据；BREP、shape、mesh、NamedShape、ElementMap、subshape map 都是单次 recompute 运行产物。
- adapter 只做协议转换，不能承载 FreeCAD 业务语义。
- `document/` 不依赖 OCCT，`graph/` 不生成几何，`runtime/` 不保存跨请求 shape，`features/` 不做 topo naming 输出补丁。
- FaceMaker / WireJoiner / ShapeFix 的几何账本放 `geometry/`；`NamedShape` / `ElementMap` / MapperHistory 放 `topo/`。
- 不允许按 fixture 名称、几何类型排序、source edge 猜测或输出端修剪来替代 FreeCAD 通用流程。
- 每新增一个支持的 `TypeId`，必须有 executor、fixture 或语义单测、diagnostics、FreeCAD 依据。
- 不支持的 `TypeId`、属性、LinkSub、subshape 或构造路径必须返回结构化 diagnostics，不能生成假成功结果。

## 验收门槛

每个阶段完成前至少满足：

- P0/P1/P2/P3a frozen fixtures 不回退。
- 新增能力有 FreeCAD oracle 或明确的暂缓边界。
- 正常 fixture diagnostics 为空。
- 错误 fixture diagnostics code 稳定。
- CLI 和 C ABI / 测试 harness 对同一输入的核心输出一致。
- 文档同步记录当前状态、FreeCAD 依据、cad-core 落点、剩余边界和下一步。

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

## 完整完成定义

第一轮完整 CAD Core 抽取完成，应同时满足：

- CAD Core 可以在没有 Qt、GUI、Workbench、Web 框架的环境下独立构建和运行。
- 输入模型是 FreeCAD 风格 `DocumentObject graph`，不是前端临时状态树。
- 持久层不保存 BREP、shape、mesh、NamedShape、ElementMap。
- property / link / placement / graph / recompute 形成通用底座，executor 不再各自解析临时 JSON。
- Sketcher、PartDesign Body 主链和常用 Body feature 能按 FreeCAD 语义重建。
- Topo naming 是正式模块，不是导出层补丁。
- 每个已支持 `TypeId` 有明确 executor、fixture、diagnostics 和 FreeCAD 依据。
- 未支持 `TypeId`、缺失链接、坏 subname、构造失败、循环依赖都有结构化 diagnostics。
- 同一 fixture 在 CLI、C ABI 和未来 adapter 下结果一致。
- 文档中能追溯每个核心语义对应的 FreeCAD 源文件和 cad-core 落点。
