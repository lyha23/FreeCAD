# M3 generated result-wire identity

## 目标

删除 `generatedOpenExportShapeForSketchInternals()` 这个 generated result-wire 过渡来源，用 FreeCAD WireJoiner 的 `aHistory`、`EdgeInfo` / `WireInfo` 生命周期和最终 `openWireCompound` 导出条件产出 result-wire identity。

M3 是当前最关键的未完成 milestone。M2 不能安全切换，核心原因就是 M3 还没完成。

M3 同时负责把 M1 留下的 `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 从“summary 后验归因”迁移到 rerun 拒绝分支的 causal blocker 统计。M3 不重新定义 M1 的 ownership 规则，不放宽 `idxVertex` / `stackPos` 顺序约束，也不靠输出数量反推 `wireInfo`。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getResultWires()`

关键字段：

- `aHistory`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::iteration`
- `openWireCompound`

`myShapesToReturn` 属于 FaceMaker 侧结果集合，不是 WireJoiner 字段；M3 不把它当成 WireJoiner result-wire identity 的实现落点。

## cad-core 落点

- `cad-core/src/geometry/wire_joiner.cpp::generatedOpenExportShapeForSketchInternals()`
- `cad-core/src/geometry/wire_joiner.cpp::buildFinalEdgeOwnership()`
- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/tests/test_p5_sketch.py`

## 当前基线

已完成：

- generated open-export 已从单一 bool 拆成 producer reason。
- producer reason 已进入 `wire_joiner_history_detail.open_export_history_entries`。
- `open_wire_compound_generated_*` 分类计数已进入 ledger。
- generated open-export bridge 已能追溯到被复制的现有 split `EdgeInfo`。
- generated open-export 已前置为 `HelperOpenExportOverridePlan`：先绑定到现有 `EdgeInfo`，再让 repeated split/exhaust rerun gate 消费 plan。
- 一对一可绑定的 generated open-export 已改为写入 `EdgeInfo::openExportOverride`，`openWireCompound` / `getOpenWires()` 在导出阶段消费 override，不再 append 游离 generated `EdgeInfo`。
- 未绑定或重复绑定的 helper candidate 已从 generated 风险字段迁到 `helper_open_export_override_unbound_edge_count` / `helper_open_export_override_duplicate_source_edge_info_count`；`helper_open_export_override_candidate_edge_count` 记录 helper 候选规模。当前重点 generated fixture 中 helper unbound / duplicate 为 0，旧 `generated_open_export_unbound_edge_count` / `generated_open_export_duplicate_source_edge_info_count` 保持 0。
- helper 产出的 open-export override 已从 `generated_open_export_*` / `open_wire_compound_generated_*` identity 字段拆出，改由 `helper_open_export_override_*` / `open_wire_compound_helper_open_export_override_*` 承载剩余 helper 依赖。当前重点 generated fixture 中旧 generated identity 计数为 0，helper override 计数仍非 0。
- helper override 已开始绑定 FreeCAD removal lifecycle evidence：`recordBuildClosedWireRemovalLifecycle()` 会区分被 `buildClosedWire()` 设为 `iteration=-1` 的 target `EdgeInfo`，以及 FreeCAD 实际传给 `aHistory->Remove(info.edge)` 的 source `EdgeInfo`。`helper_open_export_override_removed_source_edge_info_count` / `helper_open_export_override_missing_removed_source_edge_info_count` 现在按更严格的 `aHistory` source 统计并透传到 history entry。`partial_shared_closed_wire` 的 arc-lens case 已能做到 helper override 全部有 `aHistory` removed-source evidence；cross / T / segmented / closed-cycle 仍有 missing removed-source 余额。
- `buildClosedWire()` 的 target-to-source 删除关系已进入 M3 producer evidence：被删除 target `EdgeInfo` 会记录实际触发 `aHistory->Remove(info.edge)` 的 source EdgeInfo 索引，以及该 Remove source 携带的 source lineage。对应字段为 `helper_open_export_override_removed_target_edge_info_count`、`helper_open_export_override_ahistory_remove_source_edge_info_count` 和 `helper_open_export_override_ahistory_remove_source_lineage_edge_info_count`，并透传到 open-export history entry。该证据不把普通 `iteration=-1` target 冒充成 strict removed-source，只用于拆分剩余 helper 的真实 producer 缺口。
- repeated split/exhaust rerun 的下一轮 removal scan 已写回同一套 M3 producer evidence：当 rerun-created owner 会在下一轮 `buildClosedWire()` 中触发 target removal / `aHistory->Remove(info.edge)` 时，cad-core 记录 target-to-Remove-source EdgeInfo 与 Remove-source lineage，并把这部分删除计入 `wire_joiner_history_detail.deleted_history_count`。该写回保持 output-neutral，不提前修改 `iteration` / `wireInfo`，所以不会绕过 M2 的 openWireCompound 切换边界。
- `sourceEdgeArray -> split EdgeInfo` lineage 已补 copied-edge source 恢复：cad-core 现在先按 `IsSame` 绑定 sourceEdgeArray；若 face/open-wire 输入已经复制导致 TShape identity 丢失，再把与 sourceEdgeArray 完全等价的 copied EdgeInfo 绑定回同一 source index。该恢复只用于 `aHistory->AddModified(split.intersectShape, newInfo.edge)` 的 request-local source lineage，不给 helper copy edge 造 lineage，也不改变输出边集合。当前 cross / segmented / T-junction generated fixture 的 `source_lineage_missing_open_export_edge_info_count` 已收敛为 0。
- helper override 已暴露 source-lineage group 级 strict Remove-source producer evidence：`helper_open_export_override_source_lineage_removed_source_edge_info_count` / `helper_open_export_override_missing_source_lineage_removed_source_edge_info_count` 用来区分“当前 helper entry 自己就是 strict `aHistory->Remove(info.edge)` source”、“当前 entry 不是 strict source，但同一 sourceEdgeArray lineage group 里已有 strict Remove-source EdgeInfo”和“该 source lineage group 仍没有 strict producer”。这不是把缺 strict 的 helper entry 冒充为 `aHistory` source，而是把剩余 helper 缺口从 source lineage 缺失进一步细分到 producer group 缺失。当前重点结果：cross 10/2、segmented 8/2、T-junction 3/5、three-overlap 15/0、arc-lens 1/0。
- helper override 已把 `aHistory->Remove(info.edge)` source lineage 继续拆成同源 / 外源 producer evidence：`helper_open_export_override_ahistory_remove_same_source_lineage_edge_info_count` 表示 Remove source 的 sourceEdgeArray lineage 与当前 helper-selected `EdgeInfo` 相交；`helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count` 表示该 helper entry 只追到其它 sourceEdgeArray lineage 的 Remove source。这一步不改变输出，不把外源 Remove 当成当前 entry 的 producer。当前重点结果为：cross 6/1、segmented 4/1、T-junction 2/0、three-overlap 7/2、arc-lens 1/0。
- rerun gate 的 helper producer 安全判定已收窄为 strict Remove source 或同源 Remove source evidence：`helper_open_export_override_safe_ahistory_producer_evidence_edge_info_count` 表示该 helper-selected `EdgeInfo` 可以作为 M3 rerun gate 的安全 producer evidence，`helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count` 表示仍不能安全迁移。外源 Remove lineage 不再被算作 safe producer。当前重点结果为：cross 6/6、segmented 4/6、T-junction 2/6、three-overlap 7/8、arc-lens 1/0。
- helper binding 候选选择优先级已同步到 safe producer 语义：同等几何 candidate 中，已经满足 FreeCAD `openWireCompound` final gate 且具备 strict / 同源 `aHistory` producer evidence 的 `EdgeInfo` 会优先于只有外源 target-to-source Remove 关系的 candidate；非 final-gate candidate 也先选 safe producer evidence，再把外源 Remove 证据保留为风险分类。这一步不新增输出，只避免 helper binding 把外源 Remove 误当成更强 producer。
- helper override 已暴露 FreeCAD openWireCompound 导出条件证据：`helper_open_export_override_open_wire_compound_eligible_edge_info_count` 统计不靠 override 也满足 `iteration == -3 || (!wireInfo && iteration >= 0)` 的 helper-selected EdgeInfo，`helper_open_export_override_forced_open_wire_compound_edge_info_count` 统计仍被 `hasOpenExportOverride()` 强制导出的 EdgeInfo。当前重点 generated fixture 中，rerun miss live reset 已让 cross / segmented / T-junction / three-overlap 各有 1 / 1 / 2 / 3 条 helper-selected `EdgeInfo` 自然满足 final export gate；arc-lens 仍为 0。
- helper override forced 余额已按 safe producer evidence 继续拆分：`helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 表示已有 strict / 同源 `aHistory` producer evidence 但仍因 `iteration < 0` 不能走 FreeCAD final export gate；`helper_open_export_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 表示既被 forced 导出又缺安全 producer evidence。当前重点结果为：cross 6/5、segmented 4/5、T-junction 2/4、three-overlap 7/5、arc-lens 1/0。结论是 safe evidence 只是 producer 证据，不等于真实 result-wire lifecycle 已完成。
- helper binding 候选池已进入诊断：每个 helper open-export binding 会记录可替代的 source `EdgeInfo` 候选，以及其中已经满足 FreeCAD `openWireCompound` 导出条件的候选。当前重点 generated fixture 中 eligible candidate 余额为 cross 1、segmented 1、T-junction 2、three-overlap 3、arc-lens 0；这些 eligible 候选来自 repeated `findClosedWires(true)` 搜索 miss 后的 live owner reset，不是重选候选或几何形态猜测。
- helper-selected `EdgeInfo` 的 final export gate 阻挡原因已进入诊断：`helper_open_export_override_export_blocked_by_iteration_edge_info_count` 表示候选已被 `buildClosedWire()` / rerun removal 标为 `iteration < 0`，`helper_open_export_override_export_blocked_by_wire_info_edge_info_count` 表示候选仍是 active 但挂着 `wireInfo` owner。当前重点余额为：cross 11/0、segmented 9/0、T-junction 6/0、three-overlap 12/0、arc-lens 1/0。wireInfo-blocked 余额已由 FreeCAD rerun miss reset 子路径清零，下一步不能再加 helper selection 规则，而要处理“被 Remove 的候选如何形成真实 result-wire producer”。
- helper override 已增加 source-edge export shape 闸门：只有 helper edge 与 selected `EdgeInfo::edge` 是同一 OCCT TShape，或 `partial_shared_closed_wire` 的 selected `EdgeInfo` 同时具备 strict `aHistory->Remove(info.edge)` source、removed target 与 source lineage 时，才允许 `openExportOverride` 使用 selected `EdgeInfo::edge`；否则继续保留 helper edge，避免在 M2 的 source shared-vertex purge / child-wire merge 尚未完成时改变 InternalEdge 输出。当前重点 generated fixture 中，cross / segmented / T-junction / three-overlap 仍为 0，arc-lens 的 `partial_shared_closed_wire` 已达到 `helper_open_export_override_source_edge_export_shape_edge_info_count == helper_open_export_override_edge_info_count == 1`。已确认仅凭自然满足 final gate 就切 selected `EdgeInfo::edge` 会让 cross / segmented / three-overlap 的 InternalEdge 数量偏离 expected，因此该子路径必须等 M2 child-wire / source-vertex identity 准备好后再推进。
- helper override 已新增 final-gate-but-not-source-shape 诊断：`helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count` 标出已经自然满足 FreeCAD final `openWireCompound` gate，但仍不能使用 selected `EdgeInfo::edge` 的 helper entry。当前重点结果为 cross 1、segmented 1、T-junction 2、three-overlap 3、arc-lens 0。该字段把“producer gate 已满足”和“M2 shape/child-wire identity 尚未准备好”分开，避免再次把 eligible 误当成可直接切换输出。
- helper override 已显式暴露 full `aHistory` producer evidence：`helper_open_export_override_full_ahistory_producer_evidence_edge_info_count` 表示 selected `EdgeInfo` 同时具备 strict `aHistory->Remove(info.edge)` source、removed target 与 source lineage；`helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count` 表示已有完整 producer evidence 但仍不能安全使用 selected `EdgeInfo::edge` 作为输出 shape。当前重点余额为：cross 6/6、segmented 4/4、T-junction 2/2、three-overlap 5/5、arc-lens 1/0。结论是 full evidence 只能证明 producer 证据已齐，不是通用 source-edge export shape 切换条件；cross / T-junction / three-overlap 若仅凭 full evidence 切换会改变 InternalEdge 输出，只有 `partial_shared_closed_wire` 的 arc-lens case 当前通过 source-edge shape 闸门。
- safe-forced 余额已继续拆成 full producer evidence forced 与 safe-without-full forced：`helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 表示完整 `aHistory` producer evidence 已齐但仍没有形成 FreeCAD final `openWireCompound` lifecycle，`helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 表示只满足同源 safe evidence、还缺完整 Remove-source / removed-target / source-lineage 三件套。当前重点结果为：cross 6/0、segmented 4/0、T-junction 2/0、three-overlap 5/2、arc-lens 1/0。这把 three-overlap 中“同源安全但 producer 证据未满”的 2 条与“producer 证据已满但仍 forced”的 5 条分开，下一步不能再把 safe-forced 作为单一迁移目标。
- helper override 已暴露 `findSuperEdgesUpdateFirst()` 的 superEdge member/root producer evidence：`helper_open_export_override_super_edge_member_edge_info_count` 标出 helper-selected `EdgeInfo` 是否是 FreeCAD 会设为 `current->iteration = -1` 的 super-edge member，`helper_open_export_override_super_edge_member_with_root_edge_info_count` 标出同一 `superEdgeInfo` 下是否能追到 root，`helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count` 标出该 root 是否自然满足 FreeCAD final `openWireCompound` gate。root 的 safe/full `aHistory` producer evidence 也已拆出：`helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count` / `helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count` 表示 root 是否已有 strict / 同源 Remove source 证据以及完整 Remove-source / removed-target / source-lineage 三件套。当前重点结果为：cross member/root/root-eligible/root-safe/root-full = 4/4/0/4/4，segmented 4/4/0/4/4，T-junction 4/4/2/2/2，three-overlap 3/3/1/2/2，arc-lens 0/0/0/0/0；其中 forced member / missing-safe-forced member 分别为 cross 4/4、segmented 4/4、T-junction 4/4、three-overlap 3/3、arc-lens 0/0。该证据只分类 producer lifecycle，不把 helper 输出切到 root superEdge。
- superEdge root 的 final gate 与 producer evidence 交叉矩阵已进入诊断：`helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count`、`helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count`、`helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count`、`helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count` 分别标出 root 同时满足 final gate + safe evidence、满足 final gate 但缺 safe evidence、有 safe evidence 但缺 final gate、有 full evidence 但缺 final gate。当前重点结果为：cross 0/0/4/4，segmented 0/0/4/4，T-junction 0/2/2/2，three-overlap 0/1/2/2，arc-lens 0/0/0/0。该矩阵把后续 M3 缺口拆成两类：root 已有 producer evidence 但没有 final `openWireCompound` lifecycle，以及 root 已有 final gate 但还缺同源 `aHistory` producer evidence；仍不切换输出 shape。
- superEdge root 的 lifecycle 与 final-gate blocker 已继续拆分：`helper_open_export_override_super_edge_member_root_open_lifecycle_edge_info_count` / `helper_open_export_override_super_edge_member_root_closed_lifecycle_edge_info_count` 区分 FreeCAD `findSuperEdgesUpdateFirst()` 的 open root 与 closed root；`helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count` / `helper_open_export_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count` 区分 root 未进入 final `openWireCompound` gate 的直接原因。当前重点结果为：cross open/closed/iteration-blocked/wireInfo-blocked = 4/0/4/0，segmented 4/0/4/0，T-junction 4/0/2/0，three-overlap 3/0/2/0，arc-lens 0/0/0/0。结论是这些 root 已是 open-root lifecycle，不是 closed-root；缺 final gate 的余额是后续 removal 造成的 `iteration < 0` 阻挡，不是 wireInfo owner 残留。
- superEdge root 的 `iteration < 0` 阻挡已和 safe/full producer evidence 组成交叉统计：`helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count`、`helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count`、`helper_open_export_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count` 分别标出被后续 removal 阻断但已有 safe evidence、已有 full evidence、缺 safe evidence 的 root。当前重点结果为：cross 4/4/0，segmented 4/4/0，T-junction 2/2/0，three-overlap 2/2/0，arc-lens 0/0/0。结论是当前 iteration-blocked root 全部至少有 safe producer evidence，three-overlap primary-removal root 也已具备完整 target-to-Remove-source full evidence；下一步主缺口不是再找同源 Remove source，而是把这些被 removal 消费的 open root 迁成真实 result-wire producer / final `openWireCompound` child wire。
- superEdge root 的 `iteration < 0` 阻挡已按 FreeCAD `buildClosedWire()` removal 分支继续拆分：`helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count`、`helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count`、`helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count`、`helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count` 分别标出 root 被 unowned branch、primary owner branch、secondary owner branch 消费，或缺失 removal branch evidence。当前重点结果为：cross 4/0/0/0，segmented 4/0/0/0，T-junction 2/0/0/0，three-overlap 1/1/0/0，arc-lens 0/0/0/0。结论是当前 root iteration-blocked 余额已经能追到具体 removal branch，且 missing branch 为 0；下一步要按 removal branch 迁移真实 result-wire producer / final child wire，而不是继续在 helper member 或输出端补规则。
- superEdge root 已新增输出中立的 result-wire producer candidate 证据：`helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count`、`helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count`、`helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count` 分别标出被 removal 消费的 open-root 是否已有 materialized `superEdge` producer candidate，以及该 candidate 是否具备完整 `aHistory` producer evidence。当前重点结果为：cross 4/4/0，segmented 4/4/0，T-junction 2/2/0，three-overlap 2/2/0，arc-lens 0/0/0。这证明当前 iteration-blocked root 都已有真实 root `superEdge` producer candidate 和完整 root-side producer evidence。该证据不切换输出，不把 member 或 root `superEdge` 提前导出。
- result-wire producer candidate 已继续按 removal branch 拆分，并单独标出 missing-full `aHistory` 缺口所属分支。当前 candidate 的 unowned / primary / secondary / missing-branch 为：cross 4/0/0/0，segmented 4/0/0/0，T-junction 2/0/0/0，three-overlap 1/1/0/0，arc-lens 0/0/0/0；missing-full candidate 的 unowned / primary / secondary / missing-branch 为：cross 0/0/0/0，segmented 0/0/0/0，T-junction 0/0/0/0，three-overlap 0/0/0/0，arc-lens 0/0/0/0。结论是 root candidate 的 missing-full 分支已经清零；下一步应把具备 child-wire producer evidence 的 root 接入 M2/M3 共同认可的 final child-wire 输出边界。
- unowned-removal candidate 已新增 child-wire producer readiness 证据：`helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count` 表示该 root candidate 属于 unowned-removal 分支，并且已经具备完整 `aHistory` producer evidence。当前重点结果为：cross 4，segmented 4，T-junction 2，three-overlap 1，arc-lens 0。该字段锁定首批可迁移目标，但仍保持 output-neutral，不把 root `superEdge` 直接作为 helper 输出。
- unowned-removal ready root 已进入 `openWireCompound` child-wire producer 账本：`open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count` / `..._wire_built_wire_info_count` 记录 child-wire 侧已 materialize 的 root `superEdge` producer wire，当前 candidate / built 为 cross 4/4、segmented 4/4、T-junction 2/2、three-overlap 2/2、arc-lens 0/0；`..._unowned_removal_child_wire_producer_ready_*` 对应 ready 子集，当前 ready / ready-built 为 cross 4/4、segmented 4/4、T-junction 2/2、three-overlap 1/1、arc-lens 0/0。该节点证明 child-wire ledger 已能承载真实 root producer wire，但尚未把 root `superEdge` 接替 helper child-wire 输出形状。
- unowned-removal ready root 已增加 child-wire output guard：只有 root producer 是单成员 `superEdge` 时才允许直接接替 `childWire.wire`；多成员 root producer 会进入 `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count`，不改变输出。当前 cross / segmented / T-junction / three-overlap 的 ready-built 为 4 / 4 / 2 / 1，实际 output 为 0 / 0 / 0 / 0，multi-member blocked 为 4 / 4 / 2 / 1。该结果说明 root producer wire 已到 child-wire 边界，但当前会携带 sibling member，必须等 M2/M3 继续补 child-wire member 抑制或真实 final child ownership，不能在输出端修剪。
- multi-member root producer 已补正式 member coverage 账本：`open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_covered_member_edge_info_count` / `..._current_member_wire_info_count` / `..._non_current_member_edge_info_count` 记录 child-wire 侧 root producer 覆盖的完整 superEdge 成员、当前 helper child-wire 对应成员，以及直接输出 root `superEdge` 会额外携带的 non-current 成员；`..._output_blocked_non_current_member_edge_info_count` 记录被 multi-member guard 阻挡的 non-current 成员规模。当前重点结果为：cross covered/current/non-current/blocked-non-current = 8/4/4/4，segmented 8/4/4/4，T-junction 6/2/4/4，three-overlap 4/2/2/1，arc-lens 0/0/0/0。该节点把“root producer 已 ready 但不能直接输出”的原因从一个布尔 guard 拆成 FreeCAD `findSuperEdgesUpdateFirst()` 成员账本；后续应迁移 child ownership / member 抑制，而不是按输出边数量裁剪。
- child-wire 输出来源已从 helper override 总量继续拆成真实 source-edge shape 与仍依赖 helper shape：`open_wire_compound_helper_open_export_override_source_edge_export_shape_wire_info_count` 统计 child-wire 输出已使用 selected `EdgeInfo::edge` 的子集，`open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` 统计仍使用 `generatedOpenExportShapeForSketchInternals()` 产出的 helper edge 形状。当前重点结果为：cross helper/source-shape/helper-shape = 12/0/12，segmented 10/0/10，T-junction 8/0/8，three-overlap 15/0/15，arc-lens 1/1/0。结论是 `partial_shared_closed_wire` 的 arc-lens child-wire 输出已不再使用 helper shape；剩余主缺口集中在 cross / segmented / T-junction / three-overlap 的 helper-shape 输出和 root producer multi-member 阻挡。
- source-edge shape 输出已显式升级成 source-edge producer output 账本：`helper_open_export_override_source_edge_producer_output` / `open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count` / `open_export_helper_override_source_edge_producer_output_edge_info_count` 贯穿 `EdgeInfo` history、`OpenWireCompoundWireInfo` child-wire 和 M4 `SketchInternalHistoryContext`。该字段只标记 `openExportOverride` 已使用 selected `EdgeInfo::edge` 的输出来源，不把 helper binding 冒充成 final lifecycle；当前它必须与 source-edge export shape 计数一致。后续 M3 余额应优先盯 `open_wire_compound_helper_open_export_override_helper_shape_wire_info_count`，而不是把 arc-lens 这种已 source-edge producer 输出的条目混回 helper-shape 输出缺口。
- unowned-ready root producer 已补 member-suppression 缺口账本：`open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count` 记录仍需 member suppression 的 child-wire；`...requires_member_suppression_root_edge_info_count` 记录唯一 root producer 组；`...requires_member_suppression_covered_member_edge_info_count` / `...requires_member_suppression_current_member_wire_info_count` / `...requires_member_suppression_non_current_member_edge_info_count` 记录逐 child-wire 的完整成员、当前成员和 non-current 成员。当前重点结果为：cross wire/root/covered/current/non-current = 4/4/8/4/4，segmented 4/4/8/4/4，T-junction 2/1/6/2/4，three-overlap 1/1/2/1/1，arc-lens 0/0/0/0/0；其中 current + non-current 必须等于 covered。结论是 root producer 已经到达 child-wire 边界，但 child-wire 数与唯一 root producer 组不是同一维度；下一步必须补正式 child ownership / member suppression，而不是放宽 root 输出 guard。
- member-suppression 已继续从逐 child-wire 累计推进到 root-group child ownership 聚合：`...root_unique_covered_member_edge_info_count` / `...root_unique_current_member_edge_info_count` / `...root_suppressed_pending_member_edge_info_count` / `...root_pending_member_edge_info_count` 去重统计每个 root producer 的成员覆盖、已有 child owner 成员、已由 unowned-removal full `aHistory` evidence 正式抑制的 non-current 成员，以及仍缺 child ownership 的 pending 成员；`...root_complete_child_ownership_root_edge_info_count` / `...root_incomplete_child_ownership_root_edge_info_count` 标出 root 组是否已可不携带 sibling member。当前重点结果为 covered/current/suppressed/pending/complete-root/incomplete-root：cross 8/4/4/0/4/0，segmented 8/4/4/0/4/0，T-junction 3/2/1/0/1/0，three-overlap 2/1/1/0/1/0，arc-lens 0/0/0/0/0/0。结论是 root-group pending 已归零，complete child ownership root 已出现；这一步只关闭 child ownership 缺口，不直接改 helper 输出形状。
- member-suppressed current-member producer candidate 已进入 child-wire 账本，但输出仍受 source-shape 闸门保护：`...member_suppressed_wire_built_wire_info_count` / `...member_suppressed_output_candidate_wire_info_count` 标出可由当前 child owner 的真实 `EdgeInfo::wire()` 构造的 candidate，`...current_member_child_wire_producer_ready_wire_info_count` 标出 root producer evidence 已投射到当前 member child-wire identity，`...current_member_child_wire_producer_full_ahistory_evidence_wire_info_count` 标出该 current member child-wire producer 已具备完整 `aHistory` producer evidence，`...member_suppressed_output_blocked_by_source_shape_wire_info_count` 标出 child ownership 已完整但 selected `EdgeInfo::edge` / M2 child-wire identity 还不能替换 helper shape，`...member_suppressed_output_wire_info_count` 必须等 source-shape / child-wire identity ready 后才可非 0。当前重点结果为 built/candidate/current-ready/current-full/pending-block/source-shape-block/output/helper-shape：cross 4/4/4/4/0/4/0/12，segmented 4/4/4/4/0/4/0/10，T-junction 2/2/2/2/0/2/0/8，three-overlap 1/1/1/1/0/1/0/15，arc-lens 0/0/0/0/0/0/0/0。source-shape blocker 继续拆分为 block/full/missing/eligible/forced/root-ready/current-ready/current-full/current-missing/helper-shape：cross 4/4/0/0/4/4/4/4/0/12，segmented 4/4/0/0/4/4/4/4/0/10，T-junction 2/2/0/0/2/2/2/2/0/8，three-overlap 1/1/0/0/1/1/1/1/0/15，arc-lens 0/0/0/0/0/0/0/0/0/0。结论是 root-ready evidence 已正式落到 current child-wire producer identity，并已把完整 `aHistory` evidence 投射到 current member child-wire producer；但输出仍被 source-shape / M2 child-wire identity 闸门阻断，helper shape 余额仍按原数保留。
- root-group suppressed pending member 已按 full `aHistory` producer evidence 与 FreeCAD `buildClosedWire()` unowned-removal 分支写入正式账本：`...root_suppressed_pending_member_full_ahistory_producer_evidence_edge_info_count` 与 `...root_suppressed_pending_member_unowned_removal_edge_info_count` 必须等于 `...root_suppressed_pending_member_edge_info_count`。当前重点结果说明原 pending member 全部是 full evidence + unowned removal；remaining pending / missing full / missing branch 均为 0。结论是下一步不能继续新增 source-lineage 诊断解释这批余额，而要补 source-edge export shape / M2 child-wire identity，使 member-suppressed output 可以安全替换 helper shape。
- `getOpenWires()` 的输出入口已从重新扫描 `EdgeInfo` / `hasOpenExportOverride()` 前移到 `recordOpenWireCompoundLedger()` 生成的 `OpenWireCompoundWireInfo` child-wire 账本；只有没有 child-wire ledger 的旧调用才回退到 EdgeInfo 扫描。该切换让后续 root result-wire producer 可以在 child-wire 边界接替 helper 输出，但当前 child-wire 的主输出形状仍保留 helper override，因此 M3 不能标完成。
- repeated split/exhaust rerun search miss 已开始切入 live reset：`repeated_split_exhaust_rerun_miss_live_reset_edge_info_count` 对应 FreeCAD `findClosedWires(true)` 入口清空 `wireInfo` / `wireInfo2` 后搜索 miss，active helper 候选因此保持 unowned，并通过真实 `openWireCompound` gate 导出。当前重点余额为 cross 1、segmented 1、T-junction 2、three-overlap 3、arc-lens 0；这部分 helper override 不再属于 forced openWireCompound。
- `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 已从 `ledgerSummary()` 的 generated 数量后验统计，迁到 rerun 拒绝分支的 causal blocker 统计。当前 generated fixture 中该字段为 0，含义是本轮 rerun 没有实际 resettable owner 被 generated identity 阻塞，不再等同于 generated open-export 数量。

剩余 producer reason：

- `consumed_open_cutter_graph`
- `partial_junction_open_cutter`
- `closed_wire_cycle`
- `partial_shared_closed_wire`

仍未完成：

- 这些 reason 仍是过渡分类，不是真实 FreeCAD result-wire identity。
- `helper_open_export_override_edge_info_count` 仍标出 helper 过渡来源。
- `helper_open_export_override_missing_removed_source_edge_info_count` 仍标出尚未能追到 FreeCAD `aHistory->Remove(info.edge)` source 的 helper override；这些项不能视为真实 `aHistory` producer。
- `helper_open_export_override_missing_ahistory_remove_source_edge_info_count` / `helper_open_export_override_missing_ahistory_remove_source_lineage_edge_info_count` 标出连 target-to-source Remove 关系或 Remove-source lineage 都还没有覆盖的 helper override；这些是后续删除 helper 的优先缺口。
- `helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count` 标出虽然存在 `aHistory->Remove(info.edge)` source lineage，但该 source lineage 不属于当前 helper-selected `EdgeInfo`。这些项不能被算作当前 entry 的真实 result-wire producer，只能作为外源 Remove 依赖继续回到 `buildClosedWire()` / `aHistory` 传播路径处理。
- `helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count` 标出 rerun gate 不能把该 helper binding 当成安全 producer 的余额；该字段会同时覆盖缺 Remove source、只有外源 Remove source、或只有 source-lineage group 证据但当前 entry 不是 safe producer 的情况。
- `helper_open_export_override_missing_source_lineage_removed_source_edge_info_count` 标出同一 sourceEdgeArray lineage group 内也没有 strict Remove-source producer 的 helper override；这些项比只有 target-to-source 关系的项更早暴露真实 `aHistory` producer 缺口。
- `helper_open_export_override_forced_open_wire_compound_edge_info_count` 标出 `hasOpenExportOverride()` 仍在绕过 FreeCAD `build()` 的最终导出条件；只要该字段非 0，就不能把 helper override 当成真实 final `openWireCompound` child wire。
- `helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` / `helper_open_export_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 把 forced 余额拆成“有 producer 证据但缺 final result-wire lifecycle”和“producer 证据也缺”两类；前者需要补被 Remove 候选如何形成真实 result-wire producer，后者还要先补同源 `aHistory` source。
- `helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` / `helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 继续拆分 safe-forced：前者卡在 full producer evidence 之后的 final lifecycle，后者还卡在完整 producer evidence 之前。
- `helper_open_export_override_missing_open_wire_compound_eligible_candidate_edge_info_count` 标出当前 binding 候选池里仍没有任何已满足 FreeCAD final export gate 的 `EdgeInfo`；这类项不能靠 helper selection priority 修复，必须回到 `buildClosedWire()` / `findTightBound()` / `exhaustTightBound()` 生命周期迁移。
- `helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count` 标出 final gate 已满足但 source-edge shape 仍不能切换的余额；这类项不是 forced lifecycle 问题，而是 M2 child-wire / source-vertex identity 还没完成。
- `helper_open_export_override_export_blocked_by_iteration_edge_info_count` 与 `helper_open_export_override_export_blocked_by_wire_info_edge_info_count` 把 forced export 余额拆成两类：前者需要补真实 Remove/result-wire producer，后者需要补 repeated `findClosedWires(true); findTightBound()` 后 owner reset / unowned export 生命周期。当前 wireInfo 类已清零，二者之和必须等于 forced export 数量。
- `helper_open_export_override_source_edge_export_shape_edge_info_count` 标出 helper 输出形状是否已经来自 selected `EdgeInfo::edge`。该字段当前只在 `partial_shared_closed_wire` 的 arc-lens case 中为 1；其它重点 reason 仍为 0，意味着 helper edge 仍不是同一 OCCT TShape，也没有足够 producer evidence 证明直接切换安全。如果直接用自然 final-gate 或几何等价的 source edge，会触发 M2 未完成的 shared-vertex purge / child-wire merge 差异。
- `helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count` 非 0 时，说明 selected `EdgeInfo` 已有完整 `aHistory` producer evidence，但 output shape identity / child-wire 边界仍未通过 M2/M3 的正式生命周期。这类余额不能靠继续放宽 source-edge shape 闸门清零。
- `helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 非 0 时，说明 producer evidence 已完整但 selected `EdgeInfo` 仍不满足 FreeCAD final export gate；这是 Remove/result-wire lifecycle 缺口，不是 source-edge shape 闸门问题。
- `helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 非 0 时，说明该 forced entry 只有同源 safe evidence，还没达到完整 `aHistory` producer evidence；three-overlap 当前仍有该类余额。
- `helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count` / `helper_open_export_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` 非 0 时，说明 helper-selected entry 是 FreeCAD super-edge member lifecycle 中会被置 `iteration=-1` 的成员；即使已追到 root，也还没有把 root `superEdge` / final result-wire producer 正式迁移到 `openWireCompound` 主路径。root-safe/root-full 与 root-eligible 不完全重叠时，说明后续需要分别补“root 已有 producer evidence 但没有 final gate”和“root 已有 final gate 但没有 producer evidence”两类生命周期，而不是继续用 member 强制导出。
- `helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count` / `helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count` / `helper_open_export_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count` 非 0 时，说明 superEdge root 已经落在 FreeCAD open-root lifecycle，但后续 `buildClosedWire()` removal 把它从 final export gate 移走。当前 missing-safe iteration-blocked 为 0，root candidate 的 missing-full 也已归零，所以不能再把这批余额归因到“缺同源 Remove source”或“缺完整 `aHistory` 三件套”；后续要处理的是 final child-wire 输出边界。
- `helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count` / `helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count` / `helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count` / `helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count` 非 0 时，说明 root final-gate 阻挡已经能归到 `buildClosedWire()` 的具体 removal branch。missing branch 当前为 0，因此后续不应再扩大几何推断，而应分别处理 unowned removal 与 owner removal 后如何形成真实 result-wire producer。
- `helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count` 非 0 时，说明被 removal 消费的 open-root 已有 materialized `superEdge` producer candidate；`helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count` 非 0 时，还要先补完整 Remove-source / removed-target / source-lineage 证据。`getOpenWires()` 已经读取 child-wire ledger，但 candidate evidence 还没有接替 helper child-wire 输出形状，不能替代 M2 source-edge / child-wire identity。
- `helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_*_removal_edge_info_count` 非 0 时，说明 producer candidate 已能归到具体 `buildClosedWire()` removal 分支；`...missing_full_ahistory_producer_evidence_*_removal_edge_info_count` 非 0 时，说明该分支的 candidate 仍缺完整 `aHistory` 三件套。当前 root candidate missing-full 已归零，下一步不能继续在该层叠加证据字段。
- `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_non_current_member_edge_info_count` 与 `..._output_blocked_non_current_member_edge_info_count` 非 0 时，说明 root producer wire 已经进入 child-wire ledger，但完整 root `superEdge` 仍会携带当前 child-wire 以外的成员。该类余额不是 output shape 特判问题，应回到 M2/M3 的 child ownership / member suppression 正式路径处理。
- `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count` 非 0 时，说明 ready root producer 不能直接输出；`...requires_member_suppression_root_edge_info_count` 是后续真正应迁移的 root producer 组数。当前 `...root_pending_member_edge_info_count` 已归零，`...root_suppressed_pending_member_edge_info_count` 记录的 non-current member 已由 unowned-removal full `aHistory` evidence 正式抑制；剩余缺口转为 source-shape / M2 child-wire identity，而不是 child ownership。
- `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_full_ahistory_producer_evidence_edge_info_count` 与 `...root_suppressed_pending_member_unowned_removal_edge_info_count` 必须覆盖所有 `...root_suppressed_pending_member_edge_info_count`。若后续出现 suppressed member 缺 full evidence 或缺 unowned-removal branch，应先回退到 pending 而不是强行输出；当前重点 fixture 中这两项已完全覆盖 suppressed pending member。
- `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_candidate_wire_info_count` 非 0 只说明 current-member candidate wire 可构造；如果 `...member_suppressed_output_blocked_by_source_shape_wire_info_count` 同步非 0，仍不得替换 helper shape。`...blocked_by_source_shape_root_producer_ready_wire_info_count` 非 0 说明 root producer evidence 已经 ready；`...current_member_child_wire_producer_ready_wire_info_count` / `...blocked_by_source_shape_current_member_child_wire_producer_ready_wire_info_count` 非 0 说明 root-ready evidence 已经落到 current child-wire producer identity；`...blocked_by_source_shape_missing_full_ahistory_producer_evidence_wire_info_count` / `...blocked_by_source_shape_forced_open_wire_compound_wire_info_count` 非 0 说明 current child member 还没有完整 producer evidence 和 final export lifecycle。只有 source-edge export shape / M2 child-wire identity 也 ready 后，`...member_suppressed_output_wire_info_count` 才能作为 output 切换信号。
- `open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` 非 0 时，说明该 child-wire 的实际输出 shape 仍来自 `generatedOpenExportShapeForSketchInternals()` 生成的 helper edge；`...source_edge_export_shape_wire_info_count` 非 0 时，只能证明输出 shape 已切到 selected `EdgeInfo::edge`，不代表 final `openWireCompound` lifecycle 已完整，因为 forced export gate 可能仍存在。
- `open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count` 非 0 时，说明该 child-wire 已经用 selected `EdgeInfo::edge` 作为输出 producer shape；该字段当前与 `...source_edge_export_shape_wire_info_count` 保持一致，只用于把 source-edge producer 输出和真正 helper-shape 输出分开，不代表 `helper_open_export_override_forced_open_wire_compound_edge_info_count` 已清零。
- `source_lineage_missing_open_export_edge_info_count` / `open_export_helper_override_missing_source_lineage_edge_count` 在重点 generated fixture 中已收敛为 0；但这只证明 helper override 已能追到 sourceEdgeArray lineage，不代表它已经是真实 result-wire producer。
- rerun gate 已不再使用固定 `HelperOpenExportOverridePlan::identitySafe=false`；它会逐个 helper binding 检查当前 `EdgeInfo` 是否具备 strict `aHistory->Remove(info.edge)` source + source lineage，或被删除 target 能追到带 source lineage 的实际 Remove source。缺少这些证据的 binding 仍按 helper-produced identity 处理。
- helper 仍按 producer reason 产出 candidate edge；本轮只是禁止 candidate 脱离 final `EdgeInfo` 成为输出。
- strict `aHistory->Remove(info.edge)` producer 仍不完整：`consumed_open_cutter_graph`、`partial_junction_open_cutter`、`closed_wire_cycle` 还有 helper override 只能追到 source lineage 或 target-to-source 关系，尚不能把每条 open-export helper 都绑定到同一个具备 strict Remove-source evidence 的 final `EdgeInfo`。

当前主路径：

```text
splitEdges()
  -> rebuildAdjacentList()
  -> assignClosedWireOwners()
  -> recordBranchSearchCandidates()
  -> recordTightBoundLifecycle()
  -> computeHelperOpenExportOverridePlan()
  -> recordExhaustTightBoundLifecycle()
  -> recordBuildClosedWireRemovalLifecycle()
  -> recordRepeatedSplitExhaustRerunLifecycle(helperPlan)
  -> applyHelperOpenExportOverridePlan(openExportOverride, helper risk count only)
  -> recordOpenWireCompoundLedger()
  -> getOpenWires() consumes OpenWireCompoundWireInfo first
```

`repeated_split_exhaust_generated_identity_blocked_edge_info_count > 0` 不是 M1 owner lifecycle 缺失，而是 generated result-wire identity 尚未统一并且实际阻塞了 rerun live reset。该字段为 0 也不代表 M3 完成；只说明当前 rerun 没有 resettable owner 被 generated identity 阻塞。

## 设计原则

1. generated result-wire 不能再作为游离的最终输出补边。
2. generated result-wire 必须绑定回已有 source `EdgeInfo`。
3. `findClosedWires()`、`findTightBound()`、`exhaustTightBound()` 仍使用 source edge / split edge 的生命周期状态。
4. `openWireCompound` / `getOpenWires()` 在导出阶段优先消费 child-wire ledger；过渡期 child-wire 仍可携带 helper export override，不能在输出端用几何等价替换。
5. M3 不修改 M1 的 FreeCAD stack / vertexStack / edgeSet / wireSet 语义。

## 推荐结构与流程

在 `EdgeInfo` 上增加 export override，而不是 append 游离 generated `EdgeInfo`：

```cpp
struct EdgeInfo {
    TopoDS_Edge edge;

    // owner lifecycle 使用 edge；open export 使用 openExportOverride。
    std::optional<TopoDS_Edge> openExportOverride;

    bool generatedOpenExportEdge = false;
    std::string generatedOpenExportReason;
    bool generatedOpenExportSourceEdgeInfo = false;
    std::size_t generatedOpenExportSourceEdgeInfoIndex = 0;
    bool generatedOpenExportSourceEdgeInfoConsumed = false;
};
```

导出时使用：

```cpp
TopoDS_Wire EdgeInfo::exportWire() const
{
    if (openExportOverride) {
        return BRepBuilderAPI_MakeWire(*openExportOverride).Wire();
    }
    return wire();
}
```

先计算 helper export override plan，再让 rerun gate 使用该 plan，最后才应用导出 override：

```text
computeHelperOpenExportOverridePlan(finalInfo, boundedFaceShape, openEdges, closedWires)
  -> recordRepeatedSplitExhaustRerunLifecycle(finalInfo, boundedFaces, helperPlan)
  -> applyHelperOpenExportOverridePlan(finalInfo, helperPlan)
  -> recordOpenWireCompoundLedger(finalInfo)
```

建议结构：

```cpp
struct HelperOpenExportOverrideBinding {
    std::size_t sourceEdgeInfoIndex = 0;
    TopoDS_Edge helperEdge;
    std::string reason;
    bool consumedSourceEdgeInfo = false;
};

struct HelperOpenExportOverridePlan {
    bool needed = false;
    std::size_t candidateEdgeCount = 0;
    std::size_t unboundEdgeCount = 0;
    std::vector<HelperOpenExportOverrideBinding> bindings;
};
```

rerun gate 不再扫描尚未 append 的 generated edge，而是使用 `HelperOpenExportOverridePlan`：

```cpp
const bool helperIdentityUnsafe =
    helperOpenExportOverridePlanHasUnsafeProducer(finalInfo, helperPlan);

const bool canApplyWithLiveReset =
    !canApplyWithoutReset
    && resettableAssignedEdges > 0U
    && (owner.branchSearchCandidateCount == 0U || !helperIdentityUnsafe);
```

如果不能 live reset，则在实际拒绝分支记录 causal blocker：

```cpp
if (!canApplyWithoutReset && !canApplyWithLiveReset) {
    if (helperIdentityUnsafe && resettableAssignedEdges > 0U) {
        info.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount += resettableAssignedEdges;
    }
    continue;
}
```

同时删除 `ledgerSummary()` 中按 `generatedOpenExportEdgeInfoCount` 事后反推 blocker 的逻辑。

## 必收切片

1. 按 producer reason 逐类替换，不再把所有 result-wire copy 混在一个 helper。
2. 对每类 reason 找到 FreeCAD 对应路径：
   - closed wire 被消费后的 result-wire。
   - partial junction 的 open cutter result。
   - cross / segmented-cross 的 bounded-result edge。
   - partial shared closed wire 的保留边。
3. 让 result-wire edge 从真实 `EdgeInfo` / `WireInfo` / `aHistory` 生命周期产出。
4. 删除或降级 `generatedOpenExportShapeForSketchInternals()`，使其不参与 shape 输出。
5. `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 从 summary 后验统计迁移到 rerun 拒绝分支的 causal blocker 统计。
6. generated fixture 中 blocker 不再无条件等于 helper override 数量，而是等于真实被 helper-produced identity 阻塞的 rerun owner / edge 数。

当前下一步：继续压 `open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` 和 `helper_open_export_override_forced_open_wire_compound_edge_info_count`，但当前 forced 余额已经全部是 `iteration < 0` 的 Remove/result-wire producer 问题；`getOpenWires()` 已经切到 child-wire ledger 入口，superEdge root 已有 output-neutral result-wire producer candidate，root candidate missing-full 已归零，ready 子集已经 materialize 到 child-wire ledger，current-member producer candidate 已可构造，root-ready evidence 与 current-member full `aHistory` evidence 已落到 current child-wire producer identity，root-group pending member 已归零并出现 complete child ownership root。下一步应处理 `...member_suppressed_output_blocked_by_source_shape_wire_info_count`：这批 blocker 的 root/current child-wire producer 和 full `aHistory` evidence 已 ready，但 selected `EdgeInfo::edge` / M2 child-wire identity 仍不能安全替换 helper shape。后续要补 source-edge export shape / M2 child-wire identity，使 current-member producer candidate 可以安全替换 helper shape，再允许 `...member_suppressed_output_wire_info_count` 非 0；随后再处理 primary-removal root candidate。不能把 member 本身强制导出，不能绕过 child-wire ledger 直接把 root `superEdge` 塞进 helper path，也不能从 wireInfo owner reset 或输出端修正方向继续扩大。并行推进时必须注意：除已由 full `aHistory` producer evidence 覆盖的 `partial_shared_closed_wire` 外，helper edge 与 selected `EdgeInfo::edge` 当前不是同一 TShape；即使 selected `EdgeInfo` 自然满足 final gate，也不能在 M2 source shared-vertex purge / child-wire merge 尚未完成前直接用几何等价 source edge 替代输出形状。随后再处理 `consumed_open_cutter_graph` / `partial_junction_open_cutter` / `closed_wire_cycle` 中仍缺 strict Remove-source evidence 的余额。不能再把普通 `iteration=-1` target 当成已完成的 `aHistory` producer，也不能给 helper copy edge 人工补 source lineage。

## 边界

M3 只负责 result-wire identity 的来源。

M3 不负责：

- `getOpenWires()` shared-vertex purge 的最终切换。这属于 M2。
- 元素级 history 写入 `NamedShape`。这属于 M4。
- 在 `SketchInternalBuilder` 里复制 result-wire。M6 明确禁止。

## 非目标

- 不给 generated copy edge 人工补 `sourceEdgeIndices` 冒充真实 lineage。
- 不继续扩展 `generatedOpenExportShapeForSketchInternals()` 的几何形态规则。
- 不以 fixture 期望数量倒推 result-wire ownership。
- 不在 M3 中修改 `findTightBoundSplitWire()` 的 `idxVertex` / `stackPos` 顺序判断。

## 验收

完成条件：

- `generated_open_export_edge_info_count == 0`
- `open_wire_compound_generated_wire_info_count == 0`
- `generated_open_export_unbound_edge_count == 0`
- `generated_open_export_duplicate_source_edge_info_count == 0`
- `helper_open_export_override_edge_info_count == 0`
- `helper_open_export_override_candidate_edge_count == 0`
- `helper_open_export_override_unbound_edge_count == 0`
- `helper_open_export_override_duplicate_source_edge_info_count == 0`
- `helper_open_export_override_missing_removed_source_edge_info_count == 0`
- `helper_open_export_override_missing_ahistory_remove_source_edge_info_count == 0`
- `helper_open_export_override_missing_ahistory_remove_source_lineage_edge_info_count == 0`
- `helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count == 0`
- `helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count == 0`
- `helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count == 0`
- `helper_open_export_override_missing_source_lineage_removed_source_edge_info_count == 0`
- `helper_open_export_override_missing_open_wire_compound_eligible_candidate_edge_info_count == 0`
- `helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count == 0`
- `helper_open_export_override_source_edge_export_shape_edge_info_count == helper_open_export_override_edge_info_count`
- `helper_open_export_override_forced_open_wire_compound_edge_info_count == 0`
- `open_wire_compound_helper_open_export_override_wire_info_count == 0`
- `source_lineage_missing_open_export_edge_info_count == 0`
- `open_export_helper_override_missing_source_lineage_edge_count == 0`
- `repeated_split_exhaust_generated_identity_blocked_edge_info_count == 0`
- 删除 `generatedOpenExportShapeForSketchInternals()` 后，T/cross/overlap result-wire 数量仍与 oracle 一致。
- `openWireCompound`、`NamedShape.history` 和 `ElementMap` 能消费同一套 source `EdgeInfo` identity。

重点 fixture：

- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-three-overlap-circles`
- `sketch-internal-face-arc-lens`
- `sketch-internal-face-bullseye`
