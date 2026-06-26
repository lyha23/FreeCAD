# C7-M7 S3 native oracle 采集与 expected 固化

## 目标

按 S2 候选批次采集 FreeCAD native oracle，或记录 native oracle blocker / diagnostic non-goal。S3 可以新增 oracle fixture / expected / known_gap；不改 runtime C++ 主路径。

## 必读文件

- S2 完成后的 C7-M7 README、方案和矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- S2 指定的 fixture / expected / focused test 文件。
- S1 记录的 FreeCAD source authority。

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 按 S2 的 oracle plan 执行 collector 或 probe。
3. 如果采到 native oracle，expected 必须记录 FreeCAD version、ElementMap / LinkSub / writeback / reference update evidence、source authority 和 deletion conditions。
4. 如果无法证明 native lifecycle，写 known_gap 和删除条件。
5. 如果明确超出无状态 CAD Core 边界，写 diagnostic non-goal。
6. 更新 S3 相关矩阵和方案。
7. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S4。

## 合法产物

- 可以新增或更新 `cad-core/fixtures/p8/*link*` / `*import*` / `*element*` 相关 fixture。
- 可以新增或更新 `cad-core/fixtures/p8/expected/*.freecad.json`。
- 可以新增 focused oracle tests。
- 不允许改 `cad-core/src/app/link.cpp`、`cad-core/src/part/*`、adapter 或 runtime 主路径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

S3 具体 FreeCADCmd / unittest 命令以 S2/S3 矩阵记录为准。

## 完成标准

- 每个 S2 oracle candidate 都有 native oracle、native blocker 或 diagnostic non-goal 结论。
- S3 不改 C++ runtime 主路径。
- 队列推进到 S4。
