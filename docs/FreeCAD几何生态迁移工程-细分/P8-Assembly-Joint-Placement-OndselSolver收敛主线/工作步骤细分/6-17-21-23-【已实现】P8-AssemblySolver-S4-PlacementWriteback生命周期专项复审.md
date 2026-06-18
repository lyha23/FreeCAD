# 【已实现】P8 AssemblySolver S4 PlacementWriteback 生命周期专项复审

## 目标

复核 solver placement writeback 是否符合 CAD Core 无状态边界：本次请求只返回 `documentObjectUpdates` 建议，前端把更新应用到 `DocumentObject graph` 后，下一次 recompute 才消费新的 `Placement`。S4 不在后端直接改写请求 graph，不引入跨请求 placement cache，不扩展完整 FreeCAD Link writeback transaction。

## 后续修正

2026-06-18 后续实现已删除 representative fallback 和 optional unlinked build 路径；下文关于 unlinked representative 的失败记录只保留为当时问题定位，不代表当前运行路径。native solver placement expected 已在 S6 之后入库，并已在 S7 修复到 supported；当前状态以 S7、矩阵、C++ 和 C ABI capabilities 的 real-only 口径为准。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有大量非 S4 改动和 P8 seed 未跟踪文件；本轮只编辑 S4 文档、P8 入口和 S4 相关矩阵行，不 reset、不 revert、不清理。 |

## FreeCAD 依据

| FreeCAD 入口 | S4 读取到的关键语义 | cad-core 对应 |
| --- | --- | --- |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | 调用顺序为 `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`jointParts()`、`mbdAssembly->runPreDrag()`、`setNewPlacements()`；没有 grounded object 时 `return -6`。 | `solveAssemblyWithOndselAdapter()` 每次从 `DocumentObject graph` 重建 request-local solver input。 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::validateNewPlacements()` | 先遍历 grounded parts，读取旧 `Placement`，用 MBD part 计算新 placement；若 `oldPlc.isSame(newPlacement, Precision::Confusion())` 不成立，输出 `Ignoring bad solve, a grounded object (%s) moved` 并返回 `false`。 | `validateNewPlacementsEquivalent()` 在发布 updates 前检查 grounded target，失败时写 `invalid_assembly_solver_result`、清空 `placementUpdates` 并标记 `reason=grounded_object_moved`。 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::setNewPlacements()` | 遍历 `objectPartMap`，读取 `getMbdPlacement(mbdPart)`，应用 `offsetPlc`，若不同则 `propPlacement->setValue(newPlacement)` 并 `obj->purgeTouched()`。 | `assembly_utils.cpp::solverSummary()` 把 `AssemblyPlacementUpdate` 转为 `documentObjectUpdates.action=assembly_set_placement`；服务端不修改输入 graph。 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::preDrag()/doDragStep()` | 拖拽路径先 `validateNewPlacements()`，通过后才 `setNewPlacements()` 和 redraw joint placement。 | S4 只纳入 recompute/writeback 建议契约；GUI drag、postDrag 和 redraw 属于 nonGoal。 |

## cad-core writeback 契约

| 契约点 | 当前代码证据 | S4 结论 |
| --- | --- | --- |
| request-local 输入 | `buildAssemblySolveRequest()` 从 `Assembly::AssemblyObject.Group`、JointGroup、Joint、GroundedJoint 和每个 part 当前 `Placement` 重建 `AssemblySolveRequest`。 | 符合 CAD Core 无状态边界；后端不保存上一请求 solver session。 |
| update 结构 | `placementUpdateJson()` 输出 `action=assembly_set_placement`、`reason=assembly_solver_placement_writeback`、`object`、`objectId`、`typeId`、`assembly`、`joint`、`joint_type` 和 `properties.Placement`。 | update 只表达 `Placement` 属性建议，不携带 shape、BREP、mesh、solver session 或完整 Link transaction。 |
| next-request 应用 | `tests/test_p8_features.py::run_with_document_updates_applied()` 只把 `properties` 合并回 fixture graph，再重新 recompute。 | 应用后下一次 recompute 不再重复返回同一 placement update，说明消费点在前端 graph。 |
| multi-component 顺序 | `solveAssemblyWithRepresentativeAdapter()` 按 request.joints 顺序生成 updates；real path 按 request.parts 顺序收集 solved placement。当前 focused fixture 输出 `ComponentB`、`ComponentC`，target fields 为 `objectId=5/6`、`typeId=Assembly::AssemblyLink`、`properties={Placement}`。 | 当前 contract 足以发布“多组件更新建议有稳定目标字段和顺序”这一 request-local 子集；不代表 FreeCAD native order parity。 |
| invalid grounded rejection | `validateNewPlacementsEquivalent()` 发现 grounded target 被更新时写 `invalid_assembly_solver_result` 并清空 updates。 | 当前 runtime 已保留 FreeCAD 的“bad solve 不写回”语义；diagnostic target 在 unlinked representative fallback 下与 real-solver focused expectation 不同，路由 S6。 |

## S4 复核结论

| scope | 结论 | 后续路由 |
| --- | --- | --- |
| `P8ASM-SCOPE-005` | `documentObjectUpdates.action=assembly_set_placement` 的结构、只含 `Placement` 属性、next-request 应用、multi-component target fields 和 invalid grounded rejection 均有 focused 证据；但当前 focused tests 因 `CAD_CORE_HAS_ONDSEL_SOLVER=0` 仍失败，发布仍保持 `releaseGate`。 | S6 按 build mode 条件化 tests / capability wording，再发布 stateless writeback 子集。 |
| `P8ASM-SCOPE-002` | Joint / GroundedJoint DTO 是 writeback 的输入来源；S4 只确认 DTO 到 update 的 request-local contract，不声明完整 FreeCAD Joint lifecycle。 | S6 文档发布时写成输入 DTO 和 update 建议，不写后端事务。 |
| `P8ASM-SCOPE-003` | invalid grounded rejection 的 gate 存在；当前失败来自 representative fallback 下 diagnostic target 与 real solver expectation 不一致，属于 S3 已记录的 unlinked build-mode/test-route 问题。 | S6 修正 linked/unlinked focused route，或把 real-only target 断言限于 `CAD_CORE_HAS_ONDSEL_SOLVER=1`。 |
| `P8ASM-SCOPE-006` | FreeCAD native solver placement oracle 仍 `notCollected`。S4 没有运行 FreeCADCmd collector，也没有 checked-in native expected。 | S6 采集 FreeCAD native oracle 后才能裁决 parity；缺 oracle 前不得转 `supported` 或 `backendGap`。 |

## focused test 结果

| 命令 | 结果 | 归因 |
| --- | --- | --- |
| `rg -n "assembly_set_placement|documentObjectUpdates|validateNewPlacementsEquivalent|invalid_assembly_solver_result|run_with_document_updates_applied|multi_component" cad-core/src/assembly cad-core/tests/test_p8_features.py cad-core/src/adapters/c_api/c_api.cpp` | 能定位 update 结构、validation gate、next-request helper、multi-component assertions 和 C ABI publication。 | 代码证据齐全。 |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_placement_writeback_applies_to_next_request_graph` | 失败：应用 update 后已无重复 `documentObjectUpdates`，但测试期望 `real_ondsel_solver`，当前输出 `representative_ondsel_solver`。 | S3 已记录的 unlinked build-mode/test expectation 缺口，不是 writeback contract 自身缺口。 |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_invalid_grounded_distance_rejects_solver_writeback` | 失败：已输出 `invalid_assembly_solver_result` 且 `documentObjectUpdates=[]`，但 diagnostic target 当前为 `ComponentB`，测试期望 `ComponentA`。 | representative fallback 的 target 与 real solver expectation 不同；S6 需要按 build mode 条件化或补 linked route。 |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_multi_component_writeback_order_and_target_fields` | 失败：测试先断言 `real_ondsel_solver`，当前为 `representative_ondsel_solver`；手工复核当前 updates 为 `ComponentB`、`ComponentC`，只含 `Placement`，应用后下一请求无重复 updates。 | S3 build-mode 缺口；multi-component writeback contract 当前可作为 focused evidence，但不能当 FreeCAD parity。 |

## S6 路由

| 缺口 | cad_core_landing | fixture/test route | close_condition |
| --- | --- | --- | --- |
| S4 focused tests 在 unlinked build 下仍断言 real solver | `cad-core/tests/test_p8_features.py`; `cad-core/CMakeLists.txt`; `cad-core/src/assembly/joint_solver.cpp` | 三个 S4 focused tests；S3 real solver focused route | `CAD_CORE_HAS_ONDSEL_SOLVER=1` 才断言 `real_ondsel_solver` 和 real-only diagnostic target；`=0` 断言 representative fallback 的 stateless writeback contract 或跳过 real-only assertions。 |
| C ABI capability wording 可能过度声明 | `cad-core/src/adapters/c_api/c_api.cpp`; `cad-core/tests/test_adapters.py` | capabilities publication test | `placement_writeback` 可以发布 update transport 子集；`ondsel_solver_adapter` 和 real parity 必须按 build mode / oracle 状态分流。 |
| FreeCAD native placement oracle 缺失 | `cad-core/tools/collect_freecad_expected.py`; `cad-core/fixtures/c3m6`; `cad-core/fixtures/p8` | FreeCAD native oracle collector；checked-in expected | `P8ASM-SCOPE-006` 保持 `notCollected`，直到 expected 入库并能比较 native placement parity。 |

## 非目标

- 不在后端直接改写请求 graph。
- 不引入跨请求 `Placement` cache、shape/BREP cache 或 solver session。
- 不扩展完整 Link writeback transaction。
- 不把 representative fallback 输出当成 FreeCAD native solver placement oracle。
- 不因为 focused runtime test 通过或失败就关闭 `P8ASM-SCOPE-006`。
