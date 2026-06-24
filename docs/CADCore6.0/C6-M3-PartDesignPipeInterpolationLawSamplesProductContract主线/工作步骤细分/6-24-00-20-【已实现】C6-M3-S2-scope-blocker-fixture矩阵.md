# 【已实现】C6-M3 S2 scope blocker fixture 矩阵

## 完成结论

- S2 已把 S1 冻结的 request-local `LawSamples` 合同拆成可执行 scope、blocker、oracle、validation 和 non-goal 行。
- 成功路径拆成 additive 与 subtractive 两条 fixture / focused test 路线；subtractive 行显式挂到 PipeShell history 与 Body replay 风险，S5 若不能证明 replay 稳定，S6 必须继续保留 blocker，不得发布 capability 覆盖。
- invalid `LawSamples` 拆成 missing、malformed、domain / 单调 / endpoint、scale 四类 diagnostics fixture / test 行，均要求 `invalid_pipe_law_samples` 或 `missing_pipe_law_samples` 且无 fallback shape。
- capability / adapter publication、Interpolation law kernel、PipeShell history / Body replay 风险都已有独立 scope、blocker、oracle 和 validation 行。
- S2 只更新 C6-M3 文档和矩阵；没有修改 C++、fixtures、capability、C6-M1 关闭行，也没有改变 S1 DTO shape。

## 目标

把 S1 合同拆成可执行 scope、blocker、fixture、validation 和 non-goal 行。S2 的交付物是矩阵，不做代码实现。

## 必读输入

- S0/S1 已实现文档
- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`
- C6-M1 law / tangent fixture 和 tests

## 已更新矩阵

- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_non_goal_registry.tsv`
- `矩阵/c6m3_pipe_interpolation_law_backend_gap_classification.tsv`
- `矩阵/c6m3_pipe_interpolation_law_input_contract_matrix.tsv`

## 非目标

- 不新增或编辑 fixture。
- 不写 C++。
- 不发布 capability。
- 不把 C6-M1 closed rows 重新标为 pending。
- 不改变 S1 `LawSamples` DTO shape。
- 不允许 Linear / S-shape fallback。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'C6M3-BLK-|C6M3-ORC-|C6M3-SCOPE-|subtractive|invalid_pipe_law_samples|capability|PipeShell|Body replay' docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵 docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

- `rg`：命中 S2 所需 `C6M3-BLK-*`、`C6M3-ORC-*`、`C6M3-SCOPE-*`、subtractive、`invalid_pipe_law_samples`、capability、PipeShell 和 Body replay 行。
- TSV field-count check：无输出。
- `git diff --check -- docs/CADCore6.0`：无输出。
- 队列：S2 已跳过，队列头前进到 S3。
