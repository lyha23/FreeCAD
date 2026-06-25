# 【已实现】C7-M4 S4 no-code blocked 发布收口

## 目标

按 S3 route=`oracle_blocked` 执行 no-code blocked / diagnostic 发布收口。S4 不允许扩大到 full MapperHistory、full DressUp universe 或 output-side guessing。

S3 已裁决 route=`oracle_blocked`，implementation gate closed。当前 S4 只允许 no-code blocked 发布收口，不允许修改 C++、collector/probe、fixtures/expected，不允许实现 `ReferenceRecovery` 或发布 supported。

## 必读文件

- S3 完成后的本包 README、方案和矩阵。
- S2/S3 fixture / expected / blocker JSON。
- `src/App/PropertyLinks.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `cad-core/src/app`
- `cad-core/src/part`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p7_features.py`

## 执行要点

1. 记录 live baseline 和 C7-M4 queue。
2. 由于 S3 route=`oracle_blocked`，只更新 README、方案、矩阵和 P7 发布口径。
3. 不执行 C++ implementation 分支；若未来有新 native oracle 证据，必须另起步骤重新打开 gate。
4. 保持现有 blocker focused test `test_c7m3_reference_shadow_recovery_oracle_remains_blocked`；S4 不新增测试。
5. 不允许在 adapter、JSON 输出、fixture 名称、EdgeN 排序或 source shape 猜测上修正结果。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S5。

## 实现落点

当前 S4 不允许改下列实现落点；它们仅作为未来 native oracle 重开 gate 后的边界记录。

- `cad-core/src/app`：PropertyLinkSub / SubList / StableSubList / ShadowSub / ReferenceShadow 输入与更新建议。
- `cad-core/src/part`：`ReferenceShadow.brep` snapshot 校验和旧 subshape 证据读取。
- `cad-core/src/part_design/feature_dress_up.cpp`：只消费正式恢复后的 Base target / subnames。
- `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_p6_topology.py`：focused regression。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_reference_shadow_recovery_oracle_remains_blocked
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
git diff --check
```

S4 是 no-code publication closure，不运行 `cmake --build build`，只运行文档短跑和现有 blocker focused unittest。

## 完成标准

- S4 route 已落实为 no-code blocked 发布。
- docs / 现有 blocker test / expected / capability 口径一致。
- 队列推进到 S5。

## S4 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=381d56ef9e`（`381d56ef9e 文档：完成 C7-M4 S3 准入裁决`），开始状态 `git status --short -uall` 无输出。
- S2 native probe `returncode=0` 但 FreeCAD Python API 不能观察 `Base.getShadowSubs()` / `getSubValues(false/true)`；StableSubList-fed geometry 负控不能删除 blocker。
- `dressup-reference-shadow-base-recovery` 继续 `oracle_blocked`，`C7M4-BLOCKER-401` / `C7M4-GATE-401` 关闭。
- 未改 C++、collector/probe、fixtures/expected/tests，未实现 `ReferenceRecovery`，未发布 supported；S5 只做 release gate。
