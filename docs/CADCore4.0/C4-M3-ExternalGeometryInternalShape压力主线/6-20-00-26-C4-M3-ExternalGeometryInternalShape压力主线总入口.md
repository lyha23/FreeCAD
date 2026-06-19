# C4-M3 ExternalGeometry / InternalShape 压力主线总入口

## 目标

按 ExternalGeometry / InternalShape 压力主线推进 source audit、fixture/oracle、history propagation 和 topo naming 验收。

## 必读文件

- `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/6-20-00-26-C4-M3-ExternalGeometryInternalShape压力方案.md`
- `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/矩阵/external_internal_shape_scope.tsv`
- `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/矩阵/external_internal_shape_blocker_queue.tsv`
- `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/工作步骤细分/6-20-00-08-【已实现】C4-S7-M3-ExternalGeometry-InternalShape压力包.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/工作步骤细分 --format markdown
```
