# C5-M2 Boolean / Body ownership 主线总入口

## 目标

按 FreeCAD `FeatureBoolean.cpp::Boolean::execute()` 与 `Body.cpp::AllowCompound` 推进 Boolean multi-solid、multi-body ownership、Group / BaseFeature / Tip replay 和稳定 diagnostics。

## 必读文件

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/6-20-10-48-C5-M2-Boolean-BodyOwnership方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/矩阵/boolean_body_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/矩阵/boolean_body_blocker_queue.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/工作步骤细分/6-20-10-49-C5-S2-M2-Boolean-BodyOwnership.md`

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M2-Boolean-BodyOwnership主线/工作步骤细分 --format markdown
```
