# CAD Core 3.0 方案入口

本目录定义 `cad-core` 在 CAD Core 2.0 已完成后的下一阶段方案。3.0 的目标不是继续扩大零散 fixture，而是按 FreeCAD 源码模块系统复刻几何建模核心能力：文档对象语义、属性 / 链接、Sketcher、Part、PartDesign、TopoNaming、Link、Assembly、adapter 与回归 oracle 都必须能被统一矩阵追踪。

## 前置假设

本方案以 `docs/CADCore2.0` 已经完成为起点：

- 2.0 已冻结到“显式能力 + 显式 gap”状态。
- `cad-core` 已具备独立 C++17 / CMake Core、CLI / C ABI adapter、FreeCAD 风格 `DocumentObject graph` 输入、单次 recompute 输出、mesh / subshape / `NamedShape` / diagnostics、P0-P8 基础能力和 300+ 测试回归。
- 2.0 剩余缺口不再是静默 fallback，而是通过 capabilities、diagnostics、expected skipped 或 known gap 显式暴露。

## 3.0 目标

CAD Core 3.0 要把 `cad-core` 从“可支撑常用无状态建模主链”推进到“可作为 FreeCAD 几何库复刻版长期演进”的工程基座。

3.0 的核心交付不是单个 feature 数量，而是以下能力闭环：

- 每个 FreeCAD 几何语义都有源码依据、cad-core 落点、fixture / oracle 和 diagnostics。
- `NamedShape`、`ElementMap`、MapperHistory、ReferenceShadow、ExternalGeometry、Link retag 和 Assembly placement update 走同一套引用恢复模型。
- Sketcher / Part / PartDesign 的 maker history 不再各自散落，也不靠 bbox、面积、输出顺序或 fixture 名称猜 source ownership。
- 前端只持久化 FreeCAD 风格 `DocumentObject graph`；shape、mesh、`NamedShape`、ElementMap 仍是单次请求产物。
- adapter 只做协议转换，不承载建模、拓扑命名或引用恢复语义。

## 文档索引

| 文档 | 用途 |
| --- | --- |
| `00-总览.md` | 3.0 目标、阶段拆分、推进顺序和非目标 |
| `01-FreeCAD语义盘点与差距矩阵.md` | 按 FreeCAD 模块盘点需要复刻的对象、属性、maker、history 和 oracle |
| `02-TopoNaming与History完全体.md` | ShapeFix、import shape、full MapperHistory、ExternalGeometry native 生命周期和引用恢复 |
| `03-Sketcher-Part-PartDesign几何能力复刻.md` | Sketcher solver、Part workbench、PartDesign feature family 与复杂参数余量 |
| `04-Link-Assembly-运行时产品化.md` | Link `_ChildCache`、copy-on-change、cross-document、Assembly solver、Worker / WASM / Web adapter |
| `05-验收矩阵与交付规则.md` | 分阶段验收、fixture / oracle 策略、回归命令和完成判定 |
| `FreeCAD语义矩阵.md` | C3-M0 可执行语义矩阵，按 FreeCAD 模块追踪源码依据、cad-core 落点、gap、阶段和 oracle |
| `capabilities-gap对照表.md` | C2-M8 capabilities / remaining gaps 到 C3 可执行项的对照表 |
| `oracle-fixture队列.md` | C3-M1 到 C3-M7 的 oracle / fixture 队列和第一批实现切片 |

## 执行原则

- 本地 FreeCAD `src/` 是唯一语义来源；不从 fixture 输出倒推业务逻辑。
- 先建矩阵，再实现阶段；没有矩阵项、FreeCAD 依据和验收项的 feature 不进入 3.0 主线。
- 优先修复 FreeCAD 同构语义层：`app`、`sketcher`、`part`、`part_design`、`assembly` 与 `runtime`，而不是在 adapter 或输出 JSON 中补丁式修正。
- 每个阶段只更新当前基线、已完成语义、剩余缺口、验收命令和下一步，不记录流水账。
