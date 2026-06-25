# C7-M4 S0 live 基线与 C7-M3 blocked row 冻结

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
