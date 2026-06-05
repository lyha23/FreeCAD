# WireJoiner Bridge 彻底删除方案

## 1. 目标

删除 CAD Core WireJoiner 当前用于 generated open export 和 purge-as-original 的 helper bridge，让 open wire export ownership 完全来自 FreeCAD WireJoiner 内部账本。

这里的“删除 WireJoiner bridge”不是删除 `WireJoiner` 本体，而是删除这些临时桥接职责：

- `helperOpenExportOverride`
- `openExportOverride`
- `purgeAsOriginalOpenEdge`
- `LegacyHelperShapeStillUsed`
- 以及继续新增 helper reason 来修补导出结果的做法

完成后，generated open export 和 source edge ownership 应来自：

- `EdgeInfo`
- `WireInfo` / `wireInfo2`
- `iteration` / `iteration2`
- `superEdge`
- `sourceEdgeArray`
- `aHistory`
- `myShapesToReturn`
- `openWireCompound`
- `MapperHistory(aHistory)`

## 2. 非目标

- 不新增 fixture-specific pruning。
- 不在 sketch executor、adapter 或结果导出层按几何类型猜 source ownership。
- 不把 helper bridge 改名后继续作为主路径。
- 不重写 WireJoiner 本体之外的 Feature / Sketch 语义来掩盖账本缺口。

## 3. FreeCAD 依据入口

开工前必须复核这些本地源码入口：

| FreeCAD 源码 | 需要提取的语义 |
| --- | --- |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()` | WireJoiner 主状态机、closed/open wire 构造和最终输出 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()` | split 后 source edge 到 fragment 的 history |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()` | closed wire 对 EdgeInfo / WireInfo ownership 的消耗 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()` | tight bound 查找与 wireInfo/wireInfo2 关系 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()` | ownership exhaust 生命周期 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()` | final open export gate 和 `noOriginal` 语义 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp` 中 `MapperHistory(aHistory)` | WireJoiner history 到 topo naming 的传播 |

现有 `docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md` 是本方案的前置输入。若两个文档口径冲突，以 FreeCAD 源码复核后的账本迁移口径为准，并同步修正文档。

## 4. 当前 bridge 基线

当前 CAD Core bridge 主要服务两个已知原因：

| helper reason | 当前作用 | 正确替代来源 |
| --- | --- | --- |
| `partial_shared_closed_wire` | 在部分 source edge 已进入 closed wire 时，仍给 generated open export 提供临时 ownership | `WireInfo` / `wireInfo2` exhaust 生命周期、`myShapesToReturn` |
| `closed_wire_cycle` | closed wire 成环后，用 helper 标记 open export 或 purge 原始边 | `aHistory` generated/removed source map、`openWireCompound` child-wire ownership |

这些 helper 只能解释当前 bridge 为什么存在，不能作为后续继续扩展的理由。

## 5. 目标账本映射

| FreeCAD 账本/字段 | CAD Core 目标职责 |
| --- | --- |
| `sourceEdgeArray` | 保存原始 source edge identity，支持 noOriginal / purge 判断 |
| `aHistory` | 保存 generated、modified、removed source edge 到 fragment/result 的历史 |
| `myShapesToReturn` | 保存最终需要返回的 wire/edge result identity |
| `openWireCompound` | 表达 open wire child ownership，而不是靠导出层猜测 |
| `EdgeInfo::wireInfo` | primary owner 和 closed/open 消耗状态 |
| `EdgeInfo::wireInfo2` | secondary owner 和 shared edge lifecycle |
| `EdgeInfo::iteration` / `iteration2` | final export gate 和 ownership exhaust 判断 |
| `EdgeInfo::superEdge` | split fragment 到源边/合并边的关系 |
| `MapperHistory(aHistory)` | 把 WireJoiner history 传播到 NamedShape / ElementMap |

最终 open export 判断必须从上述账本自然推出。旧反思文档中的关键约束仍然有效：输出端 pruning 不是主路径，final open export 应受 FreeCAD `EdgeInfo` 状态机约束，而不是靠结果形状后处理。

## 6. 实施切片

### A. Bridge inventory 和诊断冻结

- 统计当前 `generated_open_export_bridge`、`purge_as_original_bridge`、helper reason、helper fields 的出现位置。
- 增加诊断只用于观测，不新增 helper 决策。
- 明确本轮不允许新增 helper reason。

验收：当前 bridge 使用点和剩余原因可被 `rg` 查清；没有新增 fixture-specific reason。

### B. 完整 source / split history

- 补齐 `sourceEdgeArray` 到 split fragment 的映射。
- 补齐 `aHistory` 的 generated / modified / removed 关系。
- 对 splitter 失败、source edge 未 split、source edge 一对多 fragment 保持结构化 history。

验收：source edge 到 fragment/result 的映射不依赖导出层猜测。

### C. myShapesToReturn 和 openWireCompound ownership

- 迁移 `myShapesToReturn` 等价结构。
- 让 open wire child ownership 从 `openWireCompound` / result-wire identity 进入 CAD Core。
- 删除用 helper 标记“这个 generated edge 应当导出”的主路径。

验收：generated open export 可以从 result-wire identity 推导。

### D. EdgeInfo / WireInfo exhaust 生命周期

- 完整实现 `wireInfo` / `wireInfo2` 的 primary / secondary owner 消耗。
- 实现 `iteration` / `iteration2` 对 final open export gate 的约束。
- 实现 shared edge 被 closed wire 消耗后的状态迁移。

验收：`partial_shared_closed_wire` 不再需要 helper reason。

### E. purge-as-original 替换

- 用 `sourceEdgeArray`、`aHistory` 和 `openWireCompound` child ownership 判断原始边是否应保留。
- 删除 `purgeAsOriginalOpenEdge` 主路径。
- 如果必须保留诊断字段，只能标记“这个旧 bridge 曾经会 purge”，不能影响导出结果。

验收：`closed_wire_cycle` 不再需要 helper purge marker。

### F. MapperHistory 到 ElementMap

- 把 WireJoiner `aHistory` 消费到 `MapperHistory` 等价结构。
- 让 NamedShape / ElementMap 从 history 获取 InternalEdge / InternalVertex source trace。
- 不在 sketch executor 中补 source edge 猜测。

验收：如果几何已经一致，stable subname 和 internal element 差异必须归类到 history propagation，而不是输出端修正。

### G. 删除 bridge fields

- 删除或降级 `helperOpenExportOverride`。
- 删除 `openExportOverride` 主路径。
- 删除 `purgeAsOriginalOpenEdge`。
- 删除 `LegacyHelperShapeStillUsed` 对 capability 的有效影响。
- 更新 CADCore3 gap 表：bridge 不再作为 accepted implementation。

验收：`rg` 只能在文档、历史说明或 diagnostic-only 路径中看到旧字段名，不能在 open export ownership 主路径中看到。

## 7. 验收矩阵

必须覆盖的 case：

| Case | 期望 |
| --- | --- |
| self-intersection split | fragment history 完整，open export 不靠 helper |
| inter-edge intersection split | source 一对多 fragment 可追溯 |
| bounded faces + open wires | closed wire 消耗和 open wire 导出同时正确 |
| partial shared closed wire | 不再生成 `partial_shared_closed_wire` helper reason |
| closed wire cycle | 不再生成 `closed_wire_cycle` purge helper |
| splitter failure | 保留原 edge 语义，输出 diagnostic，不靠 fixture fallback |
| MapperHistory propagation | InternalEdge / InternalVertex source trace 稳定 |
| naming order difference | 几何等价且顺序稳定时归类为命名顺序差异，不算硬失败 |

阶段回归命令：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch
python3 -m unittest tests.test_p6_topology
```

若修改会影响 CADCore3 capability / adapters，再补：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_p8_features
```

代码改动后的轻量检查：

```bash
git diff --check -- cad-core docs/CADCore3.0 docs/偏移处理
```

## 8. 完成条件

- generated open export ownership 来自真实 WireJoiner 账本。
- purge-as-original 不再由 helper marker 驱动。
- helper fields 不再参与主路径决策。
- `generated_open_export_bridge.status` 和 `purge_as_original_bridge.status` 不再作为 covered 口径存在。
- P5/P6 topology 和 sketch fixture 通过，剩余差异只允许是已记录的命名顺序差异或 unsupported case。
