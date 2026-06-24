# C6-M4 Part Workbench Sweep LocatedProfile Combined PipeShell Product Contract 主线

本目录是 CADCore6.0 的第四条主线。它聚焦 `Part::Sweep` / `BRepOffsetAPI_MakePipeShell` located profile、profile placement，以及 auxiliary + transition + tolerance combined 高级 case。

## 当前状态

- S0-S6 均为 `待执行`；工作步骤总入口只是已创建索引。
- 当前 live 代码仍把 `part-sweep-located-profile-contract` 和 `part-sweep-advanced-combined-contract` 固定为 known_gap/request_metadata_only。
- 两个 focused guard 已在建包时通过：located profile known_gap guard 与 combined known_gap/diagnostics priority guard。

## 主线边界

- 本包可以把 located profile 和 combined case 推进为 CAD Core product contract，但不声明 FreeCAD parity。
- `Part::Sweep::execute()` 的 native DocumentObject 只作为标准 Sweep source authority；高级 wrapper 依据来自 `BRepOffsetAPI_MakePipeShellPyImp.cpp`。
- 不混入 Filling、Loft、Groove、GUI/TaskPanel、PartDesign Pipe/Hole 或 persistent wrapper lifecycle。

## 队列

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```
