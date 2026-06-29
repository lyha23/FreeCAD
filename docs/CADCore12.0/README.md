# CADCore12.0

CADCore12.0 承接 CADCore11.0 队列关闭后的下一轮 capability-first 规划。当前不直接重开 C11-M1 Sweep Location overload 或 C11-M2 Filling native helper：这两条线都已关闭为 no-code retained non-parity gate，且 live capability 中 `part_workbench.sweep.remaining_gaps=[]`、`part_workbench.filling.remaining_gaps=[]`。

当前 live capability 唯一非空 `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。C9-M5 与 C10-M4 已多轮复审并保留为 `known_gap_diagnostic` / `oracle_blocked`，不是默认 C++ 实现入口。CADCore12.0 的第一包因此先做全局候选盘点：从 live capability、CADCore9/10/11 的 release gate 和 current tests 中筛出下一批真正可实现的 backend gap；如果没有满足 oracle / request-local / current mismatch 的候选，S6 必须发布 no-code backlog gate，而不是勉强实现 CopyOnChange。

## 入口

- C12-M1 总入口：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次总入口.md`
- C12-M1 方案：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次方案.md`
- C12-M1 工作步骤：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分/`
- C12-M1 矩阵：`C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/`

## 当前基线

- S0 live 冻结 `HEAD=4446df0c87`（`4446df0c87 docs: 关闭 C11-M2 S6 发布闸门`），C11-M1 / C11-M2 队列检查均只输出表头。
- S0 起点 dirty boundary 只包含 C12-M1 docs 和 `docs/CADCore12.0/README.md` 未跟踪文件；未发现 `cad-core/src`、tests、fixtures、expected 或 collector 改动。
- capability 复核命令使用 `cad-core/cad-core capabilities`，冻结输出保存到 `/tmp/c12-capabilities.json`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，known gap 状态为 `known_gap_diagnostic`，route 为 `oracle_blocked`。
- S1 source authority 已复核：capability/test/FreeCAD source/current landing 均可定位，`C12M1-BLOCKER-101` 已关闭；未采 oracle、未改 C++、未新增 fixture、未创建 implementation row。
- Assembly representative solver、Part Workbench Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 等当前存在的是 representative / narrowed / historical evidence，不是已证明的 active backend gap。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0
git diff --check
```
