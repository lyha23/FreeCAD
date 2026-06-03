# CADCore WireJoiner 临时诊断字段批量删除方案

## 1. 目标

当前 `WireJoiner` 的 offset-remediation 主路径已经从 generated/helper 输出，收敛到有限的 `ResultWireProducerIdentity` 与 `result_wire_producer_ledger_entries`。后续清理不要再逐个询问字段是否能删，而应按“同一语义层”批量删除。

本方案目标：

- 删除只服务过渡期的 summary count、legacy helper 哨兵和派生诊断字段。
- 保留 FreeCAD-shaped 账本、请求内 producer identity、runtime open-export history 和 topo/named-shape 消费所需证据。
- 测试从“某个 count 为 0”改为“逐条 entry/history 满足主语义”，避免字段删掉后丢失真实约束。

## 2. 当前基线

当前工作区已经删除了多批 legacy/generated/helper summary，包括：

- `generated_open_export_edge_info_count`
- `open_wire_compound_generated_wire_info_count`
- `open_wire_compound_legacy_helper_shape_wire_info_count`
- `result_wire_producer_blocker_legacy_helper_shape_still_used_count`
- `unowned_removal_ready_legacy_helper_shape_output_count`
- `result_wire_producer_ledger_entry_count`
- `migrated_legacy_helper_slot_count`
- `open_wire_compound_helper_open_export_override_wire_info_count`
- `helper_open_export_override_edge_info_count`
- `result_wire_producer_exported_without_helper_wire_info_count`
- `result_wire_producer_*_count` 的 producer kind/state 汇总
- `result_wire_producer_none_without_blocker_count`
- `source_shape_identity_unknown_count`
- `result_wire_producer_child_wire_ready_count`
- `result_wire_producer_source_shape_ready_count`
- `result_wire_producer_source_shape_not_ready_count`
- `graph_fallback_assigned_edge_info_count`
- `graph_secondary_owner_edge_info_count`

这些字段后续只应保留在 `tests/test_p5_sketch.py` 的 `assertNotIn` 防回流断言中。

## 3. 删除判断标准

可以删：

- 能从 `result_wire_producer_ledger_entries[*]` 直接数出来的 summary。
- 能从 `wire_joiner_history_detail.open_export_history_entries[*]` 直接数出来的 summary。
- 当前只被测试断言为 0，且没有业务代码读取的 JSON 输出字段。
- `helperOpenExportOverride*` 内部字段中 write-only、局部可计算、或已经被 `ResultWireProducerIdentity` 字段覆盖的派生字段。
- 文档里只作为旧计数器映射表存在、不再是当前验收入口的字段。

暂时保留：

- `ResultWireProducerKind`、`ResultWireProducerState`、`ResultWireBlocker`、`ResultWireProducerIdentity`。
- `result_wire_producer_ledger_entries`。
- `wire_joiner_history_detail.open_export_history_entries` 里的 producer identity 字段。
- source lineage、source edge indices、open-export history、NamedShape / ElementMap 消费用的 identity。
- OCCT shape 证据字段，例如 `openExportOverride`、`resultSlotVertexEvidenceEdge`，以及仍直接参与 output 构造的 producer wire。

## 4. 第一批：删除 result-wire blocker summary

这批是最高收益。现在 P6 gate 已经要求每个 producer entry 都是：

```text
state == ExportedWithoutHelper
blocker == None
```

因此下面这些 output summary 不再需要独立输出。保留 `ResultWireBlocker` enum 本身，只删除 count 字段。

删除字段：

```text
result_wire_producer_unknown_invariant_count
result_wire_producer_blocker_missing_source_lineage_count
result_wire_producer_blocker_missing_ahistory_remove_source_count
result_wire_producer_blocker_foreign_ahistory_source_lineage_count
result_wire_producer_blocker_foreign_ahistory_source_shape_ready_lineage_mismatch_count
result_wire_producer_blocker_foreign_ahistory_source_shape_identity_not_ready_count
result_wire_producer_blocker_foreign_ahistory_source_geometry_mismatch_count
result_wire_producer_blocker_missing_removed_target_evidence_count
result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count
result_wire_producer_blocker_final_gate_blocked_by_iteration_count
result_wire_producer_blocker_final_gate_blocked_by_wire_info_count
result_wire_producer_blocker_root_removed_by_unowned_branch_count
result_wire_producer_blocker_root_removed_by_primary_branch_count
result_wire_producer_blocker_root_removed_by_secondary_branch_count
result_wire_producer_blocker_multi_member_root_pending_suppression_count
result_wire_producer_blocker_source_shape_identity_not_ready_count
result_wire_producer_blocker_source_shape_would_purge_original_count
result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count
result_wire_producer_blocker_current_member_source_shape_would_purge_original_count
result_wire_producer_blocker_same_source_sidecar_source_shape_identity_not_ready_count
result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count
result_wire_producer_blocker_source_shape_member_vertex_identity_not_ready_count
result_wire_producer_blocker_current_member_child_wire_identity_not_ready_count
result_wire_producer_blocker_current_member_missing_sidecar_evidence_count
result_wire_producer_blocker_current_member_root_open_producer_not_ready_count
result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count
```

C++ 删除点：

- `cad-core/include/cad_core/geometry/wire_joiner.h`
  - 删除 `WireJoinerLedgerSummary` 中所有 `resultWireProducerBlocker*Count`。
  - 删除 `resultWireProducerUnknownInvariantCount`。
- `cad-core/src/geometry/wire_joiner.cpp`
  - 删除 `ledgerSummary()` 里的 `countProducerBlocker` lambda。
  - 删除 edgeInfo producer blocker count 累加。
  - 删除 childWire blocker 不一致时的 count-only 分支。
- `cad-core/src/features/sketch_object.cpp`
  - 删除对应 JSON 输出键。

测试替代：

- 在 `assert_result_wire_producer_ledger()` 中，不再读这些 count。
- 保留并强化下面的逐条检查：

```python
for producer_entry, history_entry in zip(producer_entries, helper_entries):
    self.assertEqual(producer_entry["kind"], history_entry["result_wire_producer_kind"])
    self.assertEqual(producer_entry["state"], history_entry["result_wire_producer_state"])
    self.assertEqual(producer_entry["blocker"], history_entry["result_wire_producer_blocker"])
    self.assertEqual(producer_entry["state"], "ExportedWithoutHelper")
    self.assertEqual(producer_entry["blocker"], "None")
```

- 把这个 entry/history 对齐检查提升到所有 P5 fixture，而不是只依赖重点 fixture。
- 在 `test_p5_all_fixtures_omit_generated_and_disable_legacy_helper_output()` 中，为本批全部字段追加 `assertNotIn`。

文档同步：

- `docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/04-验收矩阵与回归命令.md`
  - 删除 `result_wire_producer_unknown_invariant_count == 0` 作为输出字段验收。
  - 改成“所有 producer entry/history 的 `blocker == None`”。
- `05-旧计数器映射表.md`
  - 把 `result_wire_producer_blocker_*_count` 标为已删除，替代为 entry/history blocker。

停止条件：

- 如果删除 count 后发现某个 blocker 只存在 childWire 侧、没有 entry/history 可见证据，则不要恢复 count；应把该 childWire producer identity 补进 `result_wire_producer_ledger_entries` 或 `open_export_history_entries`。

## 5. 第二批：删除 P3/P4 current-member 输出切换 summary

这批字段现在仍参与测试，但主语义已经由 producer entry 的 `kind/state/blocker/index` 表达。它们属于阶段性迁移哨兵。

删除字段：

```text
unowned_removal_ready_slot_count
unowned_removal_current_member_producer_output_count
multi_member_root_direct_output_count
```

C++ 删除点：

- `WireJoinerLedgerSummary`
  - 删除 `unownedRemovalReadySlotCount`
  - 删除 `unownedRemovalCurrentMemberProducerOutputCount`
  - 删除 `multiMemberRootDirectOutputCount`
- `ledgerSummary()`
  - 删除 `childWire.helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberChildWireProducerReady && sourceShapeReady` 的累加。
  - 删除 `helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutput` 的累加。
  - 删除 multi-member root direct output 的累加。
- `sketch_object.cpp`
  - 删除 JSON 输出。

测试替代：

- 删除 `unowned_removal_current_member_producer_output_count == unowned_removal_ready_slot_count`。
- 删除 `multi_member_root_direct_output_count == 0`。
- 保留下面的主 gate：

```python
self.assertEqual(producer_entry["state"], "ExportedWithoutHelper")
self.assertEqual(producer_entry["blocker"], "None")
self.assertEqual(producer_entry["child_wire_info_index"], history_entry["result_wire_producer_child_wire_info_index"])
self.assertEqual(producer_entry["root_edge_info_index"], history_entry["result_wire_producer_root_edge_info_index"])
self.assertEqual(producer_entry["current_member_edge_info_index"], history_entry["result_wire_producer_current_member_edge_info_index"])
```

如果仍需要约束“multi-member root 不能直接输出”，不要恢复 summary。改成在 `ResultWireProducerLedgerEntry` 增加 per-entry 证据，例如：

```text
covered_member_edge_info_indices
non_current_member_edge_info_indices
```

然后测试逐条 entry，而不是输出总数。

## 6. 第三批：压缩 helperOpenExportOverride 内部派生字段

这批只清 C++ 内部字段，不改变 JSON contract。目标是把 `helperOpenExportOverride*` 从“大量临时派生字段”压缩成三类：

- input/source lineage evidence
- final `ResultWireProducerIdentity`
- actual OCCT output/evidence shape

优先删除候选：

```text
helperOpenExportOverrideExportBlockedByIteration
helperOpenExportOverrideExportBlockedByWireInfo
helperOpenExportOverrideForcedOpenWireCompoundEdgeInfo
helperOpenExportOverrideRemovedSourceEdgeInfo
helperOpenExportOverrideRemovedTargetEdgeInfo
helperOpenExportOverrideSafeAHistoryProducerEvidence
helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputCandidate
helperOpenExportOverrideSuperEdgeRootResultWireProducerOutput
helperOpenExportOverrideSuperEdgeRootResultWireProducerOutputBlockedByMultiMemberSuperEdge
helperOpenExportOverrideSuperEdgeRootResultWireProducerUnownedRemovalChildWireProducerReadyOutput
helperOpenExportOverrideSuperEdgeRootResultWireProducerWireBuilt
```

这些字段当前属于 write-only、局部可重算、或只服务旧 summary 的高疑似删除对象。删除时按下面顺序处理：

1. 对每个字段执行：

```bash
rg -n "\bFIELD_NAME\b" cad-core/include/cad_core/geometry/wire_joiner.h cad-core/src/geometry/wire_joiner.cpp
```

2. 如果只有声明、赋值、或赋值后只参与已删除 summary，直接删。
3. 如果参与 `classifyResultWireProducerSlot()` 或 output 构造，先改成局部 `const bool`，不要保留在 `EdgeInfo` / `OpenWireCompoundWireInfo` 结构里。
4. 每删完一组运行 `cmake --build build`，避免大批内部字段造成编译定位困难。

暂时保留：

```text
helperOpenExportOverride
helperOpenExportOverrideReason
helperOpenExportOverrideSourceEdgeInfo
helperOpenExportOverrideSourceEdgeInfoIndex
helperOpenExportOverrideAHistoryRemoveSourceEdgeInfoIndices
helperOpenExportOverrideAHistoryRemoveSourceEdgeIndices
helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfoIndices
helperOpenExportOverrideSourceEdgeExportShape
helperOpenExportOverrideSuperEdgeRootEdgeInfoIndex
helperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoIndices
helperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoIndices
```

这些字段仍承接 source lineage、root/current-member 对齐、或后续 entry 级证据，不能和临时 summary 一起删。

## 7. 第四批：收缩 owner/search/exhaust 调试 count 输出

下面这些字段在当前全量 P5 fixture 中恒为 0，但它们属于 WireJoiner ownership/search/exhaust 状态机，不是 result-wire producer 临时 summary：

```text
branch_search_outside_candidate_count
owner_propagation_unassigned_candidate_count
repeated_split_exhaust_rerun_branch_search_outside_candidate_count
repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count
repeated_split_exhaust_rerun_live_transfer_wire_info_count
repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count
repeated_split_exhaust_rerun_removal_edge_info_count
repeated_split_exhaust_rerun_removal_primary_edge_info_count
repeated_split_exhaust_rerun_removal_secondary_edge_info_count
repeated_split_exhaust_rerun_removal_unowned_edge_info_count
tight_bound_exhaust_primary_reset_blocked_edge_info_count
tight_bound_existing_wire_search_intersect_skip_count
tight_bound_existing_wire_search_only_owner_vertex_blocked_count
tight_bound_existing_wire_search_only_wire_build_blocked_count
tight_bound_live_split_wire_edge_info_count
tight_bound_live_split_wire_info_count
```

这些不建议和前面三批混删。处理方式：

- 如果测试只是断言恒 0，且该字段不在方案文档中作为剩余风险，则可以按一批删除。
- 如果字段表达 FreeCAD `WireJoinerP` 搜索栈、wireSet、iteration 或 owner propagation 的状态机，应先把测试改成几何/历史结果断言，再删 JSON 输出。
- 不要因为当前 fixture 恒 0 就删除内部状态机本身；只删除外部 JSON summary。

## 8. 最终保留的最小输出面

完成前四批后，`wire_joiner_ledger` 应只保留：

- `result_wire_producer_ledger_entries`
- source lineage / open-export history 对齐仍需要的少量 count
- WireJoiner ownership/search/exhaust 仍被 fixture 覆盖、且没有更好结构化 entry 的 count

`wire_joiner_history_detail` 应保留：

- `open_export_history_entries`
- `open_export_*_count`
- modified/generated/deleted history count
- splitter/final export history 状态

不再保留：

- generated/helper output count
- legacy helper slot count
- producer kind/state/blocker summary count
- 阶段性 ready/not-ready count
- 恒 0 graph fallback count
- 只为证明“旧路径没走”的 summary count

## 9. 执行命令

每批执行前先看边界：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git -c core.quotepath=false status --short -uall
```

字段残留扫描：

```bash
rg -n "FIELD_NAME|json_field_name" \
  cad-core/include/cad_core/geometry/wire_joiner.h \
  cad-core/src/geometry/wire_joiner.cpp \
  cad-core/src/features/sketch_object.cpp \
  cad-core/tests \
  docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分
```

每批验收：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check

cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests/test_p5_sketch.py
python3 -m unittest tests/test_p6_topology.py
```

## 10. 通过标准

- 旧字段只允许出现在 `assertNotIn` 防回流列表或旧计数器映射表的“已删除”说明中。
- `cad-core/src/features/sketch_object.cpp` 不再输出已删除字段。
- `WireJoinerLedgerSummary` 不再保存已删除字段。
- producer identity 仍逐条满足 `state == ExportedWithoutHelper`、`blocker == None`。
- runtime open-export history、producer ledger、NamedShape internal history 三者 identity 对齐。
- 不新增新的 `*_count` 来替代旧 `*_count`；需要保留语义时，优先补 per-entry 字段。

## 11. 禁止事项

- 禁止用 fixture 名称、几何类型或输出数量倒推是否删除字段。
- 禁止删除 `ResultWireBlocker` enum 后再用字符串或 bool 重新表达 blocker。
- 禁止删掉 output summary 后恢复 helper/generated 输出路径。
- 禁止把旧 summary 换成另一个 summary；需要约束时改成 entry/history 级断言。
- 禁止在 `sketch_object.cpp`、adapter 或测试 fixture 层补业务推断。
