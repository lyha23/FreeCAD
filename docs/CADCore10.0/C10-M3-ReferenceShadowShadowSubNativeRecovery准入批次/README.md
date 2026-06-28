# C10-M3 ReferenceShadow / ShadowSub Native Recovery 准入批次

本包承接 C10-M2 队列关闭后的下一轮 CAD Core 复核。主题是 stale `ReferenceShadow` / `ShadowSub` old-reference recovery 的 native 可观测性与 request-local 实现准入：先证明 FreeCAD 原生是否能稳定暴露恢复证据，再判断 cad-core 是否存在可实现 mismatch。

当前 C10-M2 已发布 no-code release gate：DressUp / Hole producer history 无 implementation row，cross-feature old-reference recovery 保持 `diagnostic_retained`。因此 C10-M3 默认不是“实现旧引用恢复”，而是用 S0-S6 闸门判断是否有 native observable evidence 足以打开实现。

## 入口

- 批次总入口：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次总入口.md`
- 批次方案：`6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0-S6 均为待执行。
- 工作步骤总入口已标 `【已实现】`，它只是队列索引，避免 goal runner 把索引当成实现步骤。
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
