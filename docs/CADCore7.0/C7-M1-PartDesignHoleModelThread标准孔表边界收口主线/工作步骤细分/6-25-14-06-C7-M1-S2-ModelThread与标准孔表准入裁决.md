# C7-M1 S2 ModelThread 与标准孔表准入裁决

## 目标

基于 S0/S1 evidence，对每个 Hole representative 给出 route。S2 是代码闸门；没有 `backend_gap_requires_implementation` 结论，不得改 C++、fixtures、expected 或 tests。

## 必读

- S0/S1 已实现步骤文件
- `矩阵/c7m1_hole_modelthread_backend_gap_classification.tsv`
- `矩阵/c7m1_hole_modelthread_oracle_fixture_matrix.tsv`
- `矩阵/c7m1_hole_modelthread_scope_review_matrix.tsv`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/p7/expected/hole-*.freecad.json`

## 动作

1. 把每个 row 裁决为 `already_closed_expected_backed`、`publication_closure_only`、`oracle_pending_collect`、`backend_gap_requires_implementation`、`historical_or_native_blocked` 或 `non_goal`。
2. 对 legacy pending expected rows，决定补 FreeCAD oracle、迁移到 supported fixtures、保留 diagnostic/historical，或标为 non-active legacy。
3. 对 ModelThread + head cut row，明确是否存在 geometry/topology/history active gap；命名顺序差异不得算硬失败。
4. 如果进入实现，写清 S3 code落点、fixture rows、focused tests、capability/docs 更新项。
5. 如果不进入实现，写清 S3/S4 的 no-code publication closure 项。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'backend_gap_requires_implementation|oracle_pending_collect|already_closed_expected_backed|publication_closure_only|historical_or_native_blocked|non_goal' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- 每个 representative 都有明确 route 和下一步。
- S3 是否允许改代码被写清楚。
- S2 文件名和标题标记为 `【已实现】` 后，队列推进到 S3。
