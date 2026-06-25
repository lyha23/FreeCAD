# 【已实现】C7-M1 S0 live 基线与 Hole 残余 gap 冻结

## 目标

冻结 C7-M1 live 起点，确认 C6-M1 到 C6-M9 队列状态，记录 `part_design.hole` capability、Hole focused tests、legacy pending expected rows 和当前工作区边界。S0 是文档/矩阵步骤，不改 C++、fixtures、expected 或 tests。

## 必读

- `docs/CADCore7.0/README.md`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/README.md`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口方案.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 动作

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 运行 C6-M1 到 C6-M9 和 C7-M1 `工作步骤细分` 的 `step_goal_queue.py`。
3. grep 当前 `part_design.hole` 的 `model_thread`、`history.covered`、`native_oracle_fixtures`、`remaining_gaps` 和 adapter assertions。
4. grep `cad-core/fixtures/p7/expected/hole-threaded-standard-*` 中仍写 oracle pending 的 rows。
5. 更新 README、总入口和矩阵中的 S0 baseline 行。

## 冻结结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1624050685`，`git log -1 --oneline` 为 `1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`；`git status --short -uall` 无输出。
- 队列状态：C6-M1 到 C6-M9 package 的 `工作步骤细分` 队列均为空；C7-M1 在 S0 执行前为 S0-S5 pending，本文件标记后队列应推进到 S1。
- capability baseline：`cad-core/src/runtime/capability_contract.cpp` 中 `part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`，`history.status=element_map_freeze_first_slice`，`history.covered` 包含 `profile_source_tool_face_mapper_history`、`point_profile_head_cut_history`、`model_thread_compound_tool_shape`、`threaded_model_thread_head_cut_native_oracle`，`history.remaining=[]`、`remaining_gaps=[]`。
- test baseline：`cad-core/tests/test_adapters.py` 断言 Hole capability fields、producer matrix、topo history status 和 `hole_threaded_model_thread_profile_head_oracle_matrix` 不在 known gaps；`cad-core/tests/test_p7_features.py` 覆盖 supported threaded heads、ModelThread metric、ModelThread counterbore 和 native oracle matrix。
- legacy expected baseline：`cad-core/fixtures/p7/expected/hole-threaded-standard-counterbore.freecad.json` 与 `hole-threaded-standard-countersink.freecad.json` 仍写 `FreeCAD Hole threaded-standard oracle pending`，`known_gap.kind=hole_thread_geometry_oracle_pending`；S0 只冻结该事实，不裁决 S1/S2 rows。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
rg -n 'part_design.*hole|model_thread|hole_threaded_model_thread|hole-supported-model-thread|hole-supported-threaded|oracle pending' cad-core/src/runtime/capability_contract.cpp cad-core/tests cad-core/fixtures/p7/expected docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- live baseline 写入本步骤、README / 总入口和矩阵。
- S0 文件名和标题标记为 `【已实现】` 后，队列推进到 S1。
