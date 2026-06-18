# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S6 Capability 与发布闸门

## 目标

在 S3-S5 通过后，发布 subshape marker placement supported subset，并确认 radius-bearing DistanceType、curve/default、GUI/session、persistent solver state、connector-only subshape marker shortcut 和非 identity bundled `offsetPlc` 没有被误发布。

## 发布结论

- C ABI 已发布 `assembly.ondsel_solver_adapter.subshape_marker_placement`，并把 `subshape_marker_placement` 加入 `covered`。
- `status=covered_representative_subset`，`mode=request_local_handleOneSide_markerPlacement`，`build_mode=CAD_CORE_HAS_ONDSEL_SOLVER=1`。
- `supported_reference_kinds=["object","Vertex","Edge","Face","mixed"]`。
- `covered` 只列 S3-S5 已证明的 object baseline、Vertex / Edge / Face JCS marker、mixed swap marker sync、real Ondsel marker consumption 和 native `placement_updates` parity。
- `active_expected_count=15`，`active_expected_groups=["S4/S5 c3m6 native marker expected"]`，`remaining_gaps=[]`。
- `request_local_boundaries` 仅发布 `identity_offset_assembly_link_subset` 和 `request_graph_no_persistent_solver_state`。
- `non_goals` 保留 `radius_bearing_distance_type`、`curve_default_distance_type`、`GUI/session`、`persistent_solver_state`、`connector_only_subshape_marker_shortcut`、`non_identity_bundled_offsetPlc`。
- DistanceType PointLine capability 已对齐 checked-in native parity：`solver_joint_classes.PointLine=["ASMTLineInPlaneJoint"]`，并保留 FreeCADCmd `tInPlaneJointE` / `offset` 与 legacy C++ source switch 的说明。

## 矩阵结论

- `MP-BLOCK-008` 已由 capability / adapter test / docs 同步关闭。
- `MP-BLOCK-009` 已由 capability non-goals 与 matrix boundary audit 关闭。
- `MP-SCOPE-013` 从 release gate 发布为 representative subset。
- `MP-SCOPE-014` 继续保持 `nonGoal`，关闭条件是 capability 已明确排除。
- `MP-BG-011` 已发布，`MP-BG-012` 已审计排除。

## 验收

```bash
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
rg -n 'subshape_marker_placement|radius-bearing|curve/default|persistent_solver_state|connector_only_subshape_marker_shortcut' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
rg -n 'MP-BLOCK-00[1-9]|MP-BLOCK-010|connector_only_marker_shortcut' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
```

本轮验证结论：

- `cmake --build cad-core/build` 通过；用于刷新 C ABI FFI 输出。
- `python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities` 通过。
- step queue 输出为空表头，S6 不再 pending。
- TSV field count 无输出，矩阵列数一致。
- 关键边界 grep 有命中，`git diff --check` 通过。

## 非目标

- 不发布 radius-bearing DistanceType。
- 不发布 curve/default DistanceType。
- 不发布 GUI/session 或 persistent solver state。
- 不发布 connector-only subshape marker placement。
- 不发布非 identity bundled `offsetPlc` 泛化支持。
