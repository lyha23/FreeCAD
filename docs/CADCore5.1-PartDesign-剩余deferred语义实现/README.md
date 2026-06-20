# CADCore5.1 PartDesign 剩余 deferred 语义实现

本包承接 `docs/CADCore5.0-PartDesign-高价值剩余语义` freeze 后用户明确要求重开的剩余项：除 GUI Attachment editor 外，C5 deferred / non-goal 边界全部进入 C5.1 实现计划。

## 入口

- 总入口：`6-20-13-37-CADCore5.1-PartDesign剩余deferred语义总入口.md`
- 一揽子方案：`6-20-13-37-CADCore5.1-PartDesign剩余deferred语义一揽子方案.md`
- 范围矩阵：`矩阵/cadcore51_scope_review_matrix.tsv`
- blocker 队列：`矩阵/cadcore51_blocker_queue.tsv`
- oracle / fixture 矩阵：`矩阵/cadcore51_oracle_fixture_matrix.tsv`
- request input contract：`矩阵/cadcore51_input_contract_matrix.tsv`
- non-goal registry：`矩阵/cadcore51_non_goal_registry.tsv`
- 验收矩阵：`矩阵/cadcore51_validation_matrix.tsv`

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.1-PartDesign-剩余deferred语义实现/工作步骤细分 --format markdown
```
