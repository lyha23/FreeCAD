# C5-M13-S2 Sweep location / combined wrapper 收窄记录

状态：`done_C5M13-S2_sweep_location_combined_narrowed`

## 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`9c0d5fe23e`
- `git log -1 --oneline`：`9c0d5fe23e docs: 完成 C5-M13 S1 blocker probe 分类`
- 起始 `git -c core.quotepath=false status --short -uall`：存在 unrelated `cad-core/src/part_design/*`、`cad-core/src/runtime/recompute.cpp`、`cad-core/tests/test_p6_topology.py`、`cad-core/tests/test_p7_features.py`、`docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`、`docs/BUG修改/*` 等脏改动，本轮不触碰。
- `FreeCADCmd --version`：`FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`
- 队列确认：`step_goal_queue.py .../C5-M13.../工作步骤细分 --format markdown` 从 `6-22-04-05-C5-M13-S2-sweepLocationCombinedWrapper修复.md` 开始。

## FreeCAD 依据

- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)`：location overload 解析 `TopoShapeVertexPy` 后调用 `BRepOffsetAPI_MakePipeShell::Add(s, v, ...)`。
- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` 和 `setTolerance()`：combined 只是在同一个 builder 上叠加 auxiliary/tolerance 后再进入 `add(Profile, Location, ...)`。

## Probe 入口

脚本：

`docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py`

命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py', encoding='utf-8').read(), 'c5m13_s2_sweep_probe.py', 'exec'))"
```

## 结论

| case 族 | 覆盖 | 结果 | 分类 |
| --- | --- | --- | --- |
| located representatives | `located_free_vertex`、`located_profile_owned_vertex`、`located_profile_coordinate_free_vertex`、`located_spine_owned_vertex`、`located_open_wire_profile` | 全部 `is_ready_before_build=true`、`status_before_build=0`，在 `builder.build()` 报 `OCCError: NCollection_Array1::Value` | location overload build-stage native-runtime blocker |
| call order / lifecycle | `located_add_before_frenet`、`located_add_before_transition`、`located_no_frenet`、`located_tolerance_before_add` | 全部在 `builder.build()` 报同一错误 | 不是 wrapper call-order / builder lifecycle 可修 |
| invalid profile representatives | `located_edge_profile`、`located_face_profile` | `add()` 阶段报 `BRepFill_Section: bad shape type of section` | 不可作为 valid expected representative |
| no-location controls | `plain_control`、`combined_no_location_control` | `build_ok=true`，可访问 `shape()`，Shell `faces=4, edges=12` | 证明 auxiliary/tolerance/no-location wrapper path 可采 |
| combined with Location | `combined_aux_tolerance_add`、`combined_tolerance_aux_add`、`combined_add_aux_tolerance`、`combined_aux_add_tolerance` | 全部在 `builder.build()` 报 `OCCError: NCollection_Array1::Value` | combined blocker 依赖 location overload |

S2 不生成或伪造 expected。两个 checked-in expected 继续保留 `known_gap`，但 metadata 已收窄为：

- free vertex 与 profile-owned vertex 都失败；
- call-order / tolerance / auxiliary 顺序变体也失败；
- 错误发生在 `build` 阶段，早于 `shape()`；
- combined no-location control 成功，combined blocker 只依赖 `add(Profile, Location, WithContact, WithCorrection)` overload。

## Delete Condition

- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker`：只有当 FreeCADCmd 对 wire profile 的 `add(Profile, Location, WithContact, WithCorrection)` 返回稳定 `shape_summary`，或上游 FreeCAD/OCCT 修复该 overload 的 build-stage failure 后删除。
- `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`：只有当 location overload 已可采，并且同一 request-local helper 能采到 auxiliary + located section + tolerance 的 combined metadata 与 `shape_summary` 后删除。

## 验收

- `cd cad-core && python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported`：通过，`processed=0 skipped=0 failed=0`。
- `cd cad-core && python3 -m unittest tests.test_p8_features tests.test_expected_fixtures`：通过，`Ran 202 tests ... OK (skipped=29)`。
- `git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core`：通过。
- `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown`：S2 不再出现，队列从 S3 开始。
