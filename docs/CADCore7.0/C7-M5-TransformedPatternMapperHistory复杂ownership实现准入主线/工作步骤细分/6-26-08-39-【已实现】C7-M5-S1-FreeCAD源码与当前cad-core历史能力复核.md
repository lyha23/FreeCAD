# 【已实现】C7-M5 S1 FreeCAD 源码与当前 cad-core 历史能力复核

## 目标

复核 transformed / pattern ownership 的 FreeCAD source authority、当前 `cad-core` 实现、fixtures、expected 和 focused tests。S1 只写文档和矩阵，不新增 fixtures/expected/tests，不运行 FreeCAD oracle，不改 C++。

## 必读文件

- `src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `src/Mod/PartDesign/App/FeatureScaled.cpp`
- `src/Mod/PartDesign/App/FeatureMultiTransform.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part_design/feature_transformed.cpp`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/p7`
- C7-M5 README、方案和矩阵

## 执行要点

1. 记录 live baseline 和 C7-M5 queue。
2. 记录 FreeCAD source authority：execution order、AddSub replay、transform list、ElementMap copy、slot ownership。
3. 复核 current `cad-core` 能力：transformed copy alias、terminal history、merge history、AddSubShape slot history、MultiTransform composition。
4. 复核哪些 fixture / expected 已经覆盖，哪些只是 geometry-equivalent，哪些缺 native lifecycle。
5. 更新 `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## S1 完成结论

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=27b2f84d6a`（`27b2f84d6a docs: 完成 C7-M5 S0 基线冻结`），开始状态 `git status --short -uall` 无输出；C7-M5 队列首项为 S1。
- FreeCAD source authority 已记录到 `矩阵/c7m5_transformed_history_source_authority.tsv`：`Transformed::execute()` 的 Body/BaseFeature、transform list、Features AddSub replay、WholeShape support fuse 和 refine order；`DressUp::getAddSubShape()` 的 SupportTransform owner 和 add/sub slot cache；`MultiTransform::getTransformations()` 的 child order、Scaled diagonal、divisor diagnostic 和 non-Scaled multiplication；`TopoShape::makeElementTransform()` 的 `copyElementMap(tmp, op)` retag source。
- current `cad-core` coverage 已复核：`namedShapeForTransformedCopy()` 保留 TransformN source aliases、original stable aliases、nested source history 和 merge history；`feature_transformed.cpp` 已按 add/sub slots 重放 transformed copies；`feature_dress_up.cpp` 已缓存 SupportTransform add/sub slots；`feature_multi_transform.cpp` 已实现 diagonal/multiply composition。
- P7 fixture/test baseline 已复核：`assert_dressup_slot_history()`、`assert_transformed_pattern_ownership()`、`test_p7_transformed_copy_preserves_terminal_stable_history()`、MultiTransform tests 和 checked-in expected 覆盖已有 `element_history_status`、terminal split/deleted、merge、TransformN aliases、SupportTransform 和 whole-shape support baseline。
- S2 输入池已明确：`C7M5-SCOPE-101` 为 `already_covered_baseline`；`C7M5-SCOPE-201` / `C7M5-SCOPE-301` 为 `s2_oracle_candidate_input_pool`；`C7M5-SCOPE-401` 为 `diagnostic_non_goal_baseline`。S1 关闭 `C7M5-BLOCKER-101`，保留 `C7M5-BLOCKER-201` 给 S2。
- S1 未采 FreeCAD oracle，未新增 fixture/expected/test，未修改 C++，未把 S0 冻结的旧 P7T closed baseline 改成 backendGap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'makeElementTransform|Transformed::execute|getAddSubShape|getTransformations|namedShapeForTransformedCopy|element_history_status|TransformN|SupportTransform|MultiTransform' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 src/Mod/PartDesign/App src/Mod/Part/App cad-core/src/part_design cad-core/src/part cad-core/tests/test_p7_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确的 oracle candidate 输入池和已覆盖 baseline。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
