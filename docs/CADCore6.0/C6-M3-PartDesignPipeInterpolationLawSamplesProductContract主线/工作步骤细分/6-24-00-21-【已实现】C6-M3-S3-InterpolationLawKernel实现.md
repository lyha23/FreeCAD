# 【已实现】C6-M3 S3 InterpolationLawKernel 实现

## 完成结论

- S3 已在 `part/topo_shape_expansion` 层实现可复用 Interpolation law kernel，不触碰 `feature_pipe.cpp` 的 `product_contract_required` 边界。
- `PipeScalingLawKind` 新增 `Interpolation`，`PipeScalingLaw` 新增 request-local `samples=[{parameter,scale},...]`，对齐 S1/S2 冻结的 `[parameter, scale]`、domain `[0,1]`、no repair / no fallback 合同。
- `makePipeScalingLaw()` 使用 OCCT `Law_Interpol` + `TColgp_Array1OfPnt2d` 构造参数/scale 样本 law；无效 samples 返回 `NamedShapeBuild.error`，不会退回 Linear、S-shape 或 no-law `Add()` 路径。
- 新增 `cad-core-c6m3-pipe-interpolation-law-probe`，直接调用 `makeElementPipeShellFromSources()`。同一 spine/profile 下，无 law bbox Y/Z 宽度为 `1.0`，Interpolation samples `[(0,1),(0.5,2),(1,1)]` 的 bbox Y/Z 宽度为 `2.0`，并保留 `part_sweep:pipeshell_history`。
- S3 只更新低层 kernel、probe、P7 focused test 和 C6-M3 文档 / 矩阵；未解析 JSON `LawSamples`，未改变 executor 输出、fixture、adapter 或 capability。

## Authority

- FreeCAD authority 仍是 enum-only：`src/Mod/PartDesign/App/FeaturePipe.cpp::TransformEnums` 暴露 `Interpolation`，但 `Pipe::execute()` 没有 active Interpolation law 分支。
- CAD Core product contract authority 来自 S1/S2：`LawSamples` 是 request-local DTO，sample 形态为 `[parameter, scale]`，参数严格递增并覆盖 `[0,1]`，scale 有限且为正。
- OCCT 依据：`Law_Interpol.hxx::Set(TColgp_Array1OfPnt2d)` 把 2D 点的 X 作为 parameter、Y 作为 function value，并要求 X 坐标升序。

## 更新产物

- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/c6m3_pipe_interpolation_law_probe.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/CMakeLists.txt`
- `矩阵/c6m3_pipe_interpolation_law_source_candidates.tsv`
- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_backend_gap_classification.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`

## 非目标保持

- 不解析 JSON `LawSamples`，S4 继续负责 `feature_pipe.cpp` DTO / diagnostics 接入。
- 不修改 `resolvePipeLaw()` 的 Interpolation `product_contract_required` 边界。
- 不新增或修改 fixtures。
- 不修改 adapter、capability、GUI/editor。
- 不把 Interpolation fallback 到 Linear / S-shape。

## 验收结果

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
./build/cad-core-c6m3-pipe-interpolation-law-probe
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

- `cmake --build build`：通过，新增 `cad-core-c6m3-pipe-interpolation-law-probe` 已构建。
- 直接 probe：通过，Interpolation bbox Y/Z 宽度从 `1.0` 增至 `2.0`，history status 保留 `part_sweep:pipeshell_history`。
- `python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest`：通过，`Ran 144 tests`。
- `git diff --check -- docs/CADCore6.0 cad-core`：通过，无输出。
- TSV field-count check：通过，无输出。
- C6-M3 队列刷新：S3 已跳过，队列头前进到 S4 `6-24-00-22-C6-M3-S4-feature_pipe接入与diagnostics.md`。
