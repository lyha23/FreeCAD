# CADCore9.0

CADCore9.0 承接 C8-M7 之后的下一轮 CAD Core 收口工作。C8-M1 到 C8-M7 队列已经清空，`topo_history.producer_matrix.import_shape.remaining=[]`，当前 live capability 里唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但该项已被 C8-M2 / C8-M6 裁为 `oracle_blocked` / `known_gap_diagnostic`，不作为 CADCore9.0 的默认实现入口；只有显式准入复审包可以重新判断是否存在 request-local DTO。

C9-M1 转向 Assembly Joint marker / `offsetPlc` request-local 扩面。目标不是完整 Assembly session，也不是 GUI solver 生命周期，而是沿 FreeCAD `AssemblyObject::handleOneSideOfJoint()`、`runPreDrag()`、`setNewPlacements()` 的同一调用链，复核并补齐当前 capability 中仍标为 non-goal / oracle-blocked 的 request-local marker 和 placement 边界。

C9-M2 承接 C9-M1 no-code closure，不再把后续拆成单个 oracle case，而是把同一 Assembly request-local solver 调用链里的 bundled `offsetPlc` native oracle、custom placement-chain expected 激活、zero Angle fallback oracle 和 diagnostics guard 作为最小完整语义批次处理。本包已在 S6 关闭 request-local bundled offset 和 exact-zero Angle fallback 的实现闸门；不会重开 C9-M1，也不会把 oracle-only row 直接写成 supported 或 backendGap。

C9-M3 承接 C9-M2 queue-empty 后的剩余 Assembly DistanceType 发布边界。本包已在 S6 消费 expected-backed backend gap：`PointCurve` 进入 supported，`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 只按 checked-in native expected 映射为 `ASMTPlanarJoint + offset`；缺 oracle 的 default families 仍留在 `default_or_todo_boundaries`，不继承 supported。

C9-M4 承接 C9-M3 queue-empty 后仍公开的 13 个 Assembly DistanceType `default_or_todo_boundaries`。它不重开 C9-M3 accepted rows，而是围绕 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 的缺 input / expected 行建立 native oracle、current comparison、capability publication 和 S6 code gate。S6 已把这 13 行发布为 expected-backed `ASMTPlanarJoint + offset=getJointDistance` supported route。

C9-M5 承接 C9-M4 queue-empty 后的 live capability gap：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。它不直接实现 FreeCAD temporary document cache，而是复审 `SubShapeBinder::setupCopyOnChange()` / `update()`、`LinkBaseExtension::syncCopyOnChange()` 和 `Document::copyObject()` 是否能导出稳定、产品批准、完全 request-local 的 CopyOnChange DTO。若 S3-S5 不能证明 DTO，S6 必须保持 `known_gap_diagnostic` / `oracle_blocked`。

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
- C9-M4 总入口：`C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/6-28-09-34-C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次总入口.md`
- C9-M4 方案：`C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/6-28-09-34-C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次方案.md`
- C9-M4 工作步骤：`C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/工作步骤细分/`
- C9-M4 矩阵：`C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/`
- C9-M5 总入口：`C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/6-28-11-24-C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包总入口.md`
- C9-M5 方案：`C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/6-28-11-24-C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包方案.md`
- C9-M5 工作步骤：`C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分/`
- C9-M5 矩阵：`C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/`

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
- C9-M3 已关闭：S0-S5 完成 source / scope / oracle / publication gate，S6 在 `dd933c7f91 docs: 关闭 C9-M3 S5 发布准入` 后落代码与发布面。`PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` expected-backed rows 当前与 cad-core parity，5 个 accepted expected 不再带 stale `DTE-NG-003` metadata；C9-M4 S6 后 `distance_type_extended_geometry.native_expected_count=31`，新增 13 个 expected-backed default rows 已发布 supported。
- C9-M4 方案包已建立，S0 已关闭：S0 live baseline 为 `pwd=/home/user/Chili3DProject/FreeCAD`、`HEAD=435f3f26b9`（`435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`），起始 status 仅包含 `docs/CADCore9.0/README.md` 修改和本 C9-M4 seed 包未提交；C9-M3 queue 已复核为空，S6 后 current capability 中 `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=31`、`default_or_todo_boundaries=[]`。
- C9-M4 S0 冻结缺 oracle 声明：S0 时 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 均缺 input / expected 并留在 `default_or_todo_boundaries`；S1-S6 需要先采集 native oracle 或记录 notCollected，不得继承 C9-M3 supported，也不得把缺 oracle rows 写成 supported/backendGap。
- C9-M4 S1 已关闭：S1 live baseline 为 `HEAD=825ad7f937`（`825ad7f937 docs: 关闭 C9-M4 S0 基线冻结`）且起始 status clean；FreeCAD `getDistanceType()` source authority 覆盖 13 个 missing rows 的 Face/Face、Vertex/Face、Edge/Face 分类和 `swapJCS` ordering，`makeMbdJointDistance()` default branch 创建 `ASMTPlanarJoint` 并写 `offset=getJointDistance(joint)`。
- C9-M4 S2 已关闭：S2 live baseline 为 `HEAD=50585ed5ae`（`50585ed5ae docs: 关闭 C9-M4 S1 源码覆盖候选`）且起始 status clean；FaceCone 交 S3，Point / Line + Surface 交 S4，CurveSurface 交 S5。13 个缺 oracle rows 只能保持 `native_oracle_required` / `notCollected`，S6 只消费 native expected-backed mismatch；capability/diagnostics 是 release gate / guard，GUI/session/cache/guessing/string rewrite/output pruning 保持 non-goal。
- C9-M4 S3 已关闭：S3 live baseline 为 `HEAD=57630544af`（`57630544af docs: 关闭 C9-M4 S2 范围准入矩阵`）且起始 status clean；`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + placement writeback，四行已由 S6 发布 supported。
- C9-M4 S4 已关闭：S4 live baseline 为 `HEAD=7fd956ea30`（`7fd956ea30 docs: 关闭 C9-M4 S3 FaceCone oracle复审`）且起始 status clean；`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + placement writeback，五行已由 S6 发布 supported。
- C9-M4 S5 已关闭：S5 live baseline 为 `HEAD=aeedc692ab`（`aeedc692ab docs: 关闭 C9-M4 S4 PointLineSurface oracle复审`）且起始 status clean；`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + Face-first `swapJCS` ordering + placement writeback，四行已由 S6 发布 supported；conic publication mirror default list 已清空。
- C9-M4 S6 已关闭：S6 live baseline 为 `HEAD=33b15325a5`（`33b15325a5 docs: 关闭 C9-M4 S5 CurveSurface oracle复审`）且起始 status clean；13 个 C9-M4 expected-backed rows 已进入 `ASMTPlanarJoint + offset=distance/getJointDistance` supported route，expected JSON 不再带 diagnostic-only `known_gap` / `nonGoal`，`distance_type_extended_geometry.native_expected_count=31`，Assembly 与 conic publication 的 `default_or_todo_boundaries=[]`，C9-M4 队列为空。
- C9-M5 方案包已建立，S0 已关闭且 S1-S6 待执行：生成基线为 `HEAD=ceef6a128b`（`ceef6a128b feat: 关闭 C9-M4 S6 默认距离类型发布闸门`）且生成前工作区干净；S0 执行基线仍为 `HEAD=ceef6a128b`，开始 status 仅包含 `docs/CADCore9.0/README.md` 修改和本 C9-M5 包未跟踪文件。C9-M4 队列为空，当前 live gap 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，发布状态为 `known_gap_diagnostic` / `oracle_blocked`；S6 code gate 只有在 S3 native evidence 与 S4 产品边界同时成立时打开。本包不重开 C9-M1 到 C9-M4 Assembly 主线，也不把 FreeCAD `_tmp_binder` / `_CopiedObjs` / `copyObject()` / `recomputeFeature(true)` full temporary-document cache 写成 supported。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```
