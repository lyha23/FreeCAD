# C6-M2 Expected Fixture Regression Recovery 主线

本目录是 CADCore6.0 的第二条主线。它消费 C6-M1 S6 留下的阶段回归失败，把 `tests.test_expected_fixtures` 中的既有 expected fixture 漂移逐项分类、复核 authority，并决定是刷新 expected、修复 C++ / schema、登记 OCCT / 环境 known gap，还是保留明确 blocker。当前 C6-M2 已通过 S6 发布闸门并关闭。

## 为什么先做 C6-M2

C6-M1 已经完成 Pipe product extension 的 focused 验收，但阶段回归仍有 15 个 expected fixture mismatch。C6-M2 已逐项恢复 expected fixture 闸门可信度，S6 阶段回归结果为 `Ran 180 tests in 80.086s`、`OK (skipped=29)`；重型收口结果为 `Ran 216 tests in 88.451s`、`OK (skipped=29)`。

## 主线边界

- 本包只处理 checked-in expected fixture 与当前 cad-core recompute / adapter 输出之间的回归差异。
- 本包不新增 Pipe `Interpolation` / `LawSamples` 几何合同。
- 本包不恢复 C5 broad deferred，也不重开已关闭的 C6-M1 blocker。
- 本包不做 blanket expected refresh；每一行 expected 更新必须有命令输出、owner 分类和删除条件。
- `ORC-013` 只登记为本地 OCCT 7.9.3 imported LinkGroup bbox known environment gap，通过 fixture-local `bbox_delta=0.028` 容纳；不声明 full FreeCAD parity。

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```
