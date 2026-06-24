# CADCore6.0

CADCore6.0 承接 C5.1 / C51X freeze 后仍有产品价值、但不能再写成 FreeCAD parity 的扩展项。当前已完成 PartDesign Pipe `Transformation=Linear/S-shape` 与 selected tangent expansion 的 C6-M1 product extension，已通过 C6-M2 恢复 expected fixture 阶段回归闸门可信度，并已把 Pipe `Transformation=Interpolation` / `LawSamples` 作为 C6-M3 CAD Core product contract 发布；C6-M4 已通过 Part Workbench Sweep located profile / combined PipeShell product contract 的 S6 发布闸门。C6-M5 已实现并发布 Part Workbench Filling Surface / SupportOrder / Param product contract：S0/S1/S2 完成 live 基线、源码依据和 implementation-ready 合同路由，S3/S4 完成 Surface / SupportOrder、ExplicitParams 与 non-boundary support/order 产品合同实现，S5 完成 fixtures / tests / capability / docs 发布，S6 完成阶段回归与 heavy release gate。上述主线仍不声明 FreeCAD parity。

## 入口

- C6-M1 总入口：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/6-23-19-42-C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线总入口.md`
- C6-M1 工作步骤：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分/`
- C6-M1 矩阵：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/矩阵/`
- C6-M2 总入口：`C6-M2-ExpectedFixtureRegressionRecovery主线/6-23-22-34-C6-M2-ExpectedFixtureRegressionRecovery主线总入口.md`
- C6-M2 工作步骤：`C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分/`
- C6-M2 矩阵：`C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/`
- C6-M3 总入口：`C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/6-24-00-16-C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线总入口.md`
- C6-M3 工作步骤：`C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分/`
- C6-M3 矩阵：`C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/`
- C6-M4 总入口：`C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/6-24-14-00-C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线总入口.md`
- C6-M4 工作步骤：`C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分/`
- C6-M4 矩阵：`C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/`
- C6-M5 总入口：`C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/6-24-16-18-C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线总入口.md`
- C6-M5 工作步骤：`C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分/`
- C6-M5 矩阵：`C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/矩阵/`

## 当前状态

- C6-M1 已实现并发布为 CAD Core product extension，不声明 FreeCAD parity 或 full PartDesign Pipe coverage。
- C6-M1 focused 验收通过：P7 feature + adapter capability 共 144 tests OK。
- C6-M2 已实现并发布：focused expected fixture gate `Ran 1 test in 49.586s`，`OK (skipped=29)`；阶段回归 `Ran 180 tests in 80.086s`，`OK (skipped=29)`；heavy 收口 `Ran 216 tests in 88.451s`，`OK (skipped=29)`。
- C6-M2 关闭时仅保留 `ORC-013` 为本地 OCCT 7.9.3 imported LinkGroup bbox known environment gap，通过 fixture-local `bbox_delta=0.028` 容纳；不声明 full FreeCAD parity。
- C6-M3 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 182 tests in 65.996s`，`OK (skipped=29)`；heavy 收口 `Ran 218 tests in 74.459s`，`OK (skipped=29)`。该发布只覆盖 request-local `LawSamples` 产品合同，不声明 FreeCAD parity 或 full PartDesign Pipe coverage。
- C6-M4 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 241 tests in 65.269s`，`OK (skipped=29)`；heavy 收口 `Ran 277 tests in 71.034s`，`OK (skipped=29)`。`part_workbench.sweep` 不声明 FreeCAD parity，不扩大 full Part surface family；两个 FreeCADCmd Location overload blocker 已从 `remaining_gaps` 移除并保留在 `narrowed_gaps` 作为 historical wrapper evidence。
- C6-M5 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 248 tests in 64.311s`，`OK (skipped=29)`；heavy 收口 `Ran 284 tests in 72.526s`，`OK (skipped=29)`。`part_workbench.filling.status=supported_expected_backed_plus_c6m5_product_contract_non_parity`，`remaining_gaps=[]`；native helper crash/timeout/notCollected 证据保留在 `narrowed_gaps` / historical evidence 中，不声明 FreeCAD parity，不扩大 full Part surface family。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
```
