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
- noOriginal candidate / shared-source match 已集中到 `OpenWireCompoundWireInfo` child-wire ledger，`getOpenWires(noOriginal)` 只消费 child-wire actual purged verdict。
- `source/vmap endpoint ledger` 与 `noOriginal shared-source edge ledger` 已作为 child-wire 诊断账本暴露。

但仍有两个 capability 不能升级：

| capability | 当前状态 | 不能升级的核心原因 |
| --- | --- | --- |
| `wire_joiner.generated_open_export_bridge` | `covered_main_path` | `ResultWireProducerPlan`、EdgeInfo `resultWireProducer*`、endpoint materialization debt 仍在解释 child-wire producer；three-overlap 仍有 3 个 child 依赖 endpoint materialization evidence |
| `wire_joiner.purge_as_original_bridge` | `covered_main_path` | noOriginal candidate 仍含 split/source bridge 口径；最终 full 条件必须建立在 FreeCAD shared-vertex source identity 和完整 child-wire ownership 上 |

当前最硬的 gate 是 three-overlap：

```text
open_wire_compound_producer_ledger_wire_from_result_slot_evidence_wire_info_count = 3
open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked_wire_info_count = 3
open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count = 6
open_wire_compound_current_member_split_ledger_result_slot_only_vertex_count = 6
open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count = 0
```

这说明 member/split ledger 已能构造 candidate，但 candidate wire 不能直接替换输出；直接切换会改变 three-overlap `InternalVertex` multiplicity。后续必须让 output vertex identity 被正式 member/split/candidate ledger 覆盖，而不是用 result-slot endpoint evidence 保住输出。

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

## cad-core 剩余债务

### 1. vmap replacement 还没有完全成为输出 identity 来源

已有：

- `sourceEdgeLedgerEdges_` 保存 mutable source/vmap ledger。
- `open_wire_compound_source_vmap_endpoint_ledger_*` 统计 output endpoint 是否被 source/vmap/split ledger 覆盖。

剩余：

- three-overlap 仍是 `30` 个 output endpoint、`0` 个 source/vmap matched endpoint。
- `endpointMaterializationEvidenceVertices` 还在 child-wire 建账边界兜住 output vertex identity。

删除条件：

- non-current-member 的 source-vmap producer 全部不依赖 endpoint materialization evidence。
- three-overlap 的 blocked current-member child 输出顶点不再是 result-slot-only identity。

### 2. splitEdges fragment-to-source ledger 仍没有替代所有 result producer bridge

已有：

- Modified / Generated / input EdgeInfo source sidecar 已拆分。
- open-cutter / cross / T / segmented / arc-lens 的旧 geometry identity fallback 已归零。

剩余：

- `ResultWireProducerPlanLedger::producerOpenExportEdges` 仍在 child-wire 建账边界提供 scoped producer edge。
- EdgeInfo `resultWireProducer*` 仍是内部诊断和 child-wire producer 解释字段。

删除条件：

- child-wire ownership 能直接由 `EdgeInfo / WireInfo / aHistory / myShapesToReturn` 形成，不需要 `ResultWireProducerPlan` 重新解释 producer edge。

### 3. openWireCompound child-wire ownership 仍未完全等价 FreeCAD

已有：

- `OpenWireCompoundWireInfo` 已承接 source lineage、splitter lineage、producer ledger wire、noOriginal verdict、current-member blocker。
- open-export history 和 topo evidence 读取 child-wire ledger，不回读 EdgeInfo helper。

剩余：

- child-wire 的完整 owner 来源还不是 FreeCAD `WireJoinerP::build()` 自然产物。
- `resultWireProducerLedgerEntry` 仍作为 producer entry / noOriginal candidate gate。

删除条件：

- child-wire slot 能追溯到 EdgeInfo export condition、WireInfo ownership、aHistory relation、sourceEdges source set。
- 缺 child-wire 的公开 history 分支长期保持 0，并且不需要 EdgeInfo 重导出。

### 4. current-member vertex identity 是 full ledger 的主要 blocker

已有：

- current-member split vertex debt 已进入 child-wire ledger。
- `wire_joiner_current_member_vertex_multiplicity_blocked` mapper diagnostic 固定 three-overlap 3 个 blocked child。

剩余：

- `output_matched_vertex_count=0`。
- `output_candidate_matched_vertex_count=0`。
- `result_slot_only_vertex_count=6`。

删除条件：

- member/split/candidate ledger 能覆盖当前输出顶点 identity。
- 直接切换不会让 three-overlap `InternalVertex` 数量从当前 oracle 退化到 16 或其它错误值。

### 5. noOriginal purge 仍不是完全 FreeCAD 等价

已有：

- per-child-wire edge / matched / unmatched 计数已按 `findSubShapesWithSharedVertex` 谓词记录。
- `branch-open-cutter` 已固定为 candidate 但 2 条 child edge 只有 1 条 matched，因此不 purge。

剩余：

- candidate 的形成仍依赖 child-wire source vertex / `splitFromInputEdge` / producer state 组合。
- 不能只启用最终 shared-source purge，否则 open-cutter / cross / T / segmented 会误删 split fragment。

删除条件：

- child-wire ownership 和 source/vmap identity 完整后，candidate 不再需要 bridge 口径；actual noOriginal purge 仅由 FreeCAD source compound shared-vertex predicate 和 child-wire ownership决定。

### 6. MapperHistory -> ElementMap 消费还不是完整 WireJoiner 路径

已有：

- NamedShape / ElementMap producer evidence 已优先消费 child-wire source/noOriginal verdict。

剩余：

- WireJoiner 自己的 `aHistory` 还没有完整进入正式 `MapperHistory(aHistory)` producer matrix。
- deleted / modified / generated / split / ambiguous 的 terminal relation 仍有部分通过诊断解释，而不是完全从 ElementMap 历史解析。

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
   - noOriginal matched + unmatched == candidate
   - source/vmap endpoint matched <= output
   - current-member vertex debt matched + unmatched == ledger count
3. 对 `branch-open-cutter`、`through-open-cutter`、`cross-cutters`、`segmented-cross-cutter`、`t-cutter` 保留 hard oracle，防止最终 shared-source purge 误删 split fragment。

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
3. producer wire materialization 优先使用 vmap-replaced edge/wire；只有 ledger 明确缺失时保留 endpoint materialization diagnostic。
4. 对 three-overlap current-member child，不直接切换 candidate wire；先证明 candidate / member / output vertex 三方 identity 关系。

新增测试：

- P5 cross / segmented / T：source-vmap producer 计数保持 4，result-slot 计数保持 0，matched endpoint 不退化。
- P5 three-overlap：保留当前输出，但新增 per-endpoint provenance，证明 6 个 result-slot-only vertex 的具体缺口。

删除条件：

- `producerLedgerWireFromEndpointMaterializationEvidence` 对非 current-member case 归零。
- docs 中把 endpoint materialization debt 限定到 current-member vertex identity，不再泛化到所有 producer。

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

测试：

- open-cutter：一条 source edge 一对多 split fragment。
- cross / T / segmented：bounded face 与 open wire 同时存在，split history 不丢。
- splitter 失败时继续使用原 edge，并输出 diagnostic，不猜 source。

删除条件：

- `split_fragment_source_identity_fallback_*` 和 `split_fragment_history_shape_geometry_bridge_*` 在 P5/P6 保持 0 后，降为内部 debug 或删除公开字段。

### 阶段 D：补齐 openWireCompound child-wire ownership

目标：让 child-wire 成为 WireJoinerP build 过程的自然产物，而不是 ResultWireProducerPlan 解释后的输出。

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
3. `ResultWireProducerPlanLedger::producerOpenExportEdges` 只允许作为 temporary bridge 读取，不再驱动 public producer entry。
4. `resultWireProducerLedgerEntry` 改为从 child-wire ownership 发布，不从 EdgeInfo candidate 发布。

测试：

- `open_wire_compound_wire_info_count == open_export_edge_info_count`。
- `open_wire_compound_missing_child_wire_history_edge_info_count == 0`。
- `result_wire_producer_ledger_entries` 数量与 child-wire producer entry 对齐。

删除条件：

- `EdgeInfo::resultWireProducerCandidate` 及相关 `resultWireProducer*` bridge 字段可删除或降为私有 debug。

### 阶段 E：收口 three-overlap current-member vertex identity

目标：删除剩余 3 个 result-slot endpoint materialization debt。

实现项：

1. 追踪 three-overlap current-member child 的三类 vertex：
   - member/split ledger vertices。
   - candidate wire vertices。
   - current output child-wire vertices。
2. 找出为什么 candidate 切换会把 `InternalVertex` 从 18 合并为 16：
   - 是 vmap replacement 时机不对。
   - 是 candidate wire 使用了错误的 shared vertex。
   - 是 FaceMaker/MapperHistory 已经生成了 distinct vertex 但 WireJoiner 未消费。
   - 是 ElementMap 消费时 merge 了本应 distinct 的 vertex。
3. 只在 identity 证明一致后替换 endpoint materialization evidence。
4. 若身份仍不一致，只扩大 vertex debt ledger，不改输出。

新增测试：

- three-overlap 专项断言：
  - `vertex_multiplicity_blocked_wire_info_count` 先保持 3。
  - 逐步要求 `output_matched_vertex_count` 增加。
  - 最终目标是 `output_unmatched_vertex_count=0`、`result_slot_only_vertex_count=0`、`output_candidate_matched_vertex_count` 覆盖当前输出顶点。
- 必须验证 `InternalVertex` 数量不退化。

删除条件：

- `open_wire_compound_producer_ledger_wire_from_result_slot_evidence_wire_info_count=0`。
- `producerLedgerWireFromEndpointMaterializationEvidence` 删除。
- `endpointMaterializationEvidenceVertices` 删除。

### 阶段 F：noOriginal purge 完全切到 FreeCAD shared-source predicate

目标：删除 split/source candidate bridge，让 noOriginal 只依赖 FreeCAD child-wire + source compound shared-vertex 语义。

实现项：

1. 只在完整 child-wire ownership 上执行 noOriginal。
2. 对每条 child edge 执行等价：

```text
source.findSubShapesWithSharedVertex(TopoShape(edge, -1)).empty()
```

3. actual purge 由“所有 child edge 都 matched”决定。
4. `branch-open-cutter` candidate/unmatched 继续不 purge。
5. open-cutter / cross / T / segmented 不得因为最终 shared-source purge 少 InternalEdge / split history。

删除条件：

- `sourceEdgeArrayOriginalOpenEdgeCandidate` 已删除状态保持。
- child-wire source vertex / `splitFromInputEdge` / producer state 组合 candidate 不再参与 noOriginal 判定。
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

## 字段删除顺序

不要一次性删所有 bridge。建议按以下顺序收口：

1. 非 current-member 的 endpoint materialization evidence 归零后，删除对应公开诊断或改成 current-member-only。
2. `split_fragment_source_identity_fallback_*` / `split_fragment_history_shape_geometry_bridge_*` 保持 0 后，降为内部 debug。
3. child-wire ownership 自足后，删除 EdgeInfo `resultWireProducerCandidate` 与公开 `result_wire_producer_ledger_entries` 对 EdgeInfo candidate 的依赖。
4. three-overlap vertex debt 归零后，删除 `endpointMaterializationEvidenceVertices` 和 `producerLedgerWireFromEndpointMaterializationEvidence`。
5. noOriginal full 后，删除 source/split candidate bridge，只保留 FreeCAD shared-source predicate。
6. MapperHistory / ElementMap full 后，再考虑把 `generated_open_export_bridge` / `purge_as_original_bridge` 从 `covered_main_path` 改状态。

任何阶段只要删除字段导致 open-cutter / cross / T / segmented / three-overlap 退化，必须回到对应账本层补 identity，不允许在输出端过滤。

## 必须保持的 fixture / oracle

### open-cutter / split fragment 不误删

- `cad-core/fixtures/p5/sketch-internal-face-through-open-cutter.json`
- `cad-core/fixtures/p5/sketch-internal-face-branch-open-cutter.json`

要求：

- split fragment 不因 final shared-source purge 丢失。
- branch-open-cutter 的 noOriginal candidate 仍因 unmatched edge 不 purge。

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

- 仍有 `ResultWireProducerPlan`、endpoint materialization evidence、EdgeInfo result-wire producer bridge 时，`generated_open_export_bridge` 不能写成 `covered_full`。
- noOriginal candidate 仍依赖 split/source bridge 时，`purge_as_original_bridge` 不能写成 `covered_full`。
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
graphify update .
cd cad-core
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

若本机 `graphify` 不在 PATH，可用当前机器已验证的 fallback：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD
/Users/admin/.cargo/bin/graphify-rs build --path . --update --format json
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
2. `ResultWireProducerPlan` 不再作为 child-wire producer 输出的必要解释层。
3. `producerLedgerWireFromEndpointMaterializationEvidence` 和 `endpointMaterializationEvidenceVertices` 删除。
4. three-overlap current-member vertex debt 归零，并且 `InternalVertex` 不退化。
5. noOriginal purge 只依赖 FreeCAD shared-source child-edge predicate 与完整 child-wire ownership。
6. WireJoiner `aHistory` 能被 `MapperHistory` / `ElementMap` 正式消费。
7. sketcher / adapter 层没有 subname、ownership、split history 的输出修正。
8. P5 open-cutter / cross / T / segmented / three-overlap / dangling-line oracle 全部稳定。
9. docs 和 C ABI capability 不再保留与 full 结论冲突的 bridge 诊断字段。

未满足以上条件前，`wire_joiner.generated_open_export_bridge` 与 `wire_joiner.purge_as_original_bridge` 只能保持 `covered_main_path` 或更低状态。
