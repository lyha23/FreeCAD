# 【已实现】P8 ParallelPerpendicularJoint S2 范围准入与 blocker 矩阵

## 目标

把 S1 最新 `source_candidates` 路由成可执行 scope、blocker、backend gap classification 和 nonGoal 队列。S2 只做准入与排队，不关闭 blocker、不实现 mapping、不采集 expected。

## live 基线

| 命令 | 当前输出 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `87f1974f8d` |
| `git log -1 --oneline` | `87f1974f8d 实现 P8 Assembly Cylindrical Joint 支持` |
| `git -c core.quotepath=false status --short -uall` | 本包入口、S0-S6 和 5 个 TSV 均为未跟踪文件；本轮只修改 S2 与本包 `scope_review`、`blocker_queue`、`backend_gap_classification`、`non_goal_registry` 四个 TSV |

## 代码证据

- FreeCAD direct `Parallel` / `Perpendicular` 映射存在：S1 已确认 `AssemblyObject::makeMbdJointOfType()` 分别返回 `ASMTParallelAxesJoint` / `ASMTPerpendicularJoint`。
- cad-core direct `Parallel` / `Perpendicular` 当前仍缺：`cad-core/src/assembly/joint_solver.cpp` 只 include `ASMTParallelAxesJoint`，没有 include `ASMTPerpendicularJoint`。
- `makeOndselJointOfType()` 只在 `joint.jointType == "Angle"` 且 `angleRadians == 0.0` 时返回 `ASMTParallelAxesJoint`；这不能视为 direct `JointType == "Parallel"` 已支持。
- `isSupportedOndselJointType()` 当前仅包含 `Fixed/Revolute/Cylindrical/Slider/Ball/Distance/Angle`，不包含 `Parallel` / `Perpendicular`。
- `cad-core/src/adapters/c_api/c_api.cpp` 当前 `supported_joint_matrix` 不含 `Parallel` / `Perpendicular`，`unsupported_joint_matrix` 仍包含 `Parallel`、`Perpendicular`、`RackPinion`、`Screw`、`Gears`、`Belt`。

## 分类规则

| 状态 | 准入条件 | 本包动作 |
| --- | --- | --- |
| `unsupportedImplementable` | 当前 published unsupported，但 FreeCAD direct source、cad-core 落点和 fixture/oracle 路由明确 | 进入 S3-S6 |
| `notCollected` | 缺 checked-in native expected 或 collector 复核 | S4 采集或验证后再允许 supported claim |
| `releaseGate` | 实现后必须同步 capability / docs / tests / upstream matrix | S5-S6 关闭 |
| `unsupported` | FreeCAD 有语义，但本包不具备完整 DTO/oracle/test 范围 | 保持 diagnostic-only |
| `nonGoal` | 与 stateless CAD Core 边界冲突或属于其它主线 | 公开排除 |

## scope 路由

| scope | 当前状态 | S2 结论 | 下一步 |
| --- | --- | --- | --- |
| `PPJ-SCOPE-001` | `supportedBaseline` | Cylindrical、hard-linked Ondsel、request-local placement writeback 是上游已支持基线；本包不重开 | 保持 baseline |
| `PPJ-SCOPE-002` | `unsupportedImplementable` | FreeCAD direct `Parallel -> ASMTParallelAxesJoint` 存在，但 cad-core direct `Parallel` 分支和 supported predicate 仍缺；`Angle=0` 路径不能替代 | S3/S4 |
| `PPJ-SCOPE-003` | `unsupportedImplementable` | FreeCAD direct `Perpendicular -> ASMTPerpendicularJoint` 存在，但 cad-core include、conversion 和 supported predicate 仍缺 | S3/S4 |
| `PPJ-SCOPE-004` | `notCollected` | Parallel / Perpendicular native expected 尚未入库，不能声明 supported | S4 |
| `PPJ-SCOPE-005` | `releaseGate` | capability、focused tests、P8 docs / TSV 需要在实现和 expected 后同步 | S5/S6 |
| `PPJ-SCOPE-006` | `unsupported` | RackPinion / Screw / Gears / Belt 继续 diagnostic-only，本包不实现 | 保持 unsupported |
| `PPJ-SCOPE-007` | `nonGoal` | GUI drag / postDrag、persistent solver session 和后端跨请求状态不属于 stateless CAD Core | 保持 nonGoal |

## blocker 可执行队列

| blocker | 关联 scope | 队列动作 | 关闭条件 |
| --- | --- | --- | --- |
| `PPJ-BLOCK-001` | `PPJ-SCOPE-002;PPJ-SCOPE-003` | S3 补 direct Parallel / Perpendicular ASMT mapping 与 supported predicate | focused runtime 看到两者为 `real_ondsel_solver` 且 `unsupported_joints` 为空 |
| `PPJ-BLOCK-002` | `PPJ-SCOPE-002;PPJ-SCOPE-004` | S4 增加 Parallel fixture 并采集 / 验证 FreeCADCmd expected | expected parity 通过且无 known_gap / fixture-name special casing |
| `PPJ-BLOCK-003` | `PPJ-SCOPE-003;PPJ-SCOPE-004` | S4 增加 Perpendicular fixture 并采集 / 验证 FreeCADCmd expected | expected parity 通过且无 known_gap / fixture-name special casing |
| `PPJ-BLOCK-004` | `PPJ-SCOPE-005` | S5/S6 同步 C ABI supported / unsupported matrix、tests 和 upstream P8 docs / TSV | Parallel / Perpendicular 只在 supported matrix，unsupported matrix 无 stale row |
| `PPJ-BLOCK-005` | `PPJ-SCOPE-006` | S5/S6 锁住 remaining unsupported 边界 | RackPinion / Screw / Gears / Belt 仍 unsupported，未发布 full solver claim |

## 矩阵回写

- `p8_parallel_perpendicular_joint_scope_review_matrix.tsv`：明确 `PPJ-SCOPE-002/003` 是 `unsupportedImplementable`，不是 supported。
- `p8_parallel_perpendicular_joint_blocker_queue.tsv`：保留 `PPJ-BLOCK-001..005` 全部 open next step，不在 S2 关闭。
- `p8_parallel_perpendicular_joint_backend_gap_classification.tsv`：把 Parallel / Perpendicular 归入 implementable unsupported，把 native expected 归入 `notCollected`，把 capability/docs 归入 `releaseGate`。
- `p8_parallel_perpendicular_joint_non_goal_registry.tsv`：GUI/session 保持 `nonGoal`；RackPinion / Screw / Gears / Belt 保持本包排除但仍是 published unsupported。

## 验收标准

- 5 个 TSV 字段数一致。
- `PPJ-SCOPE-001..007` 与 `PPJ-BLOCK-001..005` 均能在本包矩阵中检索到。
- 队列脚本显示 S2 已从队首移除，S3 成为下一个 pending。
- `git diff --check` 仅覆盖本包文档路径。

```bash
for f in docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
rg -n "PPJ-SCOPE-00[1-7]|PPJ-BLOCK-00[1-5]" docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线
```

## 非目标

- S2 不修改源代码、fixtures/tests 或 upstream P8 文件。
- S2 不关闭 blocker，不实现 mapping，不采集 expected。
- S2 不把 `PPJ-SCOPE-002/003` 写成 supported。
- S2 不扩大到 RackPinion / Screw / Gears / Belt、完整 Distance geometry、GUI drag / postDrag、persistent solver session 或 Link lifecycle。
