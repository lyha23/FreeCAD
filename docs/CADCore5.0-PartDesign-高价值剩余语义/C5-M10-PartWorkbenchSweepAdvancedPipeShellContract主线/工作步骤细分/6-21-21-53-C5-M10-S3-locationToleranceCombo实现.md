# C5-M10-S3 Location / Tolerance / Combo 实现

状态：`pending`

## 目标

在同一 advanced PipeShell DTO 中补 profile location、`WithContact` / `WithCorrection`、tolerance 和组合压力。S3 应把 location/tolerance/combo 作为同一 API 边界的一批来做，避免 S2 完成后继续留下另一个 broad advanced bucket。

## 必读

- S1/S2 完成后的 DTO、fixtures、tests 和 capability metadata。
- `BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)`
- `BRepOffsetAPI_MakePipeShellPyImp.cpp::setTolerance(tol3d, boundTol, tolAngular)`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`

## 产物

- 新增或更新 `cad-core/fixtures/c5m10/part-sweep-located-profile-contract.json`、`part-sweep-tolerance-contract.json`、`part-sweep-advanced-combined-contract.json`。
- DTO parser 支持 per-section `Location`、`WithContact`、`WithCorrection` 和 `Tolerance` 三元组；invalid location / tolerance 必须有 locatable diagnostics。
- OCCT builder option 落在 `topo_shape_expansion` 或正式 part helper，不在 C ABI adapter 写业务分支。
- Wrapper expected 能稳定采集的场景写 expected；不能采集时写 source-backed known_gap 和删除条件。
- 组合 fixture 覆盖 S2 字段与 S3 字段同时存在时的 builder option、diagnostics 优先级和 capability metadata。
- 更新 `C5M10-BLK-301`、`C5M10-SCOPE-301`、`C5M10-ORC-301`。

## 非目标

- 不重做基础 Sweep geometry。
- 不把 persistent Python wrapper lifecycle 引入 cad-core；只接受 request-local DTO。
- 不修改 C5-M6 closed support 行，除非 capability wording 需要指向 C5-M10 precise boundary。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```
