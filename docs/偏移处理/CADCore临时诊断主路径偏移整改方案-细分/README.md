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

## 使用方式

1. 先阅读 `00-总览-收敛原则与重排.md`，确认里程碑重排。
2. 用 `01-M3-generated-result-wire-identity-替换方案.md` 替代原 M3 文档内容。
3. 执行 `03-实施切片与文件落点.md` 中的 P0 到 P7；不得跳过 M2S source-shape identity gate。
4. 用 `04-验收矩阵与回归命令.md` 作为唯一收敛判据。

## 新方案核心结论

原 M3 的不收敛根因不是某个计数器没有继续细分，而是“诊断字段清零”“M2 source-shape identity”“M3 result-wire producer”“M4 history 消费”被混成了同一个完成条件。新方案将输出来源改为 `ResultWireProducerIdentity`，把所有旧 helper/generated 余额压缩到有限状态机和单一 `ResultWireBlocker` 枚举中；新增诊断字段不再是推进方式，未知情况必须作为 `UnknownInvariant` 失败，而不是继续发明新字段。

## 当前实现状态（2026-06-03）

- 已完成：P0 / P1 / M2S / P3 / P4 / P5 的 producer ledger 主路径。`EdgeInfo`、`OpenWireCompoundWireInfo`、open-export history、topo history 和测试 JSON 均携带 `ResultWireProducerIdentity`、有限 state 与有限 blocker；legacy slot 数与 producer ledger entry 数一致，`UnknownInvariant == 0`。
- 已完成：split-fragment source-shaped producer 与 source-shape gate 收敛。foreign `aHistory->Remove(info.edge)`、同源 strict sidecar、live-reset open edge、root-open current-member 均优先用 producer curve + result vertices 或 current-member child-wire ledger 输出，不再落回 helper shape。
- 已归零：当前重点 fixture 的 `ForeignAHistorySourceGeometryMismatch`、`SameSourceSidecarGeometryMismatch`、`LiveResetSourceShapeWouldPurgeOriginal`、`CurrentMemberSourceShapeWouldPurgeOriginal`、`SourceShapeIdentityNotReady`、`SourceShapeMemberVertexIdentityNotReady`、`MultiMemberRootPendingSuppression`、`CurrentMemberChildWireIdentityNotReady`、`CurrentMemberMissingSidecarEvidence`、`CurrentMemberRootOpenProducerNotReady`、`CurrentMemberSidecarGeometryMismatch`、`MissingFullAHistoryProducerEvidence`、`MissingAHistoryRemoveSource`、`MissingRemovedTargetEvidence`、`ForeignAHistorySourceLineage`、`ForeignAHistorySourceShapeIdentityNotReady`、`ForeignAHistorySourceShapeReadyLineageMismatch` 与 `SameSourceSidecarSourceShapeIdentityNotReady`。
- helper 输出基线：`cross-cutters` 为 12/12 exported，`segmented-cross-cutter` 为 10/10，`t-cutter` 为 8/8，`three-overlap-circles` 为 15/15，`arc-lens` 为 1/1；全量 `cad-core/fixtures/p5/*.json` 不再输出 generated/helper output 诊断键、result-wire blocker summary、P3/P4 current-member summary 或第四批 owner/search/exhaust 临时 JSON summary。producer ledger 逐条要求 `state == ExportedWithoutHelper` 且 `blocker == None`，并与 runtime open-export history 对齐，避免旧 finder/helper fallback 只靠汇总等式漏过输出回退。
- P6 旧 finder 已删除：`generatedOpenExportShapeForSketchInternals()` 与中间态 `legacyResultSlotLocatorEdgesForDiagnosticsOnly()` 均已从代码中移除，不再有旧 helper/generated shape 或 result-slot finder 作为 plan 数据来源。`partial_shared_closed_wire`、`consumed_open_cutter_graph`、`partial_junction_open_cutter` 与 `closed_wire_cycle` 均从 final `EdgeInfo` / source lineage / closed-wire boundary / bounded-result evidence 建 binding；`resultSlotEdge` 只保留为 root/current-member producer 的请求内顶点证据，`openExportOverride` 仍只保留选中 `EdgeInfo` 的 export gate。`open_wire_compound_helper_open_export_override_helper_shape_wire_info_count` 与 `open_wire_compound_legacy_helper_shape_wire_info_count` 已改为从 `LegacyHelperShapeStillUsed` blocker 派生；当前测试已把每个 migrated slot 锁到 `ExportedWithoutHelper / None`。
- P7 已落地 history / ElementMap identity gate：`cad-core.tests.test_p5_sketch.CadCoreP5SketchTest.test_p7_sketch_internal_history_consumes_result_wire_producer_identity` 覆盖当前重点 P5 fixture，要求 `wire_joiner_ledger`、runtime `open_export_history_entries` 与 `Sketch.InternalShape.sketch_internal_history.wire_joiner_open_export_history_entries` 的 `ResultWireProducerIdentity` 完全一致，并直接验收 `named_shape_history_missing_result_wire_identity_count == 0` 与 `element_map_result_wire_identity_mismatch_count == 0`。
- 临时诊断字段清理已落地：`result_wire_producer_blocker_*_count`、`result_wire_producer_unknown_invariant_count`、`unowned_removal_*`、`multi_member_root_direct_output_count`、第四批 owner/search/exhaust JSON summary，以及 runtime/topo open-export history 中可由 entries 直接派生的 helper/source-lineage/purge summary 已从输出删除；测试改用 producer ledger / runtime history / topo history 三方 identity 对齐、entry 计数和 `assertNotIn` 防回流。`helperOpenExportOverride*` 内部只删除 write-only 或可局部重算的派生字段，source lineage、root/current-member 对齐、actual output wire、`openExportOverride` 与 `resultSlotVertexEvidenceEdge` 保留。
- 验收要求：继续使用 `cmake --build build`、`python3 -m unittest cad-core.tests.test_p5_sketch`、`python3 -m unittest cad-core.tests.test_mvp`、`python3 -m unittest cad-core.tests.test_p6_topology` 和 `git diff --check` 作为当前回归口径。仍不得新增 `helper_open_export_override_*` 细分字段。
