# C5-M1 Revolution / Groove 参数补完主线总入口

## 目标

按 PartDesign `Revolved` 共享调用链推进 Revolution / Groove 高价值剩余参数：`TwoAngles`、`ThroughAll`、`UpTo*`、Profile subshape 和 `FuseOrder=FeatureFirst`。

## 当前状态

C5-S1 已实现为 mixed support/diagnostic：`TwoAngles` 与 Groove `ThroughAll` expected-backed，UpTo/Profile/FuseOrder diagnostic-backed。队列应返回空 pending。

## 必读文件

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/6-20-10-48-C5-M1-RevolutionGroove参数补完方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/矩阵/revolved_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/矩阵/revolved_blocker_queue.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/工作步骤细分/6-20-10-49-C5-S1-M1-RevolutionGroove参数补完.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/工作步骤细分 --format markdown
```
