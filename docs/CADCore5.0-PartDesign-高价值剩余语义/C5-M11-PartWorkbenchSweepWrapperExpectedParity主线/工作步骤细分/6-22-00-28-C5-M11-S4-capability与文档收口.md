# C5-M11-S4 capability 与文档收口

状态：`pending_C5M11-S4_docs_closeout`

## 目标

在 S2/S3 expected、tests、capability 都同步后，关闭 C5-M11 文档和 root matrix。S4 只能在 `part_sweep_wrapper_expected_collector` 已从 capability remaining gaps 删除或被精确缩窄后完成。

## 必读

- S0-S3 完成后的全部变更。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- 本包 `矩阵/*.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- `docs/CADCore3.0/capabilities-gap对照表.md` 同步 expected-backed wrapper parity，删除或精确缩窄 `part_sweep_wrapper_expected_collector` 描述。
- Root C5 矩阵中 `C5-BLK-1101`、`C5-SCOPE-1101`、`C5-ORC-1101..1104`、`C5-VAL-1101..1105` 更新为最终状态。
- 本包局部矩阵关闭 S4 之前的 pending rows。
- 本步骤文件和方案文件在实现完成后改名或标题加 `【已实现】`。
- 工作步骤队列返回空。

## 非目标

- 不借 S4 新增 collector case。
- 不把未采集 expected 的字段写成 expected-backed。
- 不提交 unrelated dirty worktree 文件。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m10 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
