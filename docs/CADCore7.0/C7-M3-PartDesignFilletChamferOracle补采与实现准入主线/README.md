# C7-M3 PartDesign Fillet Chamfer Oracle 补采与实现准入主线

本目录承接 C7-M2 release gate。C7-M2 已确认没有 `backend_gap_requires_implementation`，但留下 3 个不能发布为 supported 的 `oracle_pending_collect`：Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery。

C7-M3 的目标不是直接实现 C++，而是先补 FreeCAD oracle，随后把每个 row 裁成 `already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。S3 没有打开 code edit gate，S4 已按 no-code publication closure 同步 docs/矩阵，不改 `cad-core`。

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
- S1 已完成：live 起点 `HEAD=a0a9799608`（`a0a9799608 文档：完成 C7-M3 S0 基线冻结`），开始状态干净。S1 只做 oracle fixture / collector route 设计，没有新增 fixture/expected/tests、没有运行 FreeCAD oracle、没有跑 cad-core parity、没有改 C++；队列推进到 S2。
- S2 已完成：live 起点 `HEAD=ad03c44cfe`（`ad03c44cfe 文档：完成 C7-M3 S1 oracle fixture 设计`），开始状态干净。S2 新增 6 个 fixture JSON、5 个 FreeCADCmd-derived expected JSON 和 1 个 ReferenceShadow native oracle blocker JSON；没有改 feature executor、runtime、topo、adapter、capability 或 tests。
- S2 expected-backed oracle：`p7/fillet-pad-multi-edge`、`p7/fillet-pad-use-all-edges`、`p7/chamfer-pad-edge-flip-true`、`c3m5/chamfer-two-distances-edge-flip-true`、`c3m5/chamfer-distance-angle-edge-flip-true`。这些 expected 均由 `FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py <fixture> --out <expected>` 生成，`freecad_version=1.2.0 revision 20260519`。
- S2 blocker：`c3m5/dressup-reference-shadow-base-recovery` fixture 已新增，但 expected 记录为 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。当前 collector geometry-only 探测返回成功，但原因是 `link_sub_value()` 直接喂 `StableSubList`，不能证明旧 `SubList` 通过 `ShadowSub` / `ReferenceShadow` 原生恢复；该 row 必须在 S3 走 `oracle_blocked`，不能打开 implementation gate。
- S3 已完成：live 起点 `HEAD=ac831f3ba7`（`ac831f3ba7 文档：完成 C7-M3 S2 oracle expected 固化`），开始状态干净。当前 cad-core 对 5 个 Fillet/Chamfer expected-backed fixtures 的 bbox、volume、topology_counts 全部匹配 S2 FreeCADCmd expected；新增 focused tests 为 `test_c7m3_fillet_oracle_rows_match_expected`、`test_c7m3_chamfer_flip_direction_oracle_rows_match_expected`、`test_c7m3_reference_shadow_recovery_oracle_remains_blocked`。
- S3 gate route：`C7M3-SCOPE-101` Fillet multi-edge / `UseAllEdges` 为 `already_closed_expected_backed`；`C7M3-SCOPE-102` Chamfer `FlipDirection=true` 为 `already_closed_expected_backed`；`C7M3-SCOPE-103` stale `ReferenceShadow` / Base recovery 继续为 `oracle_blocked`。没有 `backend_gap_requires_implementation`，S4 已按 no-code publication/docs sync 落实，不改 C++。
- S4 已完成：live 起点 `HEAD=364ae7a093`（`364ae7a093 测试：完成 C7-M3 S3 parity gate`），开始状态干净。S4 只同步 root README、本包 README/总入口/方案、P7 细化文档和 C7-M3 矩阵；未改 C++ executor/runtime/topo/adapter/capability_contract、fixtures/expected/tests。5 个 Fillet / Chamfer oracle rows 发布为 expected-backed，`dressup-reference-shadow-base-recovery` 保持 `oracle_blocked`，队列推进到 S5。
- C7-M3 只处理这 3 个 oracle pending rows，不重开基础 Fillet / Chamfer、RefineModel、SupportTransform mirrored regression 或 C7-M2 已关闭的 non-goal。S4 后的下一步是 S5 release gate。

## 收口边界

- Fillet：multi-edge selected EdgeN、`UseAllEdges=true` all TopAbs_EDGE，至少覆盖与现有 `fillet-pad-edge` / `fillet-refine-true` 同族的 Body-member 场景。
- Chamfer：`FlipDirection=true` 的 ancestor face side，至少覆盖 Equal distance；S1 已判定 Two distances 与 Distance and Angle 也需要 true-side 代表，才能发布非等距 `FlipDirection=true` 支持；不重采 false-side already-backed rows。
- DressUp recovery：stale `StableSubList` + `ShadowSub` + `ReferenceShadow` + current graph 的组合恢复证据；没有完整证据时不能实现宽松 fallback。
- 发布口径：expected 只能来自 FreeCAD oracle 或明确 diagnostic，不能从当前 `cad-core` 输出倒推。

S1 裁决：Equal distance true-side 是最小 smoke；Two distances 与 Distance and Angle true-side 仍需要代表，才能发布非等距 `FlipDirection=true` 支持，因为 false-side expected 不能证明 `Size2` / `Angle` 的 true-side 解释。

## S4 发布口径

- expected-backed：`p7/fillet-pad-multi-edge`、`p7/fillet-pad-use-all-edges`、`p7/chamfer-pad-edge-flip-true`、`c3m5/chamfer-two-distances-edge-flip-true`、`c3m5/chamfer-distance-angle-edge-flip-true`。
- oracle blocked：`c3m5/dressup-reference-shadow-base-recovery` 仍是 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`，不能发布为 supported，也不能打开 implementation gap。
- code gate：closed；S4 没有改 C++ executor/runtime/topo/adapter/capability_contract，也没有新增 fixtures/expected/tests。

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
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
git diff --check
```
