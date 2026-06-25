# C7-M1 S5 阶段回归与 release gate

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
