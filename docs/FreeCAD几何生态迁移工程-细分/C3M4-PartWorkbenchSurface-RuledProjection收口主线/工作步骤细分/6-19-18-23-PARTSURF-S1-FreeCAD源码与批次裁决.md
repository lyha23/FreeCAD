# PARTSURF-S1 FreeCAD 源码与批次裁决

## 目标

复核 `Part::RuledSurface` 和 `Part::ProjectOnSurface` 的 FreeCAD 调用链，裁决 S3 第一实现批次只做 source-backed `Part::RuledSurface` edge 分支，避免把 ProjectOnSurface 全分支混入同一实现。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_source_candidates.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`

## 工作内容

1. 用 `rg` 和源码阅读确认 `RuledSurface::execute()`、`TopoShape::makeElementRuledSurface()`、`ProjectOnSurface::tryExecute()`、`projectWire()`、`projectFace()`、`createSolidIfHeight()` 的真实调用顺序。
2. 更新 source candidates 和 scope matrix，写清 RuledSurface executor 的 cad-core 分层落点。
3. 裁决 S3 是否只纳入 edge/edge，还是可以同时纳入 wire/wire；必须写明依据，不能默认扩大。
4. 裁决 ProjectOnSurface 的 S4 处理方式：窄 edge projection 实现、还是拆出后续主线。

## 非目标

- 不写 cad-core 实现。
- 不采集 expected。
- 不新增 full surface family supported 口径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

完成后把本文件重命名为 `6-19-18-23-【已实现】PARTSURF-S1-FreeCAD源码与批次裁决.md`。
