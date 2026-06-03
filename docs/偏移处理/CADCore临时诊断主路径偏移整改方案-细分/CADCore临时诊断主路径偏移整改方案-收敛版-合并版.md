# CADCore 临时诊断主路径偏移整改方案（收敛版）

生成日期：2026-06-02

本包是一整套新的可收敛方案，目标是替换原目录 `docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/` 中围绕 M3 不断扩散的写法。它不修改 GitHub 仓库，只提供可下载的替代方案文档。

## 文件说明

- `00-总览-收敛原则与重排.md`：说明为什么原方案不收敛，以及新的总体架构和里程碑顺序。
- `01-M3-generated-result-wire-identity-替换方案.md`：可直接替换原 `M3-generated-result-wire-identity.md` 的新版内容。
- `02-有限状态机与blocker枚举.md`：核心数据结构、状态机和 blocker enum 设计。
- `03-实施切片与文件落点.md`：按 `wire_joiner.h` / `wire_joiner.cpp` / `test_p5_sketch.py` 拆分可执行任务。
- `04-验收矩阵与回归命令.md`：阶段验收、最终验收、重点 fixture 和测试命令。
- `05-旧计数器映射表.md`：把旧 helper/generated 诊断字段映射到有限状态和 blocker，避免继续新增无限细分字段。
- `CADCore临时诊断主路径偏移整改方案-收敛版-合并版.md`：以上内容合并后的单文件版本。

## 使用方式

1. 先阅读 `00-总览-收敛原则与重排.md`，确认里程碑重排。
2. 用 `01-M3-generated-result-wire-identity-替换方案.md` 替代原 M3 文档内容。
3. 执行 `03-实施切片与文件落点.md` 中的 P0 到 P7；不得跳过 M2S source-shape identity gate。
4. 用 `04-验收矩阵与回归命令.md` 作为唯一收敛判据。

## 新方案核心结论

原 M3 的不收敛根因不是某个计数器没有继续细分，而是“诊断字段清零”“M2 source-shape identity”“M3 result-wire producer”“M4 history 消费”被混成了同一个完成条件。新方案将输出来源改为 `ResultWireProducerIdentity`，把所有旧 helper/generated 余额压缩到有限状态机和单一 `ResultWireBlocker` 枚举中；新增诊断字段不再是推进方式，未知情况必须作为 `UnknownInvariant` 失败，而不是继续发明新字段。


---

# 00 总览：收敛原则与里程碑重排

## 1. 当前方案为什么无法结束

原 M3 的文本已经把问题定位到了正确方向：需要删除 `generatedOpenExportShapeForSketchInternals()` 作为 generated result-wire 的过渡来源，并改由 FreeCAD WireJoiner 的 `aHistory`、`EdgeInfo` / `WireInfo` 生命周期和最终 `openWireCompound` 导出条件产出 result-wire identity。

但是原 M3 后续把三类问题混成了一个“所有 helper 计数器归零”的完成条件：

1. producer evidence：当前输出是否能追到同源 `aHistory->Remove(info.edge)`、removed target、source lineage。
2. final export gate：当前 `EdgeInfo` 是否满足 `iteration == -3 || (!wireInfo && iteration >= 0)`，或者是否仍靠 `hasOpenExportOverride()` 强制导出。
3. source-shape / child-wire identity：输出 shape 是否能从 helper edge 切到 selected `EdgeInfo::edge` 或 current-member child-wire。

其中第 3 类被原文明确归到 M2 边界，但原 M3 验收又要求 `helper_open_export_override_source_edge_export_shape_edge_info_count == helper_open_export_override_edge_info_count`、`open_wire_compound_helper_open_export_override_wire_info_count == 0`、`helper_open_export_override_edge_info_count == 0`。这导致 M3 一边说“不负责 M2 source shared-vertex purge / child-wire merge”，一边又以 M2 完成作为 M3 完成条件，所以执行过程会不断新增诊断字段，却无法形成终止判据。

## 2. 新方案的总目标

最终目标不变：删除 generated/helper result-wire 输出路径，让 `openWireCompound`、`NamedShape.history` 和 `ElementMap` 消费同一套 source `EdgeInfo` identity。

但完成方式改为：

- 不再把 `helper_open_export_override_*` 的无限细分字段作为推进单位。
- 不再把“helper 诊断 binding 存在”直接等同于“helper 输出仍存在”。
- 用一个有限状态机描述每个旧 helper binding 的迁移进度。
- 用一个有限 blocker enum 描述无法前进的原因。
- 只有 `UnknownInvariant` 可以触发新增诊断；其它 blocker 必须回到既定 owner 处理。
- M2S source-shape / child-wire identity gate 必须前置到最终 M3 输出切换之前。

## 3. 里程碑重排

### P0：冻结诊断扩散

目标：停止新增 `helper_open_export_override_*_xxx` 细分字段。

产物：

- `ResultWireProducerState`
- `ResultWireBlocker`
- `ResultWireProducerIdentity`
- `ResultWireProducerLedgerEntry`

验收：每个 helper binding 必须能映射到一个有限 state 和一个有限 blocker；不能映射的进入 `UnknownInvariant`，直接失败。

### P1：建立 result-wire producer contract

目标：把旧 helper binding 只作为“需要迁移的 child-wire slot”，真实输出来源改成 `ResultWireProducerIdentity`。

产物：

- source edge producer
- partial shared closed-wire producer
- superEdge root producer
- current-member child-wire producer
- live-reset open edge producer

验收：所有旧 helper entry 都有 producer identity 或 typed blocker；不再用 helper candidate 数量作为 M3 是否完成的主判断。

### M2S：source-shape / child-wire identity gate 前置

目标：解决原方案中“producer evidence 已 ready，但 selected `EdgeInfo::edge` / M2 child-wire identity 仍不能替换 helper shape”的阻挡。

产物：

- source-shape readiness 判定
- child-wire member identity 判定
- shared-vertex purge bridge 判定
- current-member child owner 判定

验收：`SourceShapeIdentityNotReady` 不再出现在 unowned-removal ready 子集里。

### P3：迁移 unowned-removal + member-suppressed current-member 输出

目标：优先处理当前已经证明 root pending 为 0、full `aHistory` evidence ready、current-member child-wire producer ready 的子集。

不得做：

- 不直接输出 multi-member root `superEdge`。
- 不在输出端按数量裁剪 sibling member。
- 不绕过 child-wire ledger。

验收：该子集的 `LegacyHelperShapeOutput` 归零，输出数量与 oracle 一致。

### P4：迁移 primary / secondary removal root candidate

目标：在 unowned-removal 输出闭环后，再逐分支处理 primary / secondary removal。

原则：一个 removal branch 一个切片，不允许混合上线。

### P5：补齐按 producer reason 遗留的 strict Remove-source evidence

目标：对 `consumed_open_cutter_graph`、`partial_junction_open_cutter`、`closed_wire_cycle`、`partial_shared_closed_wire` 中仍缺同源 strict `aHistory` producer 的条目，回到 `buildClosedWire()` / `findTightBound()` / `exhaustTightBound()` 生命周期补证据。

不得做：

- 不把普通 `iteration=-1` target 冒充成 `aHistory->Remove(info.edge)` source。
- 不给 helper copy edge 人工补 source lineage。

### P6：关闭 legacy helper output 并删除 generated helper shape

目标：`openWireCompound` 不再消费 helper shape；`generatedOpenExportShapeForSketchInternals()` 变成不可达，再删除。

验收：重点 fixture 全绿，helper/generated output 计数归零。

### P7：M4 / ElementMap 消费同一 identity

目标：`NamedShape.history`、`SketchInternalHistoryContext`、`ElementMap` 消费 P1-P6 生成的同一 `ResultWireProducerIdentity`，不再从 raw/internal geometry 反推。

## 4. 收敛性保证

本方案的收敛点来自三个硬约束：

1. 状态有限：每个条目只能处于 6 个状态之一，状态单调前进。
2. blocker 有限：所有已知问题必须落到 enum，不能继续新增计数字段。
3. owner 明确：每个 blocker 都有唯一 owner，不能在 M3 中把 M2/M4 问题硬清零。

如果出现新现象，不新增 `helper_open_export_override_*` 字段，而是先归入 `UnknownInvariant` 并失败；只有确认它代表新的 FreeCAD 生命周期分支时，才允许扩展 enum，并同步更新映射表和验收矩阵。


---

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


---

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


---

# 03 实施切片与文件落点

## 1. 代码落点

### `cad-core/include/cad_core/geometry/wire_joiner.h`

新增：

- `ResultWireProducerKind`
- `ResultWireProducerState`
- `ResultWireBlocker`
- `ResultWireProducerIdentity`
- `ResultWireProducerLedgerEntry`

修改：

- `EdgeInfo` 增加 `resultWireProducer`。
- `WireJoinerOpenExportHistoryEntry` 增加 producer kind/state/blocker。
- `OpenWireCompoundWireInfo` 增加 producer identity。
- `WireJoinerLedgerSummary` 可以暂时保留旧字段，但新增字段必须从 producer ledger 聚合，不再手工扩散。

### `cad-core/src/geometry/wire_joiner.cpp`

新增或重构以下函数：

```cpp
std::vector<ResultWireProducerLedgerEntry>
classifyResultWireProducerSlots(const HelperOpenExportOverridePlan& helperPlan,
                                const std::vector<EdgeInfo>& finalInfo);

void attachAHistoryProducerEvidence(std::vector<ResultWireProducerLedgerEntry>& entries,
                                    const std::vector<EdgeInfo>& finalInfo);

void attachChildWireProducerIdentity(std::vector<ResultWireProducerLedgerEntry>& entries,
                                     std::vector<OpenWireCompoundWireInfo>& childWires);

void runSourceShapeIdentityGate(std::vector<ResultWireProducerLedgerEntry>& entries,
                                const SourceShapeIdentityContext& identity);

void buildOpenWireCompoundFromProducerLedger(std::vector<OpenWireCompoundWireInfo>& childWires,
                                             const std::vector<ResultWireProducerLedgerEntry>& entries,
                                             bool allowLegacyHelperOutput);
```

重命名：

```cpp
generatedOpenExportShapeForSketchInternals()
  -> legacyHelperOpenExportShapeForDiagnosticsOnly()
```

最终删除条件：`allowLegacyHelperOutput == false` 后重点 fixture 全绿。

### `cad-core/tests/test_p5_sketch.py`

新增断言：

- `result_wire_producer_unknown_invariant_count == 0`
- 每个 `result_wire_producer_ledger_entries` 条目都是 `state == ExportedWithoutHelper` 且 `blocker == None`
- `open_wire_compound_legacy_helper_shape_wire_info_count == 0`
- 每个重点 fixture 的 blocker 分布符合阶段矩阵。

## 2. P0 切片：冻结诊断扩散

动作：

1. 增加 enum / ledger entry。
2. 保留旧 summary 字段，但从 ledger 派生。
3. 给所有 helper binding 生成 state/blocker。
4. 把未识别情况打到 `UnknownInvariant`。

验收：

```text
len(result_wire_producer_ledger_entries) == wire_joiner_history_detail.open_export_helper_override_edge_count
result_wire_producer_unknown_invariant_count == 0
```

## 3. P1 切片：producer identity contract

动作：

1. 对每个 helper binding 选择 producer kind。
2. 把 producer identity 绑定到 `EdgeInfo` 和 history entry。
3. `openWireCompound` 暂不切输出，只记录 producer identity。

验收：

```text
producer kind != None for all legacy helper slots, unless blocker != None
source_lineage_missing_open_export_edge_info_count == 0
```

## 4. M2S 切片：source-shape / child-wire identity gate

动作：

1. 给 source edge / current member child wire 增加 identity readiness 判断。
2. 判断必须同时检查：
   - same TShape，或
   - source vertex identity 完整，且
   - child ownership complete，且
   - purge bridge 不会改变输出数量。
3. 把不满足的条目标记为 `SourceShapeIdentityNotReady`。

验收：

```text
result_wire_producer_unknown_invariant_count == 0
result_wire_producer_ledger_entries[*].state/blocker 覆盖 source-shape gate
```

## 5. P3 切片：unowned-removal + member-suppressed output

动作：

1. 只选择 blocker/branch 为 unowned removal 的 ready 子集。
2. 要求 root pending member 为 0。
3. 要求 current-member child-wire full evidence ready。
4. 要求 M2S `SourceShapeReady`。
5. 输出 current-member child-wire producer，不输出 root `superEdge`。

验收：

```text
unowned_removal_ready_legacy_helper_shape_output_count == 0
unowned_removal_current_member_producer_output_count == unowned_removal_ready_slot_count
multi_member_root_direct_output_count == 0
```

## 6. P4 切片：primary / secondary removal

动作：

1. 复制 P3 的流程，但一次只放开一个 branch。
2. primary branch 全绿后才做 secondary。
3. missing branch 永远不能自动输出。

验收：

```text
primary_removal_unknown_invariant_count == 0
secondary_removal_unknown_invariant_count == 0
missing_removal_branch_output_count == 0
```

## 7. P5 切片：reason-specific strict evidence

动作：

按 reason 单独补证据：

```text
consumed_open_cutter_graph
partial_junction_open_cutter
closed_wire_cycle
partial_shared_closed_wire
```

每个 reason 的处理顺序：

1. source lineage
2. strict Remove source
3. same-source lineage
4. full evidence
5. child-wire identity
6. output switch

验收：

```text
reason_unknown_invariant_count == 0
reason_legacy_helper_shape_output_count == 0
```

## 8. P6 切片：删除 legacy helper shape

动作：

1. `allowLegacyHelperOutput` 默认改为 false。
2. 删除或禁用 `legacyHelperOpenExportShapeForDiagnosticsOnly()`。
3. 删除所有只为 helper shape 服务的输出分支。
4. 保留必要 ledger 兼容字段一轮，确认测试消费方不再依赖后再清理。

验收：

```text
open_wire_compound_legacy_helper_shape_wire_info_count == 0
open_wire_compound_helper_open_export_override_helper_shape_wire_info_count == 0
generated_open_export_edge_info_count == 0
open_wire_compound_generated_wire_info_count == 0
```

## 9. P7 切片：M4 / ElementMap 消费

动作：

1. `WireJoinerOpenExportHistoryEntry` 输出 producer identity。
2. `SketchInternalHistoryContext` 消费 producer identity。
3. `NamedShape.history` 不再从 raw geometry 反推 result-wire source。
4. `ElementMap` 用同一 identity。

验收：

```text
named_shape_history_missing_result_wire_identity_count == 0
element_map_result_wire_identity_mismatch_count == 0
```


---

# 04 验收矩阵与回归命令

## 1. 总验收指标

最终完成必须满足：

```text
generated_open_export_edge_info_count == 0
open_wire_compound_generated_wire_info_count == 0
open_wire_compound_helper_open_export_override_helper_shape_wire_info_count == 0
open_wire_compound_legacy_helper_shape_wire_info_count == 0
result_wire_producer_unknown_invariant_count == 0
source_lineage_missing_open_export_edge_info_count == 0
open_export_helper_override_missing_source_lineage_edge_count == 0
repeated_split_exhaust_generated_identity_blocked_edge_info_count == 0
```

## 2. 阶段验收矩阵

| 阶段 | 必须为 0 | 允许非 0 | 说明 |
|---|---|---|---|
| P0 | `UnknownInvariant` | `LegacyHelperShapeStillUsed` | 只冻结诊断扩散，不切输出 |
| P1 | `ProducerKind::None` with no blocker | `SourceShapeIdentityNotReady` | producer contract ready |
| M2S | `UnknownInvariant` | branch-specific blocker | source-shape gate ready |
| P3 | unowned 子集的 `LegacyHelperShapeStillUsed` | primary/secondary blocker | 只切 unowned-removal ready 子集 |
| P4 | primary/secondary 子集 unknown | 尚未处理 reason blocker | 分 branch 迁移 |
| P5 | reason-specific legacy helper output | 无 | reason 全闭环 |
| P6 | 所有 generated/helper shape output | 无 | helper shape 删除 |
| P7 | history / ElementMap identity mismatch | 无 | 下游消费同一 identity |

## 3. 重点 fixture

必须覆盖：

```text
sketch-internal-face-cross-cutters
sketch-internal-face-segmented-cross-cutter
sketch-internal-face-t-cutter
sketch-internal-face-three-overlap-circles
sketch-internal-face-arc-lens
sketch-internal-face-bullseye
```

## 4. 建议测试命令

```bash
pytest cad-core/tests/test_p5_sketch.py   -k "sketch-internal-face-cross-cutters or sketch-internal-face-segmented-cross-cutter or sketch-internal-face-t-cutter or sketch-internal-face-three-overlap-circles or sketch-internal-face-arc-lens or sketch-internal-face-bullseye"
```

若项目中 fixture 名不是 pytest `-k` 可直接匹配的测试名，则改用现有测试过滤参数，但必须保持上述六类 fixture 全覆盖。

## 5. 禁止的验收方式

- 禁止把 legacy helper summary count 作为 P0/P1 验收；slot 数以 producer ledger entries 与 open-export history 对齐为准。
- 禁止把 source-edge export shape 自然满足当成 final lifecycle 完成。
- 禁止用输出数量反推 `wireInfo` ownership。
- 禁止因为某个 fixture expected 数量变了就调整 oracle。
- 禁止在 `getOpenWires()` 输出端临时裁剪 root `superEdge` 的 sibling member。

## 6. 通过/失败判定

### 通过

满足阶段矩阵中当前阶段的“必须为 0”指标，并且所有非 0 项都属于明确允许的下一阶段 blocker。

### 失败

任一情况出现即失败：

- `UnknownInvariant > 0`
- helper shape 在 P6 后仍被输出
- generated shape 在 P6 后仍被输出
- multi-member root 直接输出
- missing branch 自动输出
- source lineage 人工补到 helper copy edge
- 普通 removed target 被当成 strict Remove source


---

# 05 旧计数器映射表

本表用于把原 M3 中不断扩展的 helper/generated 计数器压缩成有限状态和 blocker。后续不得继续新增同类 `helper_open_export_override_*` 细分字段；确需扩展时必须扩展 enum，并更新本表。

| 旧字段 / 旧概念 | 新 state | 新 blocker | 处理 owner |
|---|---|---|---|
| `generated_open_export_edge_info_count` | `LegacyHelperCandidate` | `LegacyHelperShapeStillUsed` | P6 删除 generated helper shape |
| `open_wire_compound_generated_wire_info_count` | `LegacyHelperCandidate` | `LegacyHelperShapeStillUsed` | P6 |
| `helper_open_export_override_edge_info_count` | ledger entry count | None 或具体 blocker | 已删除；直接使用 `len(result_wire_producer_ledger_entries)` |
| `helper_open_export_override_candidate_edge_count` | legacy candidate count | None | P0 后不得驱动新增切片 |
| `helper_open_export_override_unbound_edge_count` | `LegacyHelperCandidate` | `MissingSourceLineage` | M3 source lineage |
| `helper_open_export_override_duplicate_source_edge_info_count` | `LegacyHelperCandidate` | `UnknownInvariant` 或 source ambiguity | P0 失败后细化 enum |
| `helper_open_export_override_missing_removed_source_edge_info_count` | `ProducerLocated` | `MissingAHistoryRemoveSource` | M3 aHistory evidence |
| `helper_open_export_override_missing_ahistory_remove_source_edge_info_count` | `ProducerLocated` | `MissingAHistoryRemoveSource` | M3 |
| `helper_open_export_override_missing_ahistory_remove_source_lineage_edge_info_count` | `ProducerLocated` | `MissingSourceLineage` | M3 |
| `helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count` | `ProducerLocated` | `ForeignAHistorySourceLineage` | M3 |
| `helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count` | `ProducerLocated` | `MissingAHistoryRemoveSource` / `ForeignAHistorySourceLineage` / `MissingFullAHistoryProducerEvidence` | M3 |
| `helper_open_export_override_forced_open_wire_compound_edge_info_count` | `AHistoryEvidenceReady` 或 `ProducerLocated` | `FinalGateBlockedByIteration` / `FinalGateBlockedByWireInfo` | M3 lifecycle |
| `helper_open_export_override_export_blocked_by_iteration_edge_info_count` | `AHistoryEvidenceReady` | `FinalGateBlockedByIteration` | M3 removal/result-wire lifecycle |
| `helper_open_export_override_export_blocked_by_wire_info_edge_info_count` | `ProducerLocated` | `FinalGateBlockedByWireInfo` | M1/M3 owner reset |
| `helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count` | `ChildWireReady` | `SourceShapeIdentityNotReady` | M2S |
| `helper_open_export_override_source_edge_export_shape_edge_info_count` | `SourceShapeReady` | None | M2S/P3/P6 |
| `helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count` | `AHistoryEvidenceReady` | `SourceShapeIdentityNotReady` | M2S |
| `helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` | `AHistoryEvidenceReady` | `FinalGateBlockedByIteration` | M3 lifecycle |
| `helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count` | `ProducerLocated` | `MissingFullAHistoryProducerEvidence` | M3 |
| `helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count` | `AHistoryEvidenceReady` | `RootRemovedByUnownedBranch` | P3 |
| `helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count` | `AHistoryEvidenceReady` | `RootRemovedByPrimaryBranch` | P4 |
| `helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count` | `AHistoryEvidenceReady` | `RootRemovedBySecondaryBranch` | P4 |
| `helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count` | `AHistoryEvidenceReady` | `UnknownInvariant` | 失败，不自动输出 |
| `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count` | `ChildWireReady` | `MultiMemberRootPendingSuppression` | M2/M3 child ownership |
| `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_edge_info_count` | `ChildWireReady` | `MultiMemberRootPendingSuppression` | M2/M3 |
| `open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count` | `ChildWireReady` | `SourceShapeIdentityNotReady` | M2S |
| `open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` | `LegacyHelperCandidate` / `ChildWireReady` | `LegacyHelperShapeStillUsed` | P6 final switch |
| `open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count` | `ExportedWithoutHelper` | None | 已迁移输出 |
| `source_lineage_missing_open_export_edge_info_count` | any | `MissingSourceLineage` | M3 source lineage；最终必须为 0 |
| `open_export_helper_override_missing_source_lineage_edge_count` | any | `MissingSourceLineage` | M3；最终必须为 0 |
| `repeated_split_exhaust_generated_identity_blocked_edge_info_count` | causal rerun blocker | existing causal blocker | 只在实际拒绝分支统计 |

## 派生规则

```text
result_wire_producer_unknown_invariant_count
  = count(entries where blocker == UnknownInvariant)

open_wire_compound_legacy_helper_shape_wire_info_count
  = count(childWires where producer.state != ExportedWithoutHelper and legacyHelperShapeUsed)


```

## 清理顺序

1. 先新增 producer ledger。
2. 旧 counter 从 producer ledger 派生。
3. fixture 全绿后删除旧 counter 的主流程写入。
4. 最后删除 helper-generated shape 函数。
