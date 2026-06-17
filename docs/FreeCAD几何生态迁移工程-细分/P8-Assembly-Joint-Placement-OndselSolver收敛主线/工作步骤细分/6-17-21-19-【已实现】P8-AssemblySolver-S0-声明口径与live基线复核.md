# 【已实现】P8 AssemblySolver S0 声明口径与 live 基线复核

## 目标

冻结本主线的声明边界，复核当前 P8 文档、C ABI capabilities、focused tests、fixtures 和 C++ 之间的 live 状态。S0 不写 C++，不采 oracle，不把 releaseGate 提升为 supported。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有大量非 S0 改动：`AGENTS.md`、`DESIGN.md`、`cad-core/CMakeLists.txt`、P5/P6 文档、P8 主线 seed 文件和矩阵均处于修改或未跟踪状态；本步骤只编辑 P8 AssemblySolver S0 相关入口、S0 文档和矩阵行，不清理、不回退其它改动。 |

## live 复核结论

| 证据面 | 当前 live 状态 | S0 口径 |
| --- | --- | --- |
| 正式 P8 文档 | `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md` 仍写 Assembly Joint / GroundedJoint 输出 `solve=not_migrated`，不执行 OndselSolver，不更新 component placement；`00-CAD-Core完整抽取执行总览.md` 也仍把 Assembly 求解器列为未完成边界。 | 这是 publication drift：正式文档还没跟当前 C++ / tests 的 request-local solver 子集对齐，不能直接声明完整 P8 solver 已迁移。 |
| C ABI capabilities | `cad-core/src/adapters/c_api/c_api.cpp` 的 `assembly` capabilities 已暴露 `representative_solver_adapter`、`ondsel_solver_adapter`、`placement_writeback`，并列出 `documentObjectUpdates.action=assembly_set_placement`。 | capabilities 领先于正式 P8 文档；S6 发布前需要按 S3-S5 证据重新校正 wording，避免过度声明。 |
| C++ Assembly 路径 | `cad-core/src/assembly/assembly_utils.cpp::solverSummary()` 当前构建 `AssemblySolveRequest` 并调用 `solveAssemblyWithOndselAdapter()`；`assembly_object.cpp` 把 `solve` 与 `solver_adapter` 写入 Assembly metadata；`joint_solver.cpp` 有 request-local representative fallback、可选 real Ondsel adapter、grounded validation 和 placement updates。 | 代码已有 solver adapter 子集，但 S0 不改 C++，也不把该子集提升为 `supported`；S3-S5 继续验证 adapter、writeback 和 JointType 边界。 |
| focused tests | `cad-core/tests/test_p8_features.py` 已覆盖 P8 assembly input、grounded noop、real Ondsel mode、representative fallback、`assembly_set_placement`、next-request application、multi-component writeback、invalid grounded rejection 和 unsupported joint diagnostics。 | tests 是 focused coverage，不等价于 FreeCAD native oracle；缺 oracle 的 placement parity 仍保持 `notCollected`。 |
| fixtures | `cad-core/fixtures/p8` 有 Assembly display / Joint metadata expected；`cad-core/fixtures/c3m6` 有 real solver、representative fallback、unsupported joint、multi-component writeback 等 JSON 输入。 | P8 expected 主要固定元数据和代表性输出；C3M6 约束 focused runtime 行为，不从 cad-core 输出倒推 FreeCAD golden。 |

## FreeCAD 语义依据复核

| FreeCAD 入口 | 本轮读取到的关键语义 | cad-core 对应落点 |
| --- | --- | --- |
| `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | `ensureIdentityPlacements()` 后 `syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`jointParts()`、`mbdAssembly->runPreDrag()`、`setNewPlacements()`。无 grounded part 时返回 `-6`。 | `cad-core/src/assembly/joint_solver.cpp::solveAssemblyWithOndselAdapter()`；`cad-core/src/assembly/assembly_utils.cpp::solverSummary()` |
| `AssemblyObject::validateNewPlacements()` | grounded object moved 时输出 `Ignoring bad solve...` 并拒绝 solve。 | `validateNewPlacementsEquivalent()`；focused test 覆盖 invalid grounded result rejection。 |
| `AssemblyObject::setNewPlacements()` | solver 后读取 MBD part placement 并 `propPlacement->setValue(newPlacement)`。 | `documentObjectUpdates.action=assembly_set_placement`，只作为前端更新 graph 的建议，不持久化后端状态。 |
| `AssemblyObject::makeMbdAssembly()` / `fixGroundedPart()` | 创建 `OndselAssembly`，grounded part 通过 `ASMTFixedJoint` 固定。 | request-local ASMTAssembly / grounded joint conversion。 |
| `AssemblyObject::makeMbdJointOfType()` | FreeCAD 映射 Fixed、Revolute、Cylindrical、Slider、Ball、Distance、Parallel、Perpendicular、Angle、RackPinion、Screw、Gears、Belt。 | 当前 cad-core 子集为 Fixed / Revolute / Slider / Ball / Distance / Angle；复杂或未接入类型保持 unsupported / 后续分类。 |
| `src/Mod/Assembly/JointObject.py::Joint` | `JointTypes` 列出完整类型；Joint 添加 `JointType`、`Reference1` / `Reference2`、`Placement1` / `Placement2`、`Angle`、`Distance`、`Distance2`，历史迁移包含 `PropertyXLinkSubHidden`。GroundedJoint 添加 `ObjectToGround`。 | `joint_group.cpp` / `joint_solver.cpp` 解析 Joint DTO、hidden reference 和 GroundedJoint。 |
| `src/Mod/Assembly/App/JointGroup.cpp::JointGroup::getJoints()` | 遍历 group children，跳过 grounded / suppressed，保留具有 `setJointConnectors` proxy 的 joint。 | `assembly_utils.cpp::jointNames()` 与 `joint_group.cpp` 输出 request-local solver inputs。 |

## 输入

- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md`
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/src/assembly/*.cpp`
- `cad-core/include/cad_core/assembly/*.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/p8`
- `cad-core/fixtures/c3m6`

## 声明边界

| 项 | 是否纳入 | 说明 |
| --- | --- | --- |
| request-local Assembly solve DTO | 纳入 | 由 `DocumentObject graph` 每次重建 |
| real Ondsel adapter | 纳入 | 只在 `CAD_CORE_HAS_ONDSEL_SOLVER=1` 时声明 real adapter |
| representative fallback | 纳入 | 只能作为 adapter / transport 子集，不能伪装成完整 solver |
| placement writeback | 纳入 | 通过 `documentObjectUpdates` 建议前端更新 graph |
| unsupported JointType diagnostics | 纳入 | 不支持的类型必须结构化返回 |
| GUI / drag / postDrag / Simulation | 排除 | 属于 FreeCAD Workbench / interaction |
| 跨请求 solver session | 排除 | 违反 CAD Core 无状态边界 |

## 禁止声明

- “P8 Assembly 完整 solver 已迁移。”
- “representative fallback 等价于 FreeCAD OndselSolver。”
- “缺 FreeCAD oracle 的 placement 输出可以直接作为 golden。”
- “CAD Core 后端可以持久保存 solver session、Placement 或 Link 写回事务状态。”

## 状态词典

| 状态 | 含义 |
| --- | --- |
| `supported` | FreeCAD authority、cad-core 实现、fixture / expected 或 focused test 与 docs 均闭环 |
| `releaseGate` | 当前代码和测试看似覆盖，但发布文档、capability 或 expected 仍需复核 |
| `notCollected` | 缺 FreeCAD oracle / checked-in expected，不能直接写实现 |
| `backendGap` | 已有 FreeCAD authority 和 cad-core mismatch 证据，需要 C++ |
| `unsupported` | 当前不支持但属于协议可诊断范围，后续可凭 evidence 实现 |
| `nonGoal` | 本主线不做，必须写清 reopen 条件 |

## 必须回写的矩阵行

- `P8ASM-SCOPE-001`：已记录 live publication drift，仍保持 `releaseGate`。
- `P8ASM-SCOPE-002` 至 `P8ASM-SCOPE-008`：S0 仅确认 live baseline，不关闭为 `supported`。
- `P8ASM-BLOCK-001`：已把文档 / capability / tests 状态漂移写入 next step / close condition；关闭动作留给 S6。

## 发布边界

- 可以说：当前 `cad-core` 有 request-local Assembly solver adapter 子集、representative fallback、可选 real Ondsel adapter、placement writeback 建议和 unsupported joint diagnostics。
- 仍不能说：完整 FreeCAD Assembly solver、完整 Joint placement / constraint parity、完整 GUI / drag / postDrag / Simulation lifecycle、跨请求 solver session 或完整 FreeCAD Link 写回事务已迁移。
- `representative_solver_adapter` 只能作为 adapter / transport 子集；缺 FreeCAD native oracle 的 placement 输出继续标记为 `notCollected`，不得作为 golden。
- `SCOPE-002..008` 继续由 S1-S6 裁决；S0 不改变 C++、不运行 FreeCADCmd collector。

## 验收

- 根入口和步骤总入口明确 S0 已实现，S1-S6 仍待执行。
- `p8_assembly_solver_scope_review_matrix.tsv` 里不得出现未经 S2 裁决的 `supported`。
- 下面命令能定位 live drift 证据，而不是只凭旧文档判断：

```bash
rg -n "solve=not_migrated|representative_solver_adapter|ondsel_solver_adapter|placement_writeback|assembly_set_placement" docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md cad-core/src/adapters/c_api/c_api.cpp cad-core/src/assembly cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不运行 FreeCADCmd collector。
- 不新增或修改 C++。
- 不把 `releaseGate` 直接改成 `supported`。
