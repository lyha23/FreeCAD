# C4-M4 ReferenceRecovery / TopoNamingPressure 主线总入口

## 目标

把长期编辑链、引用恢复、TopoNaming、ElementMap 和 ReferenceShadow 压力回归收成一个可执行专题包，避免后续实现继续靠输出端猜测修正稳定名称。

## 必读文件

- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/6-19-23-57-C4-M4引用恢复与TopoNaming压力回归方案.md`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_scope.tsv`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_blocker_queue.tsv`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_pressure_matrix.tsv`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/6-20-00-09-【已实现】C4-S8-M4-TopoReference压力矩阵.md`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/6-20-00-10-【已实现】C4-S9-M4-TopoReference压力实现与发布.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分 --format markdown
```
