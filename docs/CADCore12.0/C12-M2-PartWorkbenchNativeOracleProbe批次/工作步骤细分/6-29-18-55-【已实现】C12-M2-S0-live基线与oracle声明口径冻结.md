# 【已实现】C12-M2 S0 live 基线与 oracle 声明口径冻结

## 目标

冻结 C12-M2 的起点：确认 C12-M1 S6 已关闭为 `no_code_backlog_gate`，记录当前 HEAD、dirty boundary、C12-M2 包边界、FreeCADCmd 可发现性和 oracle collection 口径。本步不运行 native probe。

## 必读输入

- `docs/CADCore12.0/README.md`
- `docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分/6-29-16-34-【已实现】C12-M1-S6-NextBatch发布闸门与代码授权.md`
- `docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/README.md`
- `docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次总入口.md`

## 执行步骤

1. 运行 `git status --short -uall`、`git log -1 --oneline`，记录 C12-M2 起点和本步 dirty boundary。
2. 运行 C12-M1 和 C12-M2 的 `step_goal_queue.py --format markdown`，确认 C12-M1 队列已空、C12-M2 队列从 S0 开始。
3. 只做 FreeCADCmd 发现性检查：`command -v freecadcmd || command -v FreeCADCmd || command -v freecadcmd-daily || true`。不要在本步启动 FreeCAD。
4. 在 C12-M2 README、总入口或矩阵中补齐当前基线、禁止项和 S0 冻结结论。
5. 回写 validation matrix 的 S0 验收状态。

## 更新目标

- `README.md`
- `6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次总入口.md`
- `矩阵/c12m2_partworkbench_native_oracle_validation_matrix.tsv`
- 如发现环境阻塞，补 `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```

## 完成条件

S0 完成后，必须能用一句话说明：C12-M2 是 user-approved oracle/native probe 包，代码 gate 仍关闭；本步只冻结基线，不采 expected。

## S0 冻结结论

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `HEAD=4d245a9c11`
- `git log -1 --oneline=4d245a9c11 docs: 新增 C12-M2 native oracle probe 开包`
- `git -c core.quotepath=false status --short -uall=<clean>`
- C12-M1 队列检查只输出表头，确认已空；C12-M2 队列在 S0 执行前从 S0-S6 开始。
- FreeCADCmd 仅做发现性检查：`/Users/li/.cargo/bin/freecadcmd` 可发现；S0 未启动 FreeCAD，版本 / OCCT / LibPack / runtime 分类留给 S3。

一句话结论：C12-M2 是 user-approved oracle/native probe 包，继承 C12-M1 S6 `no_code_backlog_gate`，代码 gate 仍关闭；本步只冻结 live baseline 与 oracle 声明口径，不采 expected。
