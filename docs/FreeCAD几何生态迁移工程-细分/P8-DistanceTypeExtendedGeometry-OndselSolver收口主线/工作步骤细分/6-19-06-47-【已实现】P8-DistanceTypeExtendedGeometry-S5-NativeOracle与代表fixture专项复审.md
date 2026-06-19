# P8 DistanceTypeExtendedGeometry S5 NativeOracle 与代表 fixture 专项复审【已实现】

## 目标

批量采集 FreeCADCmd native expected，证明 extended DistanceType 的 resolver evidence、ASMT class、scalar field 和 placement writeback 语义。S5 只冻结 native oracle / diagnostic 结论，S6 才能决定 supported 发布。

## live 基线

- 复核仓库：`pwd=/Users/li/Chili3DProject/FreeCAD`，本轮开始 `HEAD=8b6779b75c`，最新提交为 `8b6779b75c assembly: 完成P8扩展DistanceType S4映射`。
- 复核开始时工作区仅见 unrelated `AGENTS.md` dirty；本步骤未编辑或暂存 `AGENTS.md`。
- 原生采集使用 `FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd`，输出版本为 `FreeCAD 1.2.0devR20260519`。

## 已完成

- `cad-core/tools/collect_freecad_expected.py` 已扩展 fixture-side DistanceType oracle：`Assembly::AssemblyLink` 先解析到 linked Part object，再按 subname / primitive 记录 `reference*_primitive`、radius evidence、scalar correction、ASMT class 和 scalar field，避免只靠 `Face` / `Edge` 前缀误判。
- 已新增并采集 18 个 `cad-core/fixtures/c3m6` native expected。必选主批次全部有 checked-in FreeCADCmd expected：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`。
- representative oracle 也已覆盖 `PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`、`PointCurve`；这些文件记录 FreeCAD explicit switch 的 ASMT class / scalar 字段，但仍带 `known_gap` + `backendGap.delete_condition`，表示 S6 parity / publication gate 未关。
- default/TODO representative 采集为 diagnostic / nonGoal，不进入 supported：`PlaneCone` 代表 cone，`LineCylinder` 代表 line-surface，`CurvePlane` 代表 curve-face，`Other` 代表 edge-curve default。对应 expected 写入 `known_gap` + `nonGoal.delete_condition`，无 `backendGap`。
- `PointLine` 既有 native parity expected 未重采、未改写，仍保持 `ASMTLineInPlaneJoint.offset` 口径。

## S6 决策输入

| 类别 | cases | S5 结论 | S6 决策 |
| --- | --- | --- | --- |
| edge circle | `LineCircle`、`CircleCircle` | native expected 已采集，cad-core 当前因 marker placement / publication gate 以 `known_gap` 跳过 | 可进入 S6 parity / supported-candidate |
| face radius | `PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` | native expected 已采集，radius scalar correction 已锁定 | 可进入 S6 parity / supported-candidate |
| torus / sphere explicit | `PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere` | native expected 已采集，torus radius 为 0 的 source-backed 行为已记录 | S6 决定 supported 或继续 diagnostic |
| point curve | `PointCurve` | native expected 已采集，但 FreeCAD 源码仍是 TODO-like plane-of-curve 语义 | S6 只能在产品接受后发布，否则保留 diagnostic |
| default / TODO boundary | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` | diagnostic expected 已采集，标记 `DTE-NG-003` | 不发布；除非后续产品决策重开 |

## 验收

```bash
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 cad-core/tools/collect_freecad_expected.py --phase c3m6 --check --skip-unsupported
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
cd cad-core && python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
git diff --check -- cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

S5 采集阶段已用真实 FreeCADCmd 写入 expected；阶段验证结果以本轮提交前命令输出为准。若后续机器 FreeCADCmd / OCCT 环境不匹配，只能记录为兼容性探测，不能替代正式 oracle。

## 验证结果

- `cd cad-core && cmake --build build`：通过。
- `cd cad-core && FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py --phase c3m6 --check --skip-unsupported`：通过，`processed=50 skipped=5 failed=0`。
- `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest -k distance_type`：通过，`Ran 3 tests`。
- `cd cad-core && python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest`：通过，`Ran 1 test`，`skipped=27` 为 checked-in `known_gap` expected 的 intentional skip。
- `git diff --check -- cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线`：通过。
- `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown`：队列只剩 S6。

## 非目标

- 不用 representative fallback 输出当 native golden。
- 不把无法采集的 curve/default case 当 supported。
