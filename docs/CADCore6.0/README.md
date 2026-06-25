# CADCore6.0

CADCore6.0 承接 C5.1 / C51X freeze 后仍有产品价值、但不能再写成 FreeCAD parity 的扩展项。当前已完成 PartDesign Pipe `Transformation=Linear/S-shape` 与 selected tangent expansion 的 C6-M1 product extension，已通过 C6-M2 恢复 expected fixture 阶段回归闸门可信度，并已把 Pipe `Transformation=Interpolation` / `LawSamples` 作为 C6-M3 CAD Core product contract 发布；C6-M4 已通过 Part Workbench Sweep located profile / combined PipeShell product contract 的 S6 发布闸门。C6-M5 已实现并发布 Part Workbench Filling Surface / SupportOrder / Param product contract。C6-M6 已实现并发布 Part Workbench GeomPlateSurface remaining gap product contract，`part_workbench.geomplate.remaining_gaps=[]`，旧 G1 / ProjectedCurve2d / criteria / PlateSurface wrapper 边界保留在 `narrowed_gaps`、`non_goals` 和 historical evidence 中。C6-M7 已发布 Part Workbench Loft selected subelement request-local product contract，`part_workbench.loft.remaining_gaps=[]`，原 native-hidden diagnostic evidence 保留在 `narrowed_gaps` / historical evidence 中。C6-M8 已完成 S0/S1/S2/S3/S4/S5 并通过 Part Workbench Surface Family Published Contract Closure release gate：`ProjectOnSurface` active/non-goal overlap 已收口为 `remaining_gaps=[]`、GUI/session non-goal、native mapper hidden `narrowed_gaps` historical evidence，并继续保护 RuledSurface / Loft / Sweep / Filling / GeomPlate 的 expected-backed、product-contract-non-parity、historical evidence、non-goal 和 adapter capability 口径。C6-M9 已完成 S4 publication verification：`part_design.revolution_groove.remaining_gaps=[]`，`exact_blockers={}`，`partdesign_groove_upto_brepfeat_cut_native_failure` 保留在 `narrowed_gaps` / historical native evidence；S5 仍负责 release gate。上述主线仍不声明 FreeCAD parity。

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
- C6-M6 总入口：`C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/6-24-19-52-C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线总入口.md`
- C6-M6 工作步骤：`C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/工作步骤细分/`
- C6-M6 矩阵：`C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/`
- C6-M7 总入口：`C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/6-25-00-53-C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线总入口.md`
- C6-M7 工作步骤：`C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分/`
- C6-M7 矩阵：`C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/`
- C6-M8 总入口：`C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线总入口.md`
- C6-M8 工作步骤：`C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分/`
- C6-M8 矩阵：`C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/`
- C6-M9 总入口：`C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线总入口.md`
- C6-M9 工作步骤：`C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分/`
- C6-M9 矩阵：`C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/`

## 当前状态

- C6-M1 已实现并发布为 CAD Core product extension，不声明 FreeCAD parity 或 full PartDesign Pipe coverage。
- C6-M1 focused 验收通过：P7 feature + adapter capability 共 144 tests OK。
- C6-M2 已实现并发布：focused expected fixture gate `Ran 1 test in 49.586s`，`OK (skipped=29)`；阶段回归 `Ran 180 tests in 80.086s`，`OK (skipped=29)`；heavy 收口 `Ran 216 tests in 88.451s`，`OK (skipped=29)`。
- C6-M2 关闭时仅保留 `ORC-013` 为本地 OCCT 7.9.3 imported LinkGroup bbox known environment gap，通过 fixture-local `bbox_delta=0.028` 容纳；不声明 full FreeCAD parity。
- C6-M3 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 182 tests in 65.996s`，`OK (skipped=29)`；heavy 收口 `Ran 218 tests in 74.459s`，`OK (skipped=29)`。该发布只覆盖 request-local `LawSamples` 产品合同，不声明 FreeCAD parity 或 full PartDesign Pipe coverage。
- C6-M4 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 241 tests in 65.269s`，`OK (skipped=29)`；heavy 收口 `Ran 277 tests in 71.034s`，`OK (skipped=29)`。`part_workbench.sweep` 不声明 FreeCAD parity，不扩大 full Part surface family；两个 FreeCADCmd Location overload blocker 已从 `remaining_gaps` 移除并保留在 `narrowed_gaps` 作为 historical wrapper evidence。
- C6-M5 已实现并发布为 CAD Core product contract：`cmake --build build` 通过；阶段回归 `Ran 248 tests in 64.311s`，`OK (skipped=29)`；heavy 收口 `Ran 284 tests in 72.526s`，`OK (skipped=29)`。`part_workbench.filling.status=supported_expected_backed_plus_c6m5_product_contract_non_parity`，`remaining_gaps=[]`；native helper crash/timeout/notCollected 证据保留在 `narrowed_gaps` / historical evidence 中，不声明 FreeCAD parity，不扩大 full Part surface family。
- C6-M6 已实现并发布为 CAD Core product contract non-parity：`cmake --build build` 通过；阶段回归 `Ran 251 tests in 136.165s`，`OK (skipped=31)`；heavy 收口 `Ran 287 tests in 126.397s`，`OK (skipped=31)`。`part_workbench.geomplate.status=supported_expected_backed_projected_initial_surface_plus_c6m6_product_contract_non_parity`，`remaining_gaps=[]`；`nativeOracleBlocked` / `diagnosticOnly` / `nonGoal` 证据保留在 `narrowed_gaps` / `non_goals` / historical evidence 中。本包仍保持 source-backed helper 口径，不声明 GUI GeomPlate、native `Part::GeomPlate` DocumentObject、persistent `PlateSurface` wrapper、FreeCAD parity 或 full Part surface family。
- C6-M7 已实现并发布为 CAD Core request-local product contract non-parity：`cmake --build build` 通过；阶段回归 `Ran 254 tests in 85.216s`，`OK (skipped=31)`。`part_workbench.loft.status=supported_profile_linearize_complex_expected_backed_plus_c6m7_product_contract_non_parity`，`remaining_gaps=[]`；C5-M12 native-hidden diagnostic expected 继续作为 `narrowed_gaps` / historical evidence。C6-M7 不重开 C5-M12 已关闭的 complex profile family，不声明 `PartDesign::Loft`、GUI、FreeCAD parity 或 full Part surface family。
- C6-M8 已实现并发布为 Part Workbench surface family published contract closure：S5 起点 `HEAD=25dc0ad331`（`25dc0ad331 文档：完成 C6-M8 S4 发布一致性收口`），开始时 `git status --short -uall` 为空；`cmake --build build` 通过；阶段回归 `Ran 254 tests in 87.456s`，`OK (skipped=31)`；C6-M8 队列为空，TSV 和 diff check 通过。S3/S4 已同步 `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 的发布口径，`part_workbench.project_on_surface.remaining_gaps=[]`，GUI/session 只保留为 non-goal，native mapper hidden 保留为 `narrowed_gaps` / request-local historical evidence；S3/S4 未改 `cad-core/src/part/*` executor，未新增或修改 fixtures / expected；因未修改 `topo_shape_expansion`、ElementMap/history、collector expected 或批量 expected 文件，本包未触发 topology / broader expected 重型收口。
- C6-M9 S0 已冻结 live baseline：`HEAD=17116567e4`（`17116567e4 文档：完成 C6-M8 S5 发布闸门`），C6-M1 到 C6-M8 队列为空，C6-M9 起点队列从 S0 到 S5 pending。S1 已复核 `FeatureGroove.cpp::Groove::execute()`、`FeatureRevolved.cpp` UpTo path、`TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()`、`cad-core/src/part/topo_shape_expansion.cpp::makeElementRevolutionUntilFromSources()`、`cad-core/tests/test_p7_features.py` 和 adapter capability assertion。S2 已在 `HEAD=b850d03f46` 上裁决 `Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 的唯一 route 均为 `historical_native_failure`：两个 focused P7 fixtures 当前断言 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`，不进入 `backend_gap_requires_implementation` 或 `cad_core_product_contract_non_parity`。S3 已在 `HEAD=5dd70f0ad5` 上完成 capability/test/docs publication assertion：`part_design.revolution_groove.status=supported_c51s1_advanced_with_historical_groove_upto_native_failure`，`remaining_gaps=[]`，`exact_blockers={}`，同一 id 保留在 `narrowed_gaps` / historical native evidence。S4 已在 `HEAD=65cb5c2369` 上完成发布一致性复核：capability、adapter assertion、C6-M9 矩阵和 root README 口径一致，未改 C++、fixtures、expected、collector 或 P7 failure fixture 语义；S5 继续做 release gate。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分 --format markdown
```
