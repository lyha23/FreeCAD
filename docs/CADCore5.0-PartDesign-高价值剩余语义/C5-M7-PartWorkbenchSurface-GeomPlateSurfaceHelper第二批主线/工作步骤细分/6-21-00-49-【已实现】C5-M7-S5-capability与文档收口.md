# 【已实现】C5-M7 S5 capability 与文档收口

状态：`【已实现】`

## 收口结论

- S0-S4 已全部标为 `【已实现】`；S5 关闭后本包 `step_goal_queue.py` 输出为空。
- `part_workbench.geomplate` 发布边界为：3D default / explicit approximation / InitialSurface / Curve2dOnSurface / Point2dOnSurface / mixed G0+2D / point criteria expected-backed；G1 curve-on-surface 与 ProjectedCurve2d source-backed known_gap；curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle diagnostic-backed。
- remaining 只保留 G1 native expected oracle、ProjectedCurve2d native expected oracle、curve criteria setter `NotImplementedError`、`Part.PlateSurface.Curves` wrapper lifecycle；GUI GeomPlate feature、原生 `Part::GeomPlate` DocumentObject、fake persistent PlateSurface、Filling 扩展和 full Part surface family 均为 non-goal。
- C5-M7 包内矩阵和 C5 根矩阵已从 pending/in_progress 收为 done；C3-M8 后续清单不再保留 broad GeomPlate advanced gap。

## 目标

发布 C5-M7 的最终 supported / remaining / non-goal 边界，关闭本包队列。

## 工作

1. 更新 `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 的 `part_workbench.geomplate`。
2. 更新 `cad-core/tests/test_adapters.py` capability 断言。
3. 更新 `docs/CADCore3.0/capabilities-gap对照表.md`、`docs/CADCore3.0/06-C3-M8后续收口清单.md` 和 C5-M7 矩阵。
4. 确认 remaining gaps 不再保留 stale broad GeomPlate advanced gap，只保留本轮明确不能关闭的 precise owners。
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
