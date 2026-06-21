# C5-M13-S2 Sweep location / combined wrapper 修复

状态：`done_C5M13-S2_sweep_location_combined_narrowed`

## 目标

基于 S1 分类处理 Sweep wrapper 的两个 remaining blockers：`SectionOptions[].Location/WithContact/WithCorrection` 与 advanced combined。优先修 FreeCADCmd collector 的代表 shape / location vertex / call order；如果仍是 OCCT runtime blocker，必须把 blocker 收窄到精确条件。

## 必读

- S1 probe 矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c5m10/part-sweep-located-profile-contract.json`
- `cad-core/fixtures/c5m10/part-sweep-advanced-combined-contract.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`

## 产物

- 可采集时：新增或更新 c5m13 / c5m10 expected，包含 `shape_summary` 与 `object_fields.advanced.sections` / combined metadata。
- 不可采集时：更新 expected blocker metadata，记录更窄错误条件、复现命令、delete condition。
- Focused tests 覆盖 expected 或 narrowed blocker，不允许恢复 broad `part_sweep_wrapper_expected_collector`。
- 更新局部矩阵 `C5M13-BLK-201`、`C5M13-SCOPE-201`、`C5M13-ORC-201` 和 root `C5-ORC-1303`。

## S2 收口

- 新增 probe：`../docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py`。
- 记录：`../docs/temp/6-22-04-46-C5-M13-S2-sweepLocationCombinedWrapper收窄记录.md`。
- 结论：可采 no-location controls 成功；所有带 Location 的 free/profile/spine/open-wire representatives 和 call-order variants 都在 `builder.build()` 阶段报 `OCCError: NCollection_Array1::Value`。
- 处理：不伪造 expected；更新 `c5m10` 两个 expected 的 `known_gap.freecadcmd_evidence`，并用 focused tests 锁住 `failed_stage=build`、failing cases、successful controls 和 combined 的 `depends_on`。

## 非目标

- 不改变 native `Part::Sweep` direct property support。
- 不改 PartDesign Pipe / Hole product capability。
- 不用 cad-core 输出反推 expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```
