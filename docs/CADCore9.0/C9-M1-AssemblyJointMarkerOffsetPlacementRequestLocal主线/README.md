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
5. S4：`runPreDrag` placement writeback 生命周期复审。
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
