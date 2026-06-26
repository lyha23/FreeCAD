# C7-M5 S4 cad-core parity 与 implementation gate

## 目标

基于 S3 native oracle / blocker 结果，对当前 `cad-core` 做 parity 或 diagnostics 分类，裁决 S5 是否打开 C++ implementation gate。S4 默认不改 C++。

## 必读文件

- S3 完成后的 C7-M5 README、方案和矩阵。
- S3 新增或更新的 fixture / expected / known_gap。
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/src/part_design/feature_transformed.cpp`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/part/topo_shape.cpp`

## 执行要点

1. 记录 live baseline 和 C7-M5 queue。
2. 若 S3 是 native oracle，运行 current `cad-core` 并比较 topology、history、diagnostics、ElementMap 和 capability publication。
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
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S5 code gate 状态明确。
- 若打开 code gate，S5 范围足够窄且有 FreeCAD source authority。
- 队列推进到 S5。
