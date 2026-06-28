# C10-M3 ReferenceShadow / ShadowSub Native Recovery 准入批次

本包承接 C10-M2 队列关闭后的下一轮 CAD Core 复核。主题是 stale `ReferenceShadow` / `ShadowSub` old-reference recovery 的 native 可观测性与 request-local 实现准入：先证明 FreeCAD 原生是否能稳定暴露恢复证据，再判断 cad-core 是否存在可实现 mismatch。

当前 C10-M2 已发布 no-code release gate：DressUp / Hole producer history 无 implementation row，cross-feature old-reference recovery 保持 `diagnostic_retained`。因此 C10-M3 默认不是“实现旧引用恢复”，而是用 S0-S6 闸门判断是否有 native observable evidence 足以打开实现。

## 入口

- 批次总入口：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次总入口.md`
- 批次方案：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0-S1 已完成，S2-S6 待执行。
- S0 live 基线已冻结：`HEAD=f528b8f7f6`（`docs: 新增 C10-M3 ReferenceShadow native recovery 方案`），S0 起始 `git -c core.quotepath=false status --short -uall` 为空；`C10M3-BLOCKER-000=closed_s0`，`C10M3-SCOPE-001=baseline_frozen_s0`。
- 工作步骤总入口已标 `【已实现】`，它只是队列索引，避免 goal runner 把索引当成实现步骤。
- `C10M3-BLOCKER-101=closed_s1`；`C10M3-SRC-101..204` 已复核 live FreeCAD / current cad-core path、symbol、evidence、focused tests 与 capability landing，且未升级为 supported 或 backend gap。
- `C10M3-NG-001..007` forbidden claims / reopen condition 已确认完整；`C10M3-VAL-000..005` 为 S0 docs / matrix 验收命令，`C10M3-VAL-006..007` 为 S1 source/current scan。
- 本包不声明 stale `ReferenceShadow` / Base recovery 已 supported；只有 S3-S5 证明 native observable evidence + current mismatch 后，S6 才能打开 C++。
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
