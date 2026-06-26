# C8-M1-S6 release gate

## 目标

完成 C8-M1 发布闸门：队列、矩阵、docs、build、focused tests、必要阶段回归和最终状态同步。S6 只消费已经明确的 `supported`、`backend_gap_requires_implementation`、`oracle_blocked`、`oracle_blocker` 和 `diagnostic_non_goal` row，不新增业务范围。

## release audit

必须确认：

- S0-S5 均已按验收标准完成并重命名为 `【已实现】`。
- `step_goal_queue.py` 对本目录输出空 pending 表。
- `c8m1_shapebinder_backend_gap_classification.tsv` 无未处理 `backend_gap_requires_implementation`。
- 所有 `oracle_blocked` 都有 `delete_condition`。
- `capability_contract.cpp` 与 tests 对 supported / remaining_gaps 一致。
- 工作区没有生成物、build 目录或 `__pycache__` 混入。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0 cad-core/src cad-core/include cad-core/tests
git diff --check
```

实现回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_diagnostics
```

阶段回归条件：

- 修改 topo / reference / ElementMap 共享逻辑时，运行 `python3 -m unittest tests.test_p6_topology tests.test_p7_features tests.test_p8_features`。
- 修改 Body replay 或 profile resolver 时，运行 `python3 -m unittest tests.test_mvp tests.test_feature_flows tests.test_p7_features`。
- 修改 capability schema 时，运行 capability smoke 并检查 adapter assertions。

## 下一轮代码落点

如果 S6 仍发现未关闭项，必须写成下一轮具体代码任务，而不是只留一句“后续处理”：

| 未关闭类型 | 下一轮落点 | 禁止做法 |
| --- | --- | --- |
| ShapeBinder expected mismatch | `feature_shape_binder.cpp` + focused fixture | fixture-name 分支 |
| SubShapeBinder MakeFace / Offset mismatch | `feature_shape_binder.cpp` + `part/topo_shape_expansion.cpp` reuse | output-side pruning |
| ElementMap mismatch | `part/property_topo_shape.cpp` / executor history metadata | 用 bbox / area 猜 source |
| CopyOnChange lifecycle blocked | `app/copy_on_change.cpp` 或 diagnostic publication | backend session / temporary shape cache |

## 验收

S6 完成后必须：

- 重命名本文件为 `6-26-16-22-【已实现】C8-M1-S6-release-gate.md`。
- 更新 `docs/CADCore8.0/README.md`、本包 README、总入口和方案状态。
- 运行 `git status --short -uall`，确认只有本轮相关变更或工作区干净。

## 非目标

- 不开新 feature scope。
- 不执行全量 FreeCAD build。
- 不自动提交，除非用户明确要求或 goal 任务要求提交。
