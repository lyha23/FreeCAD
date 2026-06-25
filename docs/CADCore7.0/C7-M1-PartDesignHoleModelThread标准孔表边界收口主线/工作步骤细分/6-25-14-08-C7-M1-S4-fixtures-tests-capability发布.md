# C7-M1 S4 fixtures tests capability 发布

## 目标

把 S3 结果发布到 fixtures、focused tests、adapter capability、矩阵和 README。旧 pending rows 必须在 S4 结束时有明确去向，不能继续以 active gap 形式漂浮。

## 必读

- S2/S3 已实现步骤文件
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/p7/hole-*.json`
- `cad-core/fixtures/p7/expected/hole-*.freecad.json`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv`

## 动作

1. 同步 `part_design.hole` status、model_thread、history covered/remaining、native_oracle_fixtures、remaining_gaps。
2. 同步 adapter assertion，禁止 docs 和 capability 口径分裂。
3. 对 expected-backed rows，确认 expected 文件不是从 cad-core 输出倒推。
4. 对 legacy pending rows，写清补 oracle、迁移、historical 或 non-active 结论。
5. 更新 root README、主线 README、总入口、方案和矩阵状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_supported_threaded_heads_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_model_thread_builds_freecad_pipe_shell_tool \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_thread_table_model_thread_contract_uses_native_oracles \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_threaded_model_thread_head_cut_oracle_matrix_matches_native \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- capability、tests、fixtures/docs 口径一致。
- `remaining_gaps`、legacy pending rows 和 non-goals 状态无冲突。
- S4 文件名和标题标记为 `【已实现】` 后，队列推进到 S5。
