# 【已实现】P8 ParallelPerpendicularJoint S6 Oracle 实现与发布闸门

## 目标

消费 S2-S5 留下的 `unsupportedImplementable`、`notCollected` 和 `releaseGate`，把 Parallel / Perpendicular 作为最小 request-local JointType 支持发布，并确认 remaining unsupported / nonGoal 边界不被扩大。

## live 基线

| 命令 | 当前输出 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `a8ad9747f4` |
| `git log -1 --oneline` | `a8ad9747f4 feat: 补齐 ExternalGeometry native oracle 通道` |
| `git -c core.quotepath=false status --short -uall` | 工作区已有本包相关 cad-core 源码、测试、c3m6 fixture/expected、upstream P8 docs/TSV 和本包未跟踪文档；S6 只收口本包文档/TSV，不 reset/revert/覆盖他人改动 |
| `FreeCADCmd --version` | `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)` |

## 发布裁决

Parallel / Perpendicular 发布为 request-local real Ondsel supported 子集。发布范围只覆盖当前 c3m6 grounded native oracle 约束住的 JointType 直接映射、generic marker path、无跨请求状态的 solver summary 和 `documentObjectUpdates` 建议；不声明完整 Assembly GUI / transaction lifecycle。

## blocker 关闭表

| blocker | 状态 | 证据 |
| --- | --- | --- |
| `PPJ-BLOCK-001` | closed | `cad-core/src/assembly/joint_solver.cpp` 已 include `ASMTPerpendicularJoint`，direct `Parallel` / `Perpendicular` 返回 `MbD::ASMTParallelAxesJoint::With()` / `MbD::ASMTPerpendicularJoint::With()`，`isSupportedOndselJointType()` 包含两者 |
| `PPJ-BLOCK-002` | closed | `assembly-grounded-parallel-joint-real-solver` fixture / expected 入库，FreeCADCmd `--check` 通过，focused runtime 断言 `real_ondsel_solver`、`unsupported_joints=[]`、无 placement writeback |
| `PPJ-BLOCK-003` | closed | `assembly-grounded-perpendicular-joint-real-solver` fixture / expected 入库，FreeCADCmd `--check` 通过，focused runtime 断言 `real_ondsel_solver`、`unsupported_joints=[]`、无 placement writeback |
| `PPJ-BLOCK-004` | closed | C ABI `covered` 增加 `grounded_parallel_joint` / `grounded_perpendicular_joint`，`supported_joint_matrix` 增加 Parallel / Perpendicular，`unsupported_joint_matrix` 移除两者；`test_adapters.py` 已断言 |
| `PPJ-BLOCK-005` | closed | RackPinion / Screw / Gears / Belt 仍为唯一 remaining unsupported JointTypes；complex Distance 保持 notCollected，GUI/session 保持 nonGoal |

## scope 状态

| scope | S6 状态 | 结论 |
| --- | --- | --- |
| `PPJ-SCOPE-002` | supported | Parallel direct ASMT mapping、native expected、focused test 和 C ABI capability 发布全部闭合 |
| `PPJ-SCOPE-003` | supported | Perpendicular direct ASMT mapping、native expected、focused test 和 C ABI capability 发布全部闭合 |
| `PPJ-SCOPE-004` | supported | Parallel / Perpendicular c3m6 FreeCADCmd expected 已入库并通过 `--check` |
| `PPJ-SCOPE-005` | supported | Capability、focused tests、本包矩阵和 upstream P8 docs / TSV 已同步 |
| `PPJ-SCOPE-006` | unsupported | RackPinion / Screw / Gears / Belt 继续 diagnostic-only，等待独立 source+DTO+oracle+tests 包 |
| `PPJ-SCOPE-007` | nonGoal | GUI drag / postDrag、persistent solver session、cross-request backend state 和完整 Link lifecycle 不进入 stateless CAD Core |

## 验证结果

| 命令 | 结果 |
| --- | --- |
| `cd cad-core && cmake --build build --target cad-core cad_core_ffi` | 通过，`cad-core` 与 `cad_core_ffi` 均 built |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver` | 通过，1 test OK |
| `cd cad-core && python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts` | 通过，1 test OK |
| `cd cad-core && FREECADCMD=FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-parallel-joint-real-solver.json --check` | 通过，FreeCAD 1.2.0 / 20260519 native oracle check 无差异 |
| `cd cad-core && FREECADCMD=FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-perpendicular-joint-real-solver.json --check` | 通过，FreeCAD 1.2.0 / 20260519 native oracle check 无差异 |
| `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest` | 通过，103 tests OK |
| `cd cad-core && python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest` | 仅出现已知无关残余：`p7/hole-supported-model-thread-counterbore` volume `434.05358560604475` 与 expected `434.05359569539525` 差 `1.0089350496400584e-05`，略超 `1e-05`；未刷新无关 expected |
| `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py .../工作步骤细分 --format markdown` | S6 重命名后队列为空 |
| TSV 字段一致性检查（本包矩阵 + upstream P8 AssemblySolver 矩阵） | 通过 |
| 指定 `git diff --check` | 通过 |

## 禁止路径复核

- 未恢复 representative fallback 或 unlinked build support。
- 未用 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 JointType。
- 未在 adapter 层补业务语义。
- 未把 RackPinion / Screw / Gears / Belt 顺手映射到 supported。
- 未把完整 Distance geometry、GUI drag、postDrag 或 persistent session 混入本包。

## 非目标

- 不运行全量 FreeCAD 构建。
- 不实现 RackPinion / Screw / Gears / Belt。
- 不调整 unrelated P5/P6/P7 docs 或 matrices。
