# P8 DistanceTypeExtendedGeometry S6 实现与发布闸门

## 目标

消费 S3-S5 的 DTO、mapping、oracle 和 blocker 结论，落 C++、fixtures、focused tests、C ABI capability、docs / TSV 发布。S6 完成后队列应为空。

## 必须完成

- cad-core 支持的 extended cases 必须来自 checked-in FreeCAD expected 和 focused tests。
- C ABI capability 增加 `distance_type_extended_geometry` 或等价字段，列出 supported cases、oracle count、radius evidence fields、remaining default / curve boundaries。
- upstream P8 Assembly / DistanceType docs 回写：basic、extended、default / curve、GUI/session 边界分开。
- `DTE-BLOCK-001..008` 均有 closed / notCollected / nonGoal / backendGap 结论。
- 已实现步骤文档按仓库规则改名为 `【已实现】`。

## 本轮短跑验收

```bash
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests cad-core/fixtures/c3m6 cad-core/src/adapters/c_api/c_api.cpp docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 阶段回归

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
cd /Users/li/Chili3DProject/FreeCAD
python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

## 非目标

- 不发布未采 oracle 的 default / curve cases。
- 不扩大到 GUI/session、persistent solver state 或完整 Assembly transaction。
- 不靠 fixture 名称、bbox、shape 数量或输出修正推断 FreeCAD DistanceType。
