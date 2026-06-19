# 【已实现】PARTSURF-S0 live 基线与范围冻结

## 目标

建立 Part Workbench surface 新主线 live 基线，确认 PARTCONIC 不需要重开，并冻结本轮边界：第一实现批次为 `Part::RuledSurface` edge/edge surface，`Part::ProjectOnSurface` 只进入源码裁决和分批设计。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_scope_review_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_blocker_queue.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/6-19-17-01-C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口方案.md`

## 工作内容

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git status --short -uall`。
2. 运行 PARTCONIC 和 PARTSURF step queue，确认旧队列闭环、新队列从 S0 开始。
3. 更新 scope / blocker 矩阵里的 S0 基线字段，不改代码。
4. 若发现工作区有非本任务改动，只记录边界，不回退。

## S0 结论

- live 基线：`pwd` 为 `/Users/li/Chili3DProject/FreeCAD`；HEAD 为 `a8080d9b30`；`git log -1 --oneline` 为 `a8080d9b30 feat: 收口PARTCONIC S5能力发布`。
- live status：仅有既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`，以及本 C3M4 PartSurface 主线 docs/矩阵/步骤文件；Sketcher 改动保持不暂存、不回退、不覆盖。
- 队列结论：PARTCONIC `工作步骤细分` 队列为空；PARTSURF 队列在本文件重命名前从 `6-19-18-22-PARTSURF-S0-live基线与范围冻结.md` 开始。
- 范围冻结：S0 只完成文档/矩阵基线冻结，未改代码、未采集 oracle、未实现 cad-core；full Part surface family 和 `Part::ProjectOnSurface` 仍不是 supported。

## 非目标

- 不实现 cad-core。
- 不采集 oracle。
- 不把 full Part surface family 或 ProjectOnSurface 写成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
```

完成状态：本文件已按完成规则命名为 `6-19-18-22-【已实现】PARTSURF-S0-live基线与范围冻结.md`。
