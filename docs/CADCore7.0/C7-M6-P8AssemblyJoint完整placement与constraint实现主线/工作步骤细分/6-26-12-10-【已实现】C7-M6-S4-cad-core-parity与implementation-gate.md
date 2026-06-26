# 【已实现】C7-M6 S4 cad-core parity 与 implementation gate

## 目标

基于 S3 native oracle / blocker 结果，对当前 `cad-core` 做 parity 或 diagnostics 分类，裁决 S5 是否打开 C++ implementation gate。S4 默认不改 C++。

## 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=cd3c93b873`（`cd3c93b873 证据：完成 C7-M6 S3 native oracle 固化`），开始状态干净。
- `C7M6-ORACLE-202` 裁决为 `already_closed_expected_backed`：`cad-core` 对 `assembly-marker-custom-placement-chain-real-solver` 的 legacy recompute 与 S3 FreeCAD expected 匹配；去掉 S3 collector 预写的 `known_gap/backendGap` 元数据后，现有 expected comparator 通过。`documentObjectUpdates` placement update 最大误差约 `4.44e-16`，`reference1/reference2` connector 和 marker placement 最大误差分别约 `2.22e-16` / `1.11e-16`，`solver_adapter.status=solved`、`mode=real_ondsel_solver`、`joints=["FixedJoint"]`、`unsupported_joints=[]` 均一致。
- C API capability 发布已覆盖当前 S4 需要的能力：`assembly.ondsel_solver_adapter.available=true`，`subshape_marker_placement.status=covered_representative_subset` 且 `remaining_gaps=[]`，`placement_writeback.status=covered_full` 且 `remaining_gaps=[]`。因此 S3 expected 里的 marker `backendGap` 只保留为历史 collector 元数据，不作为 S4 backend gap。
- `C7M6-ORACLE-302` 裁决为 `oracle_blocked`：S3 只有 signed current value probe，没有 zero Angle fallback `solver_joint_class` / fallback evidence，不得从 blocked evidence 推导 backend gap。
- `C7M6-ORACLE-203` 裁决为 `oracle_blocked`：非 identity `offsetPlc` 仍缺 dedicated native `preDrag()` / bundled fixed lifecycle evidence，不得实现或发布 supported。
- S5 implementation gate 保持关闭。S5 必须是 no-code publication closure，允许修改文件仅限：`docs/CADCore7.0/README.md`、本包 `README.md`、本包方案、工作步骤总入口 / S5 / S6 文档，以及本包 `矩阵/*.tsv`。S5 不允许修改 `cad-core/src/assembly/*`、`cad-core/src/adapters/*`、`cad-core/tests/*`、`cad-core/fixtures/c3m6/*.json` 或 expected JSON。
- 本轮没有改 C++ runtime、adapter、tests、fixtures 或 expected，没有重新采集 FreeCAD expected，也没有把 current `cad-core` 输出写成 expected。

## 必读文件

- S3 完成后的 C7-M6 README、方案和矩阵。
- S3 新增或更新的 fixture / expected / known_gap。
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/src/assembly/assembly_object.cpp`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。
2. 若 S3 是 native oracle，运行 current `cad-core` 并比较 placement、solver DTO、diagnostics、documentObjectUpdates、capability publication。
3. 若 S3 是 blocker，确认 focused test 保持 blocker，不打开 implementation gate。
4. 写入 route：`already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。
5. 如果打开 implementation gate，列出 S5 允许修改的文件、FreeCAD 依据、non-goals 和 focused test 名称。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S5。

## 裁决规则

- FreeCAD native oracle 可证明 + current `cad-core` 匹配：`already_closed_expected_backed`。
- FreeCAD native oracle 可证明 + current `cad-core` 不匹配：`backend_gap_requires_implementation`。
- FreeCAD native 证据不足：`oracle_blocked`。
- FreeCAD native 明确不支持或超出无状态 CAD Core 边界：`diagnostic_non_goal`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S5 code gate 状态明确。
- 若打开 code gate，S5 范围足够窄且有 FreeCAD source authority。
- 队列推进到 S5。
