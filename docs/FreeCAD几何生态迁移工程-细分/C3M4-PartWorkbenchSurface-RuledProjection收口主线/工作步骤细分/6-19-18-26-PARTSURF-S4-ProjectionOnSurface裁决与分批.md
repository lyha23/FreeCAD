# PARTSURF-S4 ProjectionOnSurface 裁决与分批

## 目标

在 RuledSurface 首批实现后，对 `Part::ProjectOnSurface` 做 source-backed 裁决：要么实现一个窄的 edge projection 批次，要么生成后续独立主线并把本主线发布口径固定为 RuledSurface。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_ruled_surface.cpp` 或 S3 实际落点

## 工作内容

1. 复核 `ProjectOnSurface::projectWire()`、`projectFace()`、`filterShapes()`、`createSolidIfHeight()` 的分支复杂度。
2. 若 S4 实现窄批次，只允许 `Mode=Edges`、`Height=0`、`Offset=0`、单 edge/wire 到单 support face，且必须有 FreeCAD expected。
3. 若 S4 不实现代码，必须新增后续 ProjectOnSurface 主线草案或矩阵条目，明确拆分原因、第一批 candidate fixture 和 non-goals。
4. 无论是否实现 ProjectOnSurface，都要更新本主线矩阵，防止 S5 发布 full ProjectOnSurface supported。

## 非目标

- 不实现完整 face rebuild、holes、solid height、offset placement、多 projection compound 顺序。
- 不把 ProjectOnSurface 写成 full supported。
- 不修改 unrelated PartDesign / Sketcher / Assembly 代码。

## 验收

若只做裁决文档：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

若实现窄批次：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-19-18-26-【已实现】PARTSURF-S4-ProjectionOnSurface裁决与分批.md`。
