# C12-M13 FreeCAD Sweep / Pipe 剩余语义迁移批次总入口

## 目标

继续推进 C12-M12 之后尚未完成的 FreeCAD Sweep / Pipe 迁移，把 multisection vertex、Boolean / AddSubShape / rawShape 生命周期、Part Sweep mutable helper 生命周期和 ORACLE-001 用户复现分流组织成可由 `goal-step-runner` 顺序执行的批次。

## 必读文件

- `README.md`
- `7-4-10-35-C12-M13-FreeCADSweepPipe剩余语义迁移批次方案.md`
- `矩阵/c12m13_sweep_remainder_source_matrix.tsv`
- `矩阵/c12m13_sweep_remainder_scope_matrix.tsv`
- `矩阵/c12m13_sweep_remainder_oracle_matrix.tsv`
- `矩阵/c12m13_sweep_remainder_blocker_queue.tsv`
- `矩阵/c12m13_sweep_remainder_validation_matrix.tsv`
- `矩阵/c12m13_sweep_remainder_non_goal_registry.tsv`
- `docs/CADCore12.0/README.md`

## 队列顺序

1. `7-4-10-36-【已实现】C12-M13工作步骤总入口.md`
2. `7-4-10-37-C12-M13-S0-live基线与C12-M12继承冻结.md`
3. `7-4-10-38-C12-M13-S1-source与current-landing批量复核.md`
4. `7-4-10-39-C12-M13-S2-oracle批量采集与用户复现分流.md`
5. `7-4-10-40-C12-M13-S3-multisection-vertex细节迁移.md`
6. `7-4-10-41-C12-M13-S4-Boolean-AddSubShape-rawShape生命周期迁移.md`
7. `7-4-10-42-C12-M13-S5-PartSweep-mutable-helper生命周期迁移.md`
8. `7-4-10-43-C12-M13-S6-集成回归与发布闸门.md`

## 当前状态

- 工作步骤总入口已关闭：包结构、矩阵字段和队列文件已创建。
- S0 已关闭：已冻结 live baseline、dirty boundary、C12-M12 继承口径和现有 Pipe/Sweep focused surface；下一步队列应从 S1 `source 与 current landing 批量复核` 继续。
- C12-M12 final status 为 `partial_implementation_multiwire_pipe_sewing`，不是完整 Sweep / Pipe 迁移完成。
- ORACLE-001 当前仍是 `waiting_user_repro`；没有用户 request/result 时不得编造 fixture，但也不阻塞本包其它 source-backed 剩余项。

## 执行规则

- 每次只处理队列中的第一个未实现步骤，完成后刷新队列。
- S1/S2 未证明 source-backed current mismatch 的子项，不进入 S3/S4/S5 C++ 实现。
- S3、S4、S5 分别限定在 multisection vertex、PartDesign lifecycle、Part Sweep helper lifecycle；不得把三个方向混成一次无边界改动。
- 如果 FreeCADCmd / OCCT baseline 与 expected 采集环境不一致，只能记录为兼容性探测，不能替代 parity 验收。
- ORACLE-001 若在中途拿到用户 input/output，先回到 S2 追加最小复现，再决定插入 S3/S4/S5。

## 关闭条件

- 队列、矩阵、README 与根 `docs/CADCore12.0/README.md` 均指向 C12-M13。
- `step_goal_queue.py` 能读出 S0-S6。
- TSV 字段数检查通过。
- whitespace 与 `git diff --check` 通过。

## 非目标

- 不在总入口执行代码实现。
- 不重开 C12-M12 已关闭的 fixed/round selected-spine 或 multi-wire sewing。
- 不等待 ORACLE-001 才执行其它 source-backed oracle / implementation 步骤。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次 docs/CADCore12.0/README.md
git diff --check
```
