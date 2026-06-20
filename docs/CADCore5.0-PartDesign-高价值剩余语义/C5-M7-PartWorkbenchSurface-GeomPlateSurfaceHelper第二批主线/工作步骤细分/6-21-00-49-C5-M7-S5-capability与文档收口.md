# C5-M7 S5 capability 与文档收口

## 目标

发布 C5-M7 的最终 supported / remaining / non-goal 边界，关闭本包队列。

## 工作

1. 更新 `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 的 `part_workbench.geomplate`。
2. 更新 `cad-core/tests/test_adapters.py` capability 断言。
3. 更新 `docs/CADCore3.0/capabilities-gap对照表.md`、`docs/CADCore3.0/06-C3-M8后续收口清单.md` 和 C5-M7 矩阵。
4. 确认 remaining gaps 不再保留 stale broad `GeomPlate advanced constraints`，只保留本轮明确不能关闭的 precise owners。
5. 将本包工作步骤按实际完成情况重命名为 `【已实现】`，队列为空后收口。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 非目标

- 不把 Filling、ProjectOnSurface provenance 或 Sweep advanced contracts 混入 GeomPlate capability。
