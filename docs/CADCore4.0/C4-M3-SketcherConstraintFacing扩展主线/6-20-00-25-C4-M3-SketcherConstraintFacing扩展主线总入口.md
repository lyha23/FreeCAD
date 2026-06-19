# C4-M3 Sketcher Constraint-facing 扩展主线总入口

## 目标

按 Sketcher constraint-facing 扩展主线推进 DTO、状态、diagnostics、geometry update 和 adapter/capability 验收。

## 必读文件

- `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/6-20-00-25-C4-M3-SketcherConstraintFacing扩展方案.md`
- `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/矩阵/sketcher_constraint_scope.tsv`
- `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/矩阵/sketcher_constraint_blocker_queue.tsv`
- `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/工作步骤细分/6-20-00-07-C4-S6-M3-Sketcher约束关系审计.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/工作步骤细分 --format markdown
```
