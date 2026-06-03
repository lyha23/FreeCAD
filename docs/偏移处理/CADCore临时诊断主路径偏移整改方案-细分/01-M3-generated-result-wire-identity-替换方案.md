# 01 M3 generated result-wire identity（收敛替换版）

## 目标

删除 `generatedOpenExportShapeForSketchInternals()` 作为最终 result-wire 输出来源。M3 不再以不断细分 helper/generated 计数器为推进方式，而是把每个旧 helper open-export binding 迁移为一个显式的 `ResultWireProducerIdentity`。

M3 的完成定义改为：

1. 所有旧 helper output slot 都有真实 producer identity，或有已知 blocker。
2. `openWireCompound` 不再消费 helper-generated shape。
3. `generatedOpenExportShapeForSketchInternals()` 不再参与 shape 输出，最终可删除。
4. `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 只保留 causal blocker 语义，不能再从 generated/helper 输出数量后验反推。

## FreeCAD 依据

仍以 WireJoiner 主生命周期为依据：

```text
splitEdges()
  -> findClosedWires()
  -> findTightBound()
  -> exhaustTightBound()
  -> buildClosedWire()
  -> aHistory->Remove(info.edge)
  -> openWireCompound final gate
  -> getOpenWires()
```

关键字段仍是：

- `aHistory`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::iteration`
- `EdgeInfo::superEdge`
- `openWireCompound`

## M3 新边界

M3 负责：

- 识别 result-wire producer。
- 把 producer evidence 绑定到 `EdgeInfo` / `WireInfo` / child-wire ledger。
- 让 `openWireCompound` 可消费 producer identity。
- 删除 helper-generated shape 输出。

M3 不单独负责：

- M2 的 source shared-vertex purge 最终切换。
- M2 的 child-wire member identity 判定。
- M4 的 `NamedShape.history` / `ElementMap` 持久消费。

但 M3 最终输出切换依赖 M2S source-shape gate。也就是说：M3 不再“假装不负责 M2 却要求 M2 结果清零”，而是把 M2S 作为前置里程碑写入总计划。

## 核心结构

```cpp
enum class ResultWireProducerKind {
    None,
    ExistingSourceEdge,
    PartialSharedClosedWire,
    LiveResetOpenEdge,
    SuperEdgeRoot,
    CurrentMemberChildWire,
};

enum class ResultWireProducerState {
    LegacyHelperCandidate,
    ProducerLocated,
    AHistoryEvidenceReady,
    ChildWireReady,
    SourceShapeReady,
    ExportedWithoutHelper,
};

enum class ResultWireBlocker {
    None,
    MissingSourceLineage,
    MissingAHistoryRemoveSource,
    ForeignAHistorySourceLineage,
    MissingFullAHistoryProducerEvidence,
    FinalGateBlockedByIteration,
    FinalGateBlockedByWireInfo,
    RootRemovedByUnownedBranch,
    RootRemovedByPrimaryBranch,
    RootRemovedBySecondaryBranch,
    MultiMemberRootPendingSuppression,
    SourceShapeIdentityNotReady,
    LegacyHelperShapeStillUsed,
    UnknownInvariant,
};

struct ResultWireProducerIdentity {
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::LegacyHelperCandidate;
    ResultWireBlocker blocker = ResultWireBlocker::None;

    std::size_t sourceEdgeInfoIndex = npos;
    std::size_t rootEdgeInfoIndex = npos;
    std::size_t currentMemberEdgeInfoIndex = npos;
    std::size_t childWireInfoIndex = npos;

    bool hasSourceLineage = false;
    bool hasStrictRemoveSource = false;
    bool hasRemovedTarget = false;
    bool hasSameSourceRemoveLineage = false;
    bool hasFullAHistoryEvidence = false;
    bool finalGateEligible = false;
    bool childWireBuilt = false;
    bool sourceShapeReady = false;
};
```

## 新主路径

```text
computeHelperOpenExportOverridePlan()
  -> classifyResultWireProducerSlots(helperPlan)
  -> attachAHistoryProducerEvidence()
  -> recordResultWireProducerLedger()
  -> runM2SSourceShapeIdentityGate()
  -> buildOpenWireCompoundFromProducerLedger()
  -> getOpenWires() consumes OpenWireCompoundWireInfo producer identity
```

过渡期可以保留 helper plan 作为 slot finder，但必须满足：

- helper plan 不再是最终输出来源。
- helper edge 不再作为 producer identity。
- helper output 如果仍被使用，必须显式标记为 `LegacyHelperShapeStillUsed`，并只能存在于 P0/P1 过渡阶段。

## M3 必收切片

### M3-0：冻结 helper/generated 诊断字段

删除继续新增 `helper_open_export_override_*` 细分字段的任务项。后续新增分类必须进入 `ResultWireBlocker` enum。

验收：

- 每个 helper binding 都能映射到 state/blocker。
- `UnknownInvariant == 0`。
- 旧 counter 只作为兼容输出，不作为新增开发方向。

### M3-1：producer identity 落到 EdgeInfo / WireInfo / child-wire ledger

在 `EdgeInfo` 和 `OpenWireCompoundWireInfo` 上携带 `ResultWireProducerIdentity`。

验收：

- `helper_open_export_override_edge_info_count` 已删除；legacy slot 数直接使用 `len(result_wire_producer_ledger_entries)` 和 open-export history 的 helper override count。
- 但 `open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` 必须被拆成 `LegacyHelperShapeStillUsed` blocker，而不是继续新增下游字段。

### M3-2：partial_shared_closed_wire 作为首个 source-edge producer 输出闭环

该类已有 source-edge export shape 的 case 应作为模板。

验收：

- `partial_shared_closed_wire` 的 legacy helper shape 输出为 0。
- 不回退到 helper edge。

### M3-3：unowned-removal root/current-member 输出切换

当前主缺口已经从 root candidate / full evidence / pending member 转移到 source-shape gate。因此必须先通过 M2S，然后只对 unowned-removal ready 子集切换输出。

验收：

- 不输出 multi-member root `superEdge`。
- 只输出 current-member child-wire producer。
- 输出数量与 oracle 一致。
- `SourceShapeIdentityNotReady` 在 unowned-removal ready 子集中为 0。

### M3-4：primary / secondary removal root candidate

unowned-removal 闭环后，再按 branch 迁移：

1. primary-removal root candidate
2. secondary-removal root candidate
3. missing branch 只能作为失败，不允许自动输出

### M3-5：剩余 reason 的 strict producer evidence

按 reason 单独处理：

- `consumed_open_cutter_graph`
- `partial_junction_open_cutter`
- `closed_wire_cycle`
- `partial_shared_closed_wire`

验收：每个 reason 下的缺口只能落到有限 blocker，不允许新增 reason-specific helper 字段。

### M3-6：删除 generated helper shape

在最终输出已经不消费 helper shape 后：

1. 将 `generatedOpenExportShapeForSketchInternals()` 改名为 `legacyHelperOpenExportShapeForDiagnosticsOnly()`，并只允许测试诊断调用。
2. 关闭 legacy helper output flag。
3. 所有重点 fixture 通过后删除该函数。

## 新完成条件

M3 最终完成必须满足：

```text
generated_open_export_edge_info_count == 0
open_wire_compound_generated_wire_info_count == 0
result_wire_producer_unknown_invariant_count == 0
open_wire_compound_legacy_helper_shape_wire_info_count == 0
open_wire_compound_helper_open_export_override_helper_shape_wire_info_count == 0
repeated_split_exhaust_generated_identity_blocked_edge_info_count == 0
source_lineage_missing_open_export_edge_info_count == 0
open_export_helper_override_missing_source_lineage_edge_count == 0
```

以下字段不再作为 M3 单独清零前置条件，而是被映射到 state/blocker：

```text
helper_open_export_override_edge_info_count
helper_open_export_override_candidate_edge_count
helper_open_export_override_forced_open_wire_compound_edge_info_count
helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count
helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count
```

它们可以在 P0/P1 过渡期存在，但不能再驱动新增诊断切片。最终删除 helper 输出时，自然归零或退化为兼容统计。

## 禁止项

- 禁止把普通 `iteration=-1` target 当成 strict `aHistory->Remove(info.edge)` source。
- 禁止给 helper copy edge 人工补 source lineage。
- 禁止通过输出数量倒推 `wireInfo` ownership。
- 禁止在 M3 内放宽 `idxVertex` / `stackPos` 顺序判断。
- 禁止直接输出 multi-member root `superEdge`。
- 禁止在输出端裁剪 sibling member。
- 禁止绕过 `OpenWireCompoundWireInfo` child-wire ledger。
