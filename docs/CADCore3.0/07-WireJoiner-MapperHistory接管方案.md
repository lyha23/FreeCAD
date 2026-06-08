# WireJoiner MapperHistory 接管方案

## 目标

继续移除剩余 `openExportProducerEdge` history materialization staged producer edge，并保持 `historyProducerChildWireCandidate` 已删除状态，让 `MapperHistory(aHistory) -> ElementMap` 正式接管 WireJoiner 的 open-export、noOriginal、split、deleted 关系。

本方案只处理 WireJoiner full ledger。GCS / Sketch solver、Pad / Pocket UpToShape multi-face、Assembly representative fallback、Worker / WASM adapter 不在当前范围。

## 当前基线

当前 C3-M8 后状态：

- `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 仍标记 `wire_joiner.generated_open_export_bridge.status=covered_main_path`，并保留 `wire_joiner_history_materialization_ledger_open_export_producer_edge`。
- `cad-core/include/cad_core/part/wire_joiner.h::WireJoinerHistoryMaterializationEdgeEntry` 已删除 `historyProducerChildWireCandidate`；child-wire materialization candidate 由 `WireJoinerHistoryMaterializationLedger::bindings` 的 final `EdgeInfo` row 推导。该 entry 仍有 `openExportProducerEdge` staged producer edge。
- `cad-core/src/part/wire_joiner.cpp::resultWireProducerOpenExportEdge()` 仍从 history materialization entry 取 producer edge，缺失时回落到当前 `EdgeInfo.edge`。
- `cad-core/src/part/topo_shape.cpp` 已能消费 WireJoiner history event、child-wire ownership，并把唯一 child-wire source -> InternalEdge relation 写入 `Sketch.InternalShape.element_map`。
- P5 / P6 oracle 已约束：open-cutter / cross / T / segmented cutter 不丢 InternalEdge 与 split history；three-overlap `InternalVertex` 为 19；branch open-cutter 唯一 alias `Edge5 -> InternalEdge10` 成立。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()`：通过 `aHistory->Remove(info.edge)` 记录 closed-wire 消费。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`：最终按 `builder.Add(openWireCompound, info.wire())` 输出 open child wire，并重置 `sourceEdges` 为 `sourceEdgeArray`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：调用 `makeShapeWithElementMap(openWireCompound, MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op)`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`：统一消费 mapper history 并写入 ElementMap / terminal history。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp::ElementMap::getElementHistory()`：ElementMap 查询 history；唯一映射和非唯一 history 必须分层表达。

## 分层落点

| 层 | 落点 | 职责 |
| --- | --- | --- |
| `part/wire_joiner` | `cad-core/include/cad_core/part/wire_joiner.h`, `cad-core/src/part/wire_joiner.cpp` | 按 FreeCAD `EdgeInfo / WireInfo / sourceEdges / aHistory` 生命周期记录 open child wire、noOriginal、split、deleted relation；不得发布输出端修剪规则 |
| `part/topo_shape_mapper` | `cad-core/include/cad_core/part/topo_shape_mapper.h`, `cad-core/src/part/topo_shape_mapper.cpp` | 承接 WireJoiner 的 `MapperHistory(aHistory)` 事件模型，表达 preserved / generated / modified / split / deleted / ambiguous |
| `part/topo_shape` | `cad-core/src/part/topo_shape.cpp` | 消费正式 mapper history，写 `NamedShape.mapper_history`、terminal history 和唯一 `ElementMap` alias |
| `app/element_map` | `cad-core/include/cad_core/app/element_map.h`, `cad-core/src/app/element_map.cpp` | 只承接唯一 target alias；split / deleted / ambiguous 继续留在 mapper history 或 diagnostics |
| tests / fixtures | `cad-core/tests/test_p5_sketch.py`, `cad-core/tests/test_p6_topology.py`, `cad-core/tests/test_adapters.py`, `cad-core/fixtures/p5` | 固定删除 bridge 后的几何、history、ElementMap 和 capability 边界 |

## 实施步骤

### M0：冻结当前保护门

- 保留并复核 P5 open-cutter / cross / T / segmented cutter、three-overlap、branch open-cutter 的 expected。
- 在 tests 中明确断言已删除字段不得恢复：`open_wire_compound_producer_ledger_edge_materialized`、公开 noOriginal candidate、result-slot evidence、source-edge producer output/count、`summary_only:wire_joiner_history:open_export`。
- 继续允许 `generated_open_export_bridge=covered_main_path`，但只作为迁移期状态。

### M1：把 relation 统一成 `MapperHistory(aHistory)` 事件

- 在 WireJoiner part 层把 open-export relation 的来源收敛到 aHistory / sourceEdges / child-wire ownership，而不是 `ResultWireProducerIdentity` 或 output endpoint evidence。
- 事件必须覆盖：
  - open-export preserved / generated / split；
  - noOriginal deleted；
  - source edge one-to-many split；
  - source edge one-to-zero deleted；
  - ambiguous split 需要 reselect 的诊断。
- `topo_shape.cpp` 只能消费 typed mapper event，不再拼装 relation。

### M2：移除 per-edge staged producer bridge

- 删除或替换 `WireJoinerHistoryMaterializationEdgeEntry::openExportProducerEdge`。
- 保持 `WireJoinerHistoryMaterializationEdgeEntry::historyProducerChildWireCandidate` 已删除状态；不要恢复 per-edge candidate bool。
- 删除 `resultWireProducerOpenExportEdge()` 对 materialization entry producer edge 的依赖。
- 删除 capability remaining gap `wire_joiner_history_materialization_ledger_open_export_producer_edge`。
- 如果仍需要中间结构，必须命名为正式 `MapperHistory` 输入账本，而不是 producer bridge / candidate / result-slot。

### M3：ElementMap 正式接管唯一关系

- WireJoiner open-export 的唯一 source -> InternalEdge relation 写入 `ElementMap`。
- split / deleted / ambiguous 不写唯一 alias，统一留在 `NamedShape.mapper_history`、terminal history 或 diagnostics。
- noOriginal purge 后的 deleted relation 必须能从 mapper history 查到，不再只依赖 child-wire group verdict 字段解释。

### M4：capabilities 与文档冻结

- `wire_joiner.generated_open_export_bridge.status` 从 `covered_main_path` 改为 `covered_full` 的前提：
  - `openExportProducerEdge` 已从 public capability、tests 和源码结构删除，且 `historyProducerChildWireCandidate` 保持已删除；
  - open-export / noOriginal / split / deleted 关系均由正式 mapper history 进入 topo / ElementMap；
  - P5 / P6 保护 fixture 无退化；
  - adapter capability 不再保留 `wire_joiner_history_materialization_ledger_open_export_producer_edge`。
- `wire_joiner.purge_as_original_bridge.status` 升级为 full 的前提：
  - noOriginal 决策不再依赖 cad-core 独立重组账本作为语义来源；
  - deleted relation 能从 `MapperHistory(aHistory)` 消费路径证明；
  - open-cutter / cross / T / segmented cutter 不退化。

## 验收矩阵

| 验收项 | 必须证明 |
| --- | --- |
| bridge 删除 | 源码和 tests 不再引用 `openExportProducerEdge`；`historyProducerChildWireCandidate` 保持已删除 |
| capability 收口 | `generated_open_export_bridge.remaining_gaps` 不再包含 `wire_joiner_history_materialization_ledger_open_export_producer_edge` |
| open-export history | `Sketch.InternalShape.mapper_history` 仍输出 concrete `wire_joiner:open_export` event，且 relation 来自 mapper history |
| ElementMap 唯一 alias | branch open-cutter 仍固定 `Edge5 -> InternalEdge10` |
| split / deleted | split / deleted relation 不被写成错误唯一 alias，仍可在 mapper history / terminal history 查询 |
| noOriginal | dangling / split-and-dangling / pad dangling purge 成立；open-cutter / cross / T / segmented 不误删 |
| geometry | P5 three-overlap `InternalVertex` 保持 19 |

## 验收命令

短跑验证：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures
```

文档检查：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check -- docs/CADCore3.0
```

## 非目标

- 不靠 fixture 名称、bbox、面积、输出顺序、source edge 猜测补 relation。
- 不在 adapter / expected JSON / `sketch_object.cpp` 输出层修正 WireJoiner history。
- 不把 `ElementMap` 扩展成一对多 split 存储；一对多和 deleted 先留在 `MapperHistory` / terminal history。
- 不恢复已删除的 result-slot / endpoint materialization / helper override 公开字段。
