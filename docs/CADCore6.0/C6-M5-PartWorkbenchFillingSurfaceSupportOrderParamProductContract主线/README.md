# C6-M5 Part Workbench Filling Surface / SupportOrder / Param Product Contract 主线

本目录承接 C6-M4 之后的下一批 CAD Core 6.0 工作：围绕 `Part.makeFilledFace` helper，把 `part_workbench.filling` 中仍停在 native helper blocker / source-backed known gap 的 Surface、Supports/Orders、显式参数和 non-boundary support/order 行转成 CAD Core request-local product contract。

## 入口

- 主线总入口：`6-24-16-18-C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线总入口.md`
- 方案：`6-24-16-18-C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 方案包已创建；S0 已完成 live 基线与边界复核，S1-S6 待执行。
- 当前矩阵已写入 S0 证据，但不能作为 oracle、实现完成或发布通过证据。
- 本主线不声明 FreeCAD parity，不扩大 full Part surface family。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
```
