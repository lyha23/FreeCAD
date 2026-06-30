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
- S0：live 基线与 C12-M1..M8 关闭口径冻结。
- S1：live capability 和非空 `remaining_gaps` 抽取。
- S2：`narrowed_gaps`、product-contract non-parity 和 historical evidence 归类。
- S3：stable expected / product contract 与 current mismatch 准入。
- S4：最高优先候选的 FreeCAD source、cad-core 落点和验证范围复核。
- S5：implementation package authorization 或 no-code backlog 裁决。
- S6：发布闸门、README 更新和后续分流。

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
