# C7-M3 S1 oracle fixture 设计与 collector 路径

## 目标

把 3 个 oracle pending rows 设计成可采集的 FreeCAD fixtures，并确认 `collect_freecad_expected.py` 的支持路径、预期输出字段、focused tests 和 blocker 分类。S1 仍然不新增 fixtures/expected/tests，不改 C++。

## 必读

- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p7/`
- `cad-core/fixtures/c3m5/`
- `cad-core/tests/test_p7_features.py`
- `src/Mod/PartDesign/App/FeatureFillet.cpp`
- `src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- 本包 `README.md`、方案和 `矩阵/*.tsv`

## 动作

1. 记录 live baseline 和队列状态。
2. 为 Fillet multi-edge / `UseAllEdges` 设计 fixture 名、Base LinkSub、expected fields 和 collector command。
3. 为 Chamfer `FlipDirection=true` 设计 fixture 名、参数组合和 expected fields；说明是否需要 true-side Two distances / Distance and Angle。
4. 为 stale `ReferenceShadow` / Base recovery 设计最小可恢复 graph，明确 `StableSubList`、`ShadowSub`、`ReferenceShadow` 和 current graph 的关系。
5. 更新 `oracle_plan`、`fixture_plan`、`collector_gap`、`validation_matrix`。
6. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S2。

## 非目标

- 不实际新增 fixture/expected。
- 不运行 FreeCAD oracle 采集。
- 不跑 cad-core parity。
- 不裁决 backend gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
rg -n 'collect_freecad_expected|UseAllEdges|FlipDirection|ReferenceShadow|StableSubList|ShadowSub|FreeCADCmd' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p7_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 通过条件

- 每个 row 都有 fixture 设计、collector path、expected 字段和 blocker route。
- ReferenceShadow recovery 设计没有宽松 fallback。
- 队列推进到 S2。
