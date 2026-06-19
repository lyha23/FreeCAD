# PARTCONIC-S5 能力发布与提交闸门

## 目标

完成 PARTCONIC 能力发布、矩阵状态、CADCore3.0 文档同步、队列收口和中文提交。

## 必读

- 本主线全部已实现 S0-S4 步骤。
- `docs/CADCore3.0/03-【已实现】Sketcher-Part-PartDesign几何能力复刻.md`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `矩阵/part_conic_geometry_scope_review_matrix.tsv`
- `矩阵/part_conic_geometry_blocker_queue.tsv`
- `矩阵/part_conic_geometry_non_goal_registry.tsv`

## 工作内容

1. 把 supported 口径写清：Part geometry Hyperbola / Parabola finite edge、expected-backed fixtures、已验证 consumer。
2. 把 non-goal 写清：完整 Sketcher solver、GUI、fake DocumentObject、未验证 surface family、DistanceType default/TODO。
3. 关闭或保留 blocker，所有保留 blocker 必须有明确后续主线，不得假闭环。
4. 运行 queue，确认本目录只剩 S5 或清空；完成后重命名 S5。
5. 按仓库规则检查变更边界并中文提交。

## 非目标

- 不新增功能实现。
- 不顺手清理无关旧队列。
- 不改 unrelated fixture expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线 docs/CADCore3.0 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_diagnostics tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-19-17-07-【已实现】PARTCONIC-S5-能力发布与提交闸门.md`，再执行中文 commit 工作流。
