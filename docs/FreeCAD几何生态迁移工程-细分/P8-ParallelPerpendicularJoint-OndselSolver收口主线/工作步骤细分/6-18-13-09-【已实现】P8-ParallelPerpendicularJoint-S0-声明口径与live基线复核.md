# P8 ParallelPerpendicularJoint S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、状态字典和 live baseline，避免把 Parallel / Perpendicular 当成已支持或把复杂 JointType 顺手纳入。

## 输入

- `git status --short`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/src/assembly/joint_solver.cpp`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv`
- `docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/*.tsv`

## 声明口径

| 项 | 本包允许的声明 | 禁止声明 |
| --- | --- | --- |
| Parallel | FreeCAD 直接映射 `ASMTParallelAxesJoint`，cad-core 可按 request-local real Ondsel 子集实现 | 不声明完整 Assembly solver 或 GUI prevent-parallel 交互语义 |
| Perpendicular | FreeCAD 直接映射 `ASMTPerpendicularJoint`，cad-core 可按 request-local real Ondsel 子集实现 | 不声明完整 reference geometry / ambiguous subshape 解算 |
| Placement writeback | 仅为 `documentObjectUpdates.action=assembly_set_placement` 建议 | 不保存跨请求 solver state |
| Remaining JointTypes | RackPinion / Screw / Gears / Belt 继续 unsupported | 不因为本包实现简单 ASMT 映射而发布复杂 JointType |

## live baseline

| 命令 | 当前输出 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `87f1974f8d` |
| `git log -1 --oneline` | `87f1974f8d 实现 P8 Assembly Cylindrical Joint 支持` |
| `git -c core.quotepath=false status --short -uall` | 本包 `P8-ParallelPerpendicularJoint-OndselSolver收口主线` 入口、S0-S6 和 5 个 TSV 当前均为未跟踪文件；S0 只允许在本包边界内改文档 / TSV，不接管其它工作区改动 |

## 当前代码发布状态

- `cad-core/src/adapters/c_api/c_api.cpp` 的 `supported_joint_matrix` 当前只发布 `Fixed`、`Revolute`、`Cylindrical`、`Slider`、`Ball`、`Distance`、`Angle`。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `unsupported_joint_matrix` 当前仍包含 `Parallel`、`Perpendicular`、`RackPinion`、`Screw`、`Gears`、`Belt`。
- `cad-core/src/assembly/joint_solver.cpp` 当前已 include `ASMTParallelAxesJoint`，但该代码只用于 `Angle` 且 `angleRadians == 0.0` 的转换候选；没有直接处理 `joint.jointType == "Parallel"`。
- `cad-core/src/assembly/joint_solver.cpp` 当前没有 include / 构造 `ASMTPerpendicularJoint`，`isSupportedOndselJointType()` 也不包含 `Parallel` 或 `Perpendicular`。
- 因此本包 S0 冻结口径为：Parallel 存在局部 adapter 代码候选，不等于 direct Parallel 能力已发布；Parallel / Perpendicular 当前仍是 published unsupported，只有在后续 mapper、fixture / native expected、focused tests 和 capability/docs 同步后才能转为 `supported`。
- 上游 P8 AssemblySolver 矩阵当前把 `P8ASM-SCOPE-007` 记为 `unsupported_remaining`：当前 request-local supported 子集是 `Fixed/Revolute/Cylindrical/Slider/Ball/Distance/Angle`，Parallel / Perpendicular 与 RackPinion / Screw / Gears / Belt 仍保持 diagnostic-only。

## 纳入 / 排除

| 分类 | 内容 | 状态 |
| --- | --- | --- |
| 纳入 | `Parallel -> ASMTParallelAxesJoint` | unsupportedImplementable |
| 纳入 | `Perpendicular -> ASMTPerpendicularJoint` | unsupportedImplementable |
| 纳入 | c3m6 parallel / perpendicular fixtures 与 expected | notCollected |
| 纳入 | C ABI supported / unsupported matrix 更新 | releaseGate |
| 排除 | RackPinion / Screw / Gears / Belt | unsupported |
| 排除 | complex Distance geometry | notCollected |
| 排除 | GUI drag / postDrag / persistent solver session | nonGoal |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `unsupportedImplementable` | 当前 published 为 unsupported，但 FreeCAD source 与 cad-core 落点足以定义本轮实现 |
| `codeCandidateOnly` | 本地源码已有局部候选代码或 include，但尚未进入 direct JointType mapping、fixture/oracle、focused tests 和 capability 发布；不能替代 `supported` |
| `notCollected` | 还缺 FreeCADCmd native expected 或细化 oracle |
| `releaseGate` | 代码或 fixture 完成后必须通过测试 / 文档 / capability 同步才能发布 |
| `supported` | build、focused tests、expected parity、capability/docs 同步全部通过 |
| `unsupported` | 有 FreeCAD 语义但本包不实现，必须稳定 diagnostic |
| `nonGoal` | 与无状态 CAD Core 边界冲突或明确不属于本包 |

## 验收标准

- `p8_parallel_perpendicular_joint_scope_review_matrix.tsv` 中 `PPJ-SCOPE-002/003` 必须是 `unsupportedImplementable`，不得预先写 `supported`。
- `p8_parallel_perpendicular_joint_non_goal_registry.tsv` 必须包含 extra JointTypes、complex Distance、GUI/session 三类边界。
- 检查命令：

```bash
git status --short
rg -n "supported_joint_matrix|unsupported_joint_matrix|Parallel|Perpendicular" cad-core/src/adapters/c_api/c_api.cpp cad-core/src/assembly/joint_solver.cpp
rg -n "PPJ-SCOPE-00[1-7]" docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S0 不修改 C++。
- S0 不采集 native oracle。
- S0 不关闭 P8ASM-SCOPE-007，只建立本包边界。
