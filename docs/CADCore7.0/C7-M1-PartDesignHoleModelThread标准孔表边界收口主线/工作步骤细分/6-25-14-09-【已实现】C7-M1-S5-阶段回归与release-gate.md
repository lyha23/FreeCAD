# 【已实现】C7-M1 S5 阶段回归与 release gate

## 目标

完成 C7-M1 release gate：构建和 focused tests 通过，必要阶段回归通过，队列为空，README / 总入口 / 矩阵记录最终状态。

## 必读

- S0-S4 已实现步骤文件
- `docs/CADCore7.0/README.md`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/README.md`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口主线总入口.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 动作

1. 记录 S5 起点 `HEAD` 和 `git status`。
2. 运行 S4 指定 focused tests；若 S3 改 C++，运行 `cmake --build build`。
3. 若触发 expected/topo/history/adapter schema 改动，运行阶段回归或重型收口。
4. 更新 C7.0 README、主线 README、总入口、方案和矩阵为最终发布状态。
5. 把 S5 文件名和标题标记为 `【已实现】`，确认队列为空。

## S5 执行基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=dd2b919e46`（`dd2b919e46 文档：完成 C7-M1 S4 发布同步`），`git status --short -uall` 无输出。
- S4 结论：focused unittest 5 tests OK；没有代码、fixtures、expected 或 tests 改动。
- 本步仍是 no-code release closure：只更新 C7.0 README、主线 README、总入口、方案、矩阵和本步骤文件，不采集 oracle，不改 C++、fixtures、expected 或 tests，不提交 build 生成物。

## Release Gate 结果

- Build：`cd cad-core && cmake --build build` 通过。
- Focused unittest：Hole 4 个 + adapter 1 个，共 5 tests OK。
- Heavy regression：未运行。S5 没有 expected/topo/history/adapter schema 广泛变化，也没有代码、fixtures、expected 或 test 改动。
- 最终发布状态：C7-M1 队列为空；`part_design.hole.remaining_gaps=[]`、`history.remaining=[]`、`native_oracle_known_gap_fixtures=[]` 保持发布口径；legacy pending rows 保持 historical/non-active diagnostic，不作为 active backend gap 重开。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_supported_threaded_heads_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_model_thread_builds_freecad_pipe_shell_tool \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_thread_table_model_thread_contract_uses_native_oracles \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_threaded_model_thread_head_cut_oracle_matrix_matches_native \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- C7-M1 队列为空。
- Hole ModelThread / 标准孔表发布状态与 tests/capability/docs 一致。
- release gate 记录写入 README、总入口和矩阵。
