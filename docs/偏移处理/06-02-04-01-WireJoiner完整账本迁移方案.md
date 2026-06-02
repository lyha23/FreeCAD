# WireJoiner 完整账本迁移方案

时间：2026-06-02 04:01。

## 背景

`06-02-02-37-cad-core临时诊断主路径偏移整改方案.md` 已经把明显的输出端修补收回到 `WireJoiner::getOpenWires()` 路径，并让 MVP、P5 Sketch、P6 Topology 验收通过。

但当前 `cad-core` 仍然只是阶段性对齐：

```text
FaceMaker InternalShape
  -> WireJoiner generated open-export EdgeInfo
  -> getOpenWires()
  -> Sketch InternalShape / NamedShape
```

它还不是完整 FreeCAD `WireJoinerP` 账本。真正应该迁移的是：

```text
sourceEdgeArray
  -> EdgeInfo live list
  -> splitEdges()
  -> buildAdjacentList()
  -> findClosedWires()
  -> buildClosedWire()
  -> findTightBound()
  -> exhaustTightBound()
  -> compound / openWireCompound
  -> MapperHistory(aHistory)
  -> TopoShape ElementMap / NamedShape history
```

这份方案的目标，是把当前 generated open-export 过渡来源替换成完整 `WireJoinerP::aHistory`、`myShapesToReturn`、`findTightBound()` / `exhaustTightBound()` 生命周期，避免继续在 result-wire 导出上叠 fixture 规则。

## FreeCAD 依据

主要源码：

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildAdjacentList()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findClosedWires()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getResultWires()`

关键 FreeCAD 语义短句 / 字段：

- `sourceEdgeArray`
- `sourceEdges`
- `aHistory`
- `EdgeInfo::iteration`
- `EdgeInfo::iteration2`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::superEdge`
- `WireInfo::vertices`
- `WireInfo::done`
- `WireInfo::purge`
- `openWireCompound`
- open export 条件：`iteration == -3 || (!wireInfo && iteration >= 0)`
- `shape.makeShapeWithElementMap(comp, MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op)`

## 当前 cad-core 差距

当前 `cad-core/src/geometry/wire_joiner.cpp` 已经有这些阶段性结构：

- `EdgeInfo` / `WireInfo` 的简化字段。
- `buildFinalEdgeOwnership()`。
- closed-wire owner 搜索；graph fallback owner 写入路径已删除，`graph_fallback_assigned_edge_info_count` 仅保留为 0 值回归字段。
- `generatedOpenExportShapeForSketchInternals()` 转入 `EdgeInfo` 的 generated open-export 来源；`resultWireEvidence_`、`EdgeInfo::temporaryResultWireEvidence` 和 `copiedResultWireGraphProbeForSketchInternals()` 命名路径已删除。
- `ledgerSummary()`。

这些结构能解释现有 fixture，但有三个本质缺口：

1. `EdgeInfo` 生命周期不是 FreeCAD 的生命周期。

   当前 primary owner 已先由 `findClosedWires()` 风格的 closed path search 写出，single-edge closed curve 也进入 closed-wire owner；graph fallback owner 已删除。但 `findTightBound()` / `exhaustTightBound()` 还没有真正分裂 over-owned closed wire，也没有完整写出 `WireInfo.done` / `wireInfo2` 生命周期。

2. `openWireCompound` 仍有 generated open-export 过渡来源。

   当前 result-wire 导出已经收回 WireJoiner 内部 final `EdgeInfo` ledger，但它仍不是从完整 `WireJoinerP::aHistory` / `myShapesToReturn` 自然导出的最终结果。

3. `aHistory` 尚未进入 topo 正式账本。

   当前 topo 已消费 FaceMaker history，但 WireJoiner 自己的 generated / modified / deleted history 还没有完整进入 `NamedShape.history` / `ElementMap`。

## 当前实施进展

本轮已完成阶段性迁移，但尚未达到最终完成定义：

- 阶段 0 基线已确认：`cmake --build build`、`python3 -m unittest tests/test_mvp.py`、`python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py` 均通过。
- 本次 superEdge lifecycle live 增量后验证仍通过：`cmake --build build`、`python3 -m unittest tests/test_mvp.py`、`python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`、`git diff --check`。
- 本次 existing-wire `idxVertex` / `stackPos` 账本增量后验证仍通过：`cmake --build build`、`python3 -m unittest tests/test_mvp.py`、`python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`、`git diff --check`。
- 本次 `openWireCompound` child-wire 账本增量后验证仍通过：`cmake --build build`、`python3 -m unittest tests/test_mvp.py`、`python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`、`git diff --check`。
- 本次 open-export source lineage summary / per-entry producer history 增量后验证仍通过：`cmake --build build`、`python3 -m unittest tests/test_mvp.py`、`python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`、`git diff --check`。
- 阶段 1 已补齐 `cad-core` 的 `EdgeInfo` 基础账本形状：`p1`、`p2`、`mid`、`iStart`、`iEnd` 已进入 `cad-core/include/cad_core/geometry/wire_joiner.h` 与 `cad-core/src/geometry/wire_joiner.cpp`；`WireInfo` 增加 request-local adjacent vertex ledger，`rebuildAdjacentList()` 按 FreeCAD `buildAdjacentListPopulate()` 的 range 形态记录相邻端点。
- 阶段 3 已推进到基础 owner 切换：`buildFinalEdgeOwnership()` 先建立 adjacent range，再通过 closed path search 给未归属 `EdgeInfo` 写 primary `wireInfo`；single-edge closed curve 因 p1/p2 重合直接形成 closed-wire owner。`wire_joiner_ledger` 暴露 `closed_wire_assigned_edge_info_count` 和 `graph_fallback_assigned_edge_info_count`，through-open-cutter、T-junction、圆/椭圆闭合 profile 的 primary owner 均来自 closed-wire search；graph fallback owner 写入路径和 EdgeGraph helper 已删除，该计数字段当前为 0。
- 阶段 3/closed-wire stack 继续推进：`findClosedWirePath()` 已从全局 edge 扫描递归切到 FreeCAD `_findClosedWires()` 对应的 adjacent-list stack 形态，内部维护 request-local stack frame、vertexStack 和 edgeSet trace，并由 `wire_joiner_ledger.closed_wire_search_*` 暴露。primary owner 现在来自该 adjacent range stack 选择的 begin edge + `vertexStack[entry.iCurrent]` 路径；through-open-cutter、cross-cutters、T-junction 等 P5/P6 oracle 仍通过。
- 阶段 3 已补 `buildAdjacentListSkipEdges()` 的 open leaf 生命周期：`markOpenLeafEdges()` 在 closed-wire owner 搜索前扫描 adjacent range，如果某个端点没有其它 active edge，相应 `EdgeInfo::iteration` 写为 `-3`，对齐 FreeCAD “Skip edges that are connected to only one end”。dangling open line、internal-branch cutter、branch-open-cutter、through-open-cutter 的 open-export edge 现在来自真实 `iteration=-3 || (!wireInfo && iteration >= 0)` 条件，不再依赖 generated open-export 过渡来源；dangling open line 的 tight-bound branch candidate 因此归零。
- 阶段 4 已建立 per-`WireInfo` 入口：closed path search 现在同步保存 owner `WireInfo` 的 `vertices` / `wire` / `done` / split candidate trace；`recordBranchSearchCandidates()` 优先按 owner `WireInfo::vertices` 扫描 adjacent range，不再用整个 split-edge component 近似 `findTightBound()` 的入口。`wire_joiner_ledger` 新增 `closed_wire_info_count`、`closed_wire_vertex_count`、`tight_bound_done_wire_info_count`、`tight_bound_split_wire_info_count`，用于约束 through-open-cutter、T-junction、dangling open line 的 owner-wire 生命周期。
- 阶段 4 继续推进到 owner-transfer candidate 账本：`OwnerWireInfo` 已保存 `TightBoundBranchCandidate`，记录 owner vertex、adjacent vertex、inside 判定和当前 owner edge 是否会被新 `WireInfo` 接管；`wire_joiner_ledger` 新增 `tight_bound_new_wire_candidate_count`、`tight_bound_new_wire_vertex_count`、`tight_bound_owner_transfer_candidate_edge_info_count`。through-open-cutter 与 T-junction 现在能证明 inside branch candidate 已进入 new-wire / owner-transfer 候选账本，dangling open line 保持 0。
- 阶段 4 继续推进到 owner transfer / split-owner 账本：`OwnerWireInfo` 已保存 `TightBoundTransferWire`，按 FreeCAD `findTightBoundByVertices()` 的 begin vertex + search stack + adjacent branch 形态记录将要形成的新 `WireInfo`，并把 transfer wire 中仍归属旧 owner 的 `EdgeInfo::wireInfo` 改写到新 owner id；transfer 后仍归属旧 owner 的 vertices 会进入 `OwnerWireInfo::splitOwnerVertices`，`exhaustTightBound()` trace 优先消费该活动 owner 账本。当前 transfer 已增加 `_findClosedWires(beginVertex, currentVertex, &idxEnd, beginInfo.wireInfo, &stackPos)` 对应的 branch path 闭合检查：inside branch 如果不能回到 begin point，只保留 candidate trace，不生成 transfer/split；dangling open line 因此不再被记录为 split。`wire_joiner_ledger` 新增 `tight_bound_transfer_wire_info_count`、`tight_bound_transfer_wire_vertex_count`、`tight_bound_transferred_owner_edge_info_count`、`tight_bound_split_owner_wire_info_count`、`tight_bound_split_owner_vertex_count`、`tight_bound_split_owner_built_wire_count`。
- 阶段 4/splitWire 账本继续推进：`TightBoundTransferWire` 现在记录 FreeCAD `findTightBoundWithSplit()` 对应的 splitWire vertices，顺序为旧 owner remainder，再追加 adjacent branch / closed path stack 的反向 vertices；`wire_joiner_ledger.tight_bound_split_wire_vertex_count` 与 `tight_bound_split_wire_built_count` 用于约束该账本。through-open-cutter、cross-cutters、T-junction 等 case 已能证明 splitWire vertices 由 WireJoiner 内部 transfer path 产出，dangling open line 保持 0。该账本仍未参与 `openWireCompound` 导出，不能替代后续完整 `findTightBound()` / `exhaustTightBound()` 切换。
- 阶段 4/existing-wire search 继续推进：`traceExistingWireSearchForCandidate()` 已按 FreeCAD `_findClosedWiresWithExisting()` 入口记录 existing `wireInfo` 搜索的 hit、reverse-hit、purge、stack frame、vertexStack、edgeSet、backtrack 与 intersect-skip trace，并把成功 transfer wire 对应的 `idxVertex` / `stackPos` 坐标保存在 `TightBoundTransferWire` 账本中，通过 `wire_joiner_ledger.tight_bound_existing_wire_*` 暴露。该 trace 只在 WireJoiner 内部记录，不参与 `InternalShape` / `openWireCompound` 输出；through-open-cutter、cross-cutters、T-junction、three-overlap 等 owner-transfer case 已有非零 hit / reverse-hit 信号，dangling open line 保持 0。
- 阶段 4 生命周期入口已从 `SketchInternalBuilder` 后置 probe 收回到 `WireJoiner::buildFinalEdgeOwnership()`：branch search、transfer、split-owner 和 exhaust trace 现在先在 split EdgeInfo list 上运行，再追加临时 result-wire bridge；`recordBoundedFaceClassifierProbe()` 这个会改写 `wireInfo` 的 public 入口已删除。
- 阶段 5 已删除 graph-degree `wireInfo2` 兜底：`recordExhaustTightBoundLifecycle()` 现在按 FreeCAD `exhaustTightBound()` 第一段语义，在 done owner 的 vertices 中遇到已被其他 primary owner 接管的 edge 时写 `wireInfo2`；对仍缺 `wireInfo2` 的 edge，`recordExhaustAdjacentSecondaryOwners()` 会从端点 adjacent done-owner edge 出发做闭合路径搜索，对齐 `exhaustTightBoundWithAdjacent()` / `_findClosedWires(beginVertex, currentVertex)` 的入口。`assignGraphSecondaryOwners()` 已删除，`graph_secondary_owner_edge_info_count` 保留为必须为 0 的回归字段；through-open-cutter、cross-cutters、segmented-cross、T-junction 和 three-overlap 的 secondary owner 现在均来自 exhaust 账本，不再来自 graph-degree 近似。
- 阶段 5/6 之间的 `aHistory->Remove()` 账本已落地：`recordBuildClosedWireRemovalLifecycle()` 按 FreeCAD `WireJoinerP::buildClosedWire()` 中 `++counter[vertex.edgeInfo()] == 2` 的删除循环，对 done `wireInfo` / `wireInfo2` vertices 记录 consumed edge，并把删除数量写入 `WireJoinerHistorySummary::deletedHistoryCount`。由于 cad-core 还没有完整迁移 `buildAdjacentListSkipEdges()` 的 `iteration=-3`，当前实现会保护 `wireInfo == 0` 的 open-export candidate，避免把已知 open wire 误删；反复 `findClosedWires()` / `findTightBound()` 循环仍是后续迁移项。
- 阶段 5/6 已删除 `ownerContributesToLedger`：`EdgeInfo` 不再携带这个非 FreeCAD 字段，`primary_owned_edge_info_count`、`secondary_owned_edge_info_count`、`done_owned_edge_info_count` 直接按 `wireInfo` / `wireInfo2` 统计。dangling open line 现在能同时证明 closed-wire owner 进入 primary 账本，而 dangling edge 仍按 open-export 条件保留。
- 阶段 6 已继续收敛：`resultWireEvidence_` 成员、`getOpenWires()` 的 evidence 追加旁路、`EdgeInfo::temporaryResultWireEvidence` 和 `copiedResultWireGraphProbeForSketchInternals()` 命名路径已删除；T/cross/overlap 这类 result-wire edge 现在作为 `generatedOpenExportEdge` 写入 WireJoiner final `EdgeInfo` ledger，再由 `getOpenWires()` 按 `iteration == -3 || (!wireInfo && iteration >= 0)` 统一导出。`temporary_result_wire_edge_info_count` 保留为回归字段且当前为 0；`generated_open_export_edge_info_count` 标出仍由 `generatedOpenExportShapeForSketchInternals()` 生成、尚未完整落到 FreeCAD `aHistory` / `myShapesToReturn` 生命周期的 open-export edge。当前剩余 generated 范围集中在 cross / segmented-cross / T-junction / overlap / arc-lens / bullseye 这类 bounded-result edge，open leaf 类 case 已由 `iteration=-3` 覆盖。
- 阶段 6 的临时 result-wire bridge 已按本机 FreeCADCmd oracle 收窄 vertex copy 语义：cross-cutter 的 FreeCAD `InternalShape` 是两个 child compound，face child 为 `4 faces / 12 edges / 9 vertices`，open-wire child 为 `0 faces / 12 edges / 9 vertices`；open-wire edge 不与 face edge 共享 identity，但四个闭合矩形角点 vertex 与 face child 共享 identity。`generatedOpenExportShapeForSketchInternals()` 因此只在全线性 closed-boundary generated bridge 中复用 closed-wire 顶点；arc/circle/partial-overlap 类 curved result-wire 仍 copy 全部 result vertices，避免把曲线交点错误合并。该调整只提高过渡 bridge 的 FreeCAD 形态一致性，不改变 `generated_open_export_edge_info_count` 仍未归零的事实。
- `getOpenWires(noOriginal=true)` 不再使用 endpoint-touch / boundary-touch 几何旁路；当前仅保留 `purgeAsOriginalOpenEdge` 临时 identity bridge，用于在完整 `sourceEdgeArray` / `VertexInfo` identity ledger 迁移前区分原始 open edge 与 split/result edge。该桥使用 FreeCAD `findTightBoundByVertices()` 对应的 `isInside(*wireInfo, next->mid)` 语义，不应继续扩展成新形态规则。
- `purgeAsOriginalOpenEdge` 的删除条件已明确：仅保留 FreeCAD wire-level `source.findSubShapesWithSharedVertex()` purge 会让 dangling open line、internal-branch cutter 这类原始 open edge 泄漏进 InternalShape / ElementMap；删除该桥之前必须先迁移完整 `sourceEdgeArray` / `VertexInfo` identity ledger，让 split/result edge 与原始 open edge 可由 WireJoiner 自身身份账本区分。
- 本轮复核进一步确认 `purgeAsOriginalOpenEdge` 不能只靠调整 purge 顺序删除：把 `getOpenWires()` 改成“先按每个 exported EdgeInfo 建 wire，再执行 FreeCAD wire-level `noOriginal` purge，最后 merge surviving wires”后，dangling open line / internal-branch / pad-dangling 会重新多出原始 open edge，T-junction 会少一个 result-wire edge。该失败说明 cad-core 还缺 FreeCAD `add()` 里的 shared vertex identity、`findSuperEdges()` / `superEdge` 和 `openWireCompound` child-wire 结构，不能把当前 bridge 简化成单纯的 purge-before-merge。
- 阶段 6/identity 已补第一层 source vertex 账本：`EdgeInfo::sourceVertexIdentity` 在 `initializeEdgeInfo()` 中按 `TopoDS_Vertex::IsSame()` 记录 edge 两端是否来自 `sourceEdges_`，`wire_joiner_ledger` 暴露 `source_identity_*` 诊断计数。dangling open line 现在能证明唯一 open-export edge 正是 `purgeAsOriginalOpenEdge` 的 source-identity bridge candidate；T-junction 则能证明 generated result-wire open-export edge 虽有 source vertex identity，但不是该 purge bridge candidate。该账本只用于诊断，不改变 `getOpenWires()` 输出。
- 阶段 6/source lineage 已补 `sourceEdgeArray -> split EdgeInfo` 第一层来源账本：`SplitEdgeRecord` 用 OCCT splitter `Modified/Generated` history 和 `TopoDS_Shape::IsSame()` 记录 split result edge 来自哪些 source edge，最终写入 `EdgeInfo::sourceEdgeIndices` / `sourceLineageFromSplitterHistory`，并通过 `wire_joiner_ledger.source_lineage_*` 暴露。through-open-cutter、dangling open line 这类真实 split/open-export edge 现在能证明 open-export edge 带有 source lineage；T-junction、cross-cutter 的 generated open-export bridge 则表现为 `source_lineage_missing_open_export_edge_info_count == generated_open_export_edge_info_count`，明确指出这些输出仍未来自真实 `aHistory` / `myShapesToReturn` 生命周期。该账本只用于诊断，不改变 `getOpenWires()` 输出。
- 阶段 6/superEdge 已补 `findSuperEdges()` 的候选链、root materialization 与 live lifecycle：`recordSuperEdgeCandidates()` 在 `buildAdjacentList()` 后、open leaf skip 前，按 FreeCAD “connected to only one other edges” 的 adjacent range 入口记录 `SuperEdgeInfo`，并把 `superEdgeInfo` / `superEdgeRoot` / `superEdgeMemberCount` / `superEdgeClosed` 写到 `EdgeInfo`。candidate chain 会构造 root `EdgeInfo::superEdge`，并记录 `super_edge_materialized_*` 与 `super_edge_shadowed_member_edge_info_count`。
- 阶段 6/superEdge lifecycle 已切入 live `findSuperEdgesUpdateFirst()` 等价路径：materialized candidate 的非 root 成员写 `iteration = -1`，closed root 写 `iteration = -2`，open root 写当前 iteration、endpoint rewrite、source adjacent range，并同步改写 `p1/p2`、`iStart/iEnd` 与 adjacentList 中 last member -> first root 的引用，对齐 FreeCAD `vFirst.ptOther() = vLast.ptOther()` 与 `first->iStart/iEnd[idx] = last->iStart/iEnd[...]`。dangling open line 的 closed profile 现在表现为 closed superEdge root，而不再进入旧的 `closed_wire_assigned_edge_info_count` owner 账本。
- 阶段 6/EdgeInfo shape-wire 入口已落地：`cad-core` 的 `EdgeInfo` 已增加 FreeCAD 同名 `shape(forward)` / `wire(forward)` 与 `edgeReversed` / `superEdgeReversed` cache，`wireFromVertices()` 已改为通过该入口构造 candidate wire，`getOpenWires()` 已具备 live `superEdge` child-wire 消费和 source purge 分支。当前仍未删除 generated open-export 过渡来源，因此 `getOpenWires()` 还不是完全由真实 final EdgeInfo lifecycle 覆盖所有 result-wire case。
- 阶段 6/openWireCompound child-wire 账本已补：`recordOpenWireCompoundLedger()` 按 FreeCAD `WireJoinerP::build()` 的 `builder.Add(openWireCompound, info.wire())` 循环，为每个满足 `iteration == -3 || (!wireInfo && iteration >= 0)` 的 final `EdgeInfo` 保存一个 request-local child wire，并通过 `wire_joiner_ledger.open_wire_compound_*` 暴露 built、superEdge、generated、purge-bridge 和 source shared-vertex purge match 计数。该账本暂不改变 `getOpenWires()` 输出；后续删除 generated open-export 和 `purgeAsOriginalOpenEdge` 前，应先把导出主路径切到该 FreeCAD child-wire 边界。
- 阶段 6/openWireCompound 切换复核：直接让 `getOpenWires()` 读取上述 child-wire ledger 会让 dangling open line、internal-branch cutter、pad-dangling 重新泄漏原始 open edge，并让 T-junction 少一个 expected result-wire edge；因此当前只能保留 child-wire 账本作为诊断边界，输出仍走现有 merge/purge 过渡桥。该失败与此前删除 `purgeAsOriginalOpenEdge` 的失败一致，说明还缺完整 `sourceEdgeArray` / `VertexInfo` identity、generated result-wire identity 和真实 `openWireCompound` child-wire ownership。
- 阶段 7/open-export lineage summary 已推进：`WireJoinerHistorySummary` 现在从 final `EdgeInfo` export state 额外产出 `openExportSourceLineageEdgeCount`、`openExportMissingSourceLineageEdgeCount`、`openExportGeneratedEdgeCount`、`openExportGeneratedMissingSourceLineageEdgeCount` 和 `openExportPurgeBridgeEdgeCount`，并通过 `openExportEntries` 逐条记录 `openExportIndex`、`edgeInfoIndex`、source edge indices、generated flag 与 purge-bridge flag，再透传到 `SketchInternalHistoryContext` / result metadata。该信息仍是 WireJoiner 产出的 producer evidence，只用于约束后续 `MapperHistory(aHistory)` consumer；topo 还没有据此发明元素级映射。
- 阶段 6/purge 复核已完成：在 live `superEdge` 与 `EdgeInfo::wire()` 切入后，直接删除 `purgeAsOriginalOpenEdge` 字段和 `getOpenWires()` per-EdgeInfo purge 仍会让 dangling open line、internal-branch cutter、pad-dangling 泄漏原始 open edge。当前 superEdge child-wire 与 raw edge 导出前仍保留 per-EdgeInfo purge，后面还有 FreeCAD wire-level `source.findSubShapesWithSharedVertex()` purge；这两层都是过渡兼容路径，删除条件仍是完整 `sourceEdgeArray` / `VertexInfo` / `openWireCompound` identity ledger。
- 阶段 7 已建立入口：`WireJoinerHistorySummary` 从 geometry 层公开 source edge、split result、open export、open-export source lineage、OCCT splitter modified/generated/deleted 计数；`SketchInternalBuilder` / `SketchObject` 将该 summary 写入 `SketchInternalHistoryContext`；`topo::namedShapeForSketchInternalShape()` 序列化 `wire_joiner_*` history 字段，并在 `element_history_status` 中标记 `wire_joiner_history:splitter`、`modified`、`generated`、`open_export`。当前 topo 只消费 WireJoiner 自己产出的 summary，不从 raw/internal 几何反推 WireJoiner history。
- 测试已同步调整：T-junction 不再断言 result-wire edge 留在 ledger 外，而是要求其进入 `open_export_edge_info_count`、split-wire candidate 和 owner propagation trace。

仍未完成：

- `generatedOpenExportShapeForSketchInternals()` 仍在 `buildFinalEdgeOwnership()` 内作为 generated open-export EdgeInfo 来源，尚未被真实 `findTightBound()` / `exhaustTightBound()` / `aHistory` 生命周期替换；当前可用 `generated_open_export_edge_info_count` 和 `open_wire_compound_generated_wire_info_count` 直接定位仍依赖该过渡来源的输出，`temporary_result_wire_edge_info_count` 已归零。
- `findTightBoundSplitWire()` / `findTightBoundUpdateVertices()` 仍未完整迁移：当前已记录 owner-wire branch candidate、new-wire candidate、owner-transfer candidate、闭合 branch path、transfer wire、split-owner vertices、splitWire vertices、existing-wire hit / reverse-hit / purge trace、成功 transfer wire 的 `idxVertex` / `stackPos` 坐标、split candidate 和 done/exhaust trace，并已让 transfer wire 改写对应 `EdgeInfo::wireInfo`；`findClosedWires(true)` 的 primary owner path 已使用 adjacent-list stack 账本，但 `_findClosedWires()` 在 full `wireSet`、`purge`、多轮 `idxVertex` / `stackPos` 写回和 repeated split/exhaust 场景下仍未完整迁移。当前 splitWire / existing-wire trace 只作为 request-local WireJoiner 账本，不参与 final `openWireCompound` 导出，后续仍需迁移反复 split/exhaust 循环。
- graph fallback owner、graph-degree secondary owner 和 `ownerContributesToLedger` 已删除，`graph_fallback_assigned_edge_info_count` 和 `graph_secondary_owner_edge_info_count` 当前为 0；剩余过渡路径集中在 generated open-export 来源和 `purgeAsOriginalOpenEdge` identity bridge。
- 移除 `generatedOpenExportShapeForSketchInternals()` 输出接入的验证失败范围已明确：T-junction 的 `open_export_edge_info_count` 变为 0，cross / segmented-cross / circle-overlap / arc-lens 的 `InternalEdge` 数量不足。本机 FreeCADCmd probe 还显示 result-wire child 是 copied edge identity，而不是直接复用 FaceMaker face edge；因此下一步必须迁移 `findTightBound()` / `exhaustTightBound()` 的完整分裂、`myShapesToReturn` / `aHistory` 和二次 owner 搜索，不能继续在 generated open-export 来源上叠新形态规则。
- 删除 `purgeAsOriginalOpenEdge` 的直接尝试已失败：即使保留 FreeCAD wire-level `source.findSubShapesWithSharedVertex()` purge 并把它前置到 merge 之前，当前 cad-core 的 per-edge wire / merge 结构仍不能同时满足 dangling 原始 open-edge 删除和 T-junction result-wire 保留；live `superEdge` 切入后再直接删除字段和 per-EdgeInfo purge，也仍会让 dangling open line、internal-branch cutter、pad-dangling 泄漏原始 open edge。当前已具备 `EdgeInfo::sourceVertexIdentity`、`EdgeInfo::sourceEdgeIndices`、diagnostic/materialized `SuperEdgeInfo`、live `findSuperEdgesUpdateFirst()` lifecycle 和 `EdgeInfo::shape()` / `wire()` 等价入口；剩余缺口是由完整 `aHistory` / `myShapesToReturn` 产出 generated result-wire identity，并用真实 `openWireCompound` child-wire 结构替换 `generatedOpenExportShapeForSketchInternals()` 与 `purgeAsOriginalOpenEdge` 过渡桥。
- `WireJoinerP::aHistory` 尚未完整迁移：当前只消费 OCCT splitter 的 modified/generated/deleted summary、final open-export summary、open-export source lineage summary 和逐条 open-export producer entries，尚未迁移 FreeCAD `aHistory->Remove()`、ShapeFix `makeCleanWire()` history merge、`MapperHistory(aHistory)` 的元素级 source-to-result map。

## 迁移目标

最终目标不是“再让 fixture 多过几个”，而是让 `cad-core` 的 WireJoiner 主路径具备以下性质：

```text
SketchObject / SketchInternalBuilder
  -> WireJoiner.addShape(source sketch edges)
  -> WireJoiner.Build()
  -> WireJoiner.getOpenWires()
  -> MapperHistory-equivalent
  -> NamedShape / ElementMap
```

验收口径：

- `SketchInternalBuilder` 不再知道 copied result-wire、partial overlap、T/cross/cycle 这类 result-wire 形态规则。
- `getOpenWires()` 只读 final `EdgeInfo` 字段和 source identity，不做几何形态猜测。
- `NamedShape` / `ElementMap` 只消费 WireJoiner history，不从 raw/internal 几何关系发明 WireJoiner split / deleted / generated history。
- 当前 generated open-export EdgeInfo 来源和 `purgeAsOriginalOpenEdge` identity bridge 要么删除，要么降级为 diagnostic trace；`resultWireEvidence_`、graph fallback owner、graph-degree secondary owner 和 `ownerContributesToLedger` 已删除。

## 分层落点

### geometry

落点：

- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/src/geometry/wire_joiner.cpp`

职责：

- 迁移 `EdgeInfo` / `WireInfo` / `VertexInfo` / adjacent list / stack / edge set / wire set。
- 迁移 split、closed wire、tight bound、exhaust tight bound。
- 产出 `compound`、`openWireCompound` 和 WireJoiner history ledger。

### topo

落点：

- `cad-core/include/cad_core/topo/*`
- `cad-core/src/topo/*`

职责：

- 接收 WireJoiner history ledger。
- 转换为 `NamedShape.history`、`ElementMap` 和 stable subname 更新。
- 替换当前依赖 FaceMaker-only 或 exact alias 的 WireJoiner history 缺口。

### features

落点：

- `cad-core/src/geometry/sketch_internal_builder.cpp`
- `cad-core/src/features/sketch_object.cpp`

职责：

- 只表达 FreeCAD `SketchObject::buildInternals()` 调用顺序。
- 不再承载 result-wire 补边、split ownership、history 合成逻辑。

## 实施阶段

### 阶段 0：冻结基线和 trace

目标：迁移前先把当前过渡行为固化，防止边迁移边丢语义。

动作：

1. 保留当前通过的基线：
   - `python3 -m unittest tests/test_mvp.py`
   - `python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`
2. 为 WireJoiner 增加只读 trace：
   - source edge count。
   - split edge count。
   - closed wire count。
   - final open export edge count。
   - generated / modified / deleted history entry count。
3. trace 只能进 diagnostics / metadata，不能参与 shape、subshape 或 history 决策。

完成条件：

- 当前 fixture 仍全绿。
- trace 能说明每个 InternalEdge 是 source、split、tight-bound consumed 还是 open export。

### 阶段 1：迁移 EdgeInfo / WireInfo 原始账本

目标：先补 FreeCAD 账本形状，不切输出。

动作：

1. 对齐 `EdgeInfo` 字段：
   - `edge`
   - `superEdge`
   - `p1`
   - `p2`
   - `mid`
   - `box`
   - `iStart`
   - `iEnd`
   - `iteration`
   - `iteration2`
   - `wireInfo`
   - `wireInfo2`
   - curve / parameter / linear metadata。
2. 对齐 `WireInfo` 字段：
   - `vertices`
   - `wire`
   - `done`
   - `purge`
3. 增加 `VertexInfo`：
   - 指向 `EdgeInfo`。
   - start / reversed 方向。
   - `pt()` / `ptOther()` 等价能力。
4. `addShape()` / `addOpenWire()` 只负责初始化 ledger，不做导出判断。

完成条件：

- `EdgeInfo` / `WireInfo` trace 能和 FreeCAD 字段一一对照。
- 当前 `buildFinalEdgeOwnership()` 仍可作为旧输出桥接，但不再扩展新规则。

### 阶段 2：迁移 splitEdges() 和 aHistory 初始能力

目标：让 split 结果和 history 从 WireJoiner 自己产出。

动作：

1. 迁移 `splitEdges()`：
   - 原 edge 到 split edge 的 modified history。
   - 被替换 edge 的 removed/deleted history。
   - `superEdge` 关系。
2. 建立 `WireJoinerHistoryLedger`：
   - generated。
   - modified。
   - deleted。
   - source-to-result edge map。
3. 不直接写 `NamedShape`，只在 geometry 层记录 history ledger。

完成条件：

- through-open-cutter、self/inter-edge split、circle overlap 的 split edge 能追溯 source edge。
- `topo` 还未消费前，输出行为不变。

### 阶段 3：迁移 buildAdjacentList() / findClosedWires()

目标：替换 graph-cycle 近似的基础。

动作：

1. 建立 FreeCAD 风格 adjacent list：
   - 每个 EdgeInfo 端点的 adjacency range。
   - `iStart` / `iEnd`。
   - tolerance 与 bbox 过滤。
2. 迁移 `_findClosedWires()` 搜索栈：
   - `vertexStack`
   - `stack`
   - `edgeSet`
   - `wireSet`
3. 迁移 `findClosedWires()`：
   - 初始 closed wire owner。
   - open edge iteration 标记。
   - 失败 wire 的 diagnostic。

完成条件：

- 单闭合 wire、nested hole/island、overlapping closed wires 的 closed wire ledger 稳定。
- primary owner 来自 closed-wire search；single-edge closed curve 也必须由该路径归属，`graph_fallback_assigned_edge_info_count` 保持为 0。
- graph fallback owner 写入路径已删除；后续如果 closed search 无法归属某类 edge，必须补 FreeCAD `_findClosedWires()` 栈语义，不得恢复 graph fallback。

### 阶段 4：迁移 findTightBound()

目标：让 primary `wireInfo` 来自 FreeCAD branch search，而不是 bounded face 或 graph-cycle 推断。

动作：

1. 迁移 `findTightBoundByVertices()`：
   - `next == current` skip。
   - `next->iteration2 == iteration2` skip。
   - `next->iteration < 0` skip。
   - `isInside(*wireInfo, next->mid)` 判定。
2. 迁移 `findTightBoundSplitWire()`：
   - branch slicing。
   - splitWire vertices。
   - old wireInfo 到 new wireInfo 的 edge owner 转移。
3. 迁移 `findTightBoundUpdateVertices()`：
   - `wireInfo->done = true`。
   - done owner 向同 wire vertices 传播。
4. 已完成：`ownerContributesToLedger` 删除，ledger primary / secondary 计数直接来自 `wireInfo` / `wireInfo2`。

完成条件：

- T-junction、cross cutter、segmented cross cutter 的 primary owner 能由 branch search trace 解释。
- `wire_joiner_ledger.primary_owned_edge_info_count` 不再依赖 graph-cycle fallback。

### 阶段 5：迁移 exhaustTightBound()

目标：补齐 secondary `wireInfo2` 和 done wire 生命周期。

动作：

1. 迁移 `exhaustTightBoundUpdateWire()`。
2. 迁移 `exhaustTightBoundWithAdjacent()`。
3. 迁移 `exhaustTightBoundUpdateEdge()`。
4. 迁移 `exhaustTightBoundUpdateVertex()`。
5. 明确 `wireInfo2 && wireInfo2->done` 的 skip 语义。

完成条件：

- shared edge / partial overlap / multi-region cutter 的 secondary owner 能从 trace 解释。
- `secondary_owned_edge_info_count`、`exhaust_*` ledger 不再由 bounded-face classifier probe 写出。

### 阶段 6：切换 compound / openWireCompound 导出

目标：用 FreeCAD `openWireCompound` / `myShapesToReturn` / `aHistory` 生命周期替换 generated open-export 过渡来源。

动作：

1. 按 FreeCAD `buildClosedWire()` 生成 `compound`。
2. 按 FreeCAD `build()` 生成 `openWireCompound`：

   ```text
   iteration == -3 || (!wireInfo && iteration >= 0)
   ```

3. `getOpenWires(noOriginal=true)` 只做 source shared-vertex purge。
4. 删除或降级：
   - `generatedOpenExportShapeForSketchInternals()`
   - generated open-export 过渡导出逻辑。

完成条件：

- T/cross/overlap result-wire 数量仍与 oracle 一致。
- `generated_open_export_edge_info_count == 0`。
- `source_lineage_missing_open_export_edge_info_count == 0`。
- dangling open line、internal-branch cutter、pad-dangling 不再依赖 `purgeAsOriginalOpenEdge`，且原始 open edge 不进入 `InternalShape` / ElementMap。
- dangling single bounded face 不再需要特殊 owner 可见性字段。
- `SketchInternalBuilder` 不知道 result-wire 复制细节。

### 阶段 7：接入 topo history

目标：把 `MapperHistory(aHistory)` 等价能力接入 `NamedShape` / `ElementMap`。

动作：

1. 新增 WireJoiner history public result：
   - source edge。
   - generated shapes。
   - modified shapes。
   - deleted shapes。
   - openWireCompound result edge。
2. 在 `topo` 新增 WireJoiner history consumer。
3. `NamedShape.history` 的 WireJoiner split/generated/deleted 只来自该 consumer。
4. 删除 raw/internal 几何采样式 WireJoiner history 兜底。

完成条件：

- through-open-cutter、dangling filtered edge、partial overlap 的 history 能追溯 WireJoiner ledger。
- `ElementMap` 不再靠 edge endpoint / curve sampling 发明 WireJoiner history。

### 阶段 8：切换 Sketch InternalShape 主路径

目标：让 Sketch 只保留 FreeCAD 调用顺序。

目标流程：

```text
FaceMakerBuildFace result
  -> WireJoiner.addShape(raw sketch edges)
  -> WireJoiner.getOpenWires(openWires, "SKF")
  -> result.makeElementCompound({result, openWires})
  -> NamedShape consumes FaceMakerHistory + WireJoinerHistory
```

动作：

1. `SketchInternalBuilder` 删除 WireJoiner 过渡参数。
2. `Profile` selection 只依赖 InternalFace 数量和 FreeCAD profile 语义。
3. mesh / subshape response 只读最终 InternalShape。

完成条件：

- `cad-core/src/features/sketch_object.cpp` 不出现 WireJoiner result-wire 推理。
- `cad-core/src/geometry/sketch_internal_builder.cpp` 不出现 partial overlap / T/cross/cycle 规则。

### 阶段 9：清理临时路径和文档验收

动作：

1. 删除或降级所有临时过渡项：
   - `generatedOpenExportShapeForSketchInternals()`。
   - `purgeAsOriginalOpenEdge` identity bridge。
   - generated open-export 过渡来源。
   - bounded-face classifier probe 对任何 lifecycle 字段的影响。
2. 更新 `06-02-02-37-cad-core临时诊断主路径偏移整改方案.md`：
   - 把 WireJoiner 剩余风险标记为已迁移。
   - 标明删除了哪些过渡路径。
3. 新增或更新专项测试说明。

完成条件：

- 代码中不再有“为了 T/cross/overlap/dangling fixture 复制 edge”的主路径规则。
- 文档能说明 FreeCAD 文件、函数、字段、cad-core 落点和验收命令。

## 验收矩阵

必须通过：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py
```

重点 fixture：

- `sketch-internal-face-through-open-cutter`
- `sketch-internal-face-branch-open-cutter`
- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-dangling-line`
- `sketch-internal-face-split-and-dangling`
- `sketch-internal-face-three-overlap-circles`
- `sketch-internal-face-arc-lens`
- `sketch-internal-face-cubic-figure8-bspline`

新增专项验收建议：

- self-intersection 与 inter-edge intersection 同时存在。
- closed bounded faces 与 open wires 同时存在。
- source edge 一对多 split fragments。
- shared edge 同时有 `wireInfo` 和 `wireInfo2`。
- `noOriginal=true` purge 只删除 source shared-vertex open wires。
- `aHistory` generated / modified / deleted 能被 `NamedShape` 消费。

## 禁止事项

迁移期间禁止：

- 在 `SketchObject`、`SketchInternalBuilder`、adapter 或 response 层按 fixture 名称分支。
- 在 `getOpenWires()` 中新增 midpoint、boundary-touch、same-coordinate endpoint 这类几何猜测规则。
- 用 InternalShape 结果反推 `wireInfo` / `wireInfo2`。
- 通过 raw/internal geometry sampling 发明 WireJoiner split/generated/deleted history。
- 为了保住当前测试，在 generated open-export 过渡来源上继续叠 T/cross/overlap 特判。

如果某阶段短期必须保留旧桥接：

- 必须写明“临时桥接”。
- 必须写明删除条件。
- 必须只服务当前阶段切换，不允许继续扩展新形态规则。

## 风险与拆分建议

最大风险不是 OCCT 构造，而是身份账本。

建议不要一次性替换输出。正确拆分是：

```text
先补 ledger trace
  -> 再让 ledger 与现有输出并跑
  -> 再切 getOpenWires
  -> 再切 topo history
  -> 最后删 generated open-export 过渡来源
```

每个阶段都要能回答：

- FreeCAD 哪个函数写了这个字段？
- cad-core 哪个结构承接这个字段？
- 这个字段是否影响 shape / subshape / NamedShape / ElementMap？
- 如果影响，是否已经有 history 依据？

## 完成定义

这项迁移完成的标准是：

1. `WireJoiner::getOpenWires()` 的输出只来自真实 final `EdgeInfo` lifecycle。
2. `WireJoiner` 的 generated / modified / deleted history 进入 `topo` 消费路径。
3. `SketchInternalBuilder` 不再包含 result-wire 复制或形态判断。
4. 当前 generated open-export 过渡来源被删除，或只剩完全不参与输出的 diagnostic。
5. MVP、P5 Sketch、P6 Topology 验收全部通过。
6. 方案文档更新为已实现，并记录剩余非阻塞差异。
