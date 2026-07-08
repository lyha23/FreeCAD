# C13-M3 MappedName Producer Ledger 前置实现批次方案

## 背景

C13-M2 S3 已经用 focused tests 锁住 `mappedName.raw/canonical` parity：p2 / c4m6 / p6 必须输出 FreeCAD expected 的 raw/canonical mapped name，p5 / p8 必须保持 indexed-only no-fake-raw 边界。S4 worker 复核后停止实现，因为当前 cad-core 没有 FreeCAD raw mapped name 的生产账本。

FreeCAD 的 raw mapped name 不是 response 阶段格式化出来的。它在 `TopoShapeExpansion.cpp` 的 producer 链路中生成：`ensureElementMap()->encodeElementName(..., Tag, op, other.Tag)` 消费当前 shape tag、source tag 和 op postfix，然后 `ElementMap::encodeElementName()` 追加 `;:H...` tag segment。cad-core 当前 `NamedShape.elementMap` 只有 stable token 到 current name 的投影，无法还原这层 source-backed raw bytes。

## S0 关闭结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 `HEAD=095777f8ab`（`095777f8ab 文档：关闭 C13-M3 工作步骤入口`），起点 `git status --short -uall` 无输出。
- C13-M2 S3 evidence：5 个 guarded `unittest.expectedFailure` redline 已冻结 p2 Body、c4m6 Body、p6 ProbePad raw/canonical parity，以及 p5 Sketch、p8 BoxLink indexed-only no-fake-raw 边界。
- C13-M2 S4 blocker：helper/runtime codec alone cannot reconstruct FreeCAD raw names without `TopoShape.Tag` / `ElementMap::encodeElementName()` producer evidence；runtime 不得从 stable token 伪造 `mappedName.raw/canonical`。
- S0 是 docs/matrix freeze；S1-S5 继续 pending。

## 问题定义

当前 runtime 做的是：

- 遍历 `NamedShape.elementMap`。
- 过滤 indexed-only alias。
- 把 `stableName` 作为 entry key。
- 发布 `mappedName.raw = stableName`、`mappedName.canonical = stableName`。

这能支撑 C13-M1 的 request-local round-trip，但不能满足 C13-M2 focused parity，因为 expected 需要 producer raw names：

- `Pocket.#11:2;:H21,V`
- `Pad.#10:1;:G;XTR;:H41f:7,E`
- `#3d:1;:G;PSM;:H3c3:7,E`

如果没有 producer-side tag/op ledger，任何 codec 都只能靠 expected 字符串、fixture 名或对象名猜测。这不是实现 FreeCAD 语义。

## 设计原则

1. Producer first：raw mapped name 必须在 `NamedShape` / mapper history 生产阶段获得 source-backed provenance。
2. Runtime consumer only：`topo_naming_state.cpp` 只消费 ledger，不反推业务语义。
3. No fake raw：没有 source-backed raw evidence 时保持 indexed-only、history_partial gap 或 blocker，不发布 display path/stable token 伪 raw name。
4. 最小完整语义批次：同一轮覆盖 PartDesign Body/Pad/Pocket、Body/Tip recovery、UpToFace probe、Sketch indexed-only、App::Link indexed-only 边界。
5. 可回流 C13-M2：C13-M3 完成后，C13-M2 S4 应只剩 codec/publisher 收口，而不是继续补 producer evidence。

## 建议数据模型

新增或扩展 `NamedShape` 的 request-local provenance，建议概念字段如下：

| 字段 | 含义 |
| --- | --- |
| `entryKey` | `topoNamingState.elementMap.entries` 的 key，必须来自 source-backed mapped name 或已验证 stable ledger。 |
| `currentElement` | 当前 `FaceN` / `EdgeN` / `VertexN`。 |
| `sourceElement` | FreeCAD encode 的 source element 或 source mapped name。 |
| `elementType` | `F` / `E` / `V`。 |
| `producerTag` | 当前 producer shape tag，等价 FreeCAD `TopoShape.Tag`，不得从 object name 猜。 |
| `sourceTag` | source shape tag / previous tag，等价 `other.Tag`。 |
| `operationPostfix` | `;:G;XTR`、`;:M;CUT`、`;:L...`、`;PSM` 等 op/postfix。 |
| `rawMappedName` | 由 codec/helper 根据上述 evidence 生成的 raw bytes。 |
| `canonicalMappedName` | 由 helper 按 FreeCAD hash/delete canonical 规则生成。 |
| `provenanceStatus` | `source_backed`、`indexed_only`、`missing_tag`、`missing_op`、`blocked`。 |

实际字段可以更小，但必须让 S3 focused tests 不需要 expected 字符串反填就能通过。

## 实施边界

第一轮做到：

- 设计并落地 producer ledger 类型。
- 在 `NamedShape` construction / maker history / preserved source / generated / modified 传播点写入 source-backed raw mapped-name provenance。
- runtime publisher 使用 ledger 输出 `mappedName.raw/canonical`。
- S3 里 p2/c4m6/p6 raw/canonical expectedFailure 能删除并普通通过。
- p5/p8 不发布 fake raw name。

后续再做：

- 非空 `childElementMapKey` parity。
- 非空 `mapperHistoryIds` parity。
- 全量 expected topo state parity。

## 关闭条件

- C13-M3 队列全部关闭。
- C13-M2 S4 blocker 从“缺 producer ledger”变成可实现状态，或 S4 直接回流关闭。
- `tests.test_topo_naming_state_response` 普通通过，不能只靠 expectedFailure。
- 没有 expected 字符串复制、fixture 分支或 adapter 输出修剪。
