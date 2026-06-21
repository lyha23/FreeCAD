# 【已实现】C5-M10-S2 Auxiliary / Support / Binormal 实现

状态：`done_C5M10-S2_aux_support_binormal`

## S2 核查结论

- `cad-core/src/part/part_sweep.cpp` 已按 S1 DTO 解析 `AuxiliarySpine`、`AuxiliaryCurvilinear`、`SpineSupport`、`SupportMode`、canonical `Binormal` 与 legacy `BiNormal`，并保留 object/property/target/subname diagnostics。
- `cad-core/include/cad_core/part/topo_shape_expansion.h` 与 `cad-core/src/part/topo_shape_expansion.cpp` 已把 `SpineSupport` 落到正式 `PipeShellOptions`，通过 `BRepOffsetAPI_MakePipeShell::SetMode(s)` 执行 support mode；Auxiliary 与 Binormal 继续使用正式 `SetMode(...)` 分支。
- 新增 `cad-core/fixtures/c5m10/part-sweep-auxiliary-spine-contract.json`、`part-sweep-binormal-contract.json`、`part-sweep-support-mode-diagnostics.json`，对应 expected 先以 source-backed known_gap 记录 wrapper collector 缺口和删除条件。
- Focused tests 覆盖 valid Auxiliary/Binormal representatives、Support/Auxiliary/Binormal invalid diagnostics，以及 `part_workbench.sweep` capability metadata。`Location` / `Tolerance` / profile contact-correction 仍留给 S3。
- 本包 `C5M10-BLK-201`、`C5M10-SCOPE-201`、`C5M10-ORC-201` 与 root `C5-BLK-1001`、`C5-SCOPE-1001`、`C5-ORC-1003` 已更新为 S2 source/diagnostic-backed 状态。

## 目标

在同一 `PartSweepAdvancedPipeShellDTO` 下批量实现或诊断 AuxiliarySpine、spine support、Binormal 三类 builder mode。S2 不能只挑一个 auxiliary fixture；必须覆盖 valid representative、invalid target/subname/vector diagnostics 和 capability metadata。

## 必读

- S1 完成后的 source / DTO / oracle 矩阵。
- `BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()`、`setSpineSupport()`
- `FeaturePipe.cpp::Pipe::setupAlgorithm()` 中 `Mode=Auxiliary` 与 `Mode=Binormal` 分支。
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`

## 产物

- 新增或更新 `cad-core/fixtures/c5m10/part-sweep-auxiliary-spine-contract.json`、`part-sweep-binormal-contract.json`、`part-sweep-support-mode-diagnostics.json`。
- 能采集 FreeCAD wrapper expected 的场景写 `.freecad.json`；不能采集的场景写 source-backed known_gap，带 `delete_condition`。
- `cad-core/src/part/part_sweep.cpp` 解析同一 DTO 字段，`topo_shape_expansion` 使用正式 `PipeShellOptions` / OCCT builder mode，不在 adapter 层补输出。
- Invalid auxiliary/support/binormal payload 必须给出 locatable diagnostics，包含 object/property/target/subname 或 vector range。
- Focused tests 覆盖 fixtures、diagnostics 和 `part_workbench.sweep` capability metadata。
- 更新 `C5M10-BLK-201`、`C5M10-SCOPE-201`、`C5M10-ORC-201`。

## 非目标

- 不处理 profile Location / Tolerance；那是 S3。
- 不把 PartDesign Pipe capability 状态改成 Part Workbench Sweep support。
- 不使用 bbox / 输出顺序 / fixture 名推断 builder mode。

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
