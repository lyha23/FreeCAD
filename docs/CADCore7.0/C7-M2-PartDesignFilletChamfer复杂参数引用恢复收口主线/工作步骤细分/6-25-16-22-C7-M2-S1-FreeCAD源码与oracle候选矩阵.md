# C7-M2 S1 FreeCAD 源码与 oracle 候选矩阵

## 目标

阅读 FreeCAD 和 cad-core 当前实现，把 Fillet / Chamfer 复杂参数、选边、Base 引用恢复、SupportTransform 和现有 fixtures/tests 补成可裁决矩阵。S1 仍然不改 C++、fixtures、expected 或 tests。

## 必读

- `src/Mod/PartDesign/App/FeatureFillet.cpp`
- `src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/part_design/feature_transformed.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/p7/`
- 本包 `矩阵/*.tsv`

## 动作

1. 记录 live baseline 和当前队列状态。
2. 从 FreeCAD 源码摘出具体类/函数、字段和关键短句，补充 `source_candidates` 与 `input_contract`。
3. 从 cad-core 当前实现和 tests 中确认已经 covered 的 routes，补充 `oracle_fixture`。
4. 为下列候选建立 rows：Fillet multi-edge / UseAllEdges、Chamfer Two distances、Chamfer Distance and Angle、Chamfer FlipDirection、DressUp chain Base reference recovery。
5. 更新 `scope_review`、`backend_gap_classification` 和 `blocker_queue`，但不得把 unknown 直接写成 backend gap。
6. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S2。

## 非目标

- 不采集 FreeCAD oracle。
- 不新增或修改 fixture expected。
- 不做 C++ 实现。
- 不把 missing evidence 当作 backend gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
rg -n 'Radius|UseAllEdges|ChamferType|Size2|Angle|FlipDirection|getContinuousEdges|getAddSubShape|SupportTransform|makeElementFillet|makeElementChamfer' src/Mod/PartDesign/App/FeatureFillet.cpp src/Mod/PartDesign/App/FeatureChamfer.cpp src/Mod/PartDesign/App/FeatureDressUp.cpp cad-core/src/part_design/feature_dress_up.cpp
rg -n 'def test_.*(fillet|chamfer|dressup|support_transform)|fillet|chamfer|SupportTransform' cad-core/tests/test_p7_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```

## 通过条件

- 每个候选 row 都有 FreeCAD 源文件、类/函数和 cad-core 落点。
- oracle candidates 明确区分 already covered、candidate、diagnostic 和 non-goal。
- 本文件标记后，队列推进到 S2。
