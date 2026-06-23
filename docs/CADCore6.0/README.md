# CADCore6.0

CADCore6.0 承接 C5.1 / C51X freeze 后仍有产品价值、但不能再写成 FreeCAD parity 的扩展项。当前已完成 PartDesign Pipe `Transformation=Linear/S-shape` 与 selected tangent expansion 的 C6-M1 product extension；下一步先恢复阶段回归可信度，再继续新增产品合同。

## 入口

- C6-M1 总入口：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/6-23-19-42-C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线总入口.md`
- C6-M1 工作步骤：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分/`
- C6-M1 矩阵：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/矩阵/`
- C6-M2 总入口：`C6-M2-ExpectedFixtureRegressionRecovery主线/6-23-22-34-C6-M2-ExpectedFixtureRegressionRecovery主线总入口.md`
- C6-M2 工作步骤：`C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分/`
- C6-M2 矩阵：`C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/`

## 当前状态

- C6-M1 已实现并发布为 CAD Core product extension，不声明 FreeCAD parity 或 full PartDesign Pipe coverage。
- C6-M1 focused 验收通过：P7 feature + adapter capability 共 144 tests OK。
- C6-M1 阶段回归未全绿：`python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters` 为 178 tests、15 failures、28 skipped；失败集中在 `tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results`，涉及既有 expected fixture 的 bbox、diagnostic_codes、external_geometry_count、link 类型和 assembly solver_adapter 字段漂移。
- C6-M2 是 expected fixture regression recovery 主线，目标是恢复阶段回归闸门可信度；不是新的几何能力包。
- Pipe `Transformation=Interpolation` 的 `LawSamples` 几何合同仍是后续产品规格，不纳入 C6-M2。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```
