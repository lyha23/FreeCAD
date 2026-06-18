# P8 GearsBeltJoint S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、状态字典和 live baseline，避免把 Gears / Belt 当成已支持或把 RackPinion / Screw 顺手纳入。

## 输入

- `git status --short`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv`
- `docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵/*.tsv`

## 声明口径

| 项 | 本包允许的声明 | 禁止声明 |
| --- | --- | --- |
| Gears | FreeCAD 直接映射 `ASMTGearJoint`，`radiusI=Distance`、`radiusJ=Distance2`，cad-core 可按 request-local real Ondsel 子集实现 | 不声明完整齿轮建模、GUI 交互或动画传动生命周期 |
| Belt | FreeCAD 直接映射 `ASMTGearJoint`，`radiusI=Distance`、`radiusJ=-Distance2`，cad-core 可按 request-local real Ondsel 子集实现 | 不声明 belt path / pulley topology / GUI reverse lifecycle |
| Distance2 DTO | 仅用于 Gears / Belt 的第二半径输入 | 不把 `Distance2` 推广成完整 Distance geometry matrix |
| Remaining JointTypes | RackPinion / Screw 继续 unsupported | 不因为本包实现 ASMTGearJoint 而发布 special marker 或 sliding-part 语义 |

## 纳入 / 排除

| 分类 | 内容 | 状态 |
| --- | --- | --- |
| 纳入 | `Gears -> ASMTGearJoint(radiusI=Distance, radiusJ=Distance2)` | unsupportedImplementable |
| 纳入 | `Belt -> ASMTGearJoint(radiusI=Distance, radiusJ=-Distance2)` | unsupportedImplementable |
| 纳入 | `JointConstraint.distance2` 与 c3m6 Gears / Belt fixtures | notCollected |
| 纳入 | C ABI supported / unsupported matrix 更新 | releaseGate |
| 排除 | RackPinion marker rewrite | unsupported |
| 排除 | Screw sliding-part detection / swapJCS | unsupported |
| 排除 | complex Distance geometry | notCollected |
| 排除 | GUI drag / postDrag / persistent solver session | nonGoal |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `unsupportedImplementable` | 当前 published 为 unsupported，但 FreeCAD source 与 cad-core 落点足以定义本轮实现 |
| `notCollected` | 还缺 FreeCADCmd native expected 或细化 oracle |
| `releaseGate` | 代码或 fixture 完成后必须通过测试 / 文档 / capability 同步才能发布 |
| `supported` | build、focused tests、expected parity、capability/docs 同步全部通过 |
| `unsupported` | 有 FreeCAD 语义但本包不实现，必须稳定 diagnostic |
| `nonGoal` | 与无状态 CAD Core 边界冲突或明确不属于本包 |

## 验收标准

- `p8_gears_belt_joint_scope_review_matrix.tsv` 中 `GBJ-SCOPE-002/003` 必须是 `unsupportedImplementable`，不得预先写 `supported`。
- `p8_gears_belt_joint_non_goal_registry.tsv` 必须包含 RackPinion / Screw、complex Distance、GUI/session 三类边界。
- 检查命令：

```bash
git status --short
rg -n "supported_joint_matrix|unsupported_joint_matrix|Gears|Belt|RackPinion|Screw|Distance2" cad-core/src/adapters/c_api/c_api.cpp cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp
rg -n "GBJ-SCOPE-00[1-8]" docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S0 不修改 C++。
- S0 不采集 native oracle。
- S0 不关闭 P8ASM-SCOPE-007，只建立本包边界。
