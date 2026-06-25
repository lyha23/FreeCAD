# 【已实现】C7-M3 S0 live 基线与 C7-M2 pending row 冻结

## 目标

冻结 C7-M3 live 起点，确认 C7-M1/C7-M2 队列为空，并把 C7-M2 留下的 3 个 `oracle_pending_collect` rows 复制到 C7-M3 矩阵。S0 不采 oracle，不新增 fixture/expected/test，不改 C++。

## 必读

- `docs/CADCore7.0/README.md`
- `docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/README.md`
- `docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv`
- `docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/README.md`
- `docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv`

## 动作

1. 记录 live baseline 和当前队列状态。
2. 运行 C7-M1、C7-M2、C7-M3 的 `step_goal_queue.py`。
3. 复核 C7-M2 `oracle_pending_collect` rows 与 C7-M3 矩阵一致。
4. 更新 root README、本包 README、总入口、方案和矩阵中的 S0 baseline 行。
5. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S1。

## 执行记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`）。
- 开始状态：`git status --short -uall` 显示 `docs/CADCore7.0/README.md` 已修改，C7-M3 包文件为 untracked；这些均属于本轮目标文档范围，未发现无关源码、fixture、expected 或 test dirty 文件。
- 队列：C7-M1 与 C7-M2 队列为空；C7-M3 初始队列为 S0-S5 pending，S0 完成后推进到 S1。
- row 映射：`C7M2-GAP-101 -> C7M3-SCOPE-101`（Fillet multi-edge / `UseAllEdges`）、`C7M2-GAP-203 -> C7M3-SCOPE-102`（Chamfer `FlipDirection=true`）、`C7M2-GAP-301 -> C7M3-SCOPE-103`（DressUp chain stale `ReferenceShadow` / Base recovery）。

## 非目标

- 不采集 FreeCAD oracle。
- 不新增或修改 fixtures、expected、tests。
- 不改 cad-core C++。
- 不裁决 parity 或 backend gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
rg -n 'oracle_pending_collect|UseAllEdges|FlipDirection|ReferenceShadow|backend_gap_requires_implementation' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线 docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 通过条件

- C7-M3 队列推进到 S1。
- 3 个 C7-M2 oracle pending rows 在 C7-M3 矩阵中都有 scope、oracle plan、blocker 和 validation 行。
- 无代码、fixture、expected 或 test 改动。
