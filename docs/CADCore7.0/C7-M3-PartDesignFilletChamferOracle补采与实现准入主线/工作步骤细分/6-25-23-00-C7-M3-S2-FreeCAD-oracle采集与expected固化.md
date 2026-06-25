# C7-M3 S2 FreeCAD oracle 采集与 expected 固化

## 目标

按 S1 设计新增 C7-M3 fixtures，并采集 FreeCAD expected。S2 的核心产物是可信 oracle 或明确 native oracle blocker，不是 cad-core parity。

## 必读

- S1 完成后的本包 `README.md`、方案和 `矩阵/*.tsv`
- `cad-core/tools/collect_freecad_expected.py`
- S1 指定的 fixture payload 参考文件
- `AGENTS.md` 中 FreeCADCmd / oracle / OCCT 基线规则

## 动作

1. 记录 live baseline 和队列状态。
2. 新增 S1 指定的 fixture JSON。
3. 使用本机可用 `FreeCADCmd` / `freecadcmd` / `FREECADCMD` 采集 expected；若 sandbox Qt/processor 错误，按环境 blocker 记录，不把它当语义失败。
4. 若 collector 不支持某字段，优先修 collector 或记录 native oracle blocker，不得写 cad-core 输出为 expected。
5. 更新 oracle / fixture / validation 矩阵。
6. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S3。

## 非目标

- 不改 feature executor。
- 不跑 implementation parity 结论。
- 不修改 capability supported 口径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
find cad-core/fixtures/p7 -maxdepth 2 -type f | rg 'fillet|chamfer|reference|shadow|use-all|flip'
rg -n 'FreeCADCmd oracle|known_gap|native_oracle_blocker|ReferenceShadow|FlipDirection|UseAllEdges' cad-core/fixtures/p7 docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
git diff --check
```

## 通过条件

- 每个 row 有 expected-backed oracle、native oracle blocker 或明确 diagnostic fixture。
- 没有从 cad-core 输出倒推 expected。
- 队列推进到 S3。
