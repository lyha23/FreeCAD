# 【已实现】C7-M6 S1 FreeCAD 源码与 current cad-core solver 能力复核

## 目标

复核 Assembly Joint placement / constraint 的 FreeCAD source authority、当前 `cad-core` implementation、fixtures、expected 和 focused tests。S1 只写文档和矩阵，不新增 fixtures/expected/tests，不运行 FreeCAD oracle，不改 C++。

## 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1ab803cb36`（`1ab803cb36 文档：完成 C7-M6 S0 基线冻结`），开始状态干净。
- FreeCAD source authority 已复核：`AssemblyObject::solve()` 的顺序是 `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`jointParts()`、`runPreDrag()`、`setNewPlacements()` 和 `redrawJointPlacements()`；`setNewPlacements()` 只在 request-local solve 结束后写回对象 `Placement`。
- `handleOneSideOfJoint()` 已确认 marker placement 语义：读取 `Reference1/2` 与 `Placement1/2`，经 `getGlobalPlacement(nullptr, ref)` 转 object-global，再经 moving part global inverse 转 part-local，并在 bundled part 时叠加 `offsetPlc`。
- `makeMbdJointOfType()` / `makeMbdJointDistance()` 已确认 JointType / DistanceType 映射：当前 FreeCAD 路径覆盖 Fixed、Revolute、Cylindrical、Slider、Ball、Distance、Parallel、Perpendicular、Angle、RackPinion、Screw、Gears、Belt；DistanceType 通过 `getDistanceType()` 排序 / swap JCS，并用 `getJointCurrentValue()` 或 radius helper 提供 scalar evidence。
- PointLine 有明确 source / native expected 差异边界：当前源码 `makeMbdJointDistance()` 写成 `ASMTCylSphJoint`，但已入库 FreeCADCmd 1.2.0 expected 和 current focused tests 对 request-local AssemblyLink PointLine 子集约束为 `ASMTLineInPlaneJoint` + `offset`；S2 不得脱离新 native oracle 把这个历史差异改写成 active gap。
- current `cad-core` 已复核：`buildAssemblySolveRequest()` 构造 request-local solver DTO，`solveAssemblyWithRealOndselAdapter()` 使用真实 Ondsel adapter，`makeOndselJointOfType()` 发布当前映射，`documentObjectUpdates.action=assembly_set_placement` 承接 request-local writeback，unsupported / missing marker / default DistanceType 仍走 diagnostics。
- fixture/test coverage 已复核：`cad-core/fixtures/c3m6/expected` 有 50 个 Assembly expected、0 个 Assembly `backendGap`、5 个 `DTE-NG-003` diagnostic boundary、34 个 `native_marker_oracle`；`cad-core/tests/test_p8_features.py` 覆盖 grounded JointType matrix、DistanceType basic / extended / default boundary、marker native oracle batch、single / multi / partial writeback、invalid grounded、ungrounded、unsupported diagnostics、Screw / RackPinion sliding precondition 与 marker rewrite。
- `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv`、`backend_gate.tsv`、`oracle_plan.tsv` 已同步；`C7M6-BLOCKER-101` 已关闭。S2 只能从已覆盖 expected-backed baseline 之外选择 source-backed oracle candidates，不能重开已覆盖 rows 或用 current `cad-core` 输出倒推 expected。

## 必读文件

- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `src/Mod/Assembly/App/AssemblyUtils.cpp`
- `src/Mod/Assembly/JointObject.py`
- `cad-core/src/assembly/assembly_object.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/assembly/joint_group.cpp`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c3m6`
- C7-M6 README、方案和矩阵

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。
2. 记录 FreeCAD source authority：solve order、GroundedJoint sync、marker placement、JointType mapping、DistanceType、current value、placement writeback。
3. 复核 current `cad-core` 能力：request-local real Ondsel adapter、marker placement、solver DTO、documentObjectUpdates、unsupported diagnostics、capability publication。
4. 复核哪些 fixture / expected 已经覆盖，哪些只是 current runtime diagnostic，哪些缺 native lifecycle。
5. 更新 `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv`、`backend_gate.tsv`、`oracle_plan.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'AssemblyObject::solve|makeMbdJointOfType|handleOneSideOfJoint|getDistanceType|getJointCurrentValue|JointType|GroundedJoint|assembly_set_placement|documentObjectUpdates|Ondsel' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 src/Mod/Assembly cad-core/src/assembly cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确的 oracle candidate 输入池和 already-covered baseline。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
