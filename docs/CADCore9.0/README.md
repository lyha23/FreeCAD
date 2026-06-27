# CADCore9.0

CADCore9.0 承接 C8-M7 之后的下一轮 CAD Core 收口工作。C8-M1 到 C8-M7 队列已经清空，`topo_history.producer_matrix.import_shape.remaining=[]`，当前 live capability 里唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但该项已被 C8-M2 / C8-M6 裁为 `oracle_blocked` / `known_gap_diagnostic`，不作为 CADCore9.0 的默认实现入口。

C9-M1 转向 Assembly Joint marker / `offsetPlc` request-local 扩面。目标不是完整 Assembly session，也不是 GUI solver 生命周期，而是沿 FreeCAD `AssemblyObject::handleOneSideOfJoint()`、`runPreDrag()`、`setNewPlacements()` 的同一调用链，复核并补齐当前 capability 中仍标为 non-goal / oracle-blocked 的 request-local marker 和 placement 边界。

## 入口

- C9-M1 总入口：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线总入口.md`
- C9-M1 方案：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal方案.md`
- C9-M1 工作步骤：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分/`
- C9-M1 矩阵：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/`

## 当前状态

- live 基线：`HEAD=a191099068`（`a191099068 fix: 完成 C8-M7 ImportShape capability 发布闸门`），开始工作区需由 S0 复核。
- C8-M1 到 C8-M7 工作步骤队列应为空。
- current capability：`assembly.ondsel_solver_adapter.status=covered_full`，`subshape_marker_placement.status=covered_representative_subset`，当前 non-goals 包含 `non_assembly_link_subshape_primitive_frame_generalization` 与 `non_identity_bundled_offsetPlc`。
- current capability 已覆盖 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local real Ondsel adapter、basic / extended DistanceType、subshape marker placement、`runPreDrag` 和 `documentObjectUpdates.action=assembly_set_placement`。
- C9-M1 不重开完整 FreeCAD Link 账本、ShowElement 持久写回事务、cross-document lifecycle、GUI / ViewProvider / Worker / WASM / Web，也不引入跨请求 solver session。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```
