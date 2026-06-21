# C5-M11-S1 FreeCAD wrapper collector 设计

状态：`pending_C5M11-S1_wrapper_collector_design`

## 目标

为 `cad-core/tools/collect_freecad_expected.py` 设计并探测 request-local `Part.BRepOffsetAPI_MakePipeShell` collector helper。S1 必须证明同一 wrapper API 能覆盖 S2 六个代表场景，不能只为单个 auxiliary case 写临时 probe。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c5m10/*.json`
- 本包 `矩阵/c5m11_sweep_wrapper_expected_parity_source_dto_oracle_contract.tsv`

## 产物

- 新增或设计 `collect_freecad_expected.py` 中的 wrapper helper 分支，识别 C5-M10 advanced sweep fixture。
- 明确 helper 如何构造 spine、profile、auxiliary spine、support face/shape、location vertex，并按 fixture 字段调用 `setAuxiliarySpine`、`setSpineSupport`、`setBiNormalMode`、`add`、`setTolerance`、`setTransitionMode`。
- 固定 `object_fields.advanced` schema 和 builder status 字段。
- 明确 invalid support/mode/location/tolerance 仍由 cad-core focused diagnostics 验收，不强行让 FreeCAD wrapper collector 消费无效 payload。
- 更新 `C5M11-BLK-101`、`C5M11-SCOPE-101`、`C5M11-ORC-101`。

## 非目标

- 不把 wrapper helper 做成 persistent lifecycle。
- 不修改 FreeCAD upstream source。
- 不把 native `Part::Sweep` collector 扩展成 advanced direct properties。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core/tools/collect_freecad_expected.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```
