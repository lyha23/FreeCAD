# C4-M5 Assembly / Runtime / Adapter 产品化主线总入口

## 目标

把 Assembly 目标内 JointType / solver validation / placement writeback 与 Runtime / Adapter 资源契约补成一个产品化专题包。核心原则是 adapter 只做协议转换，不承接 FreeCAD 业务语义。

## 必读文件

- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/6-19-23-58-C4-M5AssemblyRuntimeAdapter产品化补齐方案.md`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/矩阵/assembly_runtime_scope.tsv`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/矩阵/assembly_runtime_blocker_queue.tsv`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分/6-20-00-11-【已实现】C4-S10-M5-Assembly目标审计与实现.md`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分/6-20-00-12-【已实现】C4-S11-M5-RuntimeAdapter契约补齐.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分 --format markdown
```
