# 【已实现】P8 ScrewRackPinionJoint S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、状态字典和 live baseline，避免把 Screw / RackPinion 当成已支持，或把 complex Distance / GUI session 顺手纳入。

## 输入

- `git status --short`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv`
- `docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/*.tsv`

## live 基线复核结果

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`602e17172c`
- `git log -1 --oneline`：`602e17172c docs: 新增P8螺旋齿条关节收口方案`
- `git -c core.quotepath=false status --short -uall`：S0 编辑前无输出，工作区干净。
- 队列复核：`step_goal_queue.py` 从 S0 起仍列出 S0-S6；本轮只执行 S0，S1-S6 不推进。

## S0 判定

- `cad-core/src/adapters/c_api/c_api.cpp` 的 `supported_joint_matrix` 仅发布 `Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle / Gears / Belt`，`unsupported_joint_matrix` 仍为 `RackPinion / Screw`。
- `cad-core/src/assembly/joint_solver.cpp::isSupportedOndselJointType()` 不包含 `RackPinion` / `Screw`，并注明两者仍缺 FreeCAD marker / sliding-part 专项路径。
- `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 当前只有 scalar `distance`、`distance2`、`angle`，尚无 `Screw` / `RackPinion` sliding-side、JCS order 或 marker rewrite 证据字段。
- FreeCAD `AssemblyObject.cpp` 仍把 `Screw` 映射为 `ASMTScrewJoint(pitch=Distance)`，前置要求 `slidingPartIndex()`，必要时 `swapJCS()`；`RackPinion` 映射为 `ASMTRackPinionJoint(pitchRadius=Distance)`，并通过 `getRackPinionMarkers()` 重写 rack marker。
- `p8_screw_rackpinion_joint_scope_review_matrix.tsv` 中 S0 相关行保持 `unsupportedImplementable` / `notCollected` / `releaseGate` / `nonGoal`，没有 `supported` 抢跑；`non_goal_registry` 已包含 complex Distance、GUI/session、full transaction 边界。
- 结论：S0 证据通过；本轮不改 C++、不采集 oracle、不关闭 P8ASM-SCOPE-007，也不把 scalar `Distance` 的 pitch / pitchRadius 推广成完整 `DistanceType` geometry 支持。

## 声明口径

| 项 | 本包允许的声明 | 禁止声明 |
| --- | --- | --- |
| Screw | FreeCAD 映射 `ASMTScrewJoint(pitch=Distance)`，且依赖 `slidingPartIndex()` / `swapJCS()` 的 request-local 判断 | 不声明完整螺旋运动 UI、动画传动生命周期或跨请求 solver state |
| RackPinion | FreeCAD 映射 `ASMTRackPinionJoint(pitchRadius=Distance)`，且依赖 `getRackPinionMarkers()` 的 rack / pinion marker rewrite | 不声明完整 rack / pinion 建模、GUI reverse 或 persistent MBD session |
| shared sliding | 只迁移同一 request graph 内 Slider joint 证据和 JCS pitch/roll 判断 | 不从几何 shape、bbox、名称或外部状态猜测 sliding side |
| Remaining geometry | complex Distance geometry 保持 notCollected | 不把 Screw / RackPinion 的 `Distance` 推广成完整 DistanceType matrix |

## 纳入 / 排除

| 分类 | 内容 | 状态 |
| --- | --- | --- |
| 纳入 | `slidingPartIndex()` request-local 等价语义 | unsupportedImplementable |
| 纳入 | `Screw -> ASMTScrewJoint(pitch=Distance)` | unsupportedImplementable |
| 纳入 | `RackPinion -> ASMTRackPinionJoint(pitchRadius=Distance)` | unsupportedImplementable |
| 纳入 | RackPinion rack / pinion side detection 与 marker rotation rewrite | unsupportedImplementable |
| 纳入 | c3m6 Screw / RackPinion fixtures、FreeCADCmd expected、C ABI publication | notCollected / releaseGate |
| 排除 | complex Distance geometry / DistanceType 全矩阵 | notCollected |
| 排除 | GUI drag / postDrag / Reverse UI / persistent solver session | nonGoal |
| 排除 | Full Assembly transaction lifecycle | nonGoal |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `unsupportedImplementable` | 当前 published 为 unsupported，但 FreeCAD source 与 cad-core 落点足以定义本轮实现 |
| `notCollected` | 还缺 FreeCADCmd native expected 或 focused oracle |
| `releaseGate` | 代码或 fixture 完成后必须通过测试 / 文档 / capability 同步才能发布 |
| `supported` | build、focused tests、expected parity、capability/docs 同步全部通过 |
| `unsupported` | 有 FreeCAD 语义但本包不实现，必须稳定 diagnostic |
| `nonGoal` | 与无状态 CAD Core 边界冲突或明确不属于本包 |

## 验收标准

- `p8_screw_rackpinion_joint_scope_review_matrix.tsv` 中 Screw / RackPinion 相关行必须是 `unsupportedImplementable` 或 `notCollected`，不得预先写 `supported`。
- `p8_screw_rackpinion_joint_non_goal_registry.tsv` 必须包含 complex Distance、GUI/session、full transaction 三类边界。
- 检查命令：

```bash
git status --short
rg -n "supported_joint_matrix|unsupported_joint_matrix|RackPinion|Screw|DistanceType|slidingPartIndex|swapJCS|getRackPinionMarkers" cad-core/src/adapters/c_api/c_api.cpp cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp src/Mod/Assembly/App/AssemblyObject.cpp
rg -n "SRJ-SCOPE-00[1-8]" docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S0 不修改 C++。
- S0 不采集 native oracle。
- S0 不关闭 P8ASM-SCOPE-007，只建立本包边界。
