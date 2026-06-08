# capabilities gap 对照表

本文件把当前 `cad_core_capabilities_json()` 暴露的能力和真实缺口对齐到 CAD Core 3.0 文档。它只记录当前基线和后续接管边界，不记录字段删除过程。

## 当前 capability 分组

| capability 分组 | 当前状态 | remaining gaps / 边界 |
| --- | --- | --- |
| `document` | FreeCAD 风格 `DocumentObject graph`、link property fields、ExternalGeometry flags、native ExternalGeo request-side pool、`elementReferenceUpdates` / `documentObjectUpdates` 已有结构化通道 | `external_geometry_lifecycle.remaining_gaps` 为空 |
| `link_transaction` | ShowElement、plain group child cache、ElementList / ElementCount owner sync、CopyOnChange transport、deep copy lifecycle 和 touched sync 已通过 update channel 表达 | remaining gaps 为空；请求 graph 仍 immutable，copy 后对象由前端保存到下一次请求 |
| `link_reference_lifecycle` | source object rename、label rename、nested Link/Group label、cross-document `FullSubList`、XLink document restore、hash mismatch、missing / pending / unloaded external document diagnostics 已有 first slice | remaining gaps 为空 |
| `topo_history` | mapper history core、producer matrix、ElementMap policy、child-map source range、ShapeFix / import / Part Offset / Offset2D / Section / DressUp / transformed / Hole history 均已拆成具体 producer 或 diagnostic | `topo_history.remaining_gaps` 为空；WireJoiner full ledger 另列为独立收口项 |
| `sketcher.solver` | conflict / redundancy / malformed / partial redundancy diagnostics、request-local DoF、dependent group metadata、常用几何关系 request-local 写回已覆盖 | remaining gaps 为空 |
| `part_workbench.offset` | `Part::Compound`、`Part::Offset`、`Part::Offset2D`、`Part::Thickness`、`Part::Section` 第一批 maker history 和 diagnostics 已覆盖 | remaining gaps 为空 |
| `part_design.body_chain` | BaseFeature / Group / Tip reroute、Origin / Datum relink、Add/Sub replay first slice 已覆盖 | remaining gaps 为空 |
| `part_design.pad_pocket` | `UpToShape` single target solid / face 主路径覆盖 | multi-face `UpToShape` 保留 diagnostic-only boundary：`unsupported_subshape_kind` |
| `part_design.hole` | thread table、head cut resource、ModelThread、profile source mapper history、compound tool shape、subtractive cut history first slice 已覆盖 | remaining gaps 为空 |
| `assembly` | grounded Fixed / Revolute / Slider / Ball / Distance / Angle 走真实 Ondsel；placement writeback lifecycle 已覆盖；representative fallback 保留 | `ondsel_solver_adapter.status=covered_full`；`representative_solver_adapter.status=covered_representative` 只表示 fallback |
| `adapters` | CLI / C ABI / Worker / WASM / streaming mesh / binary mesh 复用同一 core recompute | remaining gaps 为空；adapter 不承载建模语义 |
| `object_metadata` | Pad / Pocket / Part::Extrusion taper history metadata 已为 `covered_full` | `NamedShape.element_map_status=history_partial` 只表达当前 maker history metadata，不恢复旧 local-history gap |
| `known_gaps` | 空 | 后续新增缺口必须拆成 producer、lifecycle、feature-family、JointType 或 adapter contract 项 |

## WireJoiner capability 边界

| 项 | 当前 capability 状态 | 当前代码事实 | 后续目标 |
| --- | --- | --- | --- |
| `generated_open_export_bridge` | `covered_main_path` | `openWireCompound` child-wire ownership、history event、concrete `wire_joiner:open_export` mapper event、topo child-wire ownership consumption 和唯一 child-wire ElementMap alias 已进入主路径；three-overlap 已归零 aggregate blocker 已从公开 JSON / diagnostic_fields 删除；`historyProducerChildWireCandidate` 已从源码结构删除并进入 `deleted_fields`；`openExportProducerEdge` 读取已集中到 `wireJoinerHistoryMaterializationAHistoryProducerEdge()` / `wireJoinerHistoryMaterializationEntryHasAHistoryProducerChildWire()`，但 `WireJoinerHistoryMaterializationEdgeEntry` 仍保留该 staged producer edge | 用 `MapperHistory(aHistory)` 直接提供 open-export producer relation，再删除剩余 staged producer edge |
| `purge_as_original_bridge` | `covered_main_path` | noOriginal 公开 candidate bridge 已删除；当前 purge verdict 基于 child-wire shared-source edge ledger 和 group verdict | 让 noOriginal deleted relation 由 FreeCAD 等价 child-wire ownership 与 `MapperHistory(aHistory)` / ElementMap 消费接管 |
| `wire_joiner_open_export_element_map_unique_child_wire_alias` | covered first slice | branch open-cutter 已能把唯一 source -> InternalEdge alias 写入 `Sketch.InternalShape.element_map` | 保持 ElementMap 只写唯一 target；split / deleted / ambiguous 不猜唯一 alias |
| `wire_joiner_history_event_ledger` | covered bridge evidence | history event 已记录 relation、source lineage、splitter lineage、actual noOriginal purge 和 Modified / Generated fragment 标记 | 将 event 从 bridge evidence 收敛为 `MapperHistory(aHistory)` 的正式输入或可删除中间层 |

## 收口规则

- 不恢复笼统 `complete_mapper_history`、`known_gaps`、旧 taper local-history gap 或旧 helper output gap。
- 不用空 `remaining_gaps` 伪装 full coverage；`covered_main_path`、`covered_representative` 和 diagnostic-only boundary 必须明确写出边界。
- WireJoiner 后续只允许沿 FreeCAD `WireJoinerP::EdgeInfo / WireInfo / myShapesToReturn / MapperHistory / ElementMap` 路径推进。
- `ElementMap` 只承接唯一 target alias；split / deleted / ambiguous relation 必须保留在 `MapperHistory`、terminal history 或 diagnostics 中。
- 所有新增 capability 必须有 FreeCAD 源码依据、cad-core 落点、fixture / test 或稳定 diagnostic。

## 验收入口

文档修改：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check -- docs/CADCore3.0
```

WireJoiner full ledger 实现后优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```
