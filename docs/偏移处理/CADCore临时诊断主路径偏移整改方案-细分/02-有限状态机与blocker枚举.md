# 02 有限状态机与 blocker 枚举

## 1. 状态机

每个旧 helper binding / open-export slot 只能处于以下状态之一：

```text
LegacyHelperCandidate
  -> ProducerLocated
  -> AHistoryEvidenceReady
  -> ChildWireReady
  -> SourceShapeReady
  -> ExportedWithoutHelper
```

状态只能单调前进，不能回退。若某一轮无法前进，必须设置一个 `ResultWireBlocker`。

## 2. 状态含义

### LegacyHelperCandidate

仅表示旧 helper plan 找到了一个需要迁移的 slot。这个状态没有 producer 语义，不能作为最终输出。

### ProducerLocated

已经定位到真实 producer：source `EdgeInfo`、partial shared closed-wire、live-reset open edge、superEdge root 或 current-member child-wire。

### AHistoryEvidenceReady

producer 已具备必要 evidence：

- source lineage
- strict `aHistory->Remove(info.edge)` source，或同源 remove source
- removed target
- full producer evidence 在需要时完整

### ChildWireReady

producer 已经能落到 `OpenWireCompoundWireInfo` child-wire ledger；`getOpenWires()` 可以从 child-wire ledger 消费 identity。

### SourceShapeReady

M2S 判定通过：producer 的 edge/wire shape 可以替换 helper shape，且不会改变 InternalEdge 数量或 shared-vertex purge 结果。

### ExportedWithoutHelper

`openWireCompound` 实际输出已经不再消费 helper shape。

## 3. blocker enum

```cpp
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
```

## 4. blocker owner

| Blocker | Owner | 处理方式 |
|---|---|---|
| `MissingSourceLineage` | M3 source lineage | 回到 splitter / sourceEdgeArray lineage，不给 helper copy edge 造 lineage |
| `MissingAHistoryRemoveSource` | M3 aHistory evidence | 回到 `buildClosedWire()` removal source，不把 target 冒充 source |
| `ForeignAHistorySourceLineage` | M3 aHistory evidence | 只能作为外源依赖，不能算当前 entry producer |
| `MissingFullAHistoryProducerEvidence` | M3 producer evidence | 补齐 Remove-source / removed-target / source-lineage 三件套 |
| `FinalGateBlockedByIteration` | M3 result-wire lifecycle | 处理被 removal 消费后的真实 result-wire producer |
| `FinalGateBlockedByWireInfo` | M1/M3 owner reset | 回到 repeated `findClosedWires(true)` / live reset |
| `RootRemovedByUnownedBranch` | M3 unowned branch | current-member child-wire producer 路径优先处理 |
| `RootRemovedByPrimaryBranch` | M3 primary branch | unowned 完成后单独处理 |
| `RootRemovedBySecondaryBranch` | M3 secondary branch | primary 完成后单独处理 |
| `MultiMemberRootPendingSuppression` | M2/M3 child ownership | 不输出 root `superEdge`，先完成 member suppression |
| `SourceShapeIdentityNotReady` | M2S | 先完成 source-shape / child-wire identity gate |
| `LegacyHelperShapeStillUsed` | M3 final switch | 只能存在于过渡阶段，最终必须为 0 |
| `UnknownInvariant` | 失败 | 不允许继续新增普通计数字段；必须补 enum 或修复 bug |

## 5. 计数生成方式

以后 summary counter 应从 ledger entries 派生，而不是在主流程中继续新增字段：

```cpp
struct ResultWireProducerLedgerEntry {
    std::size_t openExportIndex = 0;
    std::size_t sourceEdgeInfoIndex = npos;
    std::size_t rootEdgeInfoIndex = npos;
    std::size_t currentMemberEdgeInfoIndex = npos;
    std::size_t childWireInfoIndex = npos;
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::LegacyHelperCandidate;
    ResultWireBlocker blocker = ResultWireBlocker::None;
};
```

测试里如果还需要旧字段，可以从 `ResultWireProducerLedgerEntry` 聚合得到。例如：

```text
open_wire_compound_helper_open_export_override_helper_shape_wire_info_count
  = count(entries where blocker == LegacyHelperShapeStillUsed)

helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count
  = count(entries where blocker in [MissingAHistoryRemoveSource, ForeignAHistorySourceLineage, MissingFullAHistoryProducerEvidence])
```

这能阻止“发现一个余额就新增一个字段”的循环。

## 6. UnknownInvariant 规则

出现未覆盖情况时：

1. 不新增 `helper_open_export_override_*` 字段。
2. 不调整 fixture expected 数量。
3. 不放宽 source-shape gate。
4. 先输出一条 `UnknownInvariant` ledger entry。
5. 只有确认它是 FreeCAD 真实生命周期分支，才扩展 enum 和本映射表。
