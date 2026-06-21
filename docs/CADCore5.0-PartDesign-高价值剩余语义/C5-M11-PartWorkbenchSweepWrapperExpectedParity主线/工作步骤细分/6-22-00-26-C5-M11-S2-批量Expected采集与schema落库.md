# C5-M11-S2 批量 expected 采集与 schema 落库

状态：`pending_C5M11-S2_expected_batch`

## 目标

用 S1 的同一 wrapper collector 批量替换 C5-M10 六个 source-backed known_gap expected。S2 是本包核心，不允许只处理一个 fixture 后把其余字段继续留成 broad collector gap。

## 必读

- S1 完成后的 collector helper。
- `cad-core/fixtures/c5m10/part-sweep-auxiliary-spine-contract.json`
- `cad-core/fixtures/c5m10/part-sweep-binormal-contract.json`
- `cad-core/fixtures/c5m10/part-sweep-support-mode-diagnostics.json`
- `cad-core/fixtures/c5m10/part-sweep-located-profile-contract.json`
- `cad-core/fixtures/c5m10/part-sweep-tolerance-contract.json`
- `cad-core/fixtures/c5m10/part-sweep-advanced-combined-contract.json`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- 替换六个 `cad-core/fixtures/c5m10/expected/*.freecad.json` 中 collectable 的 `known_gap` payload，写入 FreeCADCmd wrapper expected。
- 每个 expected 至少包含 `shape_summary` 和 `object_fields.advanced`；support diagnostics fixture 可对 invalid payload 保留 diagnostic-backed 子项。
- `collect_freecad_expected.py --phase c5m10 --check --skip-unsupported` 能稳定通过，不因为 wrapper helper 输出顺序或非确定 metadata 抖动。
- 如果某个代表场景确实无法采集，必须在本步骤和矩阵中写明 FreeCADCmd 错误、未采字段、下一批范围和为什么不影响其它五个 expected 批量替换。
- 更新 `C5M11-BLK-201`、`C5M11-SCOPE-201`、`C5M11-ORC-201`。

## 非目标

- 不用 cad-core recompute 结果生成 expected。
- 不为了通过 expected 检查改 cad-core 几何语义。
- 不放宽 expected fixture 检查。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m10 --check --skip-unsupported
python3 -m unittest tests.test_expected_fixtures tests.test_p8_features
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```
