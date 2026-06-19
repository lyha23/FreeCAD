# 【已实现】P5CONIC-S0 live 基线与边界复核

## 目标

确认本轮 conic arcs 收口的 live 仓库状态、HEAD、dirty 边界和现有能力口径。只做审计和状态记录，不改实现。

## 必读文件

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/6-19-13-43-P5-SketcherConicArcs-HyperbolaParabola收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/矩阵/p5_conic_arcs_scope_review_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/矩阵/p5_conic_arcs_blocker_queue.tsv`

## 操作

1. 在 `/Users/li/Chili3DProject/FreeCAD` 执行：
   ```bash
   pwd
   git rev-parse --short HEAD
   git log -1 --oneline
   git -c core.quotepath=false status --short -uall
   ```
2. 搜索当前 conic arcs 落点：
   ```bash
   rg -n "ArcOfHyperbola|ArcOfParabola|SketchHyperbolaArc|SketchParabolaArc|unsupported-hyperbola" cad-core/src cad-core/tests cad-core/fixtures
   ```
3. 判断当前状态属于：
   - implementationAbsent：实现钩子不存在。
   - implementationSeed：实现钩子存在但未验证。
   - validationMismatch：实现钩子存在但测试/fixture/诊断仍表达 unsupported。
   - closed：实现、fixture、诊断、docs 均已一致。
4. 更新 `p5_conic_arcs_scope_review_matrix.tsv` 与 `p5_conic_arcs_blocker_queue.tsv` 的 live 状态。

## 非目标

- 不修改 `cad-core/src/`。
- 不改 fixture expected。
- 不提交。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线
```

## 完成条件

矩阵记录当前 HEAD、dirty 边界和 conic arcs 能力状态；若判断为 closed，后续步骤应改名为 `【已实现】` 或压缩为验证收口。
