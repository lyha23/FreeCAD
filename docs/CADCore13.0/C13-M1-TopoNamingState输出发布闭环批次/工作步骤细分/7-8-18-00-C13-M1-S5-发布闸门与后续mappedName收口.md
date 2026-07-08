# C13-M1 S5 发布闸门与后续 mappedName 收口

## 目标

关闭 C13-M1 输出发布闭环，并把未完成的 FreeCAD mapped-name parity 明确拆成后续批次。

## 必读文件

- S0-S4 输出
- `../README.md`
- `../7-8-17-53-C13-M1-TopoNamingState输出发布闭环批次方案.md`
- `../../README.md`
- 全部 `矩阵/*.tsv`

## 操作

1. 更新 README / 方案发布状态。
2. 关闭 blocker queue 中 C13-M1 必须项。
3. validation matrix 记录最终 focused 命令与结果。
4. non-goal / follow-up 中明确 mapped-name encoder、child map key、mapper history id 的后续边界。
5. 若实现完成，按仓库规则把方案文件重命名为 `【已实现】` 前缀；文档-only 初稿不重命名。

## 关闭条件

- 队列关闭后只剩表头或所有步骤标为已实现。
- `git diff --check` 通过。
- C13-M1 不再含有“输出端猜 FreeCAD mapped name”的开放 blocker。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```
