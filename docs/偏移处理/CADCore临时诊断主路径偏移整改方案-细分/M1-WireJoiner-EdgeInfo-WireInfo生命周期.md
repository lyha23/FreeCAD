# M1 WireJoiner EdgeInfo / WireInfo 生命周期

## 目标

把 WireJoiner 主路径从“结果形态推断 ownership”迁回 FreeCAD `EdgeInfo` / `WireInfo` 生命周期。

M1 只负责 WireJoiner 内部账本，不负责 topo `ElementMap`，也不负责 `SketchInternalBuilder` 输出端补边。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildAdjacentList()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findClosedWires()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`

关键字段：

- `EdgeInfo::iteration`
- `EdgeInfo::iteration2`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::superEdge`
- `WireInfo::vertices`
- `WireInfo::done`
- `WireInfo::purge`

## cad-core 落点

- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/src/geometry/wire_joiner.cpp`
- `cad-core/tests/test_p5_sketch.py`

## 当前基线

已完成：

- `EdgeInfo` 已有 `p1`、`p2`、`mid`、`iStart`、`iEnd` 等基础字段。
- primary owner 已从 bounded-face classifier 转为 closed path search。
- graph fallback owner 写入路径已删除，`graph_fallback_assigned_edge_info_count == 0` 是回归字段。
- graph-degree secondary owner 已删除，`graph_secondary_owner_edge_info_count == 0` 是回归字段。
- open leaf 已能通过 `iteration=-3` 进入 open export。
- `findSuperEdgesUpdateFirst()` lifecycle 已切入 live `EdgeInfo`。
- `EdgeInfo::shape(forward)` / `EdgeInfo::wire(forward)` 已补。
- `WireInfo::purge` 已进入 request-local lifecycle 账本：`_findClosedWiresWithExisting()` / `_findClosedWiresUpdateStack()` 触发的 purge trace 会写入 `OwnerWireInfo::purge`，并通过 `tight_bound_purged_wire_info_count` 暴露。
- `purge/done/exhaust` 已推进到 consumed-removal 内的 live reset：`recordExhaustTightBoundLifecycle()` 会记录 `OwnerWireInfo::exhaustVisited`、`exhaustDone`、`exhaustDiscardedByPurge`，并通过 `tight_bound_exhaust_visited_wire_info_count`、`tight_bound_exhaust_done_wire_info_count`、`tight_bound_exhaust_discarded_purged_wire_info_count` 暴露 FreeCAD 等价的 `done2/discard2` 结果；被 discard 的 owner 不再作为 done2 seed、secondary owner search 或 consumed-edge removal 的 done owner，随后 `recordBuildClosedWireRemovalLifecycle()` 只在同一个 consumed-edge removal 分支里清空 primary `EdgeInfo::wireInfo`，由 `tight_bound_exhaust_primary_reset_edge_info_count` 记录。仍不能被该 removal 同步消费的残留 owner 才进入 `tight_bound_exhaust_primary_reset_blocked_edge_info_count`。
- `_findClosedWires()` 的 full `wireSet` 已从纯 trace 推进到 live transfer-candidate gate 和 owner purge lifecycle：`traceExistingWireSearchForCandidate()` 记录 `tight_bound_full_wire_set_insert_count`、`tight_bound_full_wire_set_erase_count`、`tight_bound_full_wire_set_abort_count`、`tight_bound_full_wire_set_purge_candidate_count`；`wireSet` abort / purge 会写入 `OwnerWireInfo::purge`，并阻止没有后续 existing-wire hit 的 candidate 继续进入 `TightBoundTransferWire`。`tight_bound_full_wire_set_blocked_transfer_count` 与增量 `tight_bound_existing_wire_purge_count` 是回归字段。该状态不做输出端 pruning。
- full `wireSet` abort 已拆成 branch-level search lifecycle：`tight_bound_full_wire_set_abort_search_count` 记录发生 abort 的 search 轮次，`tight_bound_full_wire_set_abort_resolved_by_hit_count` 记录 abort 后仍被 existing-wire hit 消化的轮次，`tight_bound_full_wire_set_abort_blocked_search_count` 记录 abort 后没有 hit、最终阻断 transfer 的轮次。这把 FreeCAD `_findClosedWiresUpdateStack()` 的 branch abort/backtrack 与最终 transfer gate 分开。
- 多轮 `idxVertex` / `stackPos` 已从 successful transfer 坐标扩展到 existing-wire search hit lifecycle：`tight_bound_existing_wire_search_idx_vertex_count` / `tight_bound_existing_wire_search_stack_pos_count` 记录每轮 `_findClosedWiresWithExisting()` 命中写回，`tight_bound_existing_wire_search_path_vertex_count` 记录命中 path 的 vertexStack 范围；`tight_bound_existing_wire_idx_vertex_count` / `tight_bound_existing_wire_stack_pos_count` 仍只表示最终 transfer wire 消费的坐标。
- existing-wire hit lifecycle 已拆出 selected / search-only / path-blocked 三类：`tight_bound_existing_wire_selected_hit_count` 表示最终 `TightBoundTransferWire` 消费的 hit；`tight_bound_existing_wire_search_only_hit_count` / `tight_bound_existing_wire_search_only_idx_vertex_count` / `tight_bound_existing_wire_search_only_stack_pos_count` 表示 `_findClosedWiresWithExisting()` 已命中但尚未进入 selected transfer 的坐标余额；`tight_bound_existing_wire_search_only_path_blocked_count` 表示 hit path 已存在但不能安全转成 selected transfer，并继续拆成 `tight_bound_existing_wire_search_only_owner_vertex_blocked_count`、`tight_bound_existing_wire_search_only_order_blocked_count`、`tight_bound_existing_wire_search_only_wire_build_blocked_count`。当前 through/split/branch/adjacent/arc-lens 以及 Pad/Extrude 衍生 internal-face case 的 search-only 余额全部是 FreeCAD `idxV <= idxEnd` 顺序约束挡住，不改输出，也不放宽成 selected transfer。
- `findTightBoundUpdateVertices()` 的 done-owner 传播已从宽泛 output trace 收窄到 FreeCAD 两个分支账本，并把 unfinished `otherWire` 分支推进为 live `EdgeInfo::wireInfo` 写回：`owner_propagation_unassigned_candidate_count` 对应 `if (!info->wireInfo)`，当前只记账不触发；`owner_propagation_other_wire_candidate_count` 对应 unfinished `otherWire` 被 done owner 覆盖，`owner_propagation_other_wire_live_edge_info_count` 记录实际执行 `info->wireInfo = beginInfo.wireInfo` 的 edge 数。该 live 写回不改变 `openWireCompound` 输出，但会让 overlap 这类 case 不再需要 repeated split/exhaust resettable fallback。
- `findTightBoundSplitWire()` 已开始切入 live splitWire owner id：只有当 remaining old-owner 顶点仍满足 `info->wireInfo == wireInfo` 时才分配 `splitWireId` 并改写 `EdgeInfo::wireInfo`；`tight_bound_live_split_wire_info_count` / `tight_bound_live_split_wire_edge_info_count` 记录实际 live 接管，cross 这类只剩 sidecar 的 case 不伪造 splitWire id。
- `exhaustTightBoundWithAdjacent()` 的 secondary-owner 搜索已从全局坐标 DFS 推进到 FreeCAD adjacent-list / edgeSet / wireSet gate：`traceExhaustAdjacentSearch()` 从 adjacent done owner seed 出发，记录 `exhaust_adjacent_search_*`、`exhaust_adjacent_wire_set_*` 和 `exhaust_adjacent_wire_info2_abort_count`；只有该搜索 hit 时才写 `EdgeInfo::wireInfo2`，cross/T-junction 能证明 hit 路径，three-overlap 能证明 miss/backtrack 与 wireInfo2 abort 路径。旧的“两端 done owner 交集”快捷 fallback 已删除，secondary owner 不再绕过 FreeCAD stack/wireSet 路径。
- `buildClosedWire()` 的 repeated split/exhaust 需求已进入 output-neutral sidecar：`recordBuildClosedWireRemovalLifecycle()` 在 consumed edge removal 后记录 `repeated_split_exhaust_cycle_count` 和 `repeated_split_exhaust_removed_edge_info_count`，并按 FreeCAD removal 分支拆成 `repeated_split_exhaust_removed_unowned_edge_info_count`、`repeated_split_exhaust_removed_secondary_edge_info_count`、`repeated_split_exhaust_removed_primary_edge_info_count`；这些字段对应删除后重跑 `findClosedWires(true); findTightBound();` 的循环。
- repeated split/exhaust 的 rerun 入口已进入 output-neutral reset/rebuild scan ledger：consumed edge removal 后用临时 `WireInfo` 复核 `findClosedWires(true)` 的扫描域，并按 FreeCAD 入口先清空 `EdgeInfo::wireInfo` / `wireInfo2` 再重建 owner。`repeated_split_exhaust_rerun_reset_primary_edge_info_count` / `repeated_split_exhaust_rerun_reset_secondary_edge_info_count` 记录 rerun 入口会清掉的旧 owner，`repeated_split_exhaust_rerun_active_edge_info_count`、`repeated_split_exhaust_rerun_owned_active_edge_info_count`、`repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count` 解释当前 active/open-leaf 扫描域；`repeated_split_exhaust_rerun_closed_wire_search_count`、`repeated_split_exhaust_rerun_closed_wire_miss_count`、`repeated_split_exhaust_rerun_closed_wire_info_count` 与后续 branch-search 字段保留 live rerun 替换点。cross/T-junction 这类 case 已能证明 reset 后会重新搜索一次，而不是被旧 owner 提前跳过。
- repeated split/exhaust rerun 保留安全 live 写回子集：当 rerun `findClosedWires(true)` 找到的新 closed `WireInfo` 对应的原始 `EdgeInfo` 仍全部 active 且 unowned，或只被旧 primary owner 阻挡且不依赖 generated open-export identity 时，`recordRepeatedSplitExhaustRerunLifecycle()` 才会写回 live `EdgeInfo::wireInfo` / `OwnerWireInfo`。otherWire live propagation 切入后，overlap rectangles / overlap circles 已不再进入 repeated split/exhaust cycle；cross-pattern / line-arc same-endpoints 这类无 generated bridge 的 resettable rerun owner 已能 live reset 并继续走 live branch scan / done；cross/T-junction 仍是 search miss；through/branch/split-dangling 只剩 open leaf；split-line/adjacent/Pad/Extrude internal-face case 记录 `repeated_split_exhaust_rerun_no_active_search_count`，说明 removal 后没有 active EdgeInfo 可作为 `findClosedWires(true)` seed，不伪造新 owner。
- repeated split/exhaust rerun 已继续推进到 live `findTightBound()` 子路径：live rerun owner 写回后会在同一 owner 上执行 branch candidate 搜索，并调用 `recordTightBoundTransferWire()` 尝试生成 transfer `WireInfo`；若没有 transfer，则按 `findTightBoundUpdateVertices()` 的完成分支标记 owner done。`repeated_split_exhaust_rerun_live_branch_search_*`、`repeated_split_exhaust_rerun_live_transfer_wire_info_count`、`repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count`、`repeated_split_exhaust_rerun_live_done_wire_info_count` 约束该路径。该路径仍只在已有 live rerun owner 上运行，不改变 generated result-wire 或 `openWireCompound` 输出。
- repeated split/exhaust rerun 已补下一轮 removal / loop-exit 边界：FreeCAD `buildClosedWire()` 初始阶段调用 `exhaustTightBound()`，但 while 循环尾部只重跑 `findClosedWires(true); findTightBound()`；下一轮 while 开头重新构造 `counter` 并只在 `++counter[vertex.edgeInfo()] == 2` 时删除 edge。`repeated_split_exhaust_rerun_removal_*` 记录 live rerun owner 写回后的下一轮 removal scan，`repeated_split_exhaust_rerun_loop_exit_no_removal_count` 记录无新增 removal时循环会退出。该路径仍用于真正进入 rerun 的 owner；overlap rectangles / overlap circles 当前由 otherWire live propagation 提前消化，不再进入该 fallback。
- repeated split/exhaust 的 M3 阻塞已进入 output-neutral ledger：`repeated_split_exhaust_generated_identity_blocked_edge_info_count` 标记 pending rerun 同时仍依赖 generated open-export EdgeInfo 的数量；through-open-cutter 这类无 generated result-wire 的 pending rerun 为 0，T/cross/overlap/lens 等 generated result-wire fixture 与 `generated_open_export_edge_info_count` 一致。

仍未完成：

- `findTightBoundSplitWire()` 已有 live splitWire owner id 的安全子集；existing-wire hit 也已记录 path vertex 并尝试用 `idxVertex/stackPos` 构造 transfer path。但 through/split/branch/adjacent-rectangles/arc-lens 以及 Pad/Extrude 衍生 internal-face case 仍表现为 `tight_bound_existing_wire_search_only_order_blocked_count > 0`，说明 hit path 存在，但按 FreeCAD `idxEnd == 0` 归一化和 `ENSURE(idxV <= idxEnd)` 不能安全转 selected transfer。剩余推进必须来自 repeated split/exhaust owner lifecycle 或 M3 generated result-wire identity，不得靠放宽 idx/stack 顺序条件消掉余额。
- full repeated split / exhaust 循环还未完整迁移。按 FreeCAD `buildClosedWire()`，删除 consumed edge 后会重跑 `findClosedWires(true)` 和 `findTightBound()`，然后进入下一轮 removal scan；不会在 while 尾部再次调用 `exhaustTightBound()`。当前 rerun 已按 FreeCAD 重置 primary/secondary owner，并能区分 active/unowned live 写回、无 generated bridge 的 resettable live reset / branch done、otherWire live propagation 后无需 rerun、下一轮 removal loop-exit、no-active seed 和 search miss，但尚未把会产生新增 removal 或依赖 generated-result identity 的 rerun owner 安全切成 live mutation。直接让该 rerun 清理 generated-result 相关 owner 会使 T/cross result-wire 从真实输出中消失，只剩 1-2 条 source-lineage open export。`repeated_split_exhaust_generated_identity_blocked_edge_info_count` 已把这类剩余切换归因到 M3 的 generated result-wire identity 替换。
- `exhaustDiscardedByPurge` 已能在 consumed-edge removal 内清空对应 primary `EdgeInfo::wireInfo`，但完整 `wireInfo2` 重搜和下一轮 removal mutation 仍未接管；直接把未消费的 owner 或 generated result-wire 相关 owner 清掉仍会使 cross generated result-wire 消失、overlap/arc 输出偏离，剩余缺口已归到 M2/M3 identity 边界，不能在 M1 内靠输出端规则补齐。

## 本轮 M1 切片

FreeCAD 调用链：

```text
WireJoinerP::build()
  -> buildClosedWire()
  -> findClosedWires(true)
  -> findTightBound()
  -> _findClosedWires(..., beginInfo.wireInfo, &stackPos)
  -> _findClosedWiresWithExisting() / _findClosedWiresUpdateStack()
  -> wireInfo->purge = true
  -> exhaustTightBound()
  -> exhaustTightBoundWithAdjacent()
  -> wireSet.clear(); wireSet.insert(next->wireInfo.get())
  -> _findClosedWires(...)
  -> _findClosedWiresUpdateEdges() inserts/erases currentInfo->wireInfo
  -> _findClosedWiresUpdateStack() aborts when wireSet already contains info.wireInfo
  -> buildClosedWire() removes consumed edges
  -> findClosedWires(true); findTightBound() repeats at loop tail
  -> next while pass removes more consumed edges or exits when no removal happens
```

关键依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::WireInfo` 持有 `bool purge = false`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::_findClosedWiresWithExisting()` 在 reverse existing-wire hit 时写 `wireInfo->purge = true`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::_findClosedWiresUpdateStack()` 在 `wireSet.contains(...)`、abort 或 `currentInfo->wireInfo2` 分支写 `wireInfo->purge = true`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::_findClosedWiresUpdateEdges()` 在 `wireSet` 非空时随当前 edge 推进执行 `wireSet.insert(currentInfo->wireInfo.get())`，回退时执行 `wireSet.erase(lastInfo.wireInfo.get())`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBoundUpdateEdge()` 在没有新 wire 且 `wireInfo->purge` 为真时执行 `wireInfo.reset()`，否则才写 `wireInfo->done = true`。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findClosedWires(true)` 入口先遍历所有 `EdgeInfo` 并清空 `info.wireInfo` 与 `info.wireInfo2`，再跳过 `iteration < 0` 或已在本轮重建出 `wireInfo` 的 edge。
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()` 删除 consumed edge 后把 `done = false`，并在循环尾部重跑 `findClosedWires(true)` 和 `findTightBound()`；下一轮 while 开头重新构造 `counter`，如果没有新的 `iteration = -1` 删除，循环退出。

cad-core 落点：

- `cad-core/include/cad_core/geometry/wire_joiner.h`：`OwnerWireInfo::purge` 与 `WireJoinerLedgerSummary::tightBoundPurgedWireInfoCount`。
- `cad-core/include/cad_core/geometry/wire_joiner.h`：`OwnerWireInfo::exhaustVisited`、`exhaustDone`、`exhaustDiscardedByPurge` 记录 `exhaustTightBoundUpdateEdge()` 的 `discard2/done2` sidecar。
- `cad-core/src/geometry/wire_joiner.cpp`：`traceExistingWireSearchForCandidate()` 的 purge trace 写入 owner lifecycle，并保存 existing-wire hit path；`recordTightBoundTransferWire()` 在普通 branch path 不成立但 existing-wire hit 成立时尝试用 `idxVertex/stackPos` 构造 transfer path，不能安全转 selected transfer 的余额写入 `tight_bound_existing_wire_search_only_path_blocked_count`，并按 owner vertex missing、FreeCAD idx/stack order blocked、wire build blocked 拆分原因；`recordExhaustTightBoundLifecycle()` 消费 purge，标记 `exhaustDiscardedByPurge` 或 `exhaustDone`，并让 `isDoneOwner()` 排除 discarded owner；`recordBuildClosedWireRemovalLifecycle()` 在 consumed-edge removal 内清理 discarded primary owner；`ledgerSummary()` 输出 purge/exhaust、live primary reset 和 remaining blocker 计数。
- `cad-core/src/geometry/wire_joiner.cpp`：`traceExistingWireSearchForCandidate()` 同步记录 full `wireSet` 的 insert / erase / abort / purge-candidate；`recordTightBoundTransferWire()` 消费该 trace，把 full `wireSet` purge 写入 `OwnerWireInfo::purge`，区分 abort 后被 hit 消化还是 blocked search，并阻止被 FreeCAD `wireSet` abort / purge 且没有后续 hit 的 candidate 继续成为 transfer wire；`recordBuildClosedWireRemovalLifecycle()` 记录删除 consumed edge 后需要 repeated split/exhaust 的 sidecar，并拆分 unowned / secondary / primary removal 分支。
- `cad-core/src/geometry/wire_joiner.cpp`：`recordRepeatedSplitExhaustRerunLifecycle()` 在 consumed edge 已标为 `iteration=-1` 后，用临时 `WireInfo` 复核 rerun 的 `findClosedWires(true)` reset/rebuild 扫描域与 new-owner/branch-search 可能性；reset 会先清空临时 `EdgeInfo::wireInfo` / `wireInfo2` 并写入 `repeated_split_exhaust_rerun_reset_primary_edge_info_count` / `repeated_split_exhaust_rerun_reset_secondary_edge_info_count`，随后重新执行 closed-wire search；若 removal 后没有 active seed，则记录 `repeated_split_exhaust_rerun_no_active_search_count`。当新 owner 的原始 edge 仍全部 active/unowned，或只需重置旧 primary owner 且不依赖 generated open-export identity 时写回 live `EdgeInfo::wireInfo`，并继续执行该 live owner 的 branch candidate、transfer wire 或 done 标记；已有 live rerun owner 后，再按下一轮 while removal 条件记录 `repeated_split_exhaust_rerun_removal_*` 和 loop-exit。否则保留为 scan ledger，不改变 `openWireCompound` 输出。
- `cad-core/src/geometry/wire_joiner.cpp`：`recordTightBoundLifecycle()` 将 `findTightBoundUpdateVertices()` 的 done-owner 传播拆成 unassigned 与 unfinished otherWire 两类；旧 `owner_propagation_candidate_count` 保持为两类之和，并在 unfinished otherWire 分支执行 live `EdgeInfo::wireInfo` 写回，计入 `owner_propagation_other_wire_live_edge_info_count`。
- `cad-core/src/geometry/wire_joiner.cpp`：`recordTightBoundLifecycle()` 为可安全迁移的 `findTightBoundSplitWire()` 子集创建 `OwnerWireInfo::splitWireId`，并把仍属于旧 owner 的 split vertices 改写到该 live owner id。
- `cad-core/src/geometry/wire_joiner.cpp`：`recordExhaustAdjacentSecondaryOwners()` 通过 `traceExhaustAdjacentSearch()` 消费 FreeCAD `exhaustTightBoundWithAdjacent()` 的 adjacent-list / wireSet / `currentInfo->wireInfo2` branch gate，不再用全局坐标 DFS 或两端 owner 交集 fallback 直接判断 secondary owner。
- `cad-core/src/features/sketch_object.cpp`：`wire_joiner_ledger.tight_bound_purged_wire_info_count`、`tight_bound_exhaust_*` 与 `tight_bound_exhaust_primary_reset_edge_info_count` 写入 result metadata。
- `cad-core/src/features/sketch_object.cpp`：`wire_joiner_ledger.tight_bound_full_wire_set_*`、`tight_bound_existing_wire_search_idx_vertex_count`、`tight_bound_existing_wire_search_stack_pos_count`、`tight_bound_existing_wire_search_path_vertex_count`、`tight_bound_existing_wire_selected_hit_count`、`tight_bound_existing_wire_search_only_*`、`tight_bound_existing_wire_search_only_*_blocked_count`、`tight_bound_live_split_wire_*`、`owner_propagation_*`、`repeated_split_exhaust_*` 与 `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 写入 result metadata，其中 `repeated_split_exhaust_rerun_reset_*` 表示 `findClosedWires(true)` 入口 reset，`repeated_split_exhaust_rerun_resettable_*` 表示 live reset 候选，`repeated_split_exhaust_rerun_live_reset_*` 表示实际 live reset，`repeated_split_exhaust_rerun_live_*` 表示安全写回的 rerun owner 及其 live branch/transfer/done 子路径，`repeated_split_exhaust_rerun_removal_*` 表示下一轮 removal scan，非 live 字段仍表示扫描域和 new-owner 可能性。
- `cad-core/tests/test_p5_sketch.py`：约束 purged owner count 只随 existing-wire purge trace 出现，且 purged owner 必须被 exhaust sidecar 消费为 discarded；primary reset 必须由 live reset 或 remaining blocker 解释，且 live reset 只能发生在 consumed unowned removal 内；existing-wire direct hit 必须有 path vertex 证据，search-only hit 必须进入 path-blocked 分类，且当前 through/split/branch/adjacent/arc-lens 与 Pad/Extrude 衍生 internal-face case 的 search-only path-block 必须全部归入 FreeCAD idx/stack order blocked；unfinished otherWire case 必须 live 写回 `EdgeInfo::wireInfo`；through/cross/T-junction case 约束 full `wireSet`、live-blocked transfer 和 repeated split/exhaust sidecar 非零，并约束 rerun reset 后的扫描域能解释当前没有新 unowned closed-wire owner；rerun live 写回和 live branch/transfer/done 字段不得超过 scan 找到的新 owner / assigned edge / branch 范围；bullseye intersected holes 约束 search hit 坐标即使没有最终 transfer 也会写入 search-only lifecycle。
- `cad-core/tests/test_p5_sketch.py`：约束 exhaust adjacent search 的 `search == hit + miss`、wireSet insert/erase、cross/T-junction hit、three-overlap miss/backtrack 和 wireInfo2 abort。

本切片边界：full `wireSet` 只驱动 transfer-candidate 生命周期，不作为输出端 pruning；purge reset 只在同一个 consumed-edge removal 内切入 live `EdgeInfo`，repeated rerun / generated result-wire 仍归因到 M3 readiness，不改变 `openWireCompound`、`NamedShape.history` 或 `ElementMap`。

## 必收切片

1. `sourceEdgeArray -> split EdgeInfo` 的 source edge / source vertex identity 完整进入 `EdgeInfo`。
2. `buildAdjacentList()` 的 adjacent range 与 skip edge lifecycle 可复核。
3. `findClosedWires()` 使用 FreeCAD stack / vertexStack / edgeSet / wireSet 结构，不回退到全局几何扫描。
4. `findTightBound()` 能写出 ordered transfer wire、split owner、new owner。
5. `exhaustTightBound()` 能写出 done owner 与 secondary owner。

## 边界

M1 产出 final `EdgeInfo` / `WireInfo` 生命周期状态和诊断账本。

M1 不直接决定：

- `openWireCompound` 是否切成输出主路径。这属于 M2。
- generated result-wire copy 如何删除。这属于 M3。
- `NamedShape.history` 如何消费。这属于 M4。

## 非目标

- 不在 M1 中用 fixture 输出数量反推 `wireInfo`。
- 不在 M1 中用 bounded face、midpoint、endpoint touch 或 partial overlap 决定 owner。
- 不在 M1 中从 `InternalShape` 反推 WireJoiner history。

## 验收

重点回归字段：

- `graph_fallback_assigned_edge_info_count == 0`
- `graph_secondary_owner_edge_info_count == 0`
- `temporary_result_wire_edge_info_count == 0`
- `tight_bound_exhaust_discarded_purged_wire_info_count <= tight_bound_purged_wire_info_count`
- existing-wire purge 出现时，purged owner 必须进入 `tight_bound_exhaust_discarded_purged_wire_info_count`
- discarded purged owner 不得继续作为 done2 seed；没有 surviving `tight_bound_exhaust_done_wire_info_count` 的 case 中 `exhaust_seed_edge_info_count == 0`
- primary reset 必须可解释：discarded purge case 中 `tight_bound_exhaust_primary_reset_edge_info_count + tight_bound_exhaust_primary_reset_blocked_edge_info_count > 0`；live reset 不超过 `repeated_split_exhaust_removed_unowned_edge_info_count`，remaining blocker 不超过 `primary_owned_edge_info_count`
- `tight_bound_full_wire_set_insert_count >= tight_bound_full_wire_set_erase_count`
- `tight_bound_full_wire_set_abort_count == tight_bound_full_wire_set_purge_candidate_count`
- `tight_bound_full_wire_set_abort_search_count == tight_bound_full_wire_set_abort_resolved_by_hit_count + tight_bound_full_wire_set_abort_blocked_search_count`
- `tight_bound_full_wire_set_abort_resolved_by_hit_count <= tight_bound_existing_wire_hit_count`
- `tight_bound_full_wire_set_blocked_transfer_count > 0` 出现在 full `wireSet` abort fixture 中，且不超过 abort/purge candidate 范围
- `tight_bound_existing_wire_purge_count` 覆盖 reverse existing-wire hit 与 full `wireSet` purge，且 `tight_bound_purged_wire_info_count` 被 `tight_bound_exhaust_discarded_purged_wire_info_count` 消费
- existing-wire search hit 出现时，`tight_bound_existing_wire_search_idx_vertex_count` / `tight_bound_existing_wire_search_stack_pos_count` / `tight_bound_existing_wire_search_path_vertex_count` 必须写入；successful transfer 坐标不得超过 search-hit 坐标；search-only hit 必须由 `tight_bound_existing_wire_search_only_path_blocked_count` 解释，且该总数必须等于 owner-vertex / order / wire-build 三类 blocked 计数之和；当前 through/split/branch/adjacent/arc-lens 与 Pad/Extrude 衍生 internal-face case 的 blocked 总数必须全部落在 `tight_bound_existing_wire_search_only_order_blocked_count`
- `tight_bound_live_split_wire_info_count <= tight_bound_split_owner_wire_info_count`，且有 live splitWire 时必须有 `tight_bound_live_split_wire_edge_info_count > 0`
- T-junction 的 splitWire owner 必须全部 live 接管；cross 这类 sidecar-only splitOwnerVertices 不得伪造 live splitWire id
- `owner_propagation_candidate_count == owner_propagation_unassigned_candidate_count + owner_propagation_other_wire_candidate_count`
- 当前触发 unfinished otherWire 的 case 中，`owner_propagation_other_wire_live_edge_info_count == owner_propagation_other_wire_candidate_count`；through-open-cutter 这类没有 done-owner 顶点传播的 case 必须为 0
- overlap rectangles / overlap circles 的 otherWire live 写回后不再进入 repeated split/exhaust cycle；T-junction / three-overlap 仍保留 generated result-wire identity blocker，不得用该 live 写回替代 M3
- `exhaust_adjacent_search_count == exhaust_adjacent_search_hit_count + exhaust_adjacent_search_miss_count`
- `exhaust_adjacent_wire_set_insert_count >= exhaust_adjacent_wire_set_erase_count`
- cross/T-junction 的 exhaust adjacent search 必须有 hit、wireSet abort 和 wireInfo2 abort；three-overlap 必须有 miss/backtrack 与 wireInfo2 abort
- through/cross/T-junction 的 `repeated_split_exhaust_cycle_count > 0`
- `repeated_split_exhaust_removed_edge_info_count == repeated_split_exhaust_removed_unowned_edge_info_count + repeated_split_exhaust_removed_secondary_edge_info_count + repeated_split_exhaust_removed_primary_edge_info_count`
- `repeated_split_exhaust_rerun_owned_active_edge_info_count <= repeated_split_exhaust_rerun_reset_primary_edge_info_count`
- `repeated_split_exhaust_rerun_closed_wire_search_count <= repeated_split_exhaust_rerun_active_edge_info_count`
- `repeated_split_exhaust_rerun_closed_wire_search_count == repeated_split_exhaust_rerun_closed_wire_info_count + repeated_split_exhaust_rerun_closed_wire_miss_count`
- `repeated_split_exhaust_rerun_resettable_closed_wire_info_count <= repeated_split_exhaust_rerun_closed_wire_info_count`
- `repeated_split_exhaust_rerun_resettable_assigned_edge_info_count <= repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count`
- `repeated_split_exhaust_rerun_live_reset_primary_edge_info_count <= repeated_split_exhaust_rerun_resettable_assigned_edge_info_count`
- `repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count <= repeated_split_exhaust_rerun_reset_secondary_edge_info_count`
- `repeated_split_exhaust_rerun_live_closed_wire_info_count <= repeated_split_exhaust_rerun_closed_wire_info_count`
- `repeated_split_exhaust_rerun_live_assigned_edge_info_count <= repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count`
- `repeated_split_exhaust_rerun_live_closed_wire_vertex_count <= repeated_split_exhaust_rerun_closed_wire_vertex_count`
- `repeated_split_exhaust_rerun_live_branch_search_candidate_count == repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count + repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count`
- `repeated_split_exhaust_rerun_live_transfer_wire_info_count <= repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count`
- `repeated_split_exhaust_rerun_live_transfer_wire_info_count + repeated_split_exhaust_rerun_live_done_wire_info_count <= repeated_split_exhaust_rerun_live_closed_wire_info_count`
- `repeated_split_exhaust_rerun_removal_edge_info_count == repeated_split_exhaust_rerun_removal_unowned_edge_info_count + repeated_split_exhaust_rerun_removal_secondary_edge_info_count + repeated_split_exhaust_rerun_removal_primary_edge_info_count`
- `repeated_split_exhaust_rerun_loop_exit_no_removal_count <= repeated_split_exhaust_rerun_removal_scan_count`
- 当 `repeated_split_exhaust_rerun_removal_edge_info_count == 0` 时，`repeated_split_exhaust_rerun_loop_exit_no_removal_count == repeated_split_exhaust_rerun_removal_scan_count`
- `repeated_split_exhaust_rerun_branch_search_candidate_count == repeated_split_exhaust_rerun_branch_search_inside_candidate_count + repeated_split_exhaust_rerun_branch_search_outside_candidate_count`
- through/branch/split-dangling 的 rerun 域只剩 `iteration=-3` open leaf；split-line/adjacent/Pad/Extrude internal-face case 的 rerun 域在 removal 后没有 active seed，必须记录 `repeated_split_exhaust_rerun_no_active_search_count == 1` 且 `repeated_split_exhaust_rerun_closed_wire_search_count == 0`；cross-cutters/T-junction 的 rerun 域会先 reset 旧 owner 并重新搜索一次，当前因 rerun removal mutation / generated result-wire identity 未接管而记为 miss，不得伪造成新的 closed-wire owner；cross-pattern / line-arc same-endpoints 没有 generated open-export identity 依赖，resettable rerun owner 必须满足 `repeated_split_exhaust_rerun_live_closed_wire_info_count == repeated_split_exhaust_rerun_closed_wire_info_count`，并把 live branch scan 归入 done 路径
- overlap rectangles / overlap circles 的 unfinished otherWire 分支必须 live 写回 done owner，并且不再进入 repeated split/exhaust rerun 域；若后续出现只被旧 primary owner 阻挡且不依赖 generated open-export identity 的新 case，才允许走 rerun live reset 子路径
- generated result-wire fixture 中 `repeated_split_exhaust_generated_identity_blocked_edge_info_count == generated_open_export_edge_info_count`，无 generated result-wire 的 pending rerun 为 0
- `closed_wire_search_*`、`tight_bound_*`、`super_edge_*` 能解释对应 fixture。

当前切片验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py
git diff --check
```
