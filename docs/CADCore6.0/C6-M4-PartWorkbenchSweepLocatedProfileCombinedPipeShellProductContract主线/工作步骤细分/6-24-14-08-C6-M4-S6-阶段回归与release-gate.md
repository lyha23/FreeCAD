# C6-M4-S6 阶段回归与 release gate

## 目标

消费 S0-S5 的所有 blocker、oracle、backendGap、releaseGate 行，执行阶段回归并关闭 C6-M4 发布状态。S6 不再引入新功能；只做回归、状态一致性、文档收口和提交准备。

## release audit

| 检查项 | 通过条件 |
| --- | --- |
| blocker queue | `C6M4-BLK-000/101/102/201/301/501` 均 closed 或有明确非发布 gap 保留。 |
| scope matrix | 每个 `scope_id` 有最终 `current_status`，无悬空 `next_step`。 |
| backend gap classification | `notCollected/backendGap/releaseGate/nonGoal/closed` 分类一致，且有证据。 |
| capability | `part_workbench.sweep` 不声明 FreeCAD parity，不扩大 full Part surface family。 |
| fixtures/tests | c5m10 guard 与 c6m4 product fixtures 同时通过或明确保留 gap。 |
| docs | README、主入口、工作步骤、矩阵、CADCore6.0 README 同步。 |

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

重型收口条件：

- S5 删除了 capability remaining gaps。
- S3/S4 改动了 `makeElementPipeShellFromSources()`、PipeShell maker history 或 NamedShape history。
- expected fixture runner 或 adapter schema 改动。

重型收口：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 验收标准

通过条件：

- S6 文件记录最终验证结果、残留 gap 或发布结论。
- 若 S0-S6 全部完成，按仓库规则把对应步骤文件重命名为 `【已实现】` 并更新所有链接。
- 若 capability 删除 blocker，`cad-core/tests/test_adapters.py` 对 remaining gaps 的断言同步。
- 最终 `git diff --check`、TSV 字段检查、queue 检查通过。
- 完成时按仓库 git 工作流提交本轮相关变更；提交前确认不混入 build 目录或 unrelated files。
