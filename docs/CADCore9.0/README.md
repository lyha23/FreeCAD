# CADCore9.0

CADCore9.0 承接 C8-M7 之后的下一轮 CAD Core 收口工作。C8-M1 到 C8-M7 队列已经清空，`topo_history.producer_matrix.import_shape.remaining=[]`，当前 live capability 里唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但该项已被 C8-M2 / C8-M6 裁为 `oracle_blocked` / `known_gap_diagnostic`，不作为 CADCore9.0 的默认实现入口。

C9-M1 转向 Assembly Joint marker / `offsetPlc` request-local 扩面。目标不是完整 Assembly session，也不是 GUI solver 生命周期，而是沿 FreeCAD `AssemblyObject::handleOneSideOfJoint()`、`runPreDrag()`、`setNewPlacements()` 的同一调用链，复核并补齐当前 capability 中仍标为 non-goal / oracle-blocked 的 request-local marker 和 placement 边界。

C9-M2 承接 C9-M1 no-code closure，不再把后续拆成单个 oracle case，而是把同一 Assembly request-local solver 调用链里的 bundled `offsetPlc` native oracle、custom placement-chain expected 激活、zero Angle fallback oracle 和 diagnostics guard 作为最小完整语义批次处理。本包已在 S6 关闭 request-local bundled offset 和 exact-zero Angle fallback 的实现闸门；不会重开 C9-M1，也不会把 oracle-only row 直接写成 supported 或 backendGap。

C9-M3 承接 C9-M2 queue-empty 后的剩余 Assembly DistanceType 发布边界。本包已在 S6 消费 expected-backed backend gap：`PointCurve` 进入 supported，`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 只按 checked-in native expected 映射为 `ASMTPlanarJoint + offset`；缺 oracle 的 default families 仍留在 `default_or_todo_boundaries`，不继承 supported。

## 入口

- C9-M1 总入口：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线总入口.md`
- C9-M1 方案：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal方案.md`
- C9-M1 工作步骤：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分/`
- C9-M1 矩阵：`C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/`
- C9-M2 总入口：`C9-M2-AssemblyRequestLocalSolverOracle批次/6-27-22-03-C9-M2-AssemblyRequestLocalSolverOracle批次总入口.md`
- C9-M2 方案：`C9-M2-AssemblyRequestLocalSolverOracle批次/6-27-22-03-C9-M2-AssemblyRequestLocalSolverOracle批次方案.md`
- C9-M2 工作步骤：`C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分/`
- C9-M2 矩阵：`C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/`
- C9-M3 总入口：`C9-M3-AssemblyDistanceTypeDefaultBoundary批次/6-28-00-06-C9-M3-AssemblyDistanceTypeDefaultBoundary批次总入口.md`
- C9-M3 方案：`C9-M3-AssemblyDistanceTypeDefaultBoundary批次/6-28-00-06-C9-M3-AssemblyDistanceTypeDefaultBoundary批次方案.md`
- C9-M3 工作步骤：`C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分/`
- C9-M3 矩阵：`C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/`

## 当前状态

- live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ebd6fd1243`（`ebd6fd1243 docs: 新增 C9-M1 Assembly Joint marker 方案包`），S0 开始 `git -c core.quotepath=false status --short -uall` 无输出。
- C8-M1 到 C8-M7 工作步骤队列已由 S0 复核为空：各 `step_goal_queue.py` 输出均只有 Markdown 表头，无 pending 行。
- current capability：`assembly.ondsel_solver_adapter.status=covered_full`、`available=true`，`subshape_marker_placement.status=covered_representative_subset`，bundled `offsetPlc` marker / writeback 已进入 request-local covered subset，当前 Assembly non-goal 保留 `non_assembly_link_subshape_primitive_frame_generalization`。
- current capability 已覆盖 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local real Ondsel adapter、basic / extended DistanceType、subshape marker placement、`runPreDrag` 和 `documentObjectUpdates.action=assembly_set_placement`。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 保持 C8 known gap / `oracle_blocked`，不进入 C9-M1 实现范围。
- C9-M1 不重开完整 FreeCAD Link 账本、ShowElement 持久写回事务、cross-document lifecycle、GUI / ViewProvider / Worker / WASM / Web，也不引入跨请求 solver session。
- C9-M1 已在 `HEAD=d52cd67a19` 后完成 no-code release gate，队列为空；C9-M2 当前为 Assembly request-local solver oracle 批次，目标是批量采集 / 激活 Assembly request-local solver oracle evidence，再按 native oracle 与 current mismatch 决定是否打开 C++ implementation gate。
- C9-M2 S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d52cd67a19`（`d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`）。S0 起始 `git -c core.quotepath=false status --short -uall` 显示仅 `docs/CADCore9.0/README.md` 和未提交的 `docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/**` seed 文档 / 矩阵 / step 文件。
- S0 复核后的 current capability 口径不变：`assembly.remaining_gaps=[]`，`assembly.ondsel_solver_adapter.status=covered_full`，`subshape_marker_placement.status=covered_representative_subset` 且 `subshape_marker_placement.remaining_gaps=[]`，`placement_writeback.status=covered_full`。
- S0 forbidden claims：`non_identity_bundled_offsetPlc` 仍是 native oracle candidate / forbidden guessing，`non_assembly_link_subshape_primitive_frame_generalization` 仍是 diagnostic non-goal，zero Angle fallback 仍缺 native expected；这些行不得在 C9-M2 S0 写成 supported 或 backendGap。
- C9-M2 S1 已关闭 source authority：S1 live 基线为 `HEAD=8dc1ec2ccd`（`8dc1ec2ccd docs: 关闭 C9-M2 S0 基线冻结`），`source_candidates` 已固化 `getMbDData()` bundled `objectPartMap.offsetPlc` 生产、`handleOneSideOfJoint()` 的 `data.offsetPlc * plc` marker 消费、`validateNewPlacements()` / `setNewPlacements()` 的 `getMbdPlacement(mbdPart) * offsetPlc` writeback 消费、Angle 0/2pi fallback source、cad-core marker/writeback/capability/test/fixture 落点；S1 未采 native oracle、未改 cad-core source / fixtures / expected / tests。
- C9-M2 S2 已关闭 scope route：S2 live 基线为 `HEAD=87f289aaba`（`87f289aaba docs: 关闭 C9-M2 S1 源码候选矩阵`），`C9M2-SCOPE-101/102/103` 到 S3 native oracle，`C9M2-SCOPE-201` 到 S4 expected activation，`C9M2-SCOPE-301/302` 到 S5 zero Angle known-gap/native oracle 与 diagnostics guard review，`C9M2-SCOPE-303/304` 到 S6 release gate，`C9M2-SCOPE-401/402` 与 `C9M2-NG-001..006` 保持 diagnostic non-goal / forbidden claims；S2 未采 native oracle、未改 cad-core source / fixtures / expected / tests。
- C9-M2 S3 已关闭 bundled `offsetPlc` native oracle：三条 expected 均证明 `offsetPlc=[2,0,0]` 非 identity，当前 cad-core writeback mismatch 交 S6 消费；S3 未改 C++ solver。
- C9-M2 S4 已关闭 custom placement-chain expected activation：`assembly-marker-custom-placement-chain-real-solver` 已进入 `test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch` focused test，测试直接断言 `native_marker_oracle` 与 `offset_boundary=identity_offset_for_two_box_assembly_link_fixture`，不把该 identity boundary 误写成 S3 non-identity bundled offset coverage。
- C9-M2 S5 已关闭 zero Angle fallback 与 diagnostics 复审：`assembly-angle-zero-and-signed-current-real-solver.freecad.json` 已由 FreeCADCmd 1.2.0 revision 20260519 采集，native solver return 0、`AngleZeroJoint.angle=0.0`、native current XY angle 为 0，证明 exact-zero Angle native route；current cad-core 对同 fixture 保留 `Angle=0` DTO 和 subshape marker evidence，但 solver 返回 `ondsel_solver_failed` 且无 placement update，因此 `C9M2-SCOPE-301/C9M2-BG-301` 路由为 `backend_gap_candidate` 交 S6。S5 focused tests 同时证明 unsupported JointType、PointCurve/default boundary、missing grounded 和 `ondsel_solver_failed` 不会静默变成 success；`invalid_assembly_solver_result` 仍由 adapter capability guard 发布。
- C9-M2 S6 已关闭 oracle 实现与发布闸门：三条 bundled `offsetPlc` expected 已与 current parity，object/subshape marker 均按 `[0.25,0.5,0.75] -> [2.25,0.5,0.75]` 应用 request-local offset，`ComponentC` writeback 为 `[6,0,2]`；zero Angle expected-backed route 现在解出 `ComponentB=[4,0,4]`，旧 `ondsel_solver_failed` 不再作为该 fixture 的 supported route。`C9M2-BLOCKER-601` 已关闭，`non_identity_bundled_offsetPlc` 从 capability non-goals 移除，primitive frame generalization 保持 diagnostic non-goal。
- C9-M3 已关闭：S0-S5 完成 source / scope / oracle / publication gate，S6 在 `dd933c7f91 docs: 关闭 C9-M3 S5 发布准入` 后落代码与发布面。`PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` expected-backed rows 当前与 cad-core parity，5 个 accepted expected 不再带 stale `DTE-NG-003` metadata；`native_expected_count=18`。`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 仍缺 input/expected，保持 `notCollected` / `native_oracle_required`。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```
