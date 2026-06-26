# C7-M6 S2 Joint 完整 placement 与 constraint oracle 候选矩阵

## 目标

把 S1 的 source / coverage 复核结果裁成最小完整语义批次，明确哪些进入 native oracle 采集，哪些保持 already covered，哪些是 diagnostic non-goal。S2 不采 oracle，不改 C++。

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
