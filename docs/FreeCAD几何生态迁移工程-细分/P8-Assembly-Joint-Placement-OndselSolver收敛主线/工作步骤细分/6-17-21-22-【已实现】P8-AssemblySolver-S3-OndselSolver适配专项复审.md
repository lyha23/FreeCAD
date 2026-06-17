# 【已实现】P8 AssemblySolver S3 OndselSolver 适配专项复审

## 目标

复核 current `cad-core` Assembly solver adapter 是否真实对齐 FreeCAD `AssemblyObject::solve()` 的 request-local 子集：grounded part、Joint DTO、real Ondsel adapter、representative fallback、unsupported diagnostics、CMake optional link 和 capability wording。S3 不改 C++，只记录 live 证据并把可执行缺口路由到 S6。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有大量非 S3 改动和 P8 seed 未跟踪文件；本轮只编辑 S3 文档、P8 入口和 S3 相关矩阵行，不 reset、不 revert、不清理。 |

## FreeCAD 依据

| FreeCAD 入口 | S3 读取到的关键语义 | cad-core 对应 |
| --- | --- | --- |
| `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`；无 grounded part 时 `return -6`；随后 `jointParts()`、`mbdAssembly->runPreDrag()`、`setNewPlacements()`。 | `solveAssemblyWithOndselAdapter()` 在有 grounded joint 且 `CAD_CORE_HAS_ONDSEL_SOLVER=1` 时进入 real path；否则返回 representative path。 |
| `AssemblyObject::makeMbdAssembly()` | 创建 `OndselAssembly` 并设置 debug。 | real adapter 中创建 request-local `MbD::ASMTAssembly`，不保存跨请求 session。 |
| `AssemblyObject::fixGroundedPart()` | 创建 assembly marker、part marker `FixingMarker`，再用 `ASMTFixedJoint` 固定 grounded part。 | `addGroundedJointToOndselAssembly()` 用 fixed joint 表达 grounded conversion。 |
| `AssemblyObject::makeMbdJointOfType()` | FreeCAD 映射 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle / RackPinion / Screw / Gears / Belt。 | 当前 cad-core real / representative 子集为 Fixed / Revolute / Slider / Ball / Distance / Angle；其它类型走 `unsupported_assembly_solver` diagnostics。 |

## S3 复核结论

| scope | 结论 | 后续路由 |
| --- | --- | --- |
| `P8ASM-SCOPE-003` | real Ondsel adapter 的 C++ path 存在，并在 linked build 中应输出 `mode=real_ondsel_solver`；但当前源码树缺 `src/3rdParty/OndselSolver/CMakeLists.txt`，当前 build flags 为 `CAD_CORE_HAS_ONDSEL_SOLVER=0`，focused test 实际返回 `representative_ondsel_solver`。 | S6 `cad_core_landing=cad-core/CMakeLists.txt;cad-core/src/assembly/joint_solver.cpp;cad-core/tests/test_p8_features.py`：恢复 linked fixture route 或把 real-solver test 按 build mode 条件化。 |
| `P8ASM-SCOPE-004` | representative fallback 是真实当前可执行路径，覆盖 no-grounded / unlinked build 的 adapter / DTO transport 子集；它不能声明 FreeCAD native solver parity。当前成功输出主要暴露 `mode=representative_ondsel_solver`，fallback reason 在 capability wording 中列为 `no_grounded_part` / `ondsel_solver_not_linked`，runtime solved metadata 未单独带 reason。 | S6 若发布需要 runtime fallback reason，补 `solver_adapter.reason` 或 focused test；否则只在 docs / capability 中声明 fallback reason。 |
| `P8ASM-SCOPE-006` | FreeCAD native solver placement oracle 仍 `notCollected`。本轮没有运行 FreeCADCmd collector，也没有 checked-in native expected；focused test 只能证明 cad-core runtime 行为，不能证明 FreeCAD parity。 | S6 采集 FreeCAD native expected 后才能判断 placement parity；缺 oracle 前不得写 `supported` 或 `backendGap`。 |
| `P8ASM-SCOPE-008` | CMake optional link 分支清楚：存在 OndselSolver CMakeLists 时 `CAD_CORE_HAS_ONDSEL_SOLVER=1` 并 link `OndselSolver`，否则 `=0`；当前本地是 `=0`。C ABI capability 当前静态声明 `ondsel_solver_adapter.status=covered_full`，在 unlinked build 下会越界。 | S6 `fixture/test route=test_adapters capabilities + linked/unlinked CMake smoke`：capability wording 必须按 build mode 分流，`=0` 时不得宣称 real Ondsel 可用。 |

## focused test 结果

| 命令 | 结果 | 解释 |
| --- | --- | --- |
| `rg -n "solveAssemblyWithOndselAdapter|solveAssemblyWithRepresentativeAdapter|real_ondsel_solver|representative_ondsel_solver|CAD_CORE_HAS_ONDSEL_SOLVER|unsupported" ...` | 能定位 real adapter、representative adapter、optional CMake flag、unsupported diagnostics 和 test assertions。 | 代码证据齐全。 |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver` | 失败：4 个 subTest 均为 `representative_ondsel_solver != real_ondsel_solver`。 | 当前 build 是 `CAD_CORE_HAS_ONDSEL_SOLVER=0`，失败显示 build-mode/test expectation 缺口，不是 native placement parity 证据。 |

## S6 路由

| 缺口 | cad_core_landing | fixture/test route | close_condition |
| --- | --- | --- | --- |
| real solver focused test 与当前 unlinked build 不匹配 | `cad-core/CMakeLists.txt`; `cad-core/src/assembly/joint_solver.cpp`; `cad-core/tests/test_p8_features.py` | `test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver`; 新增或调整 unlinked fallback focused case | linked build 才断言 `real_ondsel_solver`；unlinked build 断言 representative fallback 或跳过 real-only case。 |
| capability wording 静态 overclaim | `cad-core/src/adapters/c_api/c_api.cpp`; `cad-core/tests/test_adapters.py` | C ABI capabilities test；linked/unlinked CMake smoke | `CAD_CORE_HAS_ONDSEL_SOLVER=0` 时 capability 不宣称 real Ondsel 可用；`=1` 时才发布 request-local real adapter。 |
| native placement parity oracle 缺失 | `cad-core/tools/collect_freecad_expected.py`; `cad-core/fixtures/c3m6`; `cad-core/fixtures/p8` | FreeCAD native oracle collector；focused parity fixture | collected expected 入库前保持 `notCollected`；只有 oracle mismatch 才能升为 `backendGap`。 |

## 非目标

- 不实现 GUI drag / `postDrag()`。
- 不支持跨请求 solver session。
- 不用 representative fallback 伪装 FreeCAD native solver parity。
- 不把 `CAD_CORE_HAS_ONDSEL_SOLVER=0` 的构建声明成 real Ondsel 可用。
