# 【已实现】C7-M5 S2 复杂 ownership oracle 候选矩阵与最小批次

## 目标

把 S1 的 source / coverage 复核结果裁成最小完整语义批次，明确哪些进入 native oracle 采集，哪些保持 already covered，哪些是 diagnostic non-goal。S2 不采 oracle，不改 C++。

## 必读文件

- S1 完成后的 C7-M5 README、方案和矩阵。
- 旧 P7 Transformed 主线矩阵。
- `cad-core/fixtures/p7` 与 `cad-core/fixtures/p7/expected`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tools/collect_freecad_expected.py`

## 执行要点

1. 记录 live baseline 和 C7-M5 queue。
2. 形成 candidate 分类：`already_covered`、`oracle_candidate`、`oracle_blocker`、`backend_gap_candidate`、`diagnostic_non_goal`。
3. 对每个 `oracle_candidate` 写清 fixture 输入、FreeCAD source authority、expected 字段、collector 命令和 focused test。
4. 明确哪些旧 P7 rows 不得重开，哪些 standalone / GUI / session 路径不得采 native golden。
5. 更新 oracle plan、scope、backend gate、blocker queue、validation matrix 和方案 S2 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S3。

## 候选批次要求

- 同一 FreeCAD 调用链和同一 ownership 账本能覆盖的 case 应批量推进，不要只挑单 fixture。
- 缺 native lifecycle 的 geometry-equivalent case 不能进入 implementation gate。
- 如果只能拆小批次，必须写清下一批次范围和拆分理由。

## S2 完成结论

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=cbfbfe736d`（`cbfbfe736d docs: 完成 C7-M5 S1 源码与覆盖复核`），开始状态 `git status --short -uall` 无输出；C7-M5 队列首项为 S2。
- `C7M5-SCOPE-101` route=`already_covered`：TransformN alias、original stable alias、terminal split/deleted history、merge history 和 `element_history_status` 已由现有 P7 fixture / expected / focused tests 覆盖；S3 不新增 native oracle。
- `C7M5-SCOPE-201` route=`oracle_candidate`：S3 最小完整批次为 support-backed `cad-core/fixtures/p7/mirrored-dressup-chain-support-transform.json` 与 `cad-core/fixtures/p7/linear-pattern-pad-pocket-multi-original.json`，authority 为 `Transformed::execute()` + `DressUp::getAddSubShape()`，evidence 字段为 topology summary、slot owner/source prefixes、TransformN aliases 和 `element_history_status`。
- `C7M5-SCOPE-301` route=`oracle_candidate`：S3 最小完整批次为 support-backed `cad-core/fixtures/p7/multi-transform-linear-mirror.json` 与 `cad-core/fixtures/p7/multi-transform-scaled-diagonal.json`，authority 为 `MultiTransform::getTransformations()` + `TopoShape::makeElementTransform()`，evidence 字段为 child template order、composition topology、MultiTransform.Transform aliases 和 stable diagnostics。
- `C7M5-SCOPE-401` route=`diagnostic_non_goal`：standalone `polar-pattern-whole-shape` / `multi-transform-whole-shape` 等 geometry-equivalent smoke 不具备 Body/BaseFeature lifecycle，不能变成 native golden 或 backendGap。
- S2 没有打开 `backend_gap_candidate`；S4 之前 implementation gate 仍关闭。已更新 `oracle_plan`、`scope`、`backend_gate`、`blocker_queue`、`validation_matrix` 和方案 S2 小节；未采 oracle、未新增或修改 fixtures/expected/tests、未改 C++。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
rg -n 'oracle_candidate|already_covered|backend_gap_candidate|diagnostic_non_goal|TransformN|SupportTransform|MultiTransform|element_history_status' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S3 有明确 oracle 采集清单或 blocker 清单。
- S2 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S3。
