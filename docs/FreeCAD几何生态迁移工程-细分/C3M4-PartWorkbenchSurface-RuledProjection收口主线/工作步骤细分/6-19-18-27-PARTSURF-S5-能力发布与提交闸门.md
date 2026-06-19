# PARTSURF-S5 能力发布与提交闸门

## 目标

完成 Part surface 主线发布：同步 CADCore3.0 文档、capability metadata、fixture 队列和矩阵状态，确保只发布已验证的 `Part::RuledSurface` edge/edge 第一批；`Part::ProjectOnSurface` 只能列为 source-audited / planned，不 overclaim full surface family。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_scope_review_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/03-【已实现】Sketcher-Part-PartDesign几何能力复刻.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/capabilities-gap对照表.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/oracle-fixture队列.md`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/adapters/c_api/c_api.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_adapters.py`

## 工作内容

1. 更新 CADCore3.0 能力文档和 oracle fixture 队列，列出具体 fixtures、FreeCAD source authority、cad-core 落点和剩余 gaps。
2. 如需 capability metadata，新增 `part_workbench.ruled_surface` 或同等精确 key；不得写成 `full_surface_family`。
3. 更新 adapter tests，断言 capability JSON 只声明已验证 RuledSurface 范围，并列出 ProjectOnSurface/full surface family remaining gaps。
4. 运行本轮短跑和必要 focused tests。
5. 本步骤完成后执行中文 commit 工作流，但必须先确认暂存内容不包含既有 Sketcher 改动或本地生成物。

## 非目标

- 不补未验证的 ProjectOnSurface 分支。
- 不发布 ProjectOnSurface supported 或可用窄批次；S4 已把它拆入独立后续主线。
- 不跑全量 FreeCAD 构建。
- 不清理或回退用户既有工作区改动。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线 docs/CADCore3.0 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

完成后把本文件重命名为 `6-19-18-27-【已实现】PARTSURF-S5-能力发布与提交闸门.md`，再执行中文 commit 工作流。
