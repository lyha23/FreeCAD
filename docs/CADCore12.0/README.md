# CADCore12.0

CADCore12.0 承接 CADCore11.0 队列关闭后的下一轮 capability-first 规划。当前不直接重开 C11-M1 Sweep Location overload 或 C11-M2 Filling native helper：这两条线都已关闭为 no-code retained non-parity gate，且 live capability 中 `part_workbench.sweep.remaining_gaps=[]`、`part_workbench.filling.remaining_gaps=[]`。

当前 live capability 唯一非空 `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。C9-M5 与 C10-M4 已多轮复审并保留为 `known_gap_diagnostic` / `oracle_blocked`，不是默认 C++ 实现入口。CADCore12.0 的第一包因此先做全局候选盘点：从 live capability、CADCore9/10/11 的 release gate 和 current tests 中筛出下一批真正可实现的 backend gap。C12-M1 S6 已完成该发布闸门并选择 `no_code_backlog_gate`：本轮无代码落点，不授权 C++、fixtures、expected、oracle 采集、capability wording 或 adapter/test 改动。

用户已在 C12-M1 之后单独批准打开 C12-M2 oracle collection / native probe 包。C12-M2 不推翻 C12-M1 的 no-code 结论，也不直接打开 C++ gate；它只针对 Part Workbench historical rows 采集或阻断 stable native expected，作为后续是否能另开 implementation 包的前置证据。

C12-M2 S6 已完成并发布 `no_code_oracle_blocked_gate`：ProjectOnSurface 几何可 build，但 mapper/provenance history 仍 `native_hidden`。C12-M3 因此单独打开 ProjectOnSurface mapper / provenance native observability 包，只追问原生 API 是否能给出 source-backed、request-local provenance；它仍不是 C++ implementation 包。

## 入口

- C12-M1 总入口：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次总入口.md`
- C12-M1 方案：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次方案.md`
- C12-M1 工作步骤：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分/`
- C12-M1 矩阵：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/`
- C12-M2 总入口：`C12-M2-PartWorkbenchNativeOracleProbe批次/6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次总入口.md`
- C12-M2 方案：`C12-M2-PartWorkbenchNativeOracleProbe批次/6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次方案.md`
- C12-M2 工作步骤：`C12-M2-PartWorkbenchNativeOracleProbe批次/工作步骤细分/`
- C12-M2 矩阵：`C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/`
- C12-M3 总入口：`C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/6-29-21-29-C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次总入口.md`
- C12-M3 方案：`C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/6-29-21-29-C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次方案.md`
- C12-M3 工作步骤：`C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/工作步骤细分/`
- C12-M3 矩阵：`C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/`

## 当前基线

- S0 live 冻结 `HEAD=4446df0c87`（`4446df0c87 docs: 关闭 C11-M2 S6 发布闸门`），C11-M1 / C11-M2 队列检查均只输出表头。
- S0 起点 dirty boundary 只包含 C12-M1 docs 和 `docs/CADCore12.0/README.md` 未跟踪文件；未发现 `cad-core/src`、tests、fixtures、expected 或 collector 改动。
- capability 复核命令使用 `cad-core/cad-core capabilities`，冻结输出保存到 `/tmp/c12-capabilities.json`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，known gap 状态为 `known_gap_diagnostic`，route 为 `oracle_blocked`。
- S1 source authority 已复核：capability/test/FreeCAD source/current landing 均可定位，`C12M1-BLOCKER-101` 已关闭；未采 oracle、未改 C++、未新增 fixture、未创建 implementation row。
- S2 scope admission 已完成：`C12M1-SCOPE-001..401` 已补 owner step、current status、next step 和 close condition，`C12M1-BLOCKER-201` 已关闭，`implementation_candidate` 仅保留为 S6-only placeholder。
- S3 CopyOnChange 剩余 gap 复审已完成：C9-M5 / C10-M4 仍只提供 property/session evidence 和 retained diagnostic 裁决，App::Link `documentObjectUpdates` 是 reference-only DTO 通道，不等同 SubShapeBinder `_tmp_binder` / `_CopiedObjs` / `copyObject` lifecycle；`C12M1-SCOPE-101`、`C12M1-BLOCKER-301`、`C12M1-CAT-001` 均关闭为 retained known gap / oracle blocked，无 C++ implementation candidate。
- S4 Assembly representative / marker / writeback 复审已完成：representative_solver_adapter 仍是 `available=false` fallback metadata；subshape marker placement 与 placement writeback 已是 expected-backed current-covered request-local subset；full solver、persistent solver state 和 cross-request assembly session 保持 non-goal，无 C12-M2 implementation candidate。
- S5 Part Workbench historical narrowed 复审已完成：Sweep / Filling 继续 no-code retained non-parity，GeomPlate / ProjectOnSurface 继续 probe-only retained evidence，Loft 继续 native-hidden retained evidence；没有 stable expected/current mismatch，无 C++ implementation candidate。
- S6 NextBatch 发布闸门已完成：`C12M1-SCOPE-401`、`C12M1-BLOCKER-601`、`C12M1-CAT-004`、`C12M1-CAT-005` 与 `C12M1-VAL-601..606` 已回写；最终 action 是 `no_code_backlog_gate`，`C12M1-CAT-005` 仍为 `none_s2_placeholder`，无 implementation candidate。
- C12-M2 已创建为独立 oracle/native probe 队列。S0 live 冻结 `HEAD=4d245a9c11`（`4d245a9c11 docs: 新增 C12-M2 native oracle probe 开包`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 dirty boundary 为 `<clean>`；C12-M1 队列为空，C12-M2 队列从 S0 开始。`freecadcmd` 可发现于 `/Users/li/.cargo/bin/freecadcmd`，S0 未启动 FreeCAD，版本 / OCCT / LibPack runtime 基线留给 S3。
- C12-M2 继承 C12-M1 S6 `no_code_backlog_gate`：代码 gate 仍关闭，不授权 `cad-core/src`、`cad-core/include`、fixtures、expected、tests、adapters、capability wording 或 full build 改动；本包只为 Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 采集或阻断 stable native/request-local expected。
- C12-M2 S1 source 基线已完成：Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 均有 exact FreeCAD source authority、关键函数/短句和 existing expected / historical probe output / no-code retained 分类；`C12M2-BLOCKER-003` 关闭为无缺 source-authority 行。本步未运行 FreeCADCmd，未改代码、fixtures、expected、tests、adapters 或 capability wording。
- C12-M2 S2 范围准入已完成：Sweep 为 `probe_admitted`；Filling 为 `helper_blocked`；GeomPlate / ProjectOnSurface 为 `needs_probe_design`；Loft 为 `native_hidden_blocked`。FreeCADCmd baseline、artifact schema、current comparison、helper lifecycle、native-hidden 和 mapper/provenance blocker 已入队；backend classification 仍为 `oracle_probe_candidate` / `retained_no_expected`，不授权代码实现包。
- C12-M2 S3 通用 native probe harness 已完成：schema 固定为 `c12m2.native-probe-artifact.v1`，baseline artifact 为 `docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json`；FreeCADCmd `/Users/li/.cargo/bin/freecadcmd` 可启动并读取 FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`，LibPack / LibPackVersion 为空。S3 的 `expected_ready` 仅表示 runtime metadata ready，不发布 family geometry expected。
- C12-M2 S4 Sweep / Loft probe 已完成：Sweep Location overload 关闭为 `native_probe_blocked`，fresh artifact `docs/temp/6-29-21-55-c12m2-s4-sweep-native-probe-output.json` 仍显示 located representatives 在 build 阶段返回 `OCCError: NCollection_Array1::Value`；Sweep auxiliary / binormal / tolerance no-location controls 为 current-covered context，artifact 为 `docs/temp/6-29-21-55-c12m2-s4-sweep-options-native-probe-output.json`。Loft selected subelement 关闭为 `native_hidden`，artifact `docs/temp/6-29-21-55-c12m2-s4-loft-subelement-native-probe-output.json` 显示 tuple subelement assignment 被 `App::PropertyLinkList` 拒绝。`C12M2-BLOCKER-101` 与 `C12M2-BLOCKER-401` 已关闭，无 C++ implementation candidate。
- C12-M2 S5 Filling / GeomPlate / ProjectOnSurface probe 已完成：Filling 关闭为 `helper_blocked`，artifact `docs/temp/6-29-20-40-c12m2-s5-filling-helper-native-probe-output.json` 显示 wrapper/simple boundary controls 稳定但 helper initial-surface / support-order / explicit params 仍 crash 或 timeout；GeomPlate 只有 projected curve2d + initial surface 进入 S6 comparison candidate，artifact `docs/temp/6-29-20-40-c12m2-s5-geomplate-native-probe-output.json` 同时保留 G1 curve-on-surface `native_hidden`；ProjectOnSurface provenance 关闭为 `native_hidden`，artifact `docs/temp/6-29-20-40-c12m2-s5-project-on-surface-native-probe-output.json` 显示 geometry 可 build 但 source-backed mapper/history 全部隐藏。
- C12-M2 S6 Oracle 发布闸门已完成：唯一 S6 comparison candidate（GeomPlate projected curve2d + initial surface）通过 `docs/temp/6-29-20-58-c12m2-s6-geomplate-current-comparison.json` 和 focused unittest 证明 current cad-core 已覆盖 `cad-core/fixtures/c5m13/expected/part-geomplate-projected-curve2d-initial-surface.freecad.json`，最终分类为 `current_covered`。Sweep Location 保持 `native_probe_blocked`，Filling 保持 `helper_blocked`，Loft / ProjectOnSurface / GeomPlate G1 或 no-initial-surface native rows 保持 `native_hidden` / no-code context。C12-M2 最终发布 `no_code_oracle_blocked_gate`，不授权 C++、fixtures expected、tests、adapters 或 capability wording 改动。
- C12-M3 S0 live 冻结已完成：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7c14aa6f7a`（`7c14aa6f7a docs: 完成 C12-M2 S6 oracle 发布闸门`）。起点 dirty boundary 只包含 `docs/CADCore12.0/README.md` 修改和未跟踪的 `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/` 包；未发现 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters 或 capability wording 改动。C12-M1 / C12-M2 队列均为空；C12-M3 S0 前队列从 S0-S6 开始，S0 完成后下一步为 S1。本包继承 C12-M2 S6 `no_code_oracle_blocked_gate`，只允许复核 FreeCAD 原生 ProjectOnSurface / TopoShape history API 是否能导出 source-backed request-local provenance；不授权 C++、fixtures expected、tests、adapters、capability wording 或 full build 改动。

## 重开条件

| family | reopen condition |
| --- | --- |
| CopyOnChange | stable native copied-object expected + 产品批准 request-local DTO + current cad-core mismatch。 |
| Assembly representative / marker / writeback | 产品批准 request-local subset + expected/current mismatch。 |
| Part Workbench historical rows | stable native/request-local expected + current mismatch。 |

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0
git diff --check
```
