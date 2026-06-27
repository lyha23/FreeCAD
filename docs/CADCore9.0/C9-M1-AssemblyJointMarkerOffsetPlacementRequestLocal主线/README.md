# C9-M1 Assembly Joint marker / offset placement request-local 主线

## 定位

C9-M1 处理 Assembly request-local solver 链路中仍需要裁决的 marker / placement 边界。它从 C8-M7 队列清空后的 live capability 出发，不继续追已关闭的 ImportShape residual，也不把 `copy_on_change_full_temporary_document_cache` 重新升级为实现任务。

本包要回答三件事：

- FreeCAD `AssemblyObject::handleOneSideOfJoint()` 的 object-global 到 moving-part-local marker placement、`offsetPlc` 组合和 current cad-core marker resolver 是否已经覆盖同一 request-local 子集。
- `runPreDrag()` / `setNewPlacements()` / `validateNewPlacements()` 的 placement writeback 是否只需要 request-local `documentObjectUpdates`，以及哪些 native oracle 仍缺失。
- current capability 中 `non_identity_bundled_offsetPlc`、`non_assembly_link_subshape_primitive_frame_generalization`、zero Angle fallback class evidence 等口径，哪些应保持 non-goal / oracle-blocked，哪些可以进入 S6 受限 C++ 实现。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ebd6fd1243`（`ebd6fd1243 docs: 新增 C9-M1 Assembly Joint marker 方案包`），开始状态干净。
- C8-M1 到 C8-M7 队列已复核为空：队列脚本均只输出表头，无 pending 行。
- current capability 中 Assembly real Ondsel adapter 已可用（`assembly.ondsel_solver_adapter.available=true`），`subshape_marker_placement` 发布 `covered_representative_subset`，并把 `non_identity_bundled_offsetPlc` 与 `non_assembly_link_subshape_primitive_frame_generalization` 列为 non-goals。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 继续保留 C8 known gap / `oracle_blocked`，不进入 C9-M1。
- `cad-core/fixtures/c3m6/expected` 已有 Assembly native solver placement expected；C9-M1 必须复用这些 oracle 作为基线，不得用 current cad-core 输出倒推 FreeCAD expected。

## S1 结论

- S1 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=f6ecb77b1a`（`f6ecb77b1a docs: 完成 C9-M1 S0 基线冻结`），开始状态干净。
- FreeCAD source authority 已闭合到 `AssemblyObject.cpp::handleOneSideOfJoint()`、`validateNewPlacements()`、`setNewPlacements()`、`solve()`、`makeMbdJointOfType()`，`AssemblyUtils.cpp::getJointType()` / `getJointCurrentValue()`，以及 `JointObject.py` 的 Joint / GroundedJoint schema。关键顺序是 `getGlobalPlacement(nullptr, ref) * PlacementN`，再 `getGlobalPlacement(part, ref).inverse()`，最后在 marker creation 前应用 `offsetPlc * marker`；solver writeback 则写 `getMbdPlacement(mbdPart) * offsetPlc`。
- current cad-core coverage 已定位到 `joint_solver.cpp::resolveJointMarkerPlacement()` / real Ondsel adapter、`assembly_utils.cpp::placementUpdateJson()` / `solverSummary()`、`assembly_object.cpp` 的 request-local display update，以及 capability 的 `assembly.ondsel_solver_adapter` / `placement_writeback`。当前覆盖 object、Vertex、linear Edge、planar Face、mixed、identity-offset AssemblyLink marker subset，unsupported primitive 和 unresolved marker 走 diagnostics。
- C3M6 / P8 focused tests 已覆盖 13 个 JointType 的 real Ondsel solver、object/subshape marker evidence、`documentObjectUpdates.action=assembly_set_placement`、next-request no-op、multi-component writeback 和 unsupported diagnostics。`assembly-marker-custom-placement-chain-real-solver` expected 已包含 non-identity connector / part placement chain native evidence，但仍带旧 `known_gap/backendGap` 文案，必须交给 S3 复审，不在 S1 升级为 supported。
- `C9M1-SRC-201..403` 已写入 source evidence、cad-core landing 和 next action；`C9M1-BLOCKER-101` 已关闭为 `Closed S1`。S2 继续做 scope / blocker / non-goal 路由，不改 capability、fixtures、tests 或 cad-core 源码。

## S2 结论

- S2 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=99bee0faee`（`99bee0faee docs: 完成 C9-M1 S1 源码覆盖复核`），开始状态干净。
- `scope_review_matrix` 已统一为 S2 route 裁决表：`C9M1-SCOPE-101`、`C9M1-SCOPE-201`、`C9M1-SCOPE-202` 为 `already_covered`；`C9M1-SCOPE-102` 为 `oracle_candidate`；`C9M1-SCOPE-103`、`C9M1-SCOPE-302`、`C9M1-SCOPE-303` 为 `diagnostic_non_goal`；`C9M1-SCOPE-203` 为 `known_gap_retained`；`C9M1-SCOPE-301`、`C9M1-SCOPE-304` 为 `release_gate`。
- backend 分类已写入 `C9M1-BG-101..501`，并新增 route / close condition 字段；每行都指向对应 `scope_ids`。non-goal 注册表 `C9M1-NG-001..008` 保持无状态 CAD Core、完整 Link 生命周期、GUI、primitive frame、offsetPlc 猜测和 adapter 字符串改写排除理由。
- `C9M1-BLOCKER-201` 已关闭为 `Closed S2`。S3 只复审 marker placement 与 `offsetPlc`，S4 复审 writeback / zero Angle / diagnostics，S5 决定 capability publication，S6 只消费已被 S3-S5 证明的 implementation gate 或 release gate。

## S3 结论

- S3 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=f34f5b1639`（`f34f5b1639 docs: 完成 C9-M1 S2 范围准入矩阵`），开始状态干净。
- FreeCAD `handleOneSideOfJoint()` 不是在 linked object local frame 里猜测 marker：它先执行 `getGlobalPlacement(nullptr, ref) * PlacementN` 得到 object-global JCS，再用 `getGlobalPlacement(part, ref).inverse()` 转为 moving-part-local marker，最后在 marker creation 前应用 `data.offsetPlc * plc`。
- `offsetPlc` 是 fixed-joint bundled parts 的内部 offset：`AssemblyObject.h::MbDPartData::offsetPlc` 注释为 bundled parts 内部 offset，`getMbDData()` 用 `plc.inverse() * plci` 生成，`validateNewPlacements()` / `setNewPlacements()` 用 `getMbdPlacement(mbdPart) * offsetPlc` 校验或写回。C9-M1 不允许跨请求缓存、frontend 补猜测或 fixture 几何推断。
- current `joint_solver.cpp` 覆盖 object-level baseline、AssemblyLink identity-offset subshape marker、Vertex / linear Edge / planar Face 和 mixed request-local marker；非线性 Edge / 非平面 Face 缺 markerPlacement，保持 `unsupported_subshape_marker_primitive` / `unsupported_assembly_solver` 诊断。
- `assembly-marker-custom-placement-chain-real-solver.freecad.json` 已有 non-identity connector / part placement chain native evidence，但它的 `offset_boundary` 是 `identity_offset_for_two_box_assembly_link_fixture`，且当前 `test_p8_features.py` 没有直接断言该 fixture。因此它可作为 custom-chain oracle inventory，不能证明 non-identity bundled `offsetPlc`，也不能把缺失的 offsetPlc oracle 升成 backendGap。
- S3 route：`C9M1-SCOPE-101` / `C9M1-BG-101` 保持 `already_covered`；`C9M1-SCOPE-102` / `C9M1-BG-102` 保持 `oracle_candidate`，collector / probe 基线应是固定关节 bundle 产生非 identity `objectPartMap.offsetPlc` 的 native case，本步按非目标不采集；`C9M1-SCOPE-103` / `C9M1-BG-103` 保持 `diagnostic_non_goal`。`C9M1-BLOCKER-301` 已关闭为 `Closed S3`。

## S4 结论

- S4 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=292587a0f5`（`292587a0f5 docs: 完成 C9-M1 S3 markerPlacement 复审`），开始状态干净。
- FreeCAD `solve()` 调用 real Ondsel `runPreDrag()` 后走 `setNewPlacements()`；拖拽路径才先过 `validateNewPlacements()`。`validateNewPlacements()` 与 `setNewPlacements()` 均按 `getMbdPlacement(mbdPart) * offsetPlc` 处理 bundled part placement。
- cad-core 只发布 request-local writeback 建议：`solveAssemblyWithRealOndselAdapter()` 生成 placement updates，`assembly_utils.cpp::placementUpdateJson()` 输出 `documentObjectUpdates.action=assembly_set_placement`，`assembly_object.cpp` 只把同一 request 的 update 用于 display compound，不持久写回后端 state、placement cache 或 frontend graph。
- C3M6 expected 中 42 个文件、44 条 `solver_adapter.placement_updates.action=assembly_set_placement` 覆盖 native writeback evidence；P8/C3M6 tests 覆盖顶层 `documentObjectUpdates`、next-request no-op、多组件/partial writeback 和 unsupported diagnostics。现有 expected 仍只证明 identity `offsetPlc` boundary，non-identity bundled `offsetPlc` 保持 S3 `oracle_candidate`。
- zero Angle fallback 保持 `known_gap_retained`：FreeCAD 与 cad-core 都有 Angle zero fallback 到 parallel axes 的 source/class evidence；`assembly-angle-zero-and-signed-current-real-solver.json` 只是输入 fixture，没有对应 expected 或测试引用。C9-M1 未采 native zero Angle oracle，也没有 current mismatch 证据，不打开 S6 C++ gate。
- unsupported JointType / boundary diagnostics 仍准确，`unsupported_assembly_solver` 不会被 C9-M1 writeback 复审改成 silent success。`C9M1-SCOPE-201` / `C9M1-SCOPE-202` / `C9M1-BG-202` 已关闭为 `already_covered`，`C9M1-SCOPE-203` / `C9M1-BG-201` 保持 `known_gap_retained`，`C9M1-BLOCKER-401` 已关闭为 `Closed S4`。S5 只做 capability / diagnostics 发布准入；若 S5 不发现发布口径缺口，S6 走 no-code release gate。

## FreeCAD 依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Joint marker placement | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()` | 读取 `Reference1/2` 与 `Placement1/2`，使用 `getGlobalPlacement(nullptr, ref)` 与 `getGlobalPlacement(part, ref).inverse()` 后处理 marker placement，并组合 `offsetPlc`。 |
| solver run | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | 构建 MBD assembly 后调用 `runPreDrag()`，再写回新 placement。 |
| placement writeback | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::setNewPlacements()` | 对 solver 返回 placement 应用 `offsetPlc` 后写回 `Placement`。 |
| placement validation | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::validateNewPlacements()` | 验证拖拽或 solver 更新后 placement，仍要考虑 `offsetPlc`。 |
| Joint type / distance | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp` | `getJointType()` / `getJointCurrentValue()` 定义 JointType 与 scalar 输入读取。 |
| Python Joint schema | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | 定义 `JointType`、`Reference1/2`、`ObjectToGround`、Distance / Angle 等动态属性。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly solver | `cad-core/src/assembly/joint_solver.cpp` | request-local Joint DTO、marker resolver、real Ondsel adapter、diagnostics。 |
| assembly output | `cad-core/src/assembly/assembly_utils.cpp` | `documentObjectUpdates.action=assembly_set_placement` 和 placement update JSON。 |
| assembly group | `cad-core/src/assembly/assembly_object.cpp`、`cad-core/src/assembly/joint_group.cpp` | Assembly display 与 JointGroup 输入收集。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `assembly.ondsel_solver_adapter`、`subshape_marker_placement`、remaining / non-goal 口径。 |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | focused runtime parity、native expected 和 capability smoke。 |

## 步骤队列

1. S0：live 基线与声明口径冻结。
2. S1：FreeCAD source 与 current cad-core coverage 候选矩阵。
3. S2：scope 准入、blocker 与 non-goal 路由。
4. S3：marker placement 与 `offsetPlc` oracle 复审。
5. S4：`runPreDrag` placement writeback 生命周期复审（已完成）。
6. S5：capability / diagnostics 发布准入。
7. S6：Oracle 实现与发布闸门。

## 验收分层

本轮文档短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```

实现闸门由 S6 按矩阵裁决选择：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
```
