# C12-M2 Part Workbench Native Oracle Probe 批次方案

## 背景

C12-M1 已把 CopyOnChange、Assembly representative subset 和 Part Workbench historical rows 全部过了一轮 implementation candidate 闸门。最终结论是 `no_code_backlog_gate`：没有一个 row 同时满足 source authority、stable expected/current mismatch、request-local/product boundary。

用户已明确要求单独打开 oracle collection / native probe 包。因此 C12-M2 只做前置证据采集：把 Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 的历史证据逐一复核为可运行 probe、稳定 expected 或明确 blocker。C12-M2 不把历史 retained evidence 直接升级为实现任务。

## 方法

1. 冻结 C12-M1 S6 后的 live baseline，记录当前 HEAD、dirty boundary、FreeCADCmd / collector 可用性和正式 oracle 基线要求。
2. 从 CADCore5/6/11 的 S6、fixtures、expected、probe artifacts 和 FreeCAD source 中建立 source candidate matrix。
3. 对每个 candidate 执行范围准入：是否 request-local、是否需要 GUI/native session、是否已有 current cad-core 可比较输出、是否存在 helper/native-hidden blocker。
4. 统一 native probe artifact schema：输入、运行命令、FreeCAD 版本、OCCT/LibPack 版本、stdout/stderr、expected summary、失败分类和复现路径。
5. 对 Sweep / Loft 先走原生 DocumentObject / Part feature probe，因为它们更接近稳定产品 API。
6. 对 Filling / GeomPlate / ProjectOnSurface 单独处理 helper / wrapper / mapper blocker，不把 helper lifecycle 失败算成 cad-core mismatch。
7. S6 发布分类结果：只有 stable expected + current mismatch 的 row 才进入后续 implementation 包建议。

## 本包不做什么

- 不新增或修改 `cad-core` C++。
- 不刷新 cad-core fixtures expected 为 supported 状态。
- 不新增 adapter、frontend mock、capability wording 或 test 断言。
- 不把 FreeCAD GUI / Workbench session 依赖做成 CAD Core 产品边界。
- 不用 native probe 的失败栈直接证明 cad-core 有 bug。

## 矩阵职责

| matrix | purpose |
| --- | --- |
| source candidates | 记录 FreeCAD source authority、历史证据和候选 probe。 |
| scope review | 判断每个 row 是否具备 request-local/product boundary。 |
| blocker queue | 跟踪 FreeCADCmd、helper、native-hidden、collector schema 和 current comparison 阻塞。 |
| non-goal registry | 固化 GUI/session/persistent geometry 等明确不进入 CAD Core 的行为。 |
| backend gap classification | 只在 S6 才允许把 row 升级为 implementation candidate。 |
| probe matrix | 记录候选 probe case、artifact 命名和通过标准。 |
| validation matrix | 记录本包 docs/TSV/queue 和后续 native probe 验收命令。 |

## 后续分流

S6 若发现多个 family 同时满足 implementation 条件，后续应按 FreeCAD 调用链拆包，不在 C12-M2 内合并落代码：

- Sweep: `part_sweep` / `TopoShapeExpansion` / pipe shell binding 对齐。
- Filling: `part_filling` / filling helper / support-order parameters 对齐。
- GeomPlate: `part_geomplate` / projected curve2d / surface support 对齐。
- Loft: `part_loft` / selected subelement assignment 对齐。
- ProjectOnSurface: projection / mapper / provenance 对齐。

## 验收

本包创建阶段只做文档验收。运行 C12-M2 步骤时，S3 以后可以在本机非 sandbox FreeCADCmd 环境执行 native probe；若 sandbox 触发 Qt / processor 错误，只记录为 sandbox runtime limitation，不能当作 FreeCAD expected 失败。
