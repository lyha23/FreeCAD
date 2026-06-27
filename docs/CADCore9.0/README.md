# CADCore9.0

CADCore9.0 承接 C8-M7 之后的下一轮 CAD Core 收口工作。C8-M1 到 C8-M7 队列已经清空，`topo_history.producer_matrix.import_shape.remaining=[]`，当前 live capability 里唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但该项已被 C8-M2 / C8-M6 裁为 `oracle_blocked` / `known_gap_diagnostic`，不作为 CADCore9.0 的默认实现入口。

C9-M1 转向 Assembly Joint marker / `offsetPlc` request-local 扩面。目标不是完整 Assembly session，也不是 GUI solver 生命周期，而是沿 FreeCAD `AssemblyObject::handleOneSideOfJoint()`、`runPreDrag()`、`setNewPlacements()` 的同一调用链，复核并补齐当前 capability 中仍标为 non-goal / oracle-blocked 的 request-local marker 和 placement 边界。

C9-M2 承接 C9-M1 no-code closure，不再把后续拆成单个 oracle case，而是把同一 Assembly request-local solver 调用链里的 bundled `offsetPlc` native oracle、custom placement-chain expected 激活、zero Angle fallback oracle 和 diagnostics guard 作为最小完整语义批次处理。本包只冻结和采集 Assembly request-local solver oracle evidence；不会重开 C9-M1，也不会把 oracle-only row 直接写成 supported 或 backendGap。

## 入口

- C9-M1 总入口：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线总入口.md`
- C9-M1 方案：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal方案.md`
- C9-M1 工作步骤：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分/`
- C9-M1 矩阵：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/`
- C9-M2 总入口：`C9-M2-AssemblyRequestLocalSolverOracle批次/6-27-22-03-C9-M2-AssemblyRequestLocalSolverOracle批次总入口.md`
- C9-M2 方案：`C9-M2-AssemblyRequestLocalSolverOracle批次/6-27-22-03-C9-M2-AssemblyRequestLocalSolverOracle批次方案.md`
- C9-M2 工作步骤：`C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分/`
- C9-M2 矩阵：`C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/`

## 当前状态

- live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ebd6fd1243`（`ebd6fd1243 docs: 新增 C9-M1 Assembly Joint marker 方案包`），S0 开始 `git -c core.quotepath=false status --short -uall` 无输出。
- C8-M1 到 C8-M7 工作步骤队列已由 S0 复核为空：各 `step_goal_queue.py` 输出均只有 Markdown 表头，无 pending 行。
- current capability：`assembly.ondsel_solver_adapter.status=covered_full`、`available=true`，`subshape_marker_placement.status=covered_representative_subset`，当前 non-goals 包含 `non_assembly_link_subshape_primitive_frame_generalization` 与 `non_identity_bundled_offsetPlc`。
- current capability 已覆盖 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local real Ondsel adapter、basic / extended DistanceType、subshape marker placement、`runPreDrag` 和 `documentObjectUpdates.action=assembly_set_placement`。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 保持 C8 known gap / `oracle_blocked`，不进入 C9-M1 实现范围。
- C9-M1 不重开完整 FreeCAD Link 账本、ShowElement 持久写回事务、cross-document lifecycle、GUI / ViewProvider / Worker / WASM / Web，也不引入跨请求 solver session。
- C9-M1 已在 `HEAD=d52cd67a19` 后完成 no-code release gate，队列为空；C9-M2 当前为 Assembly request-local solver oracle 批次，目标是批量采集 / 激活 Assembly request-local solver oracle evidence，再按 native oracle 与 current mismatch 决定是否打开 C++ implementation gate。
- C9-M2 S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d52cd67a19`（`d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`）。S0 起始 `git -c core.quotepath=false status --short -uall` 显示仅 `docs/CADCore9.0/README.md` 和未提交的 `docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/**` seed 文档 / 矩阵 / step 文件。
- S0 复核后的 current capability 口径不变：`assembly.remaining_gaps=[]`，`assembly.ondsel_solver_adapter.status=covered_full`，`subshape_marker_placement.status=covered_representative_subset` 且 `subshape_marker_placement.remaining_gaps=[]`，`placement_writeback.status=covered_full`。
- S0 forbidden claims：`non_identity_bundled_offsetPlc` 仍是 native oracle candidate / forbidden guessing，`non_assembly_link_subshape_primitive_frame_generalization` 仍是 diagnostic non-goal，zero Angle fallback 仍缺 native expected；这些行不得在 C9-M2 S0 写成 supported 或 backendGap。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```
