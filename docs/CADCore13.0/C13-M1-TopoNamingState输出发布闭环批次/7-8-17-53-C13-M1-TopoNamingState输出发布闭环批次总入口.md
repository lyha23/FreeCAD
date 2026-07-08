# C13-M1 TopoNamingState 输出发布闭环批次总入口

## 目标

让 `cad-core` 正式 recompute response 采集并输出 `topoNamingState`，使输出 schema 对齐 `cad-core/fixtures/<phase>/expected/*.freecad.json` 中的 `topoNamingState`，并保证该 state 能被下一次请求消费。

## 入口文件

- README：`README.md`
- 方案：`7-8-17-53-C13-M1-TopoNamingState输出发布闭环批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前结论

- `ComputeContext` 已经保存输入 `topoNamingState`。
- `runtime/recompute.cpp` 已经用输入 state 合并旧 `ElementMap` alias。
- 正式 response 尚未发布新的 `topoNamingState`。
- 本批次先补发布闭环，不做 FreeCAD mapped-name 字节级 parity。

## 使用方式

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
```
