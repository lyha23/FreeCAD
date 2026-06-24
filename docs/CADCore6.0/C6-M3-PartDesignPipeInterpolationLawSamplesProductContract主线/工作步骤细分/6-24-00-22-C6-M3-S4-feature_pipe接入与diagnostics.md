# C6-M3 S4 feature_pipe 接入与 diagnostics

## 目标

把 S3 低层 law 接入 `cad-core/src/part_design/feature_pipe.cpp::resolvePipeLaw()` 和 `executePipeFeature()`，让合法 Interpolation 请求执行，非法 `LawSamples` 输出稳定 diagnostics。

## 必读输入

- S0-S3 已实现文档
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p7_features.py`
- `矩阵/c6m3_pipe_interpolation_law_input_contract_matrix.tsv`

## 实施内容

1. 解析 `LawSamples`，生成 request-local law DTO。
2. 合法请求输出 `pipe_law.kind=Interpolation` 和 product contract metadata。
3. 非法请求输出 S1 冻结的 diagnostics code 和 property path。
4. 删除或收窄 C6-M1 Interpolation `product_contract_required` path，禁止 fallback。
5. 更新矩阵和 S4 文档，完成后改名为 `6-24-00-22-【已实现】C6-M3-S4-feature_pipe接入与diagnostics.md`。

## 非目标

- 不改 C6-M2 expected recovery 行。
- 不把 invalid `LawSamples` 变成 `unsupported_property` 泛化诊断。
- 不引入跨请求缓存。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```
