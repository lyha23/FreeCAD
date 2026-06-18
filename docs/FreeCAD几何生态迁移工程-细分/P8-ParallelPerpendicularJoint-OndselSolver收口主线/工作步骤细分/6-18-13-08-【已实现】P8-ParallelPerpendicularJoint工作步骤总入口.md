# P8 ParallelPerpendicularJoint 工作步骤总入口

## 目标

把 Parallel / Perpendicular 两个低风险 JointType 从 remaining unsupported 队列推进到 real Ondsel request-local supported 子集，并保持其它复杂 JointType 不被误发布。

入口校验状态：已核对 S0-S6 文件名与首段、5 个矩阵表头与行数、矩阵闸门和执行顺序；本步骤总入口已实现。

当前主线状态：S0 到 S6 均为 `【已实现】`。Parallel / Perpendicular 已完成 direct real Ondsel mapping、c3m6 native expected、focused tests、C ABI capability 发布和 upstream P8 矩阵同步；剩余 RackPinion / Screw / Gears / Belt 继续 unsupported，GUI/session 继续 nonGoal。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-13-09-【已实现】P8-ParallelPerpendicularJoint-S0-声明口径与live基线复核.md` | 已实现 | 冻结支持声明、非目标和 current unsupported 基线 |
| S1 | `6-18-13-10-【已实现】P8-ParallelPerpendicularJoint-S1-FreeCAD源码候选矩阵.md` | 已实现 | 建立 FreeCAD / cad-core source candidates |
| S2 | `6-18-13-11-【已实现】P8-ParallelPerpendicularJoint-S2-范围准入与blocker矩阵.md` | 已实现 | 将候选路由到 unsupported、notCollected、nonGoal |
| S3 | `6-18-13-12-【已实现】P8-ParallelPerpendicularJoint-S3-JointType映射专项复审.md` | 已实现 | 验证 ASMTParallelAxesJoint / ASMTPerpendicularJoint 映射 |
| S4 | `6-18-13-13-【已实现】P8-ParallelPerpendicularJoint-S4-NativeOracle与placement写回专项复审.md` | 已实现 | 对齐 native expected、request-local writeback 和 expected parity |
| S5 | `6-18-13-14-【已实现】P8-ParallelPerpendicularJoint-S5-Capability与unsupported矩阵专项复审.md` | 已实现 | 同步 supported / unsupported matrix、tests、P8 docs |
| S6 | `6-18-13-15-【已实现】P8-ParallelPerpendicularJoint-S6-Oracle实现与发布闸门.md` | 已实现 | 关闭 PPJ-BLOCK-001..005 并记录发布验证 |

## 执行顺序

1. 先执行 S0，确认当前 `supported_joint_matrix` / `unsupported_joint_matrix` 和工作区状态。
2. 执行 S1 / S2，锁定 FreeCAD source authority，并把 Parallel / Perpendicular 与复杂 JointType 分开。
3. 执行 S3，落地 C++ ASMT 映射和 supported predicate。
4. 执行 S4，增加 c3m6 fixtures、FreeCADCmd expected 和 expected parity。
5. 执行 S5，同步 capabilities、focused tests 和既有 P8 AssemblySolver 矩阵。
6. 执行 S6，关闭 blocker 后再提交；未关闭前不得改名为 `【已实现】`。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_parallel_perpendicular_joint_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | FreeCAD 映射、cad-core 落点、fixture/oracle、capability 和 upstream matrix 证据已闭合 |
| `p8_parallel_perpendicular_joint_scope_review_matrix.tsv` | 当前能力路由 | PPJ-SCOPE-002/003/004/005 为 supported；PPJ-SCOPE-006 unsupported；PPJ-SCOPE-007 nonGoal |
| `p8_parallel_perpendicular_joint_blocker_queue.tsv` | 发布前 blocker | PPJ-BLOCK-001..005 已关闭 |
| `p8_parallel_perpendicular_joint_non_goal_registry.tsv` | 非目标边界 | RackPinion / Screw / Gears / Belt、复杂 Distance、GUI/session 不进入本包 |
| `p8_parallel_perpendicular_joint_backend_gap_classification.tsv` | 分类与优先级 | 本包只推进 Parallel / Perpendicular |

## 状态纪律

- `supported` 已用于 Parallel / Perpendicular 的 request-local 子集，依据是 build、focused tests、expected parity、capability/docs 同步均通过。
- 当前 `Parallel` / `Perpendicular` 不是 `backendGap`，也不再是 remaining unsupported。
- `RackPinion / Screw / Gears / Belt` 保持 unsupported；复杂 Distance geometry 保持 notCollected。
- 不允许用 `ASMTParallelAxesJoint` 已存在来跳过 FreeCADCmd expected。
- 不允许恢复 representative fallback 或 adapter 层业务补丁。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收由 S6 指定。

## 非目标

- 不实现 RackPinion / Screw / Gears / Belt。
- 不实现完整 Distance geometry matrix。
- 不实现 GUI drag / postDrag 或跨请求 solver session。
- 不修改 unrelated P5/P6/P7 packages。
