# 【已实现】C6-M3 S4 feature_pipe 接入与 diagnostics

## 完成结论

- S4 已把 S3 的 `PipeScalingLawKind::Interpolation` 接入 `cad-core/src/part_design/feature_pipe.cpp::resolvePipeLaw()`；合法 `Transformation=Interpolation` + `LawSamples` 不再返回 `product_contract_required`。
- `LawSamples` 解析严格遵守 S1 DTO：只接受 request-local array pairs `[parameter, scale]`，至少 2 个 sample，parameter 有限、位于 `[0,1]`、严格递增、首尾覆盖 `0.0/1.0`，scale 有限且为正。
- 合法请求输出 `pipe_law.kind=Interpolation`、`source/contract=cad_core_product_contract`、`domain=[0.0,1.0]`、normalized `samples` 和 `no_fallback=true`，并通过 PipeShell law 路径生成 shape。
- 缺失 `LawSamples` 返回 `missing_pipe_law_samples`，malformed/domain/scale 错误返回 `invalid_pipe_law_samples`；诊断均定位 `property=LawSamples`，`pipe_law.status=invalid`，不 fallback 到 Linear / S-shape。
- 本步未修改 `topo_shape_expansion`、fixture 文件、adapter、capability 或 GUI/editor；C6-M3 fixture 与 capability 发布仍由 S5 处理。

## 代码落点

- `cad-core/src/part_design/feature_pipe.cpp`
  - 新增 request-local `resolvePipeInterpolationSamples()`。
  - Interpolation 合法样本写入 `part::PipeScalingLawKind::Interpolation` 和 `samples`。
  - invalid/missing 分支写入 S1 稳定 diagnostics 与 no-fallback metadata。
- `cad-core/tests/test_p7_features.py`
  - 旧 C6-M1 boundary seed 改为合法 Interpolation product-contract 成功断言。
  - 新增 temp JSON 覆盖 missing / malformed / domain / scale diagnostics。
  - 保持 C6-M1 Linear / S-shape product tests 与 tangent diagnostics 路径继续通过。

## 矩阵状态

- `C6M3-SCOPE-301`、`C6M3-BLK-301`、`C6M3-ORC-301`、`C6M3-CAT-301` 已关闭为 `implemented_by_S4`。
- `C6M3-SCOPE-401/402/403` 仍保持 S5/S6：新增 C6-M3 fixtures、subtractive replay 证明和 capability / adapter 发布不在 S4 范围内。

## 非目标

- 不改 C6-M2 expected recovery 行。
- 不把 invalid `LawSamples` 变成 `unsupported_property` 泛化诊断。
- 不引入跨请求缓存。
- 不新增或编辑 fixture 文件。
- 不修改 capability / adapter。

## 验收结果

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

- `cmake --build build`：通过。
- `CadCoreP7FeatureTest`：通过，`Ran 146 tests`。
- `git diff --check -- docs/CADCore6.0 cad-core`：通过，无输出。
- TSV field-count check：通过，无输出。
- C6-M3 队列刷新：S4 已跳过，队列头前进到 S5。
