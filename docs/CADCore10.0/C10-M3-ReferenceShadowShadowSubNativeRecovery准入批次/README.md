# C10-M3 ReferenceShadow / ShadowSub Native Recovery 准入批次

本包承接 C10-M2 队列关闭后的下一轮 CAD Core 复核。主题是 stale `ReferenceShadow` / `ShadowSub` old-reference recovery 的 native 可观测性与 request-local 实现准入：先证明 FreeCAD 原生是否能稳定暴露恢复证据，再判断 cad-core 是否存在可实现 mismatch。

当前 C10-M2 已发布 no-code release gate：DressUp / Hole producer history 无 implementation row，cross-feature old-reference recovery 保持 `diagnostic_retained`。因此 C10-M3 默认不是“实现旧引用恢复”，而是用 S0-S6 闸门判断是否有 native observable evidence 足以打开实现；本包最终裁决为 docs-only no-code retained diagnostic / release gate。

## 入口

- 批次总入口：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次总入口.md`
- 批次方案：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0-S6 已完成，C10-M3 队列已关闭。
- S0 live 基线已冻结：`HEAD=f528b8f7f6`（`docs: 新增 C10-M3 ReferenceShadow native recovery 方案`），S0 起始 `git -c core.quotepath=false status --short -uall` 为空；`C10M3-BLOCKER-000=closed_s0`，`C10M3-SCOPE-001=baseline_frozen_s0`。
- 工作步骤总入口已标 `【已实现】`，它只是队列索引，避免 goal runner 把索引当成实现步骤。
- `C10M3-BLOCKER-101=closed_s1`；`C10M3-SRC-101..204` 已复核 live FreeCAD / current cad-core path、symbol、evidence、focused tests 与 capability landing，且未升级为 supported 或 backend gap。
- `C10M3-BLOCKER-201=closed_s2`；scope、blocker、non-goal 和 backend-gap 矩阵已完成范围准入，当前只有 native oracle、current comparison、diagnostic 和 release gate 路由，没有 implementation row。
- `C10M3-BLOCKER-301=closed_s3`；S3 用 `/home/user/.local/bin/freecadcmd` 安全重跑现有 C7-M4 FCStd/XML restore probe 到 `/tmp/c10m3-c7m4-reference-shadow-native-probe.evidence.json`，基线为 FreeCAD `1.2.0 revision 20260519` / Libs `1.2.0devR20260519` / OCCT `7.8.1`。restore/recompute 成功，但 Python-visible `Base` 不暴露 `ShadowSub`、`ReferenceShadow` 或 `getSubValues(false/true)`；`C10M3-SCOPE-101..102=notCollected`，`C10M3-CAT-101=notCollected`，未创建 collector、fixtures 或 expected。
- `C10M3-BLOCKER-401=closed_s4`；S4 只复审 current `cad-core` parser / recovery / ElementMap diagnostics / focused tests。因 S3 已关闭为 `notCollected` 且没有 C10-M3 collector、fixture 或 expected，current comparison 不能证明 FreeCAD mismatch；`C10M3-SCOPE-201=release_gate`，`C10M3-SCOPE-202=diagnostic_retained`，`C10M3-CAT-102=release_gate`，未创建 backend gap 或 implementation row。
- `C10M3-BLOCKER-501=closed_s5`；S5 只复审 `elementReferenceUpdates`、single-subshape `ReferenceShadow.brep`、split / deleted diagnostics、adapter boundary 和 `C10M3-NG-001..007` reopen condition。结论为 `C10M3-SCOPE-202=diagnostic_retained`、`C10M3-SCOPE-301=release_gate`、`C10M3-CAT-103=diagnostic_retained`；未运行 FreeCADCmd，未采 oracle / expected，未修改 `cad-core/src`、tests、fixtures、capability 或 adapter protocol，未新增 implementation row。
- `C10M3-BLOCKER-601=closed_s6`；S6 消费 S3-S5 后确认没有 `backend_gap_candidate`、`backend_gap_requires_implementation` 或 implementation row，发布 docs-only no-code retained diagnostic / release gate。`C10M3-SCOPE-401=release_closed`，`C10M3-CAT-104=release_closed`；未运行 FreeCADCmd，未采 oracle / expected，未修改 `cad-core/src`、tests、fixtures、capability 或 adapter protocol。
- `C10M3-NG-001..007` forbidden claims / reopen condition 已确认完整；`C10M3-VAL-000..005` 为 S0 docs / matrix 验收命令，`C10M3-VAL-006..007` 为 S1 source/current scan。
- 本包不声明 stale `ReferenceShadow` / Base recovery 已 supported；后续只有先取得 native observable evidence，再证明 current cad-core mismatch，才能另开 C++。
- CopyOnChange full temporary-document cache 仍是 retained known gap，不属于本包默认入口。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次 docs/CADCore10.0/README.md
git diff --check
```
