# capabilities gap 对照表

本文件把当前 `cad_core_capabilities_json()` 暴露的能力和真实缺口对齐到 CAD Core 3.0 文档。它只记录当前基线和后续接管边界，不记录字段删除过程。

## 当前 capability 分组

| capability 分组 | 当前状态 | remaining gaps / 边界 |
| --- | --- | --- |
| `document` | FreeCAD 风格 `DocumentObject graph`、link property fields、ExternalGeometry flags、native ExternalGeo request-side pool、`elementReferenceUpdates` / `documentObjectUpdates` 已有结构化通道 | `external_geometry_lifecycle.remaining_gaps` 为空 |
| `link_transaction` | ShowElement、plain group child cache、ElementList / ElementCount owner sync、CopyOnChange transport、deep copy lifecycle 和 touched sync 已通过 update channel 表达 | remaining gaps 为空；请求 graph 仍 immutable，copy 后对象由前端保存到下一次请求 |
| `link_reference_lifecycle` | source object rename、label rename、nested Link/Group label、cross-document `FullSubList`、XLink document restore、hash mismatch、missing / pending / unloaded external document diagnostics 已有 first slice | remaining gaps 为空 |
| `topo_history` | mapper history core、producer matrix、ElementMap policy、child-map source range、ShapeFix / import / Part Offset / Offset2D / Section / DressUp / transformed / Hole history 均已拆成具体 producer 或 diagnostic；WireJoiner full ledger 已接入 `MapperHistory(aHistory) -> ElementMap` | `topo_history.remaining_gaps` 为空 |
| `sketcher.solver` | conflict / redundancy / malformed / partial redundancy diagnostics、request-local DoF、dependent group metadata、常用几何关系 request-local 写回已覆盖 | remaining gaps 为空 |
| `part_workbench.offset` | `Part::Compound`、`Part::Offset`、`Part::Offset2D`、`Part::Thickness`、`Part::Section` 第一批 maker history 和 diagnostics 已覆盖 | remaining gaps 为空 |
| `part_design.body_chain` | BaseFeature / Group / Tip reroute、Origin / Datum relink、Add/Sub replay first slice 已覆盖 | remaining gaps 为空 |
| `part_design.pad_pocket` | `UpToShape` single target solid / face 与多面 LinkSubList 主路径覆盖；Pad / Pocket 成功 fixture 已覆盖，offset 多面和非 face selection 保留结构化失败诊断 | remaining gaps 为空；失败边界 diagnostics 为 `unsupported_property`、`unsupported_subshape_kind`、`invalid_subshape`、`missing_link_target` |
| `part_design.hole` | thread table、head cut resource、ModelThread、profile source mapper history、compound tool shape、subtractive cut history first slice 已覆盖 | remaining gaps 为空 |
| `assembly` | grounded Fixed / Revolute / Slider / Ball / Distance / Angle 走真实 Ondsel；placement writeback lifecycle 已覆盖；representative fallback 保留 | `ondsel_solver_adapter.status=covered_full`；`representative_solver_adapter.status=covered_representative` 只表示 fallback |
| `adapters` | CLI / C ABI / Worker / WASM / streaming mesh / binary mesh 复用同一 core recompute | remaining gaps 为空；adapter 不承载建模语义 |
| `object_metadata` | Pad / Pocket / Part::Extrusion taper history metadata 已为 `covered_full` | `NamedShape.element_map_status=history_partial` 只表达当前 maker history metadata，不恢复旧 local-history gap |
| `known_gaps` | 空 | 后续新增缺口必须拆成 producer、lifecycle、feature-family、JointType 或 adapter contract 项 |

## WireJoiner capability 边界

| 项 | 当前 capability 状态 | 当前代码事实 | 后续目标 |
| --- | --- | --- | --- |
| `generated_open_export_bridge` | `covered_full` | open-export relation 由 `WireJoinerMapperHistoryProducerEvidence`、openWireCompound child-wire ownership 和 concrete `wire_joiner:open_export` mapper event 进入 topo；唯一 child-wire ElementMap alias 已覆盖，split / deleted / ambiguous 留在 mapper history / terminal history | remaining gaps 为空 |
| `purge_as_original_bridge` | `covered_full` | noOriginal 公开 candidate bridge 已删除；deleted relation 由 child-wire shared-source ledger、`MapperHistory(aHistory)` 消费路径和 terminal history 表达 | remaining gaps 为空 |
| `wire_joiner_open_export_element_map_unique_child_wire_alias` | covered full ledger | branch open-cutter 已能把唯一 source -> InternalEdge alias 写入 `Sketch.InternalShape.element_map` | 继续保持 ElementMap 只写唯一 target；split / deleted / ambiguous 不猜唯一 alias |
| `wire_joiner_history_event_ledger` | covered full ledger | history event 已记录 relation、source lineage、splitter lineage、actual noOriginal purge 和 Modified / Generated fragment 标记，并作为 `MapperHistory(aHistory)` 正式输入被 topo 消费 | remaining gaps 为空 |

## 收口规则

- 不恢复笼统 `complete_mapper_history`、`known_gaps`、旧 taper local-history gap 或旧 helper output gap。
- 不用空 `remaining_gaps` 伪装 full coverage；`covered_representative` 和 diagnostic-only boundary 必须明确写出边界。
- WireJoiner 后续只允许沿 FreeCAD `WireJoinerP::EdgeInfo / WireInfo / myShapesToReturn / MapperHistory / ElementMap` 路径维护，不恢复 producer bridge / candidate / result-slot 公开字段。
- `ElementMap` 只承接唯一 target alias；split / deleted / ambiguous relation 必须保留在 `MapperHistory`、terminal history 或 diagnostics 中。
- 所有新增 capability 必须有 FreeCAD 源码依据、cad-core 落点、fixture / test 或稳定 diagnostic。

## 验收入口

文档修改：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check -- docs/CADCore3.0
```

WireJoiner full ledger 回归优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```
