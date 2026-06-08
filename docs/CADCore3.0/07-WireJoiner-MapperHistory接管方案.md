# WireJoiner MapperHistory 接管方案

## 收口结论

WireJoiner full ledger 已完成当前收口：`MapperHistory(aHistory) -> ElementMap` 正式接管 open-export、noOriginal、split、deleted 关系；`generated_open_export_bridge` 与 `purge_as_original_bridge` 均声明 `covered_full`。

已删除公开字段保持删除状态：不恢复 `historyProducerChildWireCandidate`，不恢复 result-slot、endpoint materialization、helper override、noOriginal candidate 等公开字段。当前源码和 tests 不再引用 `openExportProducerEdge` 或旧 capability gap。

本方案只处理 WireJoiner full ledger。GCS / Sketch solver、Pad / Pocket UpToShape multi-face、Assembly representative fallback、Worker / WASM adapter 不在当前范围。

## 当前基线

当前 C3-M8 后状态：

- `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 标记 `wire_joiner.generated_open_export_bridge.status=covered_full`、`wire_joiner.purge_as_original_bridge.status=covered_full`，两者 `remaining_gaps` 为空。
- `cad-core/include/cad_core/part/wire_joiner.h::WireJoinerMapperHistoryProducerEvidence` 承接 `aHistory` producer shape；`WireJoinerHistoryMaterializationEdgeEntry` 只保留 EdgeInfo / WireInfo lifecycle 状态，不拥有 staged producer shape 字段。
- `cad-core/src/part/wire_joiner.cpp` 通过 `wireJoinerMapperHistoryProducerEvidenceReady()`、`wireJoinerMapperHistoryProducerEvidenceEdge()` 和 `resultWireProducerMapperHistoryInputEdge()` 消费正式 mapper evidence；source/vmap fallback gate 只在 aHistory/openWireCompound 生命周期内生效。
- `cad-core/src/part/topo_shape.cpp` 消费 WireJoiner history event、child-wire ownership 和 mapper evidence；唯一 child-wire source -> InternalEdge relation 写入 `Sketch.InternalShape.element_map`，split / deleted / ambiguous 留在 mapper history / terminal history / diagnostics。
- P5 / P6 oracle 约束保持：open-cutter / cross / T / segmented cutter 不丢 InternalEdge 与 split history；three-overlap `InternalVertex` 为 19；branch open-cutter 唯一 alias `Edge5 -> InternalEdge10` 成立。

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

## 已完成语义

- Open-export relation 来源收敛到 `aHistory` producer evidence、`sourceEdges`、openWireCompound child-wire ownership 和 typed `wire_joiner:open_export` event；不从 `ResultWireProducerIdentity`、endpoint evidence、输出顺序或 fixture 名称猜 relation。
- Mapper history event 覆盖 preserved / generated / split / deleted，noOriginal purge 写 terminal deleted history，ambiguous split 保持 `needs_reselect`。
- `ElementMap` 只写唯一 target alias；branch open-cutter 保持 `Sketch.InternalShape.element_map.Edge5=InternalEdge10`。split / deleted / ambiguous 不写成唯一 alias。
- Adapter capability 删除 WireJoiner open-export producer-edge remaining gap；两个 bridge 均为 `covered_full`。

## 验收矩阵

| 验收项 | 必须证明 |
| --- | --- |
| bridge 删除 | 源码和 tests 不再引用旧 per-edge producer bridge；已删除公开字段保持删除 |
| capability 收口 | `generated_open_export_bridge.remaining_gaps=[]`，`purge_as_original_bridge.remaining_gaps=[]` |
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
