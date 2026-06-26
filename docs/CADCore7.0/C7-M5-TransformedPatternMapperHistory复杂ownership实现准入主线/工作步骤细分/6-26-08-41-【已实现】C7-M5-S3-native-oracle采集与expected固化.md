# 【已实现】C7-M5 S3 native oracle 采集与 expected 固化

## 目标

按 S2 候选批次采集 FreeCAD native oracle，或记录 native oracle blocker / diagnostic non-goal。S3 可以新增 oracle fixture / expected / known_gap；不改 runtime C++ 主路径。

## 必读文件

- S2 完成后的 C7-M5 README、方案和矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- S2 指定的 fixture / expected / focused test 文件。
- S1 记录的 FreeCAD source authority。

## 执行要点

1. 记录 live baseline 和 C7-M5 queue。
2. 按 S2 的 oracle plan 执行 collector 或 probe。
3. 如果采到 native oracle，expected 必须记录 FreeCAD version、ownership / history evidence、topology summary 和 source authority。
4. 如果无法证明 native lifecycle，写 known_gap 和删除条件。
5. 如果明确超出无状态 CAD Core 边界，写 diagnostic non-goal。
6. 更新 S3 相关矩阵和方案。
7. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S4。

## 合法产物

- 可以新增或更新 `cad-core/fixtures/p7/*transformed*` / `*pattern*` / `*multi-transform*` 相关 fixture。
- 可以新增或更新 `cad-core/fixtures/p7/expected/*.freecad.json`。
- 可以新增 focused oracle tests。
- 不允许改 `cad-core/src/part_design`、`cad-core/src/part`、adapter 或 runtime 主路径。

## S3 完成结论

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d8ad940e33`（`d8ad940e33 docs: 完成 C7-M5 S2 oracle 候选裁决`），开始状态 `git status --short -uall` 无输出；C7-M5 队列首项为 S3。
- native oracle check 已执行并通过：
  `cd cad-core && python3 tools/collect_freecad_expected.py fixtures/p7/mirrored-dressup-chain-support-transform.json --check && python3 tools/collect_freecad_expected.py fixtures/p7/linear-pattern-pad-pocket-multi-original.json --check && python3 tools/collect_freecad_expected.py fixtures/p7/multi-transform-linear-mirror.json --check && python3 tools/collect_freecad_expected.py fixtures/p7/multi-transform-scaled-diagonal.json --check`。
- FreeCAD runtime 输出为 `FreeCAD 1.2.0, Libs: 1.2.0devR20260519 (Git shallow)`；checked-in expected 的 `freecad_version` 为 `1.2.0 revision 20260519`。
- `C7M5-ORACLE-201` route=`native_oracle_confirmed`：`mirrored-dressup-chain-support-transform` 确认 `Body/Mirrored` topology 为 `faces=13, edges=29, vertices=18`，`Chamfer/Mirrored/Body` 的 checked-in `named_shapes` 记录 `history_partial`、`history_consumed:generated_modified`、`terminal_history:split_deleted`、`history_consumed:merge`，source prefixes 覆盖 `Chamfer.`、`Fillet.`、`Pad.`、`SketchPad.`；`linear-pattern-pad-pocket-multi-original` 确认 `Body/LinearPattern` topology 为 `faces=20, edges=48, vertices=32`，history kinds 覆盖 `modified/generated/deleted/split/merge`，source prefixes 覆盖 `Pad.`、`Pocket.`、`SketchPad.`、`SketchPocket.`、`LinearPattern.Transform1.`。
- `C7M5-ORACLE-301` route=`native_oracle_confirmed`：`multi-transform-linear-mirror` 确认 `Body/MultiTransform` topology 为 `faces=12, edges=24, vertices=16`，`named_shapes.Body.element_map_prefixes` 包含 `MultiTransform.Transform`；`multi-transform-scaled-diagonal` 确认 `Body/MultiTransform` topology 为 `faces=18, edges=36, vertices=24`、bbox 为 `[0.0, -0.5, -0.5]..[7.5, 1.5, 1.5]`、volume 为 `12.375`。
- source authority 保持 S1/S2 记录：Pattern slot ownership 依据 `Transformed::execute()` + `DressUp::getAddSubShape()`；MultiTransform composition 依据 `MultiTransform::getTransformations()` + `TopoShape::makeElementTransform()`。`collect_freecad_expected.py --check` 只重采并比较 FreeCAD native geometry payload；checked-in `named_shapes` ownership/history evidence 由 `ExpectedFixtureAssertions` 与 P7 focused tests 消费，不能把 native `--check` 误记为直接重采 MapperHistory。
- `C7M5-ORACLE-401` 保持 route=`diagnostic_non_goal`：standalone Whole shape geometry-equivalent case 不采 native golden，不转 backendGap；删除条件仍是可复现 Body/BaseFeature lifecycle native oracle。
- S4 parity 输入已固化：使用四个 checked-in expected 和 focused tests `test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache`、`test_p7_linear_pattern_replays_multi_original_add_and_sub_slots`、`test_p7_multi_transform_combines_linear_pattern_and_mirror`、`test_p7_multi_transform_scaled_child_uses_diagonal_composition` 比较 current `cad-core`。S3 未修改 expected、tests、C++ runtime、adapter 或正式主路径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

S3 具体 FreeCADCmd / unittest 命令以 S2/S3 矩阵记录为准。

## 完成标准

- 每个 S2 oracle candidate 都有 native oracle、native blocker 或 diagnostic non-goal 结论。
- S3 不改 C++ runtime 主路径。
- 队列推进到 S4。
