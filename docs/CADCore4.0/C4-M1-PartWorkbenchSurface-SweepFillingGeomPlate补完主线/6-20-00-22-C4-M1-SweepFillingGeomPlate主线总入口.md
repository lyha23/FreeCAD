# C4-M1 Sweep / Filling / GeomPlate 主线总入口

## 目标

把 Sweep advanced、Filling advanced 和 GeomPlate advanced constraints 从一个模糊 surface boundary 拆成可审计、可 deferred、可验收的补完主线。

## 必读文件

- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/6-20-00-22-C4-M1-SweepFillingGeomPlate补完方案.md`
- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/矩阵/sweep_filling_geomplate_scope.tsv`
- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/矩阵/sweep_filling_geomplate_blocker_queue.tsv`
- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/工作步骤细分/6-20-00-04-【已实现】C4-S3-M1-SweepFillingGeomPlate补完.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/工作步骤细分 --format markdown
```
