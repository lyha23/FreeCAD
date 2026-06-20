# C5-M6-S4 capability 与文档收口

## 目标

同步 C5-M6、本包矩阵、CADCore3.0 capability 文档和 `cad_core_capabilities_json()` 的发布口径，关闭队列。

## 必读

- 本目录 S0-S3 已实现文件。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 工作内容

1. 确认 `part_workbench.loft` 发布为 profile / linearize expected-backed slice。
2. 确认 `part_workbench.sweep` 发布为 multi-profile / linearize expected-backed slice。
3. 确认 remaining gaps / non-goals 没有 broad `full_part_surface_family` 或旧 `linearize_post_processing`。
4. 更新 README、矩阵和 CADCore3.0 docs。
5. 运行队列脚本；队列为空后收口。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 docs/CADCore3.0 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

完成后重命名为 `6-20-22-08-【已实现】C5-M6-S4-capability与文档收口.md`，并按仓库规则提交本轮相关改动。
