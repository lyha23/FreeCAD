# CADCore13.0

CADCore13.0 用来收口 `topoNamingState` 输出发布与 expected 对齐主线。C13-M1 已完成输出发布闭环；C13-M2 进入 FreeCAD raw mapped-name、child map key、mapper history id 字节级 parity 的最小完整语义批次。

当前批次：

| 批次 | 状态 | 入口 |
| --- | --- | --- |
| C13-M1 TopoNamingState 输出发布闭环 | completed / 已完成 | [C13-M1-TopoNamingState输出发布闭环批次](C13-M1-TopoNamingState输出发布闭环批次/README.md) |
| C13-M2 FreeCAD MappedName Parity | planned | [C13-M2-FreeCADMappedNameParity实现批次](C13-M2-FreeCADMappedNameParity实现批次/README.md) |

## 阶段边界

- 本阶段只处理 `cad-core` runtime response 中 `topoNamingState` 的收集、发布、消费回归和 fixture 对齐。
- C13-M2 只处理 FreeCAD mapped-name / child map key / mapper history id 的 focused parity，不把全量 expected fixture parity 或前端消费混进同一批次。
- 不从 `fixtures/<phase>/expected/*.freecad.json` 反推实现逻辑；expected 只作为 schema 和 oracle 对照，业务语义来源仍是 FreeCAD `TopoShape` / `ElementMap` / `PropertyLinks` 源码与 `collect_freecad_expected.py` 的 native oracle。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
git diff --check
```
