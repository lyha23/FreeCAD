# P8 AssemblySolver S6 Oracle 实现与发布闸门

## 目标

只消费 S2-S5 留下的 `notCollected`、`backendGap`、`unsupported` 和 `releaseGate` 队列。S6 要么把 releaseGate 发布成 supported，要么采集 oracle / 补 C++ / focused tests，要么明确保持 notCollected / unsupported / nonGoal。

## 输入队列

| blocker | 初始状态 | S6 动作 |
| --- | --- | --- |
| `P8ASM-BLOCK-001` | releaseGate | 回写 P8 / overview docs 和 capability 口径 |
| `P8ASM-BLOCK-002` | releaseGate | 固化 FreeCAD source authority 到相邻 C++ 注释 / docs |
| `P8ASM-BLOCK-003` | notCollected / releaseGate | 采 FreeCAD oracle 或明确当前只做 focused runtime verification |
| `P8ASM-BLOCK-004` | releaseGate | 关闭 placement writeback lifecycle 发布闸门 |
| `P8ASM-BLOCK-005` | unsupported | 决定是否实现新增 JointType；否则保留 diagnostic matrix |
| `P8ASM-BLOCK-006` | releaseGate | 验证 Ondsel linked / not linked 两种 build capability |

## 实施顺序

1. 先做 release audit：P8 文档、总览、C ABI capabilities、tests 是否一致。
2. 再做 oracle audit：若 FreeCADCmd 可用，采集 Assembly solver native expected；若不可用，保持 `notCollected` 并记录环境边界。
3. 再做 C++：只实现 S5 明确裁决的 JointType 或 mismatch blocker。
4. 再补 fixtures / expected / focused tests。
5. 最后回写矩阵状态和正式文档；只有验收通过后才允许把对应步骤文件改名为 `【已实现】`。

## 下一轮代码落点

| scope / blocker | C++ 落点 | FreeCAD authority | tests / fixtures | success criteria | 禁止捷径 |
| --- | --- | --- | --- | --- | --- |
| `P8ASM-BLOCK-003` | `cad-core/src/assembly/joint_solver.cpp`; `cad-core/src/assembly/assembly_utils.cpp` | `AssemblyObject::solve()` / `runPreDrag()` / `validateNewPlacements()` | `tests/test_p8_features.py`; `fixtures/c3m6/assembly-*-real-solver.json` | real solver / representative fallback / diagnostics 与 oracle 或 focused contract 一致 | 不把 representative output 当 native golden |
| `P8ASM-BLOCK-004` | `cad-core/src/assembly/assembly_utils.cpp` | `AssemblyObject::setNewPlacements()` | `test_c3m6_assembly_placement_writeback_applies_to_next_request_graph` | update 只写 `Placement`，下一请求无重复 update | 不在后端持久化 Placement |
| `P8ASM-BLOCK-005` | `cad-core/src/assembly/joint_solver.cpp`; `include/cad_core/assembly/joint_solver.h` | `makeMbdJointOfType()` / `makeMbdJointDistance()` | 新增 C3M6 JointType fixture 和 focused test | 新 JointType 有 DTO 字段、diagnostic / solve result 和 capability | 不按 fixture 名、Joint 名或输出排序猜结果 |
| `P8ASM-BLOCK-006` | `cad-core/CMakeLists.txt`; `cad-core/src/adapters/c_api/c_api.cpp` | FreeCAD `makeMbdAssembly()` + OndselSolver linkage | capability test | `CAD_CORE_HAS_ONDSEL_SOLVER=1/0` 下声明准确 | 不在 C ABI 声明不存在的 real solver |

## 验收分层

本轮短跑：

```bash
git diff --check
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_p8_assembly_joint_group_reports_solver_inputs_and_placement_writeback
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
cd cad-core && python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段回归：

```bash
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
cd cad-core && python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

重型收口条件：

- 修改 `CMakeLists.txt`、Ondsel link、oracle collector 或 expected fixture 时，执行 `cmake --build build`。
- 刷新 FreeCAD oracle 时，必须在本机可用 FreeCADCmd / LibPack / OCCT 基线下运行 collector，并记录 FreeCAD 版本。

## 发布闸门

- `P8ASM-SCOPE-001` 发布前，P8 文档不得继续把当前 covered solver 子集写成全部 `solve=not_migrated`。
- `P8ASM-SCOPE-006` 若仍无 oracle，必须保持 `notCollected`，不能改 supported。
- `P8ASM-SCOPE-007` 若未实现复杂 JointType，必须保持 `unsupported` diagnostic，并在 C ABI / docs 中公开。

## 非目标

- 不实现完整 Assembly Workbench。
- 不引入跨请求 solver session。
- 不把完整 Link 账本和 copy-on-change 写回事务混入本主线。
