# C6-M3 S1 LawSamples DTO 合同冻结

## 目标

冻结 `Transformation=Interpolation` 的 request / response / diagnostics 合同。S1 只写合同和矩阵，不写 C++。

## 必读输入

- S0 已实现文档
- `矩阵/c6m3_pipe_interpolation_law_input_contract_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_non_goal_registry.tsv`
- `cad-core/fixtures/c6m1/partdesign-pipe-interpolation-law-boundary.json`
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`

## 实施内容

1. 明确 `LawSamples` schema：sample 形态、参数域、scale 约束、端点要求。
2. 明确 response metadata：`pipe_law.kind`、`source`、`contract`、`domain`、`samples`。
3. 明确 diagnostics code：missing / invalid shape / nonfinite / nonmonotonic / nonpositive scale。
4. 明确旧 C6-M1 boundary fixture 的迁移策略：改为成功 fixture、保留为 legacy boundary，或新增 C6-M3 fixture 替代。
5. 更新 input contract、oracle fixture、non-goal 和 validation 矩阵。
6. 完成后将本文件改名为 `6-24-00-19-【已实现】C6-M3-S1-LawSamplesDTO合同冻结.md`。

## 非目标

- 不实现 OCCT law。
- 不改 capability。
- 不把 `LawSamples` 设计成前端长期状态。
- 不允许 implicit Linear / S-shape fallback。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'LawSamples|cad_core_product_contract|invalid_pipe_law_samples|Interpolation' docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵
git diff --check -- docs/CADCore6.0
```
