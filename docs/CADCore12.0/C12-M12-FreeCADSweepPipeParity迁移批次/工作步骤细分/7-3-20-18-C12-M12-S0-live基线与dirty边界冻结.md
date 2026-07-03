# C12-M12 S0 live 基线与 dirty 边界冻结

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
