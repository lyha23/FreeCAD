# 【已实现】C12-M12 S0 live 基线与 dirty 边界冻结

## 目标

冻结迁移开始前的真实仓库状态，确认 C12-M12 是否可以进入 source / drift / oracle 流程，并保护用户已有工作区改动。

## 必读文件

- `../README.md`
- `../7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次方案.md`
- `../矩阵/c12m12_sweep_blocker_queue.tsv`
- `../矩阵/c12m12_sweep_validation_matrix.tsv`

## 操作

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`。
2. 记录 `git -c core.quotepath=false status --short -uall`，按 `docs`、`cad-core/src`、`cad-core/tests`、`cad-core/fixtures` 分组。
3. 列出现有 sweep/pipe fixtures 与 focused tests：
   - `find cad-core/fixtures -path '*pipe*' -o -path '*sweep*'`
   - `rg -n "Part::Sweep|AdditivePipe|SubtractivePipe|PipeShell|MakePipeShell" cad-core/tests cad-core/src`
4. 确认 C12-M12 本轮允许写入的文件范围。
5. 回写 blocker / validation matrix，并将本步骤重命名为 `【已实现】`。

## 关闭条件

- dirty boundary 已记录，且非 C12-M12 改动不会被覆盖。
- 现有 sweep/pipe fixture/test surface 已列出。
- 下一步 source authority 需要复核的 FreeCAD 文件列表已确认。

## 关闭记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=2677f140ed`（`2677f140ed 文档：关闭 C12-M12 工作步骤总入口`）。
- baseline dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出；按组记录为 `docs=clean`、`cad-core/src=clean`、`cad-core/tests=clean`、`cad-core/fixtures=clean`、`other/untracked=clean`。
- 本轮允许写入范围：`docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/README.md`、`7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次总入口.md`、`工作步骤细分/7-3-20-18-【已实现】C12-M12-S0-live基线与dirty边界冻结.md`、`矩阵/c12m12_sweep_blocker_queue.tsv`、`矩阵/c12m12_sweep_validation_matrix.tsv`。
- 非 C12-M12 改动边界：baseline 未发现非 C12-M12 dirty 改动；本步骤未修改 `cad-core/src`、`cad-core/tests`、`cad-core/fixtures`、expected 或 adapters。
- 已运行 fixture 盘点：`find cad-core/fixtures \( -path '*pipe*' -o -path '*sweep*' \) -print`。命中现有 sweep/pipe fixture roots：`c3m4`、`c4m1`、`c4m2`、`c5m3`、`c5m10`、`c5m12`、`c51m4`、`c6m1`、`c6m3`、`c6m4`。
- 已运行 focused surface 盘点：`rg -n "Part::Sweep|AdditivePipe|SubtractivePipe|PipeShell|MakePipeShell" cad-core/tests cad-core/src`。命中测试面：`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/c6m3_pipe_interpolation_law_probe.cpp`；命中实现面：`cad-core/src/part/part_sweep.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part_design/feature_pipe.cpp`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/src/runtime/capability_contract.cpp`，另有 incidental OCCT use in `cad-core/src/part_design/feature_hole.cpp`。
- S1 source authority 待复核文件已确认：`src/Mod/PartDesign/App/FeaturePipe.cpp`、`src/Mod/Part/App/TopoShapeExpansion.cpp`、`src/Mod/Part/App/PartFeatures.cpp`、`src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`。
- 后续队列从 S1 `FreeCAD source authority 复核` 继续。

## 非目标

- 不运行 FreeCADCmd。
- 不修改 `cad-core/src`。
- 不更新 expected。
- 不裁决用户失败样例根因。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/工作步骤细分 --format markdown
```
