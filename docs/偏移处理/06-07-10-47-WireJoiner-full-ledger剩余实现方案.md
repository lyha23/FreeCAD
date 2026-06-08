# WireJoiner full ledger 剩余实现方案

时间：2026-06-07 10:47。

## 目标

把当前 `cad-core` 的 WireJoiner 从 `covered_main_path` bridge 推进到接近 FreeCAD `WireJoinerP` 的正式账本路径：

```text
sourceEdgeArray
  -> mutable sourceEdges / vmap replacement
  -> EdgeInfo / WireInfo / wireInfo2 lifecycle
  -> splitEdges() + BRepTools_History
  -> openWireCompound child-wire ownership
  -> getOpenWires(noOriginal)
  -> MapperHistory(aHistory)
  -> TopoShape / NamedShape / ElementMap
```

本方案只定义剩余实现路径，不把当前 bridge 写成 `covered_full`。实现过程中禁止回到输出端 pruning、fixture 名称分支、bbox / 面积 / 几何类型排序、source index 猜测或 sketch executor 合成 ownership。

## 当前基线

当前 `cad-core` 已经不是最早的输出补丁状态：

- `openExportOverride`、`helperOpenExportOverride*`、`purgeAsOriginalOpenEdge`、公开 `purge_bridge`、公开 `result_slot_vertex_evidence_output`、`EdgeInfo::openExportWire()` 等旧输出桥已删除。
- `sourceEdges_` 已作为原始 `sourceEdgeArray` ledger，`sourceEdgeLedgerEdges_` 已作为 mutable internal `sourceEdges` / vmap vertex ledger。
- `splitEdges()` fragment-to-source ledger 已区分 Modified / Generated / input EdgeInfo source sidecar，P5 open-cutter / cross / T / segmented / arc-lens 的旧 `split_fragment_identity_fallback_edge_info_count` 已固定为 0。
- open-export history / topo evidence 已直接消费 `OpenWireCompoundWireInfo::wire`，缺 child-wire 只能输出 `missing_open_wire_compound_child_wire` 诊断。
- noOriginal 公开 candidate bridge 已删除；shared-source edge ledger 已集中到 `OpenWireCompoundWireInfo` child-wire ledger，actual purge 已按最终 wire 组整体计算 `noOriginalPurgedByLedger`，`getOpenWires(noOriginal)` 只消费该组级 verdict。
- `source/vmap endpoint ledger`、`endpoint provenance ledger` 与 `noOriginal shared-source edge ledger` 已作为 child-wire 诊断账本暴露。
- 本轮新增 request-local `WireJoinerHistoryRelation`，在 `WireJoiner` open-export entry 建账时由 `OpenWireCompoundWireInfo` child-wire ledger 发布 `preserved / split / generated / deleted` 关系；`sketcher` 只转发该关系，`topo_shape.cpp` 只消费该 typed relation，旧的 topo 层 relation 拼装 fallback 已删除。
- 本轮继续新增 request-local `WireJoinerHistoryEvent`：每条 open-export entry 都挂 `wire_joiner_history_event_index`，并由 `wire_joiner_history_events` 记录 relation、source edge indices、splitter lineage、noOriginal actual purge 与 Modified / Generated fragment 标记。该 event 仍来自 child-wire ledger，只把 relation 从 entry 字段推进到 part 层可枚举 history event，不改变输出拓扑，也不替代完整 `MapperHistory(aHistory)`。
- 本轮再把 `WireJoinerHistoryEvent` 列表转入 `SketchInternalHistoryContext`：`Sketch.InternalShape.sketch_internal_history` 现在输出 `wire_joiner_history_events`，`topo_shape.cpp` 通过 event index lookup 消费 event relation/source lineage，mapper evidence 输出 `wire_joiner_history_event_consumed_by_topo=true`。entry 里的 relation/source 字段仍保留为兼容转发和诊断，但 topo 主路径不再只靠 entry 字段消费 relation。
- 本轮继续固定 topo 消费边界：`topo_shape.cpp` mapper evidence 新增 `open_wire_compound_child_wire_ownership_consumed_by_topo=true`，要求 history event 与 open-export entry 指向同一个 `openWireCompound` child-wire slot，且该 child 具备 shape identity / edge / vertex inventory；C ABI capability 的 `covered` 只新增 `topo_consumes_openWireCompound_child_wire_ownership_ledger`，而把 history materialization producer-edge 相关项列入 `remaining_gaps`。
- 本轮继续收束 open-export mapper history：当 `topo_shape.cpp` 已用 concrete `wire_joiner:open_export` source->InternalEdge event 消费 history event 与 child-wire ownership 时，`Sketch.InternalShape.mapper_history` 不再追加 `summary_only:wire_joiner_history:open_export` 兜底；C ABI capability 新增 `wire_joiner_open_export_mapper_history_concrete_events`。这只删除 summary-only 解释层，不代表完整 `MapperHistory(aHistory)` / ElementMap 生命周期已完成。
- 本轮继续收束 topo mapper evidence：`Sketch.InternalShape.mapper_history` 的 concrete `wire_joiner:open_export` evidence 不再携带 `result_wire_producer_*` identity 字段，mapper diagnostic 也不再从 `result_wire_producer_blocker` 派生 `producer_blocker:*` 状态，缺 source lineage 时输出 `missing_open_wire_compound_source_lineage` 而不是 `missing_producer_identity`；producer audit 仍保留在 `wire_joiner_ledger` / `wire_joiner_history_detail` / `Sketch.InternalShape.sketch_internal_history` entry 中。C ABI capability 新增 `topo_mapper_evidence_result_wire_producer_identity_removed` / `topo_mapper_diagnostic_result_wire_producer_blocker_removed` / `topo_mapper_diagnostic_missing_producer_identity_removed`，并把对应 mapper producer identity 字段列入 deleted_fields；这不改变 remaining per-edge producer bridge。
- 本轮新增 WireJoiner -> ElementMap 第一片：`topo_shape.cpp` 只在 event / child-wire slot 对齐、child shape identity 已记录、source->target 唯一且不覆盖既有 `element_map` 时，把 child-wire-consumed open-export history 写成 `Sketch.InternalShape.element_map` alias。当前 branch open-cutter 固定 `Edge5 -> InternalEdge10`，`element_history_status` 新增 `wire_joiner_history:element_map`；一对多 split、deleted relation 和已有 alias 仍保留 terminal mapper history，不做猜测。
- 本轮继续把 open-export ownership 前移到 `OpenWireCompoundWireInfo` child-wire slot：child-wire 现在记录 `OpenLeafIterationMinus3 / UnownedOpenEdge / AHistoryProducerChildWire / RootCurrentMemberChildProducer` 来源、EdgeInfo iteration / iteration2、owner WireInfo / wireInfo2、child shape edge/vertex inventory、history event index 与 source edge indices；公开 `open_wire_compound_export_source` 已在 final child-wire pass 中从 open-leaf / unowned / root-current-member / materialized producer-wire ledger 推导，不再直接复制 materialization edge entry 的 `openWireCompoundExportSource`；`result_wire_producer_ledger_entries` 直接消费这些 child-wire ownership 字段，并且 entry 发布 gate 已后移到 child-wire final identity，不再由 materialization edge entry 的预分类直接决定。
- 本轮把 three-overlap current-member vertex debt 从 blocked 诊断推进到 child-wire producer 输出：每个 current-member child 的输出端点都记录 member/split ledger、candidate wire 与 current child-wire output 的 `TopoDS_Vertex` identity 关系；当前 3 个 child 的 6 个输出端点均由 candidate ledger 覆盖，并以 `CurrentMemberChildWire / ExportedWithoutTransitionalSlot` 发布，不再输出 `wire_joiner_current_member_vertex_multiplicity_blocked`，endpoint-materialization、result-slot-only 以及已归零的 aggregate blocker 公开诊断也已删除。
- 本轮推进阶段 B/E 交界：`WireJoinerVmapReplacementEvent` 记录 `WireJoinerP::add()` vmap replacement 的 old vertex、new shared vertex、affected source edge / child-wire endpoint 与 replacement source edge index；`OpenWireCompoundWireInfo::EndpointProvenance` 消费 `sourceEdgeLedgerEdges_`、vmap-replaced sourceEdges、split fragment ledger 与 current-member candidate ledger。内部 `producerLedgerWireFromEndpointMaterializationEvidence`、`endpointMaterializationEvidenceVertices` 与 `WireJoinerEndpointMaterializationLedger` 已删除；公开 endpoint-materialization 兼容诊断字段也已移入 capability `deleted_fields`；three-overlap 当前 source-vmap producer wire 为 3、source-vmap endpoint matched 为 6、endpoint provenance source-vmap matched 为 1、candidate matched 为 6，`InternalVertex` oracle 更新为 19 且不退化到 16。
- 本轮继续删除公开 source-edge producer output 诊断：`openWireCompoundSourceEdgeProducerOutput`、JSON `open_wire_compound_source_edge_producer_output` 与 summary count 已移入 capability `deleted_fields`；内部 readiness helper 只私有服务 producer identity，公开历史/topo 只保留 `open_wire_compound_producer_ledger_wire_built` 证明 child-wire producer wire 物化，`open_wire_compound_producer_ledger_edge_materialized` 已移入 deleted_fields。
- 本轮继续删除 edge-level producer-ready gate：`classifyResultWireProducerSlot()` 不再因为 history materialization per-edge `openExportProducerEdge` 非空而发布 `ProducerLedgerReady`；C ABI 将 `source_shape_ready_derived_from_history_materialization_ledger_open_export_edge` 与 `edge_level_producer_ledger_ready_from_history_materialization_ledger` 移入 deleted_fields，child-wire producer edge copy gate 已移入 deleted_fields。本轮已删除 `WireJoinerHistoryMaterializationEdgeEntry::historyProducerChildWireCandidate`，child-wire materialization candidate 现在从 `WireJoinerHistoryMaterializationLedger::bindings` 的 final `EdgeInfo` row 推导；剩余 gap 只保留 per-edge `openExportProducerEdge` staged producer edge 与完整 MapperHistory / ElementMap lifecycle。

但仍有两个 capability 不能升级：

| capability | 当前状态 | 不能升级的核心原因 |
| --- | --- | --- |
| `wire_joiner.generated_open_export_bridge` | `covered_main_path` | endpoint materialization sidecar、独立 result-wire producer plan 类型、ledger 根部 `openExportProducerEdges`、binding 内 openWireCompound eligible 候选缓存、per-edge source EdgeInfo 恒等缓存、per-edge open-export gate 缓存、full AHistory 缓存、aHistory/source-lineage 复制字段、source EdgeInfo candidate list 与 per-edge `historyProducerChildWireCandidate` 布尔已删除，binding 现在只记录最终 `EdgeInfo` row；公开 `edgeInfo_resultWireProducerCandidate_internal`、公开 source 直接复制 materialization entry 的 bridge 也已移入 deleted_fields，topo evidence 已用 event/child-wire slot 对齐固定 `open_wire_compound_child_wire_ownership_consumed_by_topo`；但 history materialization per-edge `openExportProducerEdge` staged producer edge、history materialization bridge 与完整 MapperHistory / ElementMap 账本仍未接管，该 producer bridge 已列入 capability `remaining_gaps` |
| `wire_joiner.purge_as_original_bridge` | `covered_main_path` | actual purge 已改为 FreeCAD shared-source 谓词的 wire 组级 verdict；但 cad-core 仍要从多个 materialized child slot 重组 FreeCAD `openWireCompound` child wire 粒度，且 MapperHistory / ElementMap full lifecycle 未接管 |

当前最硬的 gate 是 three-overlap：

```text
open_wire_compound_producer_ledger_wire_from_result_slot_evidence* 已从公开 JSON 删除
open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked_wire_info_count 已从公开 JSON 删除
open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count 已从公开 JSON 删除
open_wire_compound_current_member_split_ledger_output_vertex_debt = 6 endpoints
open_wire_compound_current_member_split_ledger_result_slot_only_vertex* 已从公开 JSON 删除
open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count = 6
open_wire_compound_current_member_split_ledger_output_matched_vertex_count = 1
open_wire_compound_current_member_split_ledger_output_distinct_vertex_count = 6
open_wire_compound_current_member_split_ledger_candidate_distinct_vertex_count = 6
open_wire_compound_current_member_split_ledger_candidate_missing_shared_output_identity_count 已从公开 JSON 删除
open_wire_compound_producer_ledger_wire_from_source_vmap_wire_info_count = 3
open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count = 6
open_wire_compound_endpoint_provenance_wire_info_count = 15
open_wire_compound_endpoint_provenance_output_vertex_count = 30
open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count = 1
open_wire_compound_endpoint_provenance_candidate_matched_vertex_count = 6
open_wire_compound_endpoint_provenance_unmatched_vertex_count = 24
```

这说明 member/split ledger 仍未完全直接命中输出端点，但 request-local candidate ledger 已能覆盖 three-overlap 当前输出顶点，source/vmap producer 已接管 3 个 current-member child，已归零的 aggregate blocker 不再作为公开 JSON / capability diagnostic_fields 暴露，且未引入 `InternalVertex` 16 退化。后续不再是继续堆 result-slot endpoint evidence，而是继续收敛 `WireJoinerHistoryMaterializationLedger` binding / history materialization per-edge materialized child-slot bridge，并让 MapperHistory / ElementMap 正式消费 child-wire producer 账本。

## FreeCAD 依据与调用链

### 入口

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
  - 关键语义：Sketch InternalShape 先走 `FaceMakerBuildFace`，再追加 `WireJoiner` open wires。

### WireJoiner 主账本

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`
  - `sourceEdgeArray`：原始输入 edge 顺序账本。
  - `sourceEdges`：会被 `add()` / vmap replacement 改写的 mutable source 集合。
  - `vmap`：端点邻近搜索和 vertex identity replacement 账本；`add()` 的关键注释是确保 coincident vertices 实际成为同一个 `TopoDS_Vertex`。
  - `EdgeInfo`：保存 edge、端点、`iteration`、`iteration2`、`wireInfo`、`wireInfo2`、`superEdge`。
  - `WireInfo`：保存 closed / tight-bound owner wire 的 vertices、done / purge 生命周期。
  - `splitEdges()`：用 splitter 修改 edge，并写 `aHistory->AddModified(...)` / generated history。
  - `buildClosedWire()`：closed wire 成功后对 consumed edge 调用 `aHistory->Remove(...)`。
  - `findTightBound()` / `exhaustTightBound()`：通过 `wireInfo2` 和 owner 转移处理 tight bound / repeated split。
  - `build()`：构建 `openWireCompound`，open export 条件是 `iteration == -3 || (!wireInfo && iteration >= 0)`。
  - `getOpenWires(noOriginal=true)`：先从 `openWireCompound` 取 child wires，再用 `source.findSubShapesWithSharedVertex(TopoShape(edge, -1))` 判断 noOriginal purge，最后调用 `shape.makeShapeWithElementMap(comp, MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op)`。

### FaceMaker / MapperHistory / ElementMap

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::Build()` / `postBuild()`
  - `myShapesToReturn` 是 FaceMaker 最终返回 shape 的来源。
  - `postBuild()` 会用 `MapperHistory(myPreSplitHistory)` 和 splitter history 建 ElementMap。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`
  - `MapperHistory(aHistory)` 是 WireJoiner history 进入 TopoShape / ElementMap 的正式入口。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
  - 承接 `TopoShape` mapper 消费和 subshape history。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
  - 承接 stable subname、child map、terminal split / deleted / ambiguous history。

### 本轮 cad-core 分层映射

- `WireJoinerP::add()` vmap replacement：落在 `cad-core/src/part/wire_joiner.cpp::edgeWithLedgerVertexReplacements()`、`sourceEdgeLedgerEdges_` 与 `WireJoinerVmapReplacementEvent`；只记录 typed vertex identity，不在 sketcher / topo 输出端猜 ownership。
- `WireJoinerP::splitEdges()` history：落在 `splitEdgesAtIntersections()`、`SplitEdgeRecord`、`splitFragmentProducerLedgerEdgesByEdgeInfo` 与 `splitFragmentProducerLedgerEventsByEdgeInfo`；Modified / Generated / input EdgeInfo sidecar 只提供 source lineage 和 replacement event evidence。
- `WireJoinerP::build()` openWireCompound：落在 `recordOpenWireCompoundLedger()` 与 `OpenWireCompoundWireInfo`；child-wire endpoint provenance 消费 mutable source/vmap/split ledger 与 current-member candidate，不再消费 endpoint materialization evidence。
- `WireJoinerP::getOpenWires(noOriginal)` + `MapperHistory(aHistory)`：当前只由 `getOpenWires()`、`WireJoinerHistoryEvent`、`wire_joiner_history_detail` 和 `topo_shape.cpp` evidence 消费 typed child-wire ledger；完整 `MapperHistory(aHistory) -> ElementMap` full 仍是后续，不在本轮实现。

## cad-core 剩余债务

### 1. vmap replacement 还没有完全成为输出 identity 来源

已有：

- `sourceEdgeLedgerEdges_` 保存 mutable source/vmap ledger。
- `open_wire_compound_source_vmap_endpoint_ledger_*` 统计 output endpoint 是否被 source/vmap/split ledger 覆盖。
- `open_wire_compound_endpoint_provenance_*` 保存最终 child-wire 输出端点对 source/vmap、vmap replacement event 与 current-member candidate 的 identity 覆盖计数；endpoint-materialization 公开计数已删除。

剩余：

- three-overlap 仍是 `30` 个 output endpoint，其中 source/vmap endpoint ledger matched 已推进到 6，最终 endpoint provenance source-vmap matched 为 1。
- three-overlap endpoint provenance 显示 `candidate_matched=6`；P5 cross / T 等非退化 gate 仍保留原 source-vmap producer matched 12 / 11，最终输出 provenance 不再暴露 endpoint materialization 命中。
- `endpointMaterializationEvidenceVertices`、独立 result-wire producer plan 类型、ledger 根部 `openExportProducerEdges`、binding 内 openWireCompound eligible 候选缓存、per-edge source EdgeInfo 恒等缓存、per-edge open-export gate 缓存、full AHistory 缓存、aHistory/source-lineage 复制字段、source EdgeInfo candidate list 与 per-edge `historyProducerChildWireCandidate` 布尔已从 child-wire 建账边界删除；binding 现在只保留最终 `EdgeInfo` row 与 request-local result-slot endpoint evidence。剩余不是 endpoint sidecar 或恒等/并行缓存，而是 history materialization per-edge `openExportProducerEdge` staged producer edge / producer bridge。

删除条件：

- non-current-member 的 source-vmap producer 全部不依赖 endpoint materialization evidence。
- three-overlap 的 current-member child 输出顶点不再是 result-slot-only identity。

### 2. splitEdges fragment-to-source ledger 仍没有替代所有 result producer bridge

已有：

- Modified / Generated / input EdgeInfo source sidecar 已拆分。
- open-cutter / cross / T / segmented / arc-lens 的旧 geometry identity fallback 已归零。

剩余：

- history materialization per-edge `openExportProducerEdge` 仍在 child-wire 建账边界提供内部 staged producer edge；public `open_wire_compound_export_source` 已不再直接复制该 per-edge source。
- EdgeInfo `partialSharedClosedWireProducer` 与 `resultWireProducerSourceEdgeInfo*`/eligible/full-aHistory-evidence 与 `resultWireProducerSuperEdge*` 缓存已删除，binding 内 openWireCompound eligible 候选缓存、per-edge source EdgeInfo bool/index 恒等缓存、per-edge open-export gate 缓存、full AHistory 缓存、aHistory/source-lineage 复制字段和 per-edge `historyProducerChildWireCandidate` 布尔也已删除；对应分类直接读取 `WireJoinerHistoryMaterializationLedger` binding 的 final EdgeInfo row / per-edge entry，并按 EdgeInfo 当前 `iteration / wireInfo / buildClosedWire / aHistory Remove` 状态即时判断 open-export gate、full AHistory evidence 与 source-lineage sidecar；`ResultWireProducerIdentity` 和 producer 分类细节仍由 history materialization binding / per-edge entry 承载。

删除条件：

- child-wire ownership 能直接由 `EdgeInfo / WireInfo / aHistory / myShapesToReturn` 形成，不需要 history materialization binding 重新提供 export-source gate。

### 3. openWireCompound child-wire ownership 仍未完全等价 FreeCAD

已有：

- `OpenWireCompoundWireInfo` 已承接 source lineage、splitter lineage、producer ledger wire、noOriginal verdict、current-member blocker。
- open-export history 和 topo evidence 读取 child-wire ledger，不回读 EdgeInfo helper。
- child-wire slot 已显式记录 open export source、EdgeInfo iteration / iteration2、owner WireInfo / wireInfo2、root/current-member child producer 标记、history event index 和 child shape identity inventory；公开 open export source 已由 final child-wire identity 推导。
- `resultWireProducerLedgerEntryForChildWire()` 已直接消费 child-wire ownership，并把同一字段转发到 `wire_joiner_ledger` / `wire_joiner_history_detail` / `Sketch.InternalShape` history / topo evidence。

剩余：

- child-wire ownership 已接近 `WireJoinerP::build()` export condition，public source 不再依赖 materialization entry copy，per-edge `historyProducerChildWireCandidate` 布尔也已删除；但 history materialization per-edge `openExportProducerEdge` 仍在建账边界提供内部 staged producer edge。
- history materialization per-edge `openExportProducerEdge` 仍作为 producer wire 物化 evidence；公开 noOriginal candidate gate 已删除。

删除条件：

- child-wire slot 能追溯到 EdgeInfo export condition、WireInfo ownership、aHistory relation、sourceEdges source set。
- 缺 child-wire 的公开 history 分支长期保持 0，并且不需要 EdgeInfo 重导出。

### 4. current-member vertex identity 已从 blocker 推进为删除前验证项

已有：

- current-member split vertex debt 已进入 child-wire ledger。
- `wire_joiner_current_member_vertex_multiplicity_blocked` mapper diagnostic 已归零；three-overlap 3 个 current-member child 现在以 `split_stable_subname` history event 和 `CurrentMemberChildWire / ExportedWithoutTransitionalSlot` producer identity 输出。
- endpoint provenance ledger 已把 three-overlap 3 个 current-member child 的 6 个输出端点定位为 `source_vmap=1`、`candidate=6`；source-vmap endpoint ledger matched 为 6。
- per-endpoint output vertex debt 已记录每个输出端点的 member/split、candidate 和 current output identity 状态；当前 6 个输出端点中 1 个直接命中 raw member/split 顶点，6 个均命中 candidate ledger；旧 `result_slot_only_identity` 字段已移入 capability `deleted_fields`。

剩余：

- `output_matched_vertex_count=1`。
- `output_candidate_matched_vertex_count=6`。
- `result_slot_only_vertex*` 公开字段已删除，后续不再作为 gate 使用。

删除条件：

- 删除 endpoint materialization sidecar 后，candidate ledger 仍覆盖当前输出顶点 identity。
- three-overlap `InternalVertex` 数量当前固定为 19，不退化到 16 或其它错误值。

### 5. noOriginal purge 仍不是完全 FreeCAD 等价

已有：

- per-child-wire edge / matched / unmatched 计数已按 `findSubShapesWithSharedVertex` 的几何+端点谓词记录。
- actual purge 已在 `recordOpenWireCompoundLedger()` 后按最终 wire 组整体计算：只有组内所有 edge 都命中原始 source compound 时才标记 `noOriginalPurgedByLedger`。
- `branch-open-cutter` 已固定为 2 条 child edge 只有 1 条 shared-source matched，因此不 purge；T / segmented 中 exact-source child 也会因所在 wire 组含非 source split fragment 而保留。

剩余：

- cad-core 的 `openWireCompound` child slot 仍不是 FreeCAD `getSubTopoShapes(TopAbs_WIRE)` 的完整等价物，需要在账本内按 endpoint connectivity 重组 wire 粒度。
- 公开 noOriginal candidate 字段与 source/split candidate bridge 已删除；后续仍需让完整 `WireInfo / EdgeInfo / openWireCompound / MapperHistory` 生命周期直接产出真实 child-wire ownership，避免 cad-core 继续依赖重组账本模拟 FreeCAD child-wire 粒度。

删除条件：

- child-wire ownership 和 source/vmap identity 完整后，actual noOriginal purge 仅由 FreeCAD source compound shared-vertex predicate 和真实 `openWireCompound` wire ownership 决定；公开 candidate bridge 保持删除状态。

### 6. MapperHistory -> ElementMap 消费还不是完整 WireJoiner 路径

已有：

- NamedShape / ElementMap producer evidence 已优先消费 child-wire source/noOriginal verdict。
- 唯一 source->InternalEdge 且 event / child-wire slot 对齐的 WireJoiner open-export entry 已能写入 `NamedShape.element_map`；P5 branch open-cutter 固定 `Edge5 -> InternalEdge10`，并输出 `wire_joiner_history:element_map`。

剩余：

- WireJoiner 自己的 `aHistory` 还没有完整进入正式 `MapperHistory(aHistory)` producer matrix。
- deleted / modified / generated / split / ambiguous 的 terminal relation 仍有部分通过诊断解释；一对多 split 仍不写唯一 alias，必须等完整 ElementMap history 解析。

删除条件：

- `TopoShape::makeShapeWithElementMap(..., MapperHistory(aHistory), sourceEdges, op)` 的 cad-core 等价 API 能消费 WireJoiner history。
- `Sketch.InternalShape` 的 `InternalEdgeN / InternalVertexN` source trace 来自 ElementMap history，不来自 sketcher 或 adapter 输出修正。

## 实施阶段

### 阶段 A：冻结当前 bridge 边界和防回退测试

目标：确保后续改动不能重新引入旧输出补丁。

实现项：

1. 在 `tests.test_adapters` 中继续固定 deleted fields：
   - `helper_open_export_override`
   - `purge_bridge`
   - `result_slot_vertex_evidence_output`
   - `transitional_result_slot_candidate`
   - `edge_info_open_export_wire_helper`
2. 在 P5/P6 helper 中固定：
   - `missing_open_wire_compound_child_wire == false`
   - `open_wire_compound_missing_child_wire_history_edge_info_count == 0`
   - noOriginal shared-source matched edge + unmatched edge == child edge count
   - source/vmap endpoint matched <= output
   - current-member vertex debt matched + unmatched == ledger count
3. 对 `branch-open-cutter`、`through-open-cutter`、`cross-cutters`、`segmented-cross-cutter`、`t-cutter` 保留 hard oracle，防止 child-level shared-source purge 误删 split fragment。

验收：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

### 阶段 B：把 vmap replacement 从诊断推进到 child-wire identity

目标：让 `WireJoinerP::add()` 的 vmap replacement 成为 child-wire 顶点 identity 的正式来源。

实现落点：

- `cad-core/include/cad_core/part/wire_joiner.h`
- `cad-core/src/part/wire_joiner.cpp`
- 必要时补 `cad-core/src/part/topo_shape*.cpp` 中的 vertex identity helper。

实现项：

1. 把当前 `sourceEdgeLedgerEdges_` 拆成显式结构，例如：
   - 原始 `sourceEdgeArray` ledger。
   - mutable `sourceEdges` ledger。
   - vmap replacement event ledger：old vertex、new shared vertex、affected edge。
2. `recordOpenWireCompoundLedger()` 不再只统计 endpoint 是否匹配 source/vmap vertex，而是保存 per-child-wire endpoint replacement provenance。
3. producer wire materialization 优先使用 vmap-replaced edge/wire；ledger 明确缺失时只保留 unmatched / blocker 诊断，不恢复 endpoint materialization diagnostic。
4. 对 three-overlap current-member child，不直接切换 candidate wire；先证明 candidate / member / output vertex 三方 identity 关系。

新增测试：

- P5 cross / segmented / T：source-vmap producer 计数保持 4，旧 result-slot evidence 公开字段保持删除状态，matched endpoint 不退化。
- P5 three-overlap：保留当前输出，但新增 per-endpoint provenance，证明 6 个 current-member output vertex 已由 candidate ledger 覆盖，且不再是 result-slot-only identity。

本轮进度：

- 已新增 `OpenWireCompoundWireInfo::EndpointProvenance` child-wire 账本，记录每个最终输出端点是否命中 source/vmap ledger、vmap replacement event 或 current-member candidate ledger。
- `wire_joiner_ledger` / `wire_joiner_history_detail` / `Sketch.InternalShape` history / `topo_shape.cpp` evidence 均转发 `open_wire_compound_endpoint_provenance_*`；`generated_open_export_bridge.covered` 新增 `child_wire_endpoint_provenance_ledger`。
- 旧 `open_wire_compound_source_vmap_endpoint_ledger_*` 仍保留为 producer materialization gate；three-overlap current-member source-vmap matched 已推进到 6，新增 endpoint provenance 表达最终 child-wire 输出端点状态。
- 本轮已删除 endpoint materialization bridge 字段：`endpointMaterializationEvidenceVertices`、`producerLedgerWireFromEndpointMaterializationEvidence` 与 `WireJoinerEndpointMaterializationLedger`；独立 result-wire producer plan 类型也已折叠进 `WireJoinerHistoryMaterializationLedger`，ledger 根部 `openExportProducerEdges` 已并入 per-edge entry，binding 内 openWireCompound eligible 候选缓存、per-edge source EdgeInfo bool/index 恒等缓存、per-edge open-export gate 缓存、full AHistory 缓存、aHistory/source-lineage 复制字段、source EdgeInfo candidate list 和 per-edge `historyProducerChildWireCandidate` 布尔已删除。仍保留 history materialization per-edge `openExportProducerEdge` / producer entry。

删除条件：

- 独立 result-wire producer plan 不再作为 child-wire producer 输出的必要解释层；剩余 binding 必须继续收敛到 WireJoiner history / child-wire ownership。
- history materialization per-edge `openExportProducerEdge` bridge 收敛到 child-wire ownership / history event 账本。

### 阶段 C：把 splitEdges history 做成 WireJoinerHistory 正式账本

目标：让 fragment-to-source 不再依赖 sidecar 命名，而是形成可被 MapperHistory 消费的 WireJoiner history。

实现落点：

- `cad-core/src/part/wire_joiner.cpp`
- `cad-core/src/part/topo_shape_mapper.*`
- `cad-core/src/part/topo_shape_expansion.*`

实现项：

1. 引入 request-local `WireJoinerHistory` 或等价结构，字段最少覆盖：
   - source edge。
   - modified fragments。
   - generated fragments。
   - removed / consumed source edges。
   - child-wire relation。
2. `splitEdges()` 写入该 history，而不是只写 EdgeInfo sidecar。
3. `buildClosedWire()` 的 consumed / deleted 写入同一 history。
4. `openWireCompound` child-wire 构造时保存 history handle / relation id。
5. `topo_shape.cpp` 的 producer evidence 从 child-wire history 读 relation，不再拼装 split source arrays。

本轮进度：

- 已完成第 1 / 4 / 5 项的第一片：`WireJoinerHistoryRelation` 先覆盖 open-export relation id，来源是 child-wire ledger 上已存在的 source lineage、splitter lineage、actual noOriginal purge verdict 与 result-wire producer identity。
- 已完成第 1 项的第二片：`WireJoinerHistoryEvent` 已进入 `WireJoinerHistorySummary`，runtime `wire_joiner_history_detail` 输出 `wire_joiner_history_events`、`wire_joiner_history_event_count` 和 `wire_joiner_history_event_from_child_wire_ledger_count`；open-export entry、`Sketch.InternalShape` history 和 topo evidence 只转发 `wire_joiner_history_event_index` / `wire_joiner_history_event_from_child_wire_ledger`。
- 已完成第 1 / 5 项的第三片：`SketchInternalHistoryContext` 现在持有 `SketchInternalWireJoinerHistoryEvent` 列表，`topo_shape.cpp` 的 open-export mapper evidence 通过 event index 查找该列表，并优先使用 event relation/source edge indices；`generated_open_export_bridge.covered` 新增 `topo_consumes_wire_joiner_history_event_ledger`。
- `wire_joiner_history_relation_from_child_wire_ledger` 已进入 open-export history、`Sketch.InternalShape` history、topo evidence 与 C ABI capability covered list；测试固定 topo 消费该字段后仍得到原有 `preserved / split / generated / deleted` 关系。
- `topo_open_export_relation_fallback_removed` 已进入 C ABI capability covered list；`topo_shape.cpp` 不再从 source arrays、split flags、producer kind 或 exported geometry 重建 relation，缺 child-wire ledger 时不转发默认 `Preserved` relation。
- 这还不是完整 `WireJoinerHistory`：`splitEdges()` / `buildClosedWire()` 的 aHistory producer matrix、ambiguous split 与 ElementMap terminal relation 仍未全部迁入正式 mapper。

测试：

- open-cutter：一条 source edge 一对多 split fragment。
- cross / T / segmented：bounded face 与 open wire 同时存在，split history 不丢。
- splitter 失败时继续使用原 edge，并输出 diagnostic，不猜 source。

删除条件：

- `split_fragment_source_identity_fallback_*` 和 `split_fragment_history_shape_geometry_bridge_*` 在 P5/P6 保持 0 后，降为内部 debug 或删除公开字段。
- topo 层 legacy relation 拼装 fallback 已删除；后续不得用 sketcher / adapter 合成 relation，也不得恢复 topo 层按 source arrays、split flags 或 producer kind 的 relation 推断。

### 阶段 D：补齐 openWireCompound child-wire ownership

目标：让 child-wire 成为 WireJoinerP build 过程的自然产物，而不是 history materialization binding 解释后的输出。

实现项：

1. 在 `EdgeInfo` / `WireInfo` lifecycle 里显式记录 open export source：
   - `iteration == -3` open leaf。
   - `!wireInfo && iteration >= 0` unowned open edge。
   - current-member / root superEdge producer。
2. `openWireCompound` child-wire slot 保存：
   - source EdgeInfo index。
   - owner WireInfo / wireInfo2。
   - source edge lineage。
   - WireJoinerHistory relation id。
   - child shape identity。
3. history materialization per-edge `openExportProducerEdge` 只允许作为 temporary bridge 读取，不再驱动 public producer entry；`historyProducerChildWireCandidate` 布尔已删除，候选从 binding final `EdgeInfo` row 推导。
4. `resultWireProducerLedgerEntry` 改为从 child-wire ownership 发布，不从 EdgeInfo candidate 发布。

测试：

- `open_wire_compound_wire_info_count == open_export_edge_info_count`。
- `open_wire_compound_missing_child_wire_history_edge_info_count == 0`。
- `result_wire_producer_ledger_entries` 数量与 child-wire producer entry 对齐。

删除条件：

- history materialization per-edge `openExportProducerEdge` 可删除或降为私有 debug。

### 阶段 E：收口 three-overlap current-member vertex identity

目标：在已删除 result-slot endpoint materialization sidecar 后，保持 three-overlap `InternalVertex` oracle 不退化，并继续收敛 producer bridge。

实现项：

1. 追踪 three-overlap current-member child 的三类 vertex：
   - member/split ledger vertices。
   - candidate wire vertices。
   - current output child-wire vertices。
2. 继续固定 candidate 切换后的 `InternalVertex` oracle，避免回到 16 或其它错误值。
3. 删除 `endpointMaterializationEvidenceVertices` 后，确认 history/topo evidence 仍只消费 candidate / child-wire producer identity。
4. 若删除 sidecar 后出现退化，回到 child-wire vertex ledger 补身份来源，不恢复 result-slot endpoint 输出。

新增测试：

- three-overlap 专项断言：
  - `vertex_multiplicity_blocked_wire_info_count` 公开字段不存在，旧值只保留在 capability `deleted_fields`。
  - `output_unmatched_vertex_count` 公开字段不存在，旧 `result_slot_only_vertex*` 字段不存在。
  - `output_candidate_matched_vertex_count=6` 覆盖当前输出顶点。
- 必须验证 `InternalVertex` 数量不退化。

删除条件：

- `open_wire_compound_producer_ledger_wire_from_result_slot_evidence*` 公开字段已删除。
- `producerLedgerWireFromEndpointMaterializationEvidence` 已删除。
- `endpointMaterializationEvidenceVertices` 已删除。

### 阶段 F：noOriginal purge 完全切到 FreeCAD shared-source predicate

目标：公开 split/source candidate bridge 已删除，继续把 noOriginal 剩余债务收敛到真实 FreeCAD child-wire ownership + source compound shared-vertex 语义。

实现项：

1. 只在完整 child-wire ownership 上执行 noOriginal。
2. 对每条 child edge 执行等价：

```text
source.findSubShapesWithSharedVertex(TopoShape(edge, -1)).empty()
```

3. actual purge 由“所有 child edge 都 matched”决定。
4. `branch-open-cutter` shared-source matched/unmatched 继续不 purge。
5. open-cutter / cross / T / segmented 不得因为 child-level shared-source purge 少 InternalEdge / split history。

删除条件：

- `sourceEdgeArrayOriginalOpenEdgeCandidate` 已删除状态保持。
- child-wire source vertex / `splitFromInputEdge` / producer state 组合 candidate 公开字段保持删除，noOriginal 判定只消费 shared-source edge ledger 与组级 verdict。
- `purge_as_original_bridge.status` 才能考虑从 `covered_main_path` 推进。

### 阶段 G：MapperHistory(aHistory) -> ElementMap 正式消费

目标：让 WireJoiner 的 modified / generated / deleted / split history 通过 TopoShape / ElementMap 进入 stable subname，而不是停留在 producer evidence。

实现项：

1. 在 `part/topo_shape_mapper.*` 或 `part/topo_shape_expansion.*` 增加 WireJoinerHistory mapper。
2. `getOpenWires()` 的 cad-core 等价路径输出：

```text
makeShapeWithElementMap(comp, MapperHistory(wireJoinerHistory), sourceEdges, op)
```

3. `NamedShape.mapper_history` 只记录 mapper 消费后的 relation。
4. `ElementMap` terminal 状态覆盖：
   - split。
   - deleted / no_original_purge。
   - ambiguous split requires reselect。
   - generated open wire。
5. sketcher 只消费 part/topo API 返回结果，不合成 `InternalEdgeN / InternalVertexN`。

测试：

- `Sketch.InternalShape.element_map` 中 noOriginal purged edge 不出现唯一 active map。
- split fragment source trace 来自 WireJoinerHistory。
- bounded face + open wire 混合时 InternalFace / InternalEdge / InternalVertex 稳定。

删除条件：

- WireJoiner producer evidence 不再需要解释 subname 修正，只作为 debug / audit。
- `generated_open_export_bridge.status` 可从 `covered_main_path` 推进到更接近 full 的状态。

本轮进度：

- open-export relation 已从 topo 层字段推断前移到 WireJoiner child-wire ledger，并以 typed relation 和 request-local `WireJoinerHistoryEvent` 进入 runtime history / `SketchInternalHistoryContext` / topo evidence；topo legacy relation fallback 已删除，且 open-export mapper evidence 已通过 event index 消费 event relation/source lineage。这只是 `MapperHistory(aHistory)` 正式消费前的第一片，尚未替代 `TopoShape::makeShapeWithElementMap(..., MapperHistory(aHistory), sourceEdges, op)`。
- open-export mapper history 已不再输出 `summary_only:wire_joiner_history:open_export`：已有 child-wire event 和 ownership evidence 时，`Sketch.InternalShape.mapper_history` 只保留 concrete `wire_joiner:open_export` source->InternalEdge event。该改动删除的是 summary-only 兜底，不删除 producer evidence，也不改变 `generated_open_export_bridge.status=covered_main_path`。
- topo mapper evidence 已不再输出 `result_wire_producer_*` identity 字段，mapper diagnostic 也不再输出 `producer_blocker:*` / `source_shape_identity_not_ready` / WireJoiner `missing_producer_identity` 这类 producer-identity 派生状态：这些字段和 blocker 仍在 producer ledger / history detail 中作为 audit，不再作为 mapper event 解释层字段。该改动只删除 topo mapper 对 producer identity 的转发，不删除 child-wire ownership 或 producer ledger。
- topo result-wire identity 0 值计数已删除：`named_shape_history_missing_result_wire_identity_count` 与 `element_map_result_wire_identity_mismatch_count` 不再从 topo 回读 producer identity 生成，C ABI 用 `topo_result_wire_identity_counters_removed` 固定。后续 NamedShape / ElementMap 接收状态只看 history event、child-wire ownership、ElementMap alias 和 terminal mapper history。
- WireJoiner ElementMap 第一片已接入：唯一 child-wire-consumed source->InternalEdge relation 现在可成为 `Sketch.InternalShape.element_map` alias，branch open-cutter 固定 `Edge5 -> InternalEdge10`；ambiguous split 和 existing alias 不覆盖，full lifecycle 仍未完成。
- openWireCompound child-wire ownership 已新增 export source / owner WireInfo / child shape identity / history event index，并由 result-wire producer entry 直接消费；producer entry 是否发布现在由 child-wire final identity 后置决定，`result_wire_producer_entry_gate_from_materialization_entry_identity` 已进入 deleted_fields。公开 `open_wire_compound_export_source` 也已由 final child-wire identity 推导，`open_wire_compound_export_source_from_materialization_entry` 已进入 deleted_fields。per-edge `historyProducerChildWireCandidate` 布尔已删除，child-wire materialization candidate 从 binding final `EdgeInfo` row 推导；history materialization per-edge `openExportProducerEdge` 仍是建账边界的 temporary bridge，尚未删除。
- C ABI 已用 `result_wire_producer_entry_gate_after_child_wire_identity` 固定该后置 gate：public entry 只由 `OpenWireCompoundWireInfo::resultWireProducer` final identity 发布，不再由 `WireJoinerHistoryMaterializationEdgeEntry::resultWireProducer` 预分类直接发布。
- three-overlap current-member vertex debt 已从 blocked 诊断推进为 producer 输出：6 个输出端点均命中 candidate ledger，source-vmap endpoint ledger matched 为 6，旧 result-slot-only 字段和已归零 aggregate blocker 字段已删除，mapper history 不再输出 `wire_joiner_current_member_vertex_multiplicity_blocked`。
- 当前 three-overlap `output_candidate_matched_vertex_count=6`、`open_wire_compound_producer_ledger_wire_from_source_vmap_wire_info_count=3`，`CurrentMemberChildWire / ExportedWithoutTransitionalSlot` 已覆盖原 `SuperEdgeRoot` current-member 输出；`InternalVertex` oracle 固定为 19。旧 `candidate_missing_shared_output_identity_count` / `vertex_multiplicity_blocked_wire_info_count` / `output_unmatched_vertex_count` 只作为已删除字段保存在 capability `deleted_fields`。
- history materialization per-edge `openExportProducerEdge` 与 MapperHistory / ElementMap full lifecycle 仍未收口；当前 ElementMap 只覆盖唯一 child-wire alias 第一片，noOriginal 公开 candidate bridge 已删除，但 cad-core 仍通过重组账本模拟 FreeCAD child-wire 粒度，因此 `generated_open_export_bridge` 与 `purge_as_original_bridge` 继续保持 `covered_main_path`。

## 字段删除顺序

不要一次性删所有 bridge。建议按以下顺序收口：

1. 非 current-member 的 endpoint materialization evidence 归零后，删除对应公开诊断或改成 current-member-only。
2. `split_fragment_source_identity_fallback_*` / `split_fragment_history_shape_geometry_bridge_*` 保持 0 后，降为内部 debug。
3. child-wire ownership 自足后，删除 history materialization per-edge `openExportProducerEdge` 与公开 `result_wire_producer_ledger_entries` 对 EdgeInfo candidate 的依赖。
4. 已删除 `endpointMaterializationEvidenceVertices`、`producerLedgerWireFromEndpointMaterializationEvidence`、公开 endpoint-materialization 兼容诊断、独立 result-wire producer plan 类型、binding 内 openWireCompound eligible 候选缓存、per-edge source EdgeInfo bool/index 恒等缓存、per-edge open-export gate 缓存、full AHistory 缓存、aHistory/source-lineage 复制字段、source EdgeInfo candidate list、per-edge `historyProducerChildWireCandidate` 布尔，以及 three-overlap 已归零 aggregate blocker 公开字段；下一步收敛 history materialization per-edge producer bridge。
5. noOriginal full 前，公开 source/split candidate bridge 必须保持删除状态；剩余收口只补真实 child-wire ownership 与 FreeCAD shared-source predicate。
6. MapperHistory / ElementMap full 后，再考虑把 `generated_open_export_bridge` / `purge_as_original_bridge` 从 `covered_main_path` 改状态。

任何阶段只要删除字段导致 open-cutter / cross / T / segmented / three-overlap 退化，必须回到对应账本层补 identity，不允许在输出端过滤。

## 必须保持的 fixture / oracle

### open-cutter / split fragment 不误删

- `cad-core/fixtures/p5/sketch-internal-face-through-open-cutter.json`
- `cad-core/fixtures/p5/sketch-internal-face-branch-open-cutter.json`

要求：

- split fragment 不因 child-level shared-source purge 丢失。
- branch-open-cutter 的 noOriginal shared-source ledger 仍因 unmatched edge 不 purge。

### cross / T / segmented helper ledger 不退化

- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-t-cutter`

要求：

- source-vmap producer ledger 不退回 result-slot endpoint bridge。
- split source lineage 保持，不恢复 identity fallback。

### bounded face + open wire 同时存在

- `sketch-internal-face-three-overlap-circles`
- `part-extrusion-facemaker-bullseye-intersected-holes`
- `sketch-internal-face-arc-lens`

要求：

- bounded faces 与 open wires 同时进入 history。
- current-member vertex identity 不退化。
- `InternalVertex` 数量不因 candidate 直切被合并。

### noOriginal purge

- `sketch-internal-face-dangling-line`
- `sketch-internal-face-split-and-dangling`
- `pad-dangling-line-profile`

要求：

- 1/1 child edge matched 后 purge。
- deleted / no_original_purge 只来自 actual purged verdict。

## capabilities / 文档更新规则

每个阶段结束都必须同步：

- `docs/CADCore3.0/00-总览.md`
- `docs/CADCore3.0/06-C3-M8后续收口清单.md`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `docs/CADCore3.0/FreeCAD语义矩阵.md`

状态规则：

- 仍有 history materialization binding / per-edge `openExportProducerEdge`、raw history materialization candidate bridge 或 history materialization bridge 时，`generated_open_export_bridge` 不能写成 `covered_full`；`historyProducerChildWireCandidate` 布尔已删除，不再作为剩余 blocker 计入。
- 只要 noOriginal 仍依赖 cad-core 重组账本而不是真实 FreeCAD child-wire ownership，`purge_as_original_bridge` 不能写成 `covered_full`；公开 candidate bridge 必须保持删除状态。
- 新增临时 bridge 必须在相邻代码注释和 `06-C3-M8后续收口清单.md` 写清：
  - FreeCAD 正确路径。
  - 当前保留原因。
  - 适用边界。
  - 删除条件。
  - 后续替换步骤。

## 验收分层

### 本轮短跑

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD
git diff --check
cd cad-core
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

### 阶段回归

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p5_sketch \
  tests.test_p6_topology \
  tests.test_p7_features \
  tests.test_p8_features \
  tests.test_expected_fixtures \
  tests.test_adapters
```

### 重型收口

只在阶段完成、runner/oracle 改动或准备改 capability 状态时执行：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_mvp \
  tests.test_diagnostics \
  tests.test_feature_flows \
  tests.test_p5_sketch \
  tests.test_p6_topology \
  tests.test_p7_features \
  tests.test_p8_features \
  tests.test_expected_fixtures \
  tests.test_adapters
```

## 完成判定

WireJoiner full ledger 不能只靠测试绿来判定。必须同时满足：

1. `openWireCompound` child-wire ownership 由 `EdgeInfo / WireInfo / aHistory / sourceEdges` 直接产生。
2. 独立 result-wire producer plan 不再作为 child-wire producer 输出的必要解释层，且剩余 materialization binding 已收敛到正式 WireJoiner history / child-wire ownership。
3. `producerLedgerWireFromEndpointMaterializationEvidence` 和 `endpointMaterializationEvidenceVertices` 删除。
4. three-overlap current-member vertex debt 归零，并且 `InternalVertex` 不退化。
5. noOriginal purge 只依赖 FreeCAD shared-source predicate 与完整 child-wire ownership。
6. WireJoiner `aHistory` 能被 `MapperHistory` / `ElementMap` 正式消费。
7. sketcher / adapter 层没有 subname、ownership、split history 的输出修正。
8. P5 open-cutter / cross / T / segmented / three-overlap / dangling-line oracle 全部稳定。
9. docs 和 C ABI capability 不再保留与 full 结论冲突的 bridge 诊断字段。

未满足以上条件前，`wire_joiner.generated_open_export_bridge` 与 `wire_joiner.purge_as_original_bridge` 只能保持 `covered_main_path` 或更低状态。
