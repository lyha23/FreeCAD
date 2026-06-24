# 【已实现】C6-M3 S5 fixtures tests capability 发布

## 完成结论

- S5 已把 `Transformation=Interpolation` + request-local `LawSamples` 从临时 focused coverage 发布为正式 C6-M3 product-contract fixtures、expected gate、focused tests 和 capability / adapter assertions。
- 新增 additive product fixture `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-product.json`，断言 `pipe_law.kind=Interpolation`、`source/contract=cad_core_product_contract`、domain、normalized samples、`no_fallback=true`、bbox / topology 和 PipeShell history。
- 新增 subtractive product fixture `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-subtractive-product.json`，证明 `add_sub=sub`、`part_sweep:pipeshell_history`、`part_design_pipe:sewing` 和 Body `replayed_subtractive_features` 均稳定，因此 `C6M3-BLK-402` 在 S5 关闭。
- 新增 diagnostics fixture `cad-core/fixtures/c6m3/partdesign-pipe-invalid_pipe_law_samples-diagnostics.json`，覆盖 missing、object/scalar/missing-entry/extra-entry/null/string/too-few malformed、domain / monotonic / endpoint、scale zero/negative/non-number，均返回 `missing_pipe_law_samples` 或 `invalid_pipe_law_samples`，定位 `property=LawSamples`，无 shape、无 Linear / S-shape fallback。
- capability 只发布 CAD Core product extension：新增 `Transformation=Interpolation LawSamples product contract`、列出 c6m3 fixtures、暴露 missing / invalid diagnostics，并移除 `partdesign_pipe_interpolation_law_product_contract_required` exact blocker / remaining gap；没有声明 FreeCAD native parity 或 full PartDesign Pipe coverage。

## 更新产物

- `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-product.json`
- `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-subtractive-product.json`
- `cad-core/fixtures/c6m3/partdesign-pipe-invalid_pipe_law_samples-diagnostics.json`
- `cad-core/fixtures/c6m3/expected/*.freecad.json`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_backend_gap_classification.tsv`
- `矩阵/c6m3_pipe_interpolation_law_source_candidates.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`

## 非目标保持

- 不发布 full PartDesign Pipe coverage。
- 不声明 FreeCAD parity；expected 文件只是本仓库现有 gate 约定下的 CAD Core product-contract oracle。
- 不新增 LawSamples GUI / editor 合同。
- 不新增 LawSamples repair、sorting、clamping、endpoint injection、scale fallback。
- 不在 adapter 层写 LawSamples 业务逻辑。

## 验收结果

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

- `cmake --build build`：通过。
- `CadCoreP7FeatureTest` + adapter capability assertion：通过，`Ran 146 tests`。
- expected fixture gate：通过，`Ran 1 test`，历史 known-gap rows 仍为 skip。
- `git diff --check -- docs/CADCore6.0 cad-core`：通过，无输出。
- TSV field-count check：通过，无输出。
- 队列刷新：S5 已跳过，队列头前进到 S6。
