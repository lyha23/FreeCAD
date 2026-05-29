# P0：Core 壳

P0 固定 CAD Core 的最小独立运行边界。目标不是生成几何，而是证明核心库可以在没有 Qt、GUI、Workbench 和 Web 服务的环境下解析文档、生成 recompute plan、执行 registry 调度并返回结构化 diagnostics。

## 当前基线

- `cad-core-lib`、`cad-core` CLI、`cad_core_ffi` target 已存在。
- 输入使用 FreeCAD 风格 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`。
- `document::parseDocument()` 解析对象、目标、属性和依赖链接。
- `graph` 基于 document dependency links 生成目标相关执行顺序。
- `runtime` 持有单次请求内的 `ComputeContext`、diagnostics、feature registry 和执行顺序。
- 未知 `TypeId`、缺失目标、循环依赖、坏输入结构返回 diagnostics，不生成假成功。

## 模块边界

| 模块 | P0 责任 |
| --- | --- |
| `document/` | JSON 到中立对象模型，不依赖 OCCT |
| `graph/` | 拓扑排序和循环诊断，不生成 shape |
| `runtime/` | registry 调度和 diagnostics 汇总 |
| `adapters/` | CLI / C ABI 参数和结果转换 |

## 保持规则

- P0 不引入几何特判。
- P0 不保存跨请求状态。
- CLI / C ABI 不承载业务语义。
- 任何后续阶段新增属性或 executor，都不能破坏 P0 的 parse / graph / diagnostics 口径。

## 验收

- 空文档、缺失 `Objects`、无效 target、未知 `TypeId`、循环依赖都有稳定 diagnostics。
- `recompute.objs` 只选择目标，不改变 `DocumentObject graph`。
- `git diff --check` 对文档和代码变更保持干净。
