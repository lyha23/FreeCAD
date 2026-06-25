# 【已实现】C7-M4 S0 live 基线与 C7-M3 blocked row 冻结

## 目标

冻结 C7-M4 live 起点，确认 C7-M1/C7-M2/C7-M3 队列为空，并把 C7-M3 留下的 `dressup-reference-shadow-base-recovery` oracle blocker 复制到 C7-M4 矩阵。S0 不采 oracle，不新增 fixture/expected/test，不改 C++。

## 必读文件

- `docs/CADCore7.0/README.md`
- `docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/README.md`
- `docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv`
- `docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/README.md`
- `docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv`
- `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`
- `cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`
- `cad-core/tests/test_p7_features.py`

## 执行要点

1. 记录 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 跑 C7-M1/C7-M2/C7-M3/C7-M4 队列，确认前置队列为空，C7-M4 从 S0 开始。
3. 复核 C7-M3 `C7M3-SCOPE-103`、`C7M3-GATE-103`、`C7M3-ORACLE-301` 的最终 route。
4. 复核 blocker expected 中的 `known_gap.kind`、`reason` 和 `delete_condition`。
5. 更新 C7-M4 README/总入口/方案/矩阵中的 S0 状态，只记录值得后续执行依赖的基线结论。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S1。

## S0 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9bb2cd22af`（`9bb2cd22af docs: 收口 C7-M4 工作步骤总入口索引`），开始状态 `git status --short -uall` 无输出。
- 队列状态：C7-M1/C7-M2/C7-M3 `工作步骤细分` 队列为空；C7-M4 从 S0 起步，S0 完成后推进到 S1。
- 继承 blocker：`C7M3-SCOPE-103` / `C7M3-GATE-103` / `C7M3-ORACLE-301` / `c3m5/dressup-reference-shadow-base-recovery` 继续是 `oracle_blocked`，expected 仍记录 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。
- fixture / expected / test 状态：`dressup-reference-shadow-base-recovery.json` 仍包含 stale `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow`；expected 的删除条件仍要求证明不靠 StableSubList-fed geometry 的 native recovery；P7 blocked test 和 P6 reference update tests 仅作为现状证据，S0 未修改。
- S0 未采 oracle、未运行 FreeCADCmd、未新增或修改 fixtures/expected/tests、未改 C++、collector、runtime、topo 或 adapter。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- C7-M4 live 起点、前置队列、C7-M3 blocker 和当前 expected/test 状态已冻结。
- S0 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S1。
