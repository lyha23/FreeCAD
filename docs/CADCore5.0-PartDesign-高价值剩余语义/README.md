# CADCore5.0 PartDesign 高价值剩余语义

本目录承接 CADCore4.0 freeze 后留下的 PartDesign 具体边界，只推进前端 CAD 运行时高价值、可 oracle、可 cad-core 分层落地的剩余语义。

## 入口

- 总览方案：`6-20-10-47-【已实现】CADCore5.0-PartDesign高价值剩余语义总览方案.md`
- 全局 source 候选：`矩阵/cadcore5_source_candidates.tsv`
- 全局 scope 矩阵：`矩阵/cadcore5_scope_review_matrix.tsv`
- 全局 blocker 队列：`矩阵/cadcore5_blocker_queue.tsv`
- fixture / oracle 矩阵：`矩阵/cadcore5_fixture_oracle_matrix.tsv`
- non-goal registry：`矩阵/cadcore5_non_goal_registry.tsv`
- 验收矩阵：`矩阵/cadcore5_validation_matrix.tsv`
- C5-M5 freeze summary：`C5-M5-Freeze收口主线/6-20-12-45-C5-M5-freeze收口总结.md`

## 专题包

| 专题包 | 入口 | 队列 |
| --- | --- | --- |
| C5-M0 范围审计与基线重建 | `C5-M0-范围审计与基线重建主线/6-20-10-48-C5-M0范围审计与基线重建主线总入口.md` | `C5-M0-范围审计与基线重建主线/工作步骤细分/` |
| C5-M1 Revolution / Groove 参数补完 | `C5-M1-RevolutionGroove参数补完主线/6-20-10-48-C5-M1-RevolutionGroove参数补完主线总入口.md` | `C5-M1-RevolutionGroove参数补完主线/工作步骤细分/` |
| C5-M2 Boolean / Body ownership | `C5-M2-Boolean-BodyOwnership主线/6-20-10-48-C5-M2-Boolean-BodyOwnership主线总入口.md` | `C5-M2-Boolean-BodyOwnership主线/工作步骤细分/` |
| C5-M3 Loft / Pipe 高级分支 | `C5-M3-LoftPipe高级分支主线/6-20-10-48-C5-M3-LoftPipe高级分支主线总入口.md` | `C5-M3-LoftPipe高级分支主线/工作步骤细分/` |
| C5-M4 Datum Attachment 引用稳定 | `C5-M4-DatumAttachment-引用稳定主线/6-20-10-48-C5-M4-DatumAttachment引用稳定主线总入口.md` | `C5-M4-DatumAttachment-引用稳定主线/工作步骤细分/` |
| C5-M5 Freeze 收口 | `C5-M5-Freeze收口主线/6-20-10-48-C5-M5-Freeze收口主线总入口.md` | `C5-M5-Freeze收口主线/工作步骤细分/` |
| C5-M6 Part Workbench Surface Profile / PostProcess 第二批（已收口） | `C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线总入口.md` | `C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/` |

C5-M6 已完成最终发布收口：`part_workbench.loft` 只发布 profile / `Linearize=true` expected-backed slice，剩余 `complex_profile_family` 路由到 `future_loft_complex_profile_family`；`part_workbench.sweep` 只发布 multi-profile / `Linearize=true` expected-backed slice，advanced contract 路由到 `future_sweep_advanced_contract`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M0-范围审计与基线重建主线/工作步骤细分 --format markdown
```
