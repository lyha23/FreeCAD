# C7-M3 PartDesign Fillet Chamfer Oracle 补采与实现准入主线

本目录承接 C7-M2 release gate。C7-M2 已确认没有 `backend_gap_requires_implementation`，但留下 3 个不能发布为 supported 的 `oracle_pending_collect`：Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery。

C7-M3 的目标不是直接实现 C++，而是先补 FreeCAD oracle，随后把每个 row 裁成 `already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。只有 S3 明确打开 code edit gate 后，S4 才能改 `cad-core`。

## 入口

- 主线总入口：`6-25-22-57-C7-M3-PartDesignFilletChamferOracle补采与实现准入主线总入口.md`
- 方案：`6-25-22-57-C7-M3-PartDesignFilletChamferOracle补采与实现准入方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`），创建前 `git status --short -uall` 无输出。
- S0 live 基线已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`）；开始状态包含目标文档 dirty worktree（root README modified、C7-M3 包 untracked），没有无关源码、fixture、expected 或 test 改动。
- C7-M1 和 C7-M2 队列均为空；C7-M3 从 S0-S5 pending 起步，S0 完成后队列推进到 S1。
- C7-M2 最终 route：Chamfer Two distances、Chamfer Distance and Angle、SupportTransform mirrored / chained DressUp regression 为 inherited `already_closed_expected_backed`；Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 为 `oracle_pending_collect`。
- S0 已把 C7-M2 最终 3 个 oracle pending rows 逐项冻结到 C7-M3 矩阵：`C7M2-GAP-101 -> C7M3-SCOPE-101`（Fillet multi-edge / `UseAllEdges`）、`C7M2-GAP-203 -> C7M3-SCOPE-102`（Chamfer `FlipDirection=true`）、`C7M2-GAP-301 -> C7M3-SCOPE-103`（stale `ReferenceShadow` / Base recovery）。
- C7-M3 只处理这 3 个 oracle pending rows，不重开基础 Fillet / Chamfer、RefineModel、SupportTransform mirrored regression 或 C7-M2 已关闭的 non-goal。

## 收口边界

- Fillet：multi-edge selected EdgeN、`UseAllEdges=true` all TopAbs_EDGE，至少覆盖与现有 `fillet-pad-edge` / `fillet-refine-true` 同族的 Body-member 场景。
- Chamfer：`FlipDirection=true` 的 ancestor face side，至少覆盖 Equal distance；Two distances 与 Distance and Angle 只在 fixture 设计阶段判断是否需要 true-side 代表，不重采 false-side already-backed rows。
- DressUp recovery：stale `StableSubList` + `ShadowSub` + `ReferenceShadow` + current graph 的组合恢复证据；没有完整证据时不能实现宽松 fallback。
- 发布口径：expected 只能来自 FreeCAD oracle 或明确 diagnostic，不能从当前 `cad-core` 输出倒推。

## 非目标

- 不实现 GUI、TaskPanel、交互选择器或 preview UI。
- 不扩展 Draft、Thickness 或 full DressUp universe。
- 不把 full topo naming / full MapperHistory 作为本包目标。
- 不允许在 adapter、executor 输出端、fixture 名称、边编号或 source edge 形态上猜测引用恢复。
- 不重开 C7-M2 already-closed rows，除非新 oracle 证明现有 expected-backed 行失效。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
