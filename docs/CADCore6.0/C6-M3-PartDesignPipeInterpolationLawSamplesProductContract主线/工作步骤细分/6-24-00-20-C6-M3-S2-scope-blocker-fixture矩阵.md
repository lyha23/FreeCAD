# C6-M3 S2 scope blocker fixture 矩阵

## 目标

把 S1 合同拆成可执行 scope、blocker、fixture、validation 和 non-goal 行。S2 的交付物是矩阵，不做代码实现。

## 必读输入

- S0/S1 已实现文档
- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`
- C6-M1 law / tangent fixture 和 tests

## 实施内容

1. 为成功 additive Interpolation law 建 fixture / test row。
2. 为 subtractive Interpolation law 建 fixture / test row，若风险较高必须写 S5/S6 删除条件。
3. 为非法 `LawSamples` 建 diagnostics row。
4. 为 capability / adapter publication 建 release row。
5. 为 OCCT law kernel 或 history propagation 风险建 blocker row。
6. 完成后将本文件改名为 `6-24-00-20-【已实现】C6-M3-S2-scope-blocker-fixture矩阵.md`。

## 非目标

- 不新增未批准 fixture。
- 不把 capability 发布提前到 implementation 前。
- 不把 C6-M1 closed rows 重新标为 pending。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'C6M3-BLK-|C6M3-ORC-|C6M3-SCOPE-' docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
