# C7-M4 DressUp ReferenceShadow 原生恢复证据与实现准入主线

本目录承接 C7-M3 release gate。C7-M3 已把 Fillet multi-edge / `UseAllEdges` 与 Chamfer `FlipDirection=true` 发布为 expected-backed；唯一没有关闭为 supported 的 row 是 `dressup-reference-shadow-base-recovery`，当前 route 为 `oracle_blocked`。

C7-M4 的目标不是直接给 `cad-core` 增加宽松 fallback，而是先证明 FreeCAD 原生 `PropertyLinkSub` / `ShadowSub` / `ReferenceShadow` 恢复路径是否成立。只有 native oracle 明确证明 stale Base 可以恢复，并且当前 `cad-core` parity 失败时，才打开 C++ implementation gate。

## 入口

- 主线总入口：`6-26-00-49-C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线总入口.md`
- 方案：`6-26-00-49-C7-M4-DressUpReferenceShadow原生恢复证据与实现准入方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=edae0ef938`（`edae0ef938 文档：完成 C7-M3 S5 发布闸门`），创建前 `git status --short -uall` 无输出。
- S0 live 基线已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9bb2cd22af`（`9bb2cd22af docs: 收口 C7-M4 工作步骤总入口索引`），开始状态 `git status --short -uall` 无输出。
- C7-M1 / C7-M2 / C7-M3 `工作步骤细分` 队列均为空；C7-M4 从 S0 起步，S0 完成后队列推进到 S1。
- C7-M3 最终 blocker 已复制为 C7-M4 S0 基线：`C7M3-SCOPE-103` / `C7M3-GATE-103` / `C7M3-ORACLE-301` / `c3m5/dressup-reference-shadow-base-recovery` 保持 `oracle_blocked`；expected 仍是 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。
- S0 只同步文档和矩阵；未采 FreeCAD oracle、未运行 FreeCADCmd、未新增或修改 fixtures/expected/tests、未改 C++。S1 才能设计 native probe，S2 才能补 native oracle 证据，S3 才能裁决 implementation gate。

## 收口边界

- 必须证明 FreeCAD native 恢复路径，而不是用 `collect_freecad_expected.py::link_sub_value()` 直接把 `StableSubList` 喂给 FreeCAD 后采 geometry。
- 证据必须同时覆盖 stale `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow`、current graph 和最终 `DressUp::Base.getShadowSubs()` / geometry 结果。
- 如果 FreeCAD native probe 不能证明恢复，则继续发布为 `oracle_blocked` 或 `diagnostic_non_goal`，不打开实现。
- 如果 FreeCAD 可恢复但 `cad-core` 不一致，实现落点必须是 `cad-core/src/app` / `cad-core/src/part` / `cad-core/src/part_design` 的正式 reference recovery 路径；不得在 adapter、输出 JSON、fixture 名称或边编号上猜测。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
