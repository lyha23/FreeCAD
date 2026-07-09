# C5-M8-S1a cad-core 范围纠偏

状态：`done_cad_core_scope_corrected`

## 当前结论

- 上一轮把 `Part.makeFilledFace(...)` helper oracle 问题处理成 `src/Mod/Part/App/AppPartPy.cpp` 源码修复，超出了 C5-M8 的交付范围；本包只关心 `cad-core` 抽取实现，FreeCAD `src/` 只作为语义依据读取。
- `AppPartPy.cpp` 已恢复到 S1a 之前的内容；后续 C5-M8 step 不得修改 FreeCAD 上游源码来“修 oracle”。
- 当前机器安装版 `FreeCADCmd` 可用，但历史 C5-M8 Filling native helper probe 的 `surface` case 仍以 `139` 退出，support/order helper probe 也不是可写入 expected 的稳定基线。该 probe 脚本现已移除；这个事实只记录为 native helper oracle 缺口，不再作为必须修 FreeCAD 源码的任务。
- S1 可以继续，但边界必须改成 cad-core-only：优先使用现有 `FreeCADCmd` 能稳定返回的 expected；若 helper kwargs 仍不可采集，则只能做 source-backed / diagnostic-backed / known_gap，不能伪造 expected，也不能把 direct wrapper probe 当成 helper oracle。

## 目标

关闭错误的“修 FreeCAD helper oracle”方向，把 S1 的进入条件改回 `cad-core` 范围：实现或收敛 `Surface` / `Supports` / `Orders` 时，只更新 `cad-core`、fixtures、tests、capabilities 和本包文档矩阵。

## 必读

- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `C5-M8 Filling native helper probe（脚本已移除）`
- 本包 S1 step 和局部矩阵

## 产物

- 撤回 `AppPartPy.cpp` 的 S1a 源码改动，确保 `src/` 不再承载本包交付。
- 更新 C5-M8 局部矩阵和 C5 全局矩阵，把 S1a 标成范围纠偏完成。
- 更新 S1 step：后续不得要求先修 FreeCAD 源码；expected 只能来自现有可运行 `FreeCADCmd`，否则保持显式 diagnostic / known_gap。

## 非目标

- 不实现 cad-core Filling DTO。
- 不采集或提交 S1 expected。
- 不修改 FreeCAD `src/` 来恢复 helper oracle。
- 不把 direct `Part.BRepOffsetAPI.MakeFilling` wrapper probe 当成 `Part.makeFilledFace(...)` helper oracle。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- src/Mod/Part/App/AppPartPy.cpp docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown
```
