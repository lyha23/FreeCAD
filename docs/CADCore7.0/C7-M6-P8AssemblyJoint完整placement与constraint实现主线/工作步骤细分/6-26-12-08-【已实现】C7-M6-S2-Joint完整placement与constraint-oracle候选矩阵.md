# 【已实现】C7-M6 S2 Joint 完整 placement 与 constraint oracle 候选矩阵

## 目标

把 S1 的 source / coverage 复核结果裁成最小完整语义批次，明确哪些进入 native oracle 采集，哪些保持 already covered，哪些是 diagnostic non-goal。S2 不采 oracle，不改 C++。

## 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9b37644b3e`（`9b37644b3e 文档：完成 C7-M6 S1 源码覆盖复核`），开始状态干净。
- 已保留 already-covered expected-backed baseline：grounded JointType matrix、DistanceType basic / extended expected、DTE-NG-003 default / TODO diagnostic boundary、34 个 `native_marker_oracle` marker expected、single / multi / partial / next-request `assembly_set_placement` writeback。
- S3 oracle candidates 已选定：`C7M6-ORACLE-202` planned fixture `assembly-marker-custom-placement-chain-real-solver`，覆盖非 identity `Placement1/2` connector 与 object-global 到 part-local marker chain；`C7M6-ORACLE-302` planned fixture `assembly-angle-zero-and-signed-current-real-solver`，覆盖 Angle zero fallback 与 placement-derived signed Distance/current value evidence。
- `C7M6-ORACLE-203` 保持 `oracle_blocker`：FreeCAD source 显示 bundled `offsetPlc` 同时影响 `handleOneSideOfJoint()` 和 `setNewPlacements()`，但当前 c3m6 fixtures/expected 只证明 identity offset boundary；S3 必须先证明 native lifecycle 或记录 `native_oracle_blocked`。
- GUI / drag / persistent solver / cross-request backend state / full Link lifecycle / Worker / WASM / Web adapter 保持 `diagnostic_non_goal`。
- S2 没有 `backend_gap_candidate`，没有打开 S4/S5 implementation gate；`C7M6-BLOCKER-201` 已关闭，队列推进到 S3。

## 必读文件

- S1 完成后的 C7-M6 README、方案和矩阵。
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/fixtures/c3m6` 与 `cad-core/fixtures/c3m6/expected`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tools/collect_freecad_expected.py`

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。
2. 形成 candidate 分类：`already_covered`、`oracle_candidate`、`oracle_blocker`、`backend_gap_candidate`、`diagnostic_non_goal`。
3. 对每个 `oracle_candidate` 写清 fixture 输入、FreeCAD source authority、expected 字段、collector 命令和 focused test。
4. 明确哪些 P8 baseline rows 不得重开，哪些 GUI / session / persistent solver 路径不得采 native golden。
5. 更新 oracle plan、scope、backend gate、blocker queue、validation matrix 和方案 S2 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S3。

## S3 输入清单

| oracle | route | fixture / probe | FreeCAD authority | expected fields | collector / test |
| --- | --- | --- | --- | --- | --- |
| `C7M6-ORACLE-202` | `oracle_candidate` | `cad-core/fixtures/c3m6/assembly-marker-custom-placement-chain-real-solver.json` | `AssemblyObject::handleOneSideOfJoint()`、`JointObject.py::Joint.createProperties()` | `solver_adapter`、`native_marker_oracle` connector/object-global/part-local/JCS/marker placements、`documentObjectUpdates` | `cd cad-core && python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-marker-custom-placement-chain-real-solver.json --out fixtures/c3m6/expected/assembly-marker-custom-placement-chain-real-solver.freecad.json`; focused test `test_c3m6_assembly_marker_custom_placement_chain_matches_native_expected` |
| `C7M6-ORACLE-302` | `oracle_candidate` | `cad-core/fixtures/c3m6/assembly-angle-zero-and-signed-current-real-solver.json` | `AssemblyObject::makeMbdJointOfType()`、`AssemblyUtils.cpp::getJointCurrentValue()`、`UtilsAssembly.py::getJointDistance/getJointXYAngle` | zero Angle fallback class/evidence、signed Distance/current value, `native_marker_oracle.current_value`, `placement_updates` | `cd cad-core && python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-angle-zero-and-signed-current-real-solver.json --out fixtures/c3m6/expected/assembly-angle-zero-and-signed-current-real-solver.freecad.json`; focused test `test_c3m6_assembly_angle_zero_and_signed_current_value_oracles_match_native_expected` |
| `C7M6-ORACLE-203` | `oracle_blocker` | Source-backed bundled `offsetPlc` probe before fixture naming | `AssemblyObject::handleOneSideOfJoint()`、`AssemblyObject::setNewPlacements()` | `offsetPlc` effect on marker placement and writeback, or explicit native blocker | S3 must promote to `oracle_candidate` only after proving lifecycle; otherwise record `native_oracle_blocked` |

## 候选批次要求

- 同一 FreeCAD 调用链和同一 solver / marker / writeback 账本能覆盖的 case 应批量推进，不要只挑单 fixture。
- 缺 native lifecycle 的 GUI / drag session / cross-request case 不能进入 implementation gate。
- 如果只能拆小批次，必须写清下一批次范围和拆分理由。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
rg -n 'oracle_candidate|already_covered|backend_gap_candidate|diagnostic_non_goal|JointType|GroundedJoint|assembly_set_placement|documentObjectUpdates|Ondsel' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S3 有明确 oracle 采集清单或 blocker 清单。
- S2 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S3。
