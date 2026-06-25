# 【已实现】C7-M1 S4 fixtures tests capability 发布

## 目标

把 S3 结果发布到 fixtures、focused tests、adapter capability、矩阵和 README。旧 pending rows 必须在 S4 结束时有明确去向，不能继续以 active gap 形式漂浮。

## S4 执行基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d4574e4b92`（`d4574e4b92 文档：完成 C7-M1 S3 发布收口`），`git status --short -uall` 无输出。
- 队列状态：S4 执行前 C7-M1 队列为 S4-S5 pending；本文件标记为 `【已实现】` 后，队列应推进到 S5。
- 本步遵守 S2/S3 no-code 结论：未修改 C++、fixtures、expected 或 tests，未采集 oracle，未运行 full regression 或 cmake build。

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

## 发布同步结论

- `part_design.hole` capability、adapter assertions、focused tests、fixtures/docs 已对齐：`model_thread.status=done_first_slice`、`model_thread.geometry=pipe_shell`、`history.status=element_map_freeze_first_slice`、`history.remaining=[]`、`native_oracle_known_gap_fixtures=[]`、`remaining_gaps=[]`。
- `native_oracle_fixtures` 保持 `p7/hole-supported-threaded-dynamic-iso2009`、`p7/hole-supported-threaded-dynamic-din7984`、`p7/hole-supported-model-thread-metric`、`p7/hole-point-profile`、`p7/hole-supported-point-counterbore`、`p7/hole-supported-model-thread-counterbore`。
- expected-backed rows 的 expected JSON 明确写有 `FreeCADCmd oracle from ...`、`freecad_version=1.2.0 revision 20260519`、topology/volume，不是从当前 `cad-core` 输出倒推。
- legacy pending rows 的 expected JSON 仍写 `hole_thread_geometry_oracle_pending` 和不要从 current cad-core output 冻结几何；S4 保持其 `historical_or_native_blocked` / historical non-active diagnostic 结论，不采集 oracle、不迁移 fixtures。
- `C7M1-BLK-401` capability/docs drift blocker 已关闭；队列推进到 S5 release gate。

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

## S4 验收结果

- Focused unittest：5 tests OK。
- TSV shape、文档尾随空白、queue、`git diff --check` 在本步完成后作为收口验证执行。
