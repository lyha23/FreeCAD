# C5-M3 Loft / Pipe 高级分支主线总入口

## 目标

按 PartDesign Loft / Pipe 各自源码调用链推进 advanced branches，重点是 Sections、Closed、multi-wire ordering、Pipe orientation / Transformation / Transition 和 sewing MapperHistory。

## 必读文件

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/矩阵/loft_pipe_advanced_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/矩阵/loft_pipe_advanced_blocker_queue.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/工作步骤细分/6-20-10-49-C5-S3-M3-LoftPipe高级分支.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/工作步骤细分 --format markdown
```
