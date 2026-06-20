# PROJSURF-S0 live 基线与第二批范围冻结

## 目标

刷新 `ProjectOnSurface` 独立主线的 live 状态，把“第一批待实现”的旧草案口径改为“第一批已 supported_expected_backed_first_slice，第二批高级分支待实现”，并冻结后续 S1-S5 的最小完整语义批次。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/6-19-19-18-C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线草案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/矩阵/part_project_on_surface_plan_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/矩阵/part_project_on_surface_second_batch_queue.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_project_on_surface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/adapters/c_api/c_api.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_adapters.py`

## 工作内容

1. 记录 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 复核现有第一批：`Part::ProjectOnSurface` executor、fixtures、expected、focused tests 和 capability `supported_expected_backed_first_slice`。
3. 更新草案和矩阵，只把已验证的 Edges/Height=0/Offset=0/单 Projection 写成已完成。
4. 固定第二批范围：face rebuild / holes、Height solid、Offset placement、多 Projection ordering、capability 发布。
5. 不得把 ProjectOnSurface full support 或 projected edge mapper/history 写成完成。

## 非目标

- 不写 C++ executor。
- 不新增 fixture / expected。
- 不修改 capability JSON 的 supported 范围，除非只是修正明显过时的文档说明。
- 不清理其它包的 stale 队列。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线
```

完成后把本文件重命名为 `6-20-19-51-【已实现】PROJSURF-S0-live基线与第二批范围冻结.md`。按仓库规则提交本轮相关改动；提交前确认暂存内容只包含本 ProjectOnSurface 包的 docs/矩阵。
