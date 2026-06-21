# C5-M12-S5 capability 与文档收口

状态：`pending_C5M12-S5_capability_docs_closeout`

## 目标

在 S2-S4 expected、tests、capability 都同步后，关闭 C5-M12 package 和 root C5 matrix。S5 只能在 collectable expected 与 remaining narrowed blockers 都有当前证据后完成。

## 必读

- S0-S4 完成后的全部变更。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- Root C5 矩阵。
- 本包矩阵。
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- 更新 C3 capability gap、C5 README、root C5 matrices。
- 更新本包局部矩阵，关闭 S5 之前 pending rows。
- 本步骤文件和方案文件在实现完成后改名或标题加 `【已实现】`。
- 工作步骤队列返回空。

## 非目标

- 不新增 oracle case。
- 不把仍未采集 expected 的字段写成 expected-backed。
- 不提交 unrelated dirty worktree 文件。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
