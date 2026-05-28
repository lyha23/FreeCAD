# P0：Core 壳和空 recompute

P0 的目标是先得到一个可运行的无 Qt 程序。它不需要生成真实几何，但必须能加载文档、识别对象、构造 recompute 计划，并稳定输出 diagnostics。

## 当前状态

当前 `/cad-core` 已完成 P0 基线：

- 有独立 C++/CMake `cad-core-lib` 和 `cad-core` CLI。
- parser 已固定 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`，并拒绝旧 lowercase schema。
- diagnostics、registry、依赖图、一次性 `ComputeContext` 和 recompute 输出已存在。
- 当前链接发现只识别 `App::PropertyLinkSub` 以及它组成的数组。

进入 P2 前需要补一层属性归一化：把 FreeCAD 的 `PropertyLink`、`PropertyLinkList`、`PropertyLinkSub`、`PropertyLinkSubList` 都归一成统一 graph edge 和 subname 数据，executor 不应该直接依赖 JSON 包装形态。

## Step 1：固定 DocumentObject Graph 输入模型

做什么：

- 定义 FreeCAD 风格 `Objects[]`、`DocumentObject`、`Properties`、`PropertyValue`。
- 固定对象必填字段：`Name`、`ID`、`TypeId`、`Properties`。
- 固定 recompute 运行参数：`recompute.objs`。
- 只接受 `DocumentObject graph` JSON，不直接吃 FreeCAD 内部对象或前端状态。

建议最小字段：

```text
Document
  Objects[]
    Name
    ID
    TypeId
    Properties
      Label?
      Placement?
      Visibility?
      PropertyLink*
  recompute
    objs[]
```

`recompute.objs` 是本次 API 的计算目标，不是持久文档字段。`Label`、`Placement`、`Visibility` 如果出现，也必须放在 `Properties` 里，属性名沿用 FreeCAD。

产出：

- 一份 `fixtures/mvp/empty.json`。
- 一份 `fixtures/mvp/unknown-type.json`。
- 输入解析失败、缺字段、重复对象名都有 diagnostics。

验收：

- 空文档可以解析。
- 重复 `Name` 会报错。
- 未知 `TypeId` 不会被当作空 shape 成功处理。

## Step 2：建立 Core 目录和目标

做什么：

- 建一个不依赖 `src/Gui`、Qt、Web 框架的 core 目标。
- 先按模块边界组织代码，具体语言和构建系统可以按项目实际情况落地。

建议边界：

```text
cad-core/
  document/
  graph/
  runtime/
  features/
  geometry/
  topo/
  adapters/cli/
```

产出：

- 一个可编译的 core 库目标。
- 一个 `cad-core` CLI 目标。
- Core 目标不能链接 Qt 或 GUI 模块。

验收：

- 单独构建 `cad-core` 不需要 FreeCAD GUI。
- CLI 能打印版本或 help。

## Step 3：实现 diagnostics

做什么：

- 统一错误输出格式。
- 每条诊断至少包含：`severity`、`code`、`message`。
- 如果和对象相关，必须包含 `object`；如果和属性相关，必须包含 `property`。

建议格式：

```json
{
  "severity": "error",
  "code": "missing_link_target",
  "object": "Pad",
  "property": "Profile",
  "message": "Profile links to missing object Sketch"
}
```

产出：

- `Diagnostics` 数据结构。
- 解析阶段、依赖分析阶段、执行阶段共用同一格式。

验收：

- diagnostics 可以序列化到结果 JSON。
- 同一输入重复运行时 diagnostics 顺序稳定。

## Step 4：实现 FeatureRegistry

做什么：

- 建立 `TypeId -> FeatureExecutor` 白名单。
- P0 可以先注册 stub executor。
- 不支持的 `TypeId` 必须进入 diagnostics。

MVP 初始白名单：

```text
Sketcher::SketchObject
PartDesign::Body
PartDesign::Pad
```

产出：

- `FeatureRegistry`。
- `FeatureExecutor` 基类或接口。
- `UnsupportedFeatureExecutor` 只负责报错，不负责生成空 shape。

验收：

- `unknown-type.json` 返回 `unsupported_type`。
- 已注册类型会进入 recompute 流程。

## Step 5：实现空 recompute 链路

做什么：

- 从 `recompute.objs` 找到目标对象。
- 根据对象属性里的链接构造最小依赖关系。
- 生成拓扑顺序。
- 创建一次性 `ComputeContext`。
- 按顺序调用 executor。
- 回收 `ComputeContext`。

P0 只需要支持对象链接属性：

```json
{
  "Profile": {
    "PropertyType": "App::PropertyLinkSub",
    "value": "Sketch",
    "SubList": []
  }
}
```

产出：

- `ComputeContext`。
- `RecomputePlan`。
- `recompute(document, objs)`。

验收：

- `recompute.objs` 指向不存在对象会报 diagnostics。
- 缺失链接会报 diagnostics。
- 循环依赖会报 diagnostics。
- 空 executor 不生成几何，但 recompute 结果格式稳定。

## Step 6：实现 CLI 输入输出

做什么：

- 提供 `recompute` 命令。
- 从 JSON 文件读取 Document。
- 将结果写到 JSON 文件。
- 退出码只表达命令是否执行完成；建模错误看 diagnostics。

命令形式：

```bash
cad-core recompute fixtures/mvp/empty.json --output out/empty.result.json
```

产出：

- `adapters/cli`。
- `empty.result.json` 样例。
- `unknown-type.result.json` 样例。

验收：

- 命令重复执行结果稳定。
- 输出至少包含 `objects`、`diagnostics`、`mesh`、`subshapes` 这些稳定顶层字段。
- 没有 Web server、Session、JWT、上传目录等依赖。
