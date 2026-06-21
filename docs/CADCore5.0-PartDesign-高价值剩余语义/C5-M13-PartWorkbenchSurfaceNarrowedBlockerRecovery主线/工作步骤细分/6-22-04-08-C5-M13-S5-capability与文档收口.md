# C5-M13-S5 capability 与文档收口

状态：`pending_C5M13-S5_docs_closeout`

## 目标

在 S2-S4 的 expected / blocker / tests 都完成后，收口 capability metadata、C3 gap 文档、C5 root 矩阵和本包局部矩阵。S5 的核心是让 `part_workbench.sweep/filling/geomplate` 的 remaining gaps 只保留真实、精确、可复现的 blocker，不再有 broad wording。

## 必读

- S0-S4 完成后的全部变更。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- `cad-core/src/adapters/c_api/c_api.cpp` capability metadata 与 focused tests 同步 expected-backed / diagnostic-only / narrowed blocker 状态。
- `docs/CADCore3.0/capabilities-gap对照表.md` 更新 C5-M13 结论。
- Root `C5-SRC-013`、`C5-SCOPE-1301`、`C5-BLK-1301`、`C5-ORC-1301..1306`、`C5-VAL-1301..1306` 更新为最终状态。
- 本包局部矩阵关闭 S5 之前的 pending rows。
- 本方案和 S5 步骤实现完成后改名或标题加 `【已实现】`，队列返回空。

## 非目标

- 不新增 S2-S4 之外的新 surface family scope。
- 不把 uncollectable blocker 写成 expected-backed。
- 不提交 unrelated dirty worktree 文件。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
