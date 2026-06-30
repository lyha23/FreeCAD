# C12-M9 CAD Core implementation candidate 再盘点批次

C12-M9 是 C12-M8 关闭后的下一轮候选盘点包，不是直接 C++ implementation 包。

C12-M8 已把当前唯一 live `remaining_gaps`：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 关闭为 `no_code_retained_diagnostic` published。C12-M9 的目标不是绕过这个结论，而是重新从 live capability、C12-M1..M8 发布闸门、`narrowed_gaps` 和 current tests 中筛出是否存在新的可实现候选。

## 当前基线

- 创建基线：`HEAD=75e7a58723`（`75e7a58723 docs: 关闭 C12-M8 S6 发布闸门`）。
- 创建时 worktree clean。
- C12-M1..M8 工作步骤队列均为空。
- live capability 唯一非空 `remaining_gaps` 仍是 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- CopyOnChange 继续是 `known_gap_diagnostic` / `oracle_blocked`，diagnostic 为 `copy_on_change_full_temporary_document_cache_not_supported`。
- C12-M8 最终事实继续有效：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`。

## 入口关闭

- 入口关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=982ee025ce`（`982ee025ce fix: 修复 Body 替换 Tip 后 refined 继承`），起点 worktree clean。
- 关闭前 C12-M9 队列显示工作步骤总入口与 S0-S6 均 pending；入口关闭后队列从 S0 继续。
- C12-M9 矩阵 TSV 字段数检查通过。
- 本入口只关闭队列索引，不执行 S0-S6 实质盘点，不修改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d7602e1bd2`（`d7602e1bd2 文档：关闭 C12-M9 工作步骤总入口`），起点 `git -c core.quotepath=false status --short -uall` 无输出，即 worktree clean。
- S0 执行前 C12-M9 队列第一项为 `7-1-01-25-C12-M9-S0-live基线与继承口径冻结.md`，后续仍为 S1-S6；S0 关闭后队列应从 S1 继续。
- C12-M1..M8 `工作步骤细分` 队列均只输出 markdown 表头，继承口径可作为 closed release gate 输入。
- live capability snapshot 保存到 `/tmp/c12m9-s0-capabilities.json`；唯一非空 `remaining_gaps` 为 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- CopyOnChange known gap 继续为 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- live `narrowed_gaps` presence 已冻结为 `part_design.revolution_groove`、`part_workbench.filling`、`part_workbench.geomplate`、`part_workbench.loft`、`part_workbench.project_on_surface`、`part_workbench.sweep`。
- C12-M8 retained diagnostic 继续继承：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`；S0 不做 current mismatch 判断，不运行 FreeCADCmd，不修改 production code、fixtures、expected、tests、adapters 或 capability source。

## S1 live capability 抽取

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=a7e7eb040f`（`a7e7eb040f 文档：冻结 C12-M9 S0 live 基线`），起点 worktree clean。
- S1 执行前 C12-M9 队列第一项为 `7-1-01-26-C12-M9-S1-live-capability与remaining-gap抽取.md`，后续为 S2-S6；S1 关闭后队列应从 S2 继续。
- live capability snapshot 保存到 `/tmp/c12m9-capabilities.json`。唯一非空 `remaining_gaps` 仍为 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- live `known_gaps` 只有 `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache`，current 为 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- CopyOnChange reopen condition 继承 C12-M8：必须先有更强 native copied graph artifact，再通过 request-local DTO approval，再确认 current mismatch；S1 不把该 remaining gap 自动升级为 implementation。
- live `narrowed_gaps` presence 为 15 条：`part_design.revolution_groove` 1 条，`part_workbench.filling` 6 条，`part_workbench.geomplate` 4 条，`part_workbench.loft` 1 条，`part_workbench.project_on_surface` 1 条，`part_workbench.sweep` 2 条。S1 只记录 presence，不做 S2 归类或 S3 mismatch 判断。
- publication authority 位于 `cad-core/src/runtime/capability_contract.cpp::capabilityContractJson()`、`diagnosticCodeList()`、`ondselSolverCapabilityJson()` 和 `part_design.sub_shape_binder` capability block；adapter assertions 位于 `cad-core/tests/test_adapters.py` 的 capability smoke / web contract / narrowed gap assertions 与 `cad-core/tests/test_c8_shapebinder.py` 的 CopyOnChange known gap assertions。
- `C12M9-SRC-001..003` 已写入 current evidence，`C12M9-SCOPE-101` 裁决为 `retained_blocker_needs_further_gate`，`C12M9-CAT-001` 记录为 retained known gap，`C12M9-BLOCKER-101` 已关闭。

## S2 narrowed gaps 与产品契约归类

- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=e0a9b08d2a`（`e0a9b08d2a 文档：关闭 C12-M9 S1 live capability 抽取`），起点 worktree clean。
- S2 执行前 C12-M9 队列第一项为 `7-1-01-27-C12-M9-S2-narrowed-gaps与产品契约归类.md`，后续为 S3-S6；S2 关闭后队列应从 S3 继续。
- live capability snapshot 已刷新到 `/tmp/c12m9-capabilities.json`。S2 只消费 capability 与 C12-M2/M3/M4/M6/M7/M8 README，不运行 FreeCADCmd，不新增 fixture/expected，不修改 production code、tests、adapters 或 capability source。
- Groove UpTo 归类为 `product_diagnostic_contract_non_parity_retained`：live `part_design.revolution_groove.status=supported_c12m7_groove_upto_product_diagnostic_contract`，narrowed gap route 为 `product_diagnostic_contract_non_parity`，FreeCAD native parity 仍为 false；reopen/delete condition 是同一 FreeCAD/LibPack/OCCT oracle baseline 证明 UpToFirst 与 UpToFace native success，并出现 current mismatch。
- RuledSurface wire/wire 归类为 `current_supported_retained`：live `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed` 且 `remaining_gaps=[]`，继承 C12-M6 `wire_wire_admitted_current_supported`；只有 regression 或 checked-in expected/current mismatch 才重开。
- Part Workbench narrowed gaps 已逐类关闭 S2 分类：Sweep 是 product-contract non-parity，保留 Location/native-probe blocker 与 current-covered context；Filling 是 helper-blocked/product-contract non-parity；GeomPlate 中 projected curve2d initial surface 为 current-covered，其余 curve criteria、G1 curve-on-surface、PlateSurface wrapper lifecycle、no-initial-surface oracle blocker 按 request-local contract/native-hidden/oracle-blocked/non-goal 保留；Loft 是 native-hidden product-contract non-parity；ProjectOnSurface 是 native-hidden/request-local ledger product contract。
- Assembly 归类为 request-local covered subset 与 non-goal 并存：`ondsel_solver_adapter.status=covered_full`，覆盖 grounded joints、extended distance geometry、subshape marker placement、runPreDrag 和 placement validation；`representative_solver_adapter.status=covered_representative`，full solver、persistent solver state、cross-request assembly session 继续是 non-goals。
- `C12M9-SCOPE-201/202/301/401` 已关闭为分类完成，`C12M9-CAT-002..005` 已写入 S2 decision，`C12M9-NG-004/006` 继续保留，`C12M9-BLOCKER-201` 已关闭。S2 没有打开 implementation row，S3 仍需按 stable expected/product contract、request-local boundary 和 current mismatch 逐项准入。

## S3 expected 与 current mismatch 准入

- S3 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=514409a568`（`514409a568 docs: 关闭 C12-M9 S2 narrowed gaps 归类`），起点 worktree clean。
- S3 执行前 C12-M9 队列第一项为 `7-1-01-28-C12-M9-S3-expected与current-mismatch准入.md`，关闭后文件为 `7-1-01-28-【已实现】C12-M9-S3-expected与current-mismatch准入.md`，队列应从 S4 继续。
- live capability 已刷新到 `/tmp/c12m9-capabilities-s3.json`。S3 只消费 live capability、S2 矩阵和 C12-M2/M3/M4/M6/M7/M8 README，不运行 FreeCADCmd，不新增或修改 fixture expected，不修改 `cad-core/src`、`include`、tests、adapters 或 capability source。
- `C12M9-CAT-001` CopyOnChange：expected source=`none`，request-local boundary=`needs product decision`，current comparison=`blocked/not comparable`。C12-M8 native copied graph gate 未过且完整 DTO 未批准，current retained diagnostic 不能被写成 implementation mismatch。
- `C12M9-CAT-002` Groove UpTo：expected source=`product diagnostic contract`，request-local boundary=`approved`，current comparison=`current-covered`。该行继续是 product diagnostic contract non-parity，不是 native parity success。
- `C12M9-CAT-003` RuledSurface wire/wire：expected source=`checked-in expected`，request-local boundary=`approved`，current comparison=`current-covered`。未发现 checked-in expected/current mismatch。
- `C12M9-CAT-004` Part Workbench narrowed rows：expected source 与 boundary 为 mixed；current comparison 为 current-covered 或 not comparable。Sweep/Filling/GeomPlate/Loft/ProjectOnSurface 仍分别落在 current-covered、native-hidden、helper-blocked、oracle-blocked、product-contract non-parity 或 non-goal，没有 mismatch-confirmed 行。
- `C12M9-CAT-005` Assembly：request-local subset 有 checked-in expected/current-covered；full solver、persistent solver state 和 cross-request session 是 non-goals。未发现 request-local subset mismatch。
- `C12M9-CAT-006` authorization placeholder：没有 admitted candidate，current comparison not comparable。S3 结论为 `no S4 implementation candidate yet`。
- `C12M9-BLOCKER-301` 已关闭为 `closed_s3_no_admitted_candidate`，`C12M9-VAL-301` 已记录实际复核结果。S4 只能在无新增证据时记录 no-candidate source review，不得授权 implementation package。

## S4 最高优先候选 source 与验证范围复核

- S4 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=50c3691d8c`（`50c3691d8c docs: 关闭 C12-M9 S3 mismatch 准入`），起点 worktree clean。
- S4 执行前 C12-M9 队列第一项为 `7-1-01-29-C12-M9-S4-最高优先候选source与验证范围复核.md`，后续为 S5-S6；关闭后文件为 `7-1-01-29-【已实现】C12-M9-S4-最高优先候选source与验证范围复核.md`，队列应从 S5 继续。
- S4 复核 S3 结果与 backend/scope/non-goal 矩阵：`C12M9-CAT-001..005` 均为 `not_s4_candidate`，`C12M9-CAT-006` 为 `no_s4_candidate_yet`；没有 admitted `mismatch-confirmed` 行。
- S4 结果为 `no_candidate_after_s3_gate`。没有可写 FreeCAD source authority、cad-core landing、fixtures / tests 或 implementation surface；不得发明 C++ 范围。
- `C12M9-BLOCKER-401` 已关闭为 no-candidate evidence。`C12M9-SCOPE-501` / `C12M9-CAT-006` 作为 S5 输入只能走 `no_code_backlog_gate`，或在后续新证据出现时另开 oracle / product-contract package；本 S4 不创建后续 implementation 包。

## S5 implementation package authorization 裁决

- S5 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d644685d8d`（`d644685d8d docs: 关闭 C12-M9 S4 无候选复核`），起点 worktree clean。
- S5 执行前 C12-M9 队列第一项为 `7-1-01-30-C12-M9-S5-implementation-package-authorization裁决.md`，后续为 S6；关闭后文件为 `7-1-01-30-【已实现】C12-M9-S5-implementation-package-authorization裁决.md`，队列应从 S6 继续。
- S5 核对 S4 输入：没有 admitted `mismatch-confirmed` row，`C12M9-BLOCKER-401=closed_s4_no_candidate_after_s3_gate`，`C12M9-CAT-006=s5_input_no_code_backlog_gate`。
- S5 final decision 为 `no_code_backlog_gate`。不创建 implementation package，不修改 C++，不刷新 expected，不删除 CopyOnChange known gap，不删除 Groove product diagnostic contract。
- `C12M9-BLOCKER-501` 已关闭为 `closed_s5_no_code_backlog_gate`，`C12M9-VAL-501` 记录为 `passed_s5_no_code_backlog_gate`。
- 后续只能在新 oracle / product-contract evidence 同时证明 stable expected 或 approved product contract、request-local boundary 与 current mismatch 时另开包；本 S5 不授权当前实现包。

## S6 发布闸门与后续分流

- S6 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=ef3f457447`（`ef3f457447 docs: 关闭 C12-M9 S5 无代码积压闸门`），起点 `git -c core.quotepath=false status --short -uall` 无输出，即 worktree clean。
- S6 执行前 C12-M9 队列第一项为 `7-1-01-31-C12-M9-S6-发布闸门与后续分流.md`；关闭后文件为 `7-1-01-31-【已实现】C12-M9-S6-发布闸门与后续分流.md`，队列应只输出表头。
- C12-M9 final publication 为 `no_code_backlog_gate`。S0-S5 最终结果保持有效：CopyOnChange retained blocker、narrowed gaps / product-contract non-parity 分类、no mismatch-confirmed row、`no_candidate_after_s3_gate`、未授权 implementation package。
- 本包不创建 implementation package，不补 C++，不刷新 expected，不重开 C12-M8 CopyOnChange，不把 helper-blocked / native-hidden / product-contract non-parity 写成 supported。
- `C12M9-BLOCKER-601` 已关闭，`C12M9-VAL-601` 记录最终队列闭合；后续只能在新 oracle / product-contract evidence 同时满足 stable expected 或 approved product contract、request-local boundary 与 current mismatch 时另开新包。

## 候选准入规则

任一 C12-M9 候选必须同时满足三项，才允许 S5/S6 产出后续 implementation package：

1. FreeCAD source authority 或已批准 product-contract authority 明确，且语义能在 CAD Core 无状态 request-local 边界内表达。
2. 有 stable native expected、checked-in expected-backed fixture，或已批准的 product diagnostic / product contract expected。
3. current `cad-core` 与该 expected 或 contract 存在可复现 mismatch，且 mismatch 不能由 docs wording、adapter publication 或已知 non-parity contract 解释。

如果任一项不成立，C12-M9 只能发布 no-code backlog gate 或下一轮 oracle / product-contract 准入包，不授权 C++。

## 初始候选池

- CopyOnChange：active remaining gap，但被 C12-M8 保留为 native evidence blocker；只有更强 native copied graph artifact 先重开并通过 S2/S3/S4，才可进入 implementation。
- Groove UpTo：C12-M7 已发布 product diagnostic contract；只有同一 FreeCAD / LibPack / OCCT baseline 证明 native success 且 current mismatch，才可另开 geometry implementation candidate。
- RuledSurface wire/wire：C12-M6 已发布 `wire_wire_admitted_current_supported`；只有 checked-in expected 与 current output 出现真实 mismatch 才重开。
- ProjectOnSurface provenance：C12-M4 已发布 request-local ledger 产品契约；FreeCAD native mapper/history oracle unavailable 不能单独重开代码。
- Sweep / Filling / GeomPlate / Loft narrowed gaps：多数是 native helper blocked、native hidden 或 product-contract non-parity；只有 stable expected + current mismatch 同时成立才进入实现候选。
- Assembly representative / marker / writeback：当前 request-local subset 已 covered；full solver、persistent solver state 和 cross-request session 仍是 non-goal。

## 工作步骤

- 入口：确认 C12-M9 队列和包结构（已完成）。
- S0：live 基线与 C12-M1..M8 关闭口径冻结（已完成）。
- S1：live capability 和非空 `remaining_gaps` 抽取（已完成）。
- S2：`narrowed_gaps`、product-contract non-parity 和 historical evidence 归类（已完成）。
- S3：stable expected / product contract 与 current mismatch 准入（已完成，未产生 S4 implementation candidate）。
- S4：最高优先候选的 FreeCAD source、cad-core 落点和验证范围复核（已完成，`no_candidate_after_s3_gate`）。
- S5：implementation package authorization 或 no-code backlog 裁决（已完成，`no_code_backlog_gate`，未授权 implementation package）。
- S6：发布闸门、README 更新和后续分流（已完成，`no_code_backlog_gate`，队列关闭）。

## 入口

- 总入口：`7-1-01-23-C12-M9-CADCoreImplementationCandidate再盘点批次总入口.md`
- 方案：`7-1-01-23-C12-M9-CADCoreImplementationCandidate再盘点批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次 docs/CADCore12.0/README.md
git diff --check
```
