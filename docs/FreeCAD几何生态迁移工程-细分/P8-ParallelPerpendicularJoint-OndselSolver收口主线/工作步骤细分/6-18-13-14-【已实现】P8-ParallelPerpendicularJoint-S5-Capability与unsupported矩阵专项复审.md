# 【已实现】P8 ParallelPerpendicularJoint S5 Capability 与 unsupported 矩阵专项复审

## 目标

关闭 `PPJ-BLOCK-004` 和 `PPJ-BLOCK-005`：同步 C ABI capabilities、focused tests、既有 P8 AssemblySolver docs / TSV，并保护 remaining unsupported matrix。

## live 基线

| 命令 | 当前输出 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `a8ad9747f4` |
| `git log -1 --oneline` | `a8ad9747f4 feat: 补齐 ExternalGeometry native oracle 通道` |
| `git -c core.quotepath=false status --short -uall` | 工作区已有 S4 留下的 `cad-core/src/assembly/joint_solver.cpp`、`cad-core/tests/test_p8_features.py` 脏改动和新增 c3m6 Parallel / Perpendicular fixture / expected；S5 不接管、不回退这些改动，只消费其已验证语义。 |

## FreeCAD 依据

- Parallel / Perpendicular 已有直接 ASMT class mapping。
- RackPinion / Screw / Gears / Belt 依赖 distance / distance2 / special marker / sliding part / radius 语义，不进入本包。

## scope 表

| scope | 复审结果 |
| --- | --- |
| `PPJ-SCOPE-005` | `cad-core/src/adapters/c_api/c_api.cpp` 已发布 `grounded_parallel_joint` / `grounded_perpendicular_joint`，`supported_joint_matrix` 增加 Parallel / Perpendicular，`unsupported_joint_matrix` 移除它们 |
| `PPJ-SCOPE-006` | unsupported matrix 仍只包含 RackPinion / Screw / Gears / Belt；不发布完整复杂 JointType 支持 |
| `PPJ-SCOPE-007` | GUI drag / postDrag、persistent solver session 和 Link lifecycle nonGoal 未被能力发布稀释 |

## 必须回写的矩阵行

- `PPJ-CAND-009`：S4 native expected 证据已被 S5 capability 发布消费。
- `PPJ-CAND-010`：upstream `P8ASM-SCOPE-007` 已移除 Parallel / Perpendicular 的 unsupported_remaining wording。
- `PPJ-BLOCK-004`：已关闭，C ABI capabilities、adapter test 和 upstream docs / TSV 已同步。
- `PPJ-BLOCK-005`：已关闭，RackPinion / Screw / Gears / Belt 仍是唯一 remaining unsupported JointTypes。

## 复审结果

- `ondselSolverCapabilityJson().covered` 增加 `grounded_parallel_joint` 和 `grounded_perpendicular_joint`。
- C ABI `supported_joint_matrix` 现在为 `Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle`。
- C ABI `unsupported_joint_matrix` 现在只保留 `RackPinion / Screw / Gears / Belt`。
- `cad-core/tests/test_adapters.py` 同步断言 covered keys、supported matrix 和 unsupported matrix。
- `cad-core/tests/test_p8_features.py` 已在 S4 脏改动中包含 Parallel / Perpendicular grounded real solver cases；S5 未修改该文件。
- 上游 P8 AssemblySolver 总入口、`P8ASM-SCOPE-007`、backend gap、blocker 和 nonGoal wording 只移除了 Parallel / Perpendicular 的 unsupported_remaining，保留 RackPinion / Screw / Gears / Belt unsupported、complex Distance notCollected、GUI/session nonGoal。

## 验收标准

- `cad-core/src/adapters/c_api/c_api.cpp` 的 `ondselSolverCapabilityJson().covered` 增加 grounded Parallel / Perpendicular 覆盖项。
- `supported_joint_matrix` 包含 `Parallel` / `Perpendicular`。
- `unsupported_joint_matrix` 只保留 RackPinion / Screw / Gears / Belt。
- `cad-core/tests/test_adapters.py` 与 S4 已有 `cad-core/tests/test_p8_features.py` 断言同步。
- 既有 P8 AssemblySolver 主线的 `P8ASM-SCOPE-007` / backend gap / blocker / nonGoal 行只更新 Parallel / Perpendicular 相关内容。
- 本包 `scope_review`、`blocker_queue`、`backend_gap_classification` 已把 capability releaseGate 改为 closed / supported，remaining unsupported boundary 保持。
- 检查命令：

```bash
rg -n "supported_joint_matrix|unsupported_joint_matrix|grounded_parallel_joint|grounded_perpendicular_joint|Parallel|Perpendicular|RackPinion|Screw|Gears|Belt" cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

## 验收结果

- `cmake --build build --target cad-core cad_core_ffi`：通过。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver`：通过，2 tests OK。

## 非目标

- 不把 RackPinion / Screw / Gears / Belt 从 unsupported 移除。
- 不把 Parallel / Perpendicular 支持扩展成完整 Assembly transaction lifecycle。
- 不改 unrelated CADCore3.0 阶段文档，除非其 current capability 文案已经和 tests 冲突。
