# Helper 输出迁移要求

## 背景

当前 `generated/helper result-wire` 路径的代码量不大，但它承担的是临时兜底输出：直接生成一段看起来正确的 open edge / result wire。它能让 fixture 数量短期对齐，但没有完整证明这条边在 FreeCAD WireJoiner 生命周期里来自哪个真实 producer。

因此，本迁移的难点不在于删除几百行 helper 代码，而在于删除以后必须由 FreeCAD 可追溯的拓扑身份系统接管输出。

## 大白话解释

helper 代码少，是因为它做的是“先画出来再说”：看到缺一条 open edge / result wire，就临时生成一条形状补上。这样 fixture 数量可能对了，画面也可能看起来对，但它没有交代这条边真正是谁。

CAD Core 这里不能只要“画出来”。前端后续还要拿这条边做选择、引用、重算、稳定命名和引用恢复，所以它必须知道：

- 这条边原来来自哪个 FreeCAD `EdgeInfo`。
- 它是不是 FreeCAD `aHistory->Remove(info.edge)` 记录出来的真实结果。
- 它属于哪个 wire / superEdge / current member。
- 下次 recompute 时还能不能找到同一条边。
- 后面的 `NamedShape.history` 和 `ElementMap` 能不能继续追踪它。

所以，删除 helper 不是难在删代码，而是难在把“看起来像这条边”的临时形状，换成“FreeCAD 账本里能证明身份的这条边”。如果只按数量改测试，或者在输出端裁剪到数量一致，就会留下隐藏问题：现在 fixture 过了，后面 Pad/Pocket 引用、StableSubName、ReferenceShadow 或 ElementMap 会追错对象。

## 核心要求

删除 helper 输出前，剩余每一条 result wire 都必须能回答以下问题：

1. 它来自哪个 `EdgeInfo`。
2. 它是否对应真实的 `aHistory->Remove(info.edge)` producer。
3. 它属于哪个 `WireInfo`、`superEdge` 或 current-member child wire。
4. 切换到 source/current-member 输出后，是否会被 FreeCAD `getOpenWires(noOriginal=true)` purge。
5. `NamedShape.history`、`ElementMap` 和后续引用更新是否能消费同一份 `ResultWireProducerIdentity`。

只要这些问题没有被 ledger 和 history 证明，就不能因为输出数量看起来正确而删除 helper。

## 当前迁移判定

数量对齐只能说明当前 fixture 的几何外观接近，不能单独作为通过标准。

阶段通过必须同时满足：

- `result_wire_producer_unknown_invariant_count == 0`。
- 每个 legacy helper slot 都有有限 `ResultWireProducerState` 和 `ResultWireBlocker`。
- `result_wire_producer_exported_without_helper_wire_info_count` 与迁移目标一致。
- 剩余非 0 项只能是已知 blocker，不能落回未知诊断或新增无限细分字段。
- 不能把 helper copy edge、普通 removed target 或 fixture 特判冒充成真实 source edge。

最终 P6 删除 helper/generated 输出路径必须满足：

```text
open_wire_compound_legacy_helper_shape_wire_info_count == 0
open_wire_compound_helper_open_export_override_helper_shape_wire_info_count == 0
generated_open_export_edge_info_count == 0
open_wire_compound_generated_wire_info_count == 0
result_wire_producer_exported_without_helper_wire_info_count == migrated_legacy_helper_slot_count
```

## 剩余 blocker 的处理原则

`ForeignAHistorySourceGeometryMismatch`：
已找到 foreign `aHistory->Remove(info.edge)` source，但该 source curve 不能直接代表当前 result edge。处理时必须证明 producer curve 与 result edge 的几何关系，不能只按数量补输出。

`SameSourceSidecarGeometryMismatch`：
同 source lineage 的 strict sidecar 已找到，且可能已经 source-shaped，但 sidecar curve 仍不能匹配或包含当前 result edge。处理时必须保留 sidecar producer identity，并解决几何 ownership mismatch。

`LiveResetSourceShapeWouldPurgeOriginal` / `CurrentMemberSourceShapeWouldPurgeOriginal`：
切到 source/current-member 输出后会被 FreeCAD `getOpenWires(noOriginal=true)` 视为原始线 purge。处理时必须对齐 FreeCAD purge 语义，而不是绕过 `noOriginal` gate 或输出端裁剪。

## 禁止事项

- 禁止用 fixture 名称、几何类型排序、输出数量或测试 expected 值反推业务语义。
- 禁止在 `getOpenWires()` 输出端临时裁剪 sibling member 来凑数量。
- 禁止把普通 `iteration = -1` target 当成 strict `aHistory->Remove(info.edge)` source。
- 禁止继续新增 `helper_open_export_override_*` 无限细分字段来代替有限 blocker。
- 禁止在 `sketch_object.cpp`、adapter 或导出层补 geometry 猜测来绕过 WireJoiner / topo 账本。

## 完成定义

本迁移完成不是“helper 代码被删掉”，而是：

1. 所有 legacy helper slot 都由 `ResultWireProducerIdentity` 主路径输出。
2. helper/generated shape 输出路径不可达并删除。
3. open-export history、topo history、`NamedShape.history` 和 `ElementMap` 消费同一份 producer identity。
4. 重点 fixture 和相关语义测试通过，且剩余差异不依赖 helper output。
