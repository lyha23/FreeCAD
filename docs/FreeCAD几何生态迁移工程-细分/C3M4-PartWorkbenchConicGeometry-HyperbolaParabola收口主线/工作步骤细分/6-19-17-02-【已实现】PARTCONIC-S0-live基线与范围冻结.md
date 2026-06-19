# 【已实现】PARTCONIC-S0 live 基线与范围冻结

## 目标

建立 PARTCONIC 新主线的 live 基线，确认 P5CONIC 不需要重开，并冻结本轮边界：Part geometry Hyperbola / Parabola 曲线、finite edge、oracle、消费者裁决，不进入 Sketcher solver / GUI / broad surface family。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/6-19-17-01-C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/矩阵/part_conic_geometry_scope_review_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/矩阵/part_conic_geometry_blocker_queue.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/6-19-13-43-P5-SketcherConicArcs-HyperbolaParabola收口方案.md`

## 工作内容

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git status --short -uall`。
2. 运行 P5CONIC 和 PARTCONIC step queue，确认旧队列闭环、新队列从 S0 开始。
3. 更新 scope / blocker 矩阵里的 S0 基线字段，不改代码。
4. 若发现工作区有非本任务改动，只记录边界，不回退。

## S0 live 结论

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- HEAD：`6f70a6ad6a`。
- 最新提交：`6f70a6ad6a feat: 收口P5圆锥弧草图支持`。
- 工作区边界：已有 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp` 改动，以及本 C3M4 主线文档 / 矩阵未跟踪文件；S0 只更新本主线文档 / 矩阵，不回退既有改动。
- 队列结论：P5CONIC queue 为空；PARTCONIC queue 在 S0 收口前从本文件开始，完成改名后下一项应为 S1。

## 非目标

- 不实现 cad-core。
- 不采集 oracle。
- 不把 Part surface family 写成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线
```

完成后把本文件重命名为 `6-19-17-02-【已实现】PARTCONIC-S0-live基线与范围冻结.md`。
