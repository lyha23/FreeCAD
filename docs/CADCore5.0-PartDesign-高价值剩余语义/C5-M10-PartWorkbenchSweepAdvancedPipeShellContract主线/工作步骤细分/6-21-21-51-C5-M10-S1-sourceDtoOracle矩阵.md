# C5-M10-S1 source / DTO / oracle 矩阵

状态：`pending`

## 目标

读 FreeCAD 与 cad-core 当前实现，冻结 C5-M10 的 advanced PipeShell request-local DTO / API 边界。重点是把 wrapper 与 PartDesign Pipe 使用的同一 `BRepOffsetAPI_MakePipeShell` builder 语义映射到 `part_workbench.sweep` advanced contract，同时明确 native `Part::Sweep` 本身没有这些直接属性。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- 在本包方案或局部矩阵中补全字段级合同：`AuxiliarySpine`、`AuxiliaryCurvilinear`、`SpineSupport` / `SupportMode`、`Binormal`、profile `Location`、`WithContact`、`WithCorrection`、`Tolerance.tol3d/boundTol/tolAngular`。
- 明确每个字段的 FreeCAD 依据、cad-core 落点、oracle 可采字段、expected schema、diagnostic code/property/target/subname 要求。
- 判断哪些字段能用 FreeCADCmd wrapper expected 批量采集；不能采集的字段写 source-backed known_gap 和 delete_condition，不得用 cad-core 输出倒推 expected。
- 更新 `C5M10-BLK-101`、`C5M10-SCOPE-101`、`C5M10-ORC-101` 与 root 对应 C5-M10 source/oracle 行。

## 非目标

- 不实现几何能力。
- 不缩小到单个 case；S1 必须让 S2/S3 能按同一 DTO 边界批量推进。
- 不把 PartDesign Pipe 或 Hole 发布为本包产品支持。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```
