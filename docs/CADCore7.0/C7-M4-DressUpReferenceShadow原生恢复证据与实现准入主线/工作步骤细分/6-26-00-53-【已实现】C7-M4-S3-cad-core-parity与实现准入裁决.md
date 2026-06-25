# 【已实现】C7-M4 S3 cad-core parity 与实现准入裁决

## 目标

基于 S2 native oracle / blocker 结果，对当前 `cad-core` 做 parity 或 diagnostics 分类，裁决 S4 是否打开 C++ implementation gate。S3 默认不改 C++。

## 必读文件

- S2 完成后的本包 README、方案和矩阵。
- S2 产生或更新的 fixture / expected / blocker JSON。
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/app`
- `cad-core/src/part`

## 执行要点

1. 记录 live baseline 和 C7-M4 queue。
2. 若 S2 是 native oracle，运行 current `cad-core` 并比较 geometry、diagnostics、link update 建议。
3. 若 S2 是 blocker，确认 focused test 保持 blocker，不打开 implementation gate。
4. 写入 route：`already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。
5. 如果打开 implementation gate，必须列出 S4 允许修改的文件、FreeCAD 依据、non-goals 和 focused test 名称。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S4。

## 裁决规则

- FreeCAD native 可恢复 + current cad-core 匹配：`already_closed_expected_backed`。
- FreeCAD native 可恢复 + current cad-core 不匹配：`backend_gap_requires_implementation`。
- FreeCAD native 证据不足：`oracle_blocked`。
- FreeCAD native 明确不支持或超出无状态 CAD Core 边界：`diagnostic_non_goal`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S4 code gate 状态明确。
- 若打开 code gate，S4 范围足够窄且有 FreeCAD source authority。
- 队列推进到 S4。

## S3 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=01aeef0217`（`01aeef0217 证据：补齐 C7-M4 S2 native probe`），开始状态 `git status --short -uall` 无输出。
- route=`oracle_blocked`。S2 的 native FCStd/XML restore probe 和 StableSubList-fed 负控均 `returncode=0`，但没有新增能观察 `Base.getShadowSubs()` / `getSubValues(false/true)` 的 native oracle 证据；`cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json` 仍是 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。
- implementation gate 保持 closed。S3 未改 C++、collector/probe、fixture 或 expected；StableSubList-fed geometry 输出仍只能作为 bypass guard，不能删除 blocker，不能发布 supported。
- S4 边界：只允许 no-code publication closure，更新 README、总入口、方案、矩阵和发布口径；不允许实现 `ReferenceRecovery`、不允许修改 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/part_design`，不扩大到 full DressUp / MapperHistory。
