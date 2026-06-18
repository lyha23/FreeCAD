# P8 DistanceTypeExtendedGeometry S5 NativeOracle 与代表 fixture 专项复审

## 目标

批量采集 FreeCADCmd native expected，证明 extended DistanceType 的 resolver evidence、ASMT class、scalar field 和 placement writeback 语义。S5 决定哪些 cases 可以进入 S6 supported 发布。

## 必须完成

- 批量 fixtures 至少覆盖 `LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`。
- 对 `PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`、`PointCurve` 至少采 representative oracle 或写明采集阻塞。
- 对 cone / line-surface / curve-face / `Other` 不得沉默：必须形成 expected、diagnostic-only、notCollected 或 nonGoal 结论。
- expected 中若出现 `known_gap`，必须写清 cad-core 落点和删除条件。

## 验收

```bash
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 cad-core/tools/collect_freecad_expected.py --phase c3m6 --check --skip-unsupported
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest
git diff --check -- cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

若本机 FreeCADCmd / OCCT 环境不匹配，只能记录为兼容性探测，不能替代正式 oracle。

## 非目标

- 不用 representative fallback 输出当 native golden。
- 不把无法采集的 curve/default case 当 supported。
