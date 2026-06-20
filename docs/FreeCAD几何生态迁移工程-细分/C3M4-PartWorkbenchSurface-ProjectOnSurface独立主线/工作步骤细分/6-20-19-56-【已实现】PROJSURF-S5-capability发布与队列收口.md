# PROJSURF-S5 capability 发布与队列收口

## 目标

在 S1-S4 的 fixtures、expected、executor 和 focused tests 全部通过后，更新 `ProjectOnSurface` capability、文档矩阵和发布口径，关闭本队列；只发布已验证分支，不把 full ProjectOnSurface 或 mapper/history provenance 写成完成。

## 必读

- 本目录所有已实现 S0-S4 文件。
- `/Users/li/Chili3DProject/FreeCAD/docs/接口规定/01-cad-recompute全量输入输出接口.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/03-【已实现】Sketcher-Part-PartDesign几何能力复刻.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/capabilities-gap对照表.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/CADCore3.0/oracle-fixture队列.md`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/adapters/c_api/c_api.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_adapters.py`

## 工作内容

1. 更新 `part_workbench.project_on_surface` capability：`status`、`mode_values`、`covered`、`fixtures`、`diagnostics`、`remaining_gaps` 和 `non_goals` 必须和 S1-S4 实际验证一致。
2. 更新 adapter tests，断言 face rebuild、height、offset、多 projection 已发布，同时 mapper/provenance、GUI task panel、未验证高级分支仍保持 gap/non-goal。
3. 更新 CADCore3.0 / 接口规定 / 本包矩阵，只记录当前基线、关键结论、FreeCAD 依据、代码落点、剩余风险和验收命令。
4. 运行本轮短跑、focused tests 和 queue script；队列为空后再收口。
5. 提交前确认暂存内容只包含本轮 ProjectOnSurface/cad-core/docs 相关文件，不混入 build 产物或用户其它改动。

## 非目标

- 不修改未验证分支的 fixture expected。
- 不跑全量 FreeCAD 构建。
- 不把 projected edge provenance / ElementMap mapper history 宣称为完成。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 docs/CADCore3.0 docs/接口规定 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

完成后把本文件重命名为 `6-20-19-56-【已实现】PROJSURF-S5-capability发布与队列收口.md`，执行中文 commit 工作流，并证明工作区没有本轮遗留改动。
