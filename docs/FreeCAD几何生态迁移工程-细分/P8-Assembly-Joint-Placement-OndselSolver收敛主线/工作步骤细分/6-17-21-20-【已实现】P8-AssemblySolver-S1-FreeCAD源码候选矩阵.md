# 【已实现】P8 AssemblySolver S1 FreeCAD 源码候选矩阵

## 目标

建立 Assembly solver、Joint input、placement writeback、JointType mapping 和 cad-core adapter 的 source authority。S1 只补候选，不裁决 supported / backendGap。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有非 S1 改动与 P8 seed 未跟踪文件；本轮只编辑 P8 AssemblySolver S1 文档、根入口、步骤总入口和 `p8_assembly_solver_source_candidates.tsv`，不 reset、不 revert、不清理其它文件。 |

## FreeCAD 依据

| 轴 | 源码入口 | 需要摘录的关键行为 |
| --- | --- | --- |
| solve 主流程 | `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`runPreDrag()`、`setNewPlacements()`、无 grounded part 时 `return -6` |
| bad solve validation | `AssemblyObject.cpp::validateNewPlacements()` | `Ignoring bad solve, a grounded object (%s) moved` 与 `oldPlc.isSame(newPlacement, Precision::Confusion())` |
| writeback | `AssemblyObject.cpp::setNewPlacements()` | `getMbdPlacement(mbdPart)`、`offsetPlc`、`propPlacement->setValue(newPlacement)`、`obj->purgeTouched()` |
| JointType mapping | `AssemblyObject.cpp::makeMbdJointOfType()` | Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle / RackPinion / Screw / Gears / Belt 到 ASMT joint 类 |
| Distance details | `AssemblyObject.cpp::makeMbdJointDistance()` | `DistanceType` 分支、`distanceIJ`、`offset`、`getFaceRadius`、`getEdgeRadius` |
| Joint properties | `src/Mod/Assembly/JointObject.py::Joint` | `JointTypes`、`JointUsingDistance2`、`Reference1/2`、`Placement1/2`、`Distance`、`Distance2`、`Angle`、`App::PropertyXLinkSubHidden` 迁移 |
| GroundedJoint | `src/Mod/Assembly/JointObject.py::GroundedJoint` | `App::PropertyLinkGlobal ObjectToGround`、`Placement` / `LinkPlacement` read-only 生命周期 |
| JointGroup | `src/Mod/Assembly/App/JointGroup.cpp::JointGroup::getJoints()` | `Suppressed`、grounded/deactivated filter、`Proxy`、`hasAttr("setJointConnectors")` |

## cad-core 候选落点

| 轴 | cad-core 入口 |
| --- | --- |
| solver DTO | `cad-core/include/cad_core/assembly/joint_solver.h` |
| solver adapter | `cad-core/src/assembly/joint_solver.cpp` |
| solver summary | `cad-core/src/assembly/assembly_utils.cpp` |
| Assembly executor | `cad-core/src/assembly/assembly_object.cpp` |
| JointGroup executor | `cad-core/src/assembly/joint_group.cpp` |
| optional Ondsel link | `cad-core/CMakeLists.txt` |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` |
| focused tests | `cad-core/tests/test_p8_features.py` |

## 本轮候选基线

- `p8_assembly_solver_source_candidates.tsv` 已从 seed 12 行补为 `P8ASM-CAND-001` 至 `P8ASM-CAND-018`。
- FreeCAD authority 覆盖 `AssemblyObject::solve()`、`validateNewPlacements()`、`setNewPlacements()`、`fixGroundedPart()`、`makeMbdJointOfType()`、`makeMbdJointDistance()`、`syncGroundedJoints()`、`JointObject.py::Joint` / `GroundedJoint` 和 `JointGroup::getJoints()`。
- cad-core authority 覆盖 `AssemblySolveRequest` / `JointConstraint` / `AssemblyPlacementUpdate`、`solveAssemblyWithOndselAdapter()`、`solveAssemblyWithRepresentativeAdapter()`、`solverSummary()`、`executeAssemblyObject()`、`executeAssemblyFeaturePython()`、`CAD_CORE_HAS_ONDSEL_SOLVER`、C ABI `assembly` capabilities 和 P8 focused tests。
- S1 只建立候选证据：未采 oracle，未改 C++，未把任何候选裁决为 `supported`、`backendGap`、`notCollected` 或 `unsupported`。

## 候选 TSV 字段

`candidate_id	source_file	freecad_symbol	semantic_axis	source_evidence	cad_core_landing	scope_hint	next_step`

候选行不得使用 `supported`、`backendGap` 等状态词替代证据。

## 必须回写的矩阵行

- `P8ASM-CAND-001` 至 `P8ASM-CAND-012`：已补全 source / symbol / evidence。
- 本轮新增 `P8ASM-CAND-013` 至 `P8ASM-CAND-018`，用于记录 CMake optional Ondsel link、Assembly executor publication、Joint / GroundedJoint publication、focused tests、GroundedJoint 属性和 `syncGroundedJoints()`。

## 验收

- `p8_assembly_solver_source_candidates.tsv` 覆盖 `AssemblyObject::solve()`、`validateNewPlacements()`、`setNewPlacements()`、`makeMbdJointOfType()`、`makeMbdJointDistance()`、`JointObject.py::Joint`、`GroundedJoint`、`JointGroup::getJoints()` 和 cad-core DTO / adapter / publication / tests 落点。
- `source_evidence` 使用源码可追溯短句、字段名或函数名，不用泛化描述替代证据。
- 本轮验收命令：

```bash
rg -n "AssemblyObject::solve|validateNewPlacements|setNewPlacements|makeMbdJointOfType|JointTypes|class Joint|JointGroup::getJoints" src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/JointObject.py src/Mod/Assembly/App/JointGroup.cpp
rg -n "AssemblySolveRequest|solveAssemblyWithOndselAdapter|representative_ondsel_solver|assembly_set_placement|CAD_CORE_HAS_ONDSEL_SOLVER" cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly cad-core/CMakeLists.txt cad-core/src/adapters/c_api/c_api.cpp
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/p8_assembly_solver_source_candidates.tsv
git diff --check
```

## 非目标

- 不把 FreeCAD GUI、view provider、TaskPanel 或 Simulation probe 加入候选。
- 不采集 native expected。
- 不改 `cad-core`。
- 不裁决 S2-S6 的 `releaseGate`、`notCollected`、`backendGap`、`unsupported` 或 `nonGoal`。
