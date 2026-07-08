# C13-M1 S0 live 基线与 schema 冻结

## 目标

冻结当前代码路径和 expected `topoNamingState` schema，确认 C13-M1 只补输出发布闭环。

## 必读文件

- `../README.md`
- `../7-8-17-53-C13-M1-TopoNamingState输出发布闭环批次方案.md`
- `cad-core/include/cad_core/runtime/compute_context.h`
- `cad-core/src/app/document.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json`
- `cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json`

## 操作

1. 记录 `git status --short`、`git rev-parse --short=10 HEAD`、`git log -1 --oneline`。
2. 确认 `Document::topoNamingState` 输入路径存在。
3. 确认 `ComputeContext::topoNamingState` 与 `mergeTopoNamingStateElementMap()` 当前行为。
4. 用 `jq '.topoNamingState | keys'` 抽样 expected schema。
5. 冻结 C13-M1 非目标：不做 mapped-name 字节级编码，不做全量 fixture parity，不改 frontend。

## 关闭条件

- `矩阵/c13m1_topo_state_scope_matrix.tsv` 中 S0 行更新为 `closed`。
- `矩阵/c13m1_topo_state_contract_matrix.tsv` 明确 C13-M1 必须字段和后续字段。
- blocker queue 中 S0 blocker 关闭。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
git diff --check
```
