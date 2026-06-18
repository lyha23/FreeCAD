# P8 ScrewRackPinionJoint 工作步骤总入口

## 目标

把 `Screw` / `RackPinion` 两个 remaining special JointType 从 unsupported 队列推进到 real Ondsel request-local supported 子集，并保持 complex Distance geometry、GUI/session lifecycle 不被误发布。

当前收口状态：工作步骤索引已完成校验；S0 已完成 live 基线复核，S1 到 S6 仍为待执行。已建立初始矩阵骨架，但尚未完成 `slidingPartIndex()`、`swapJCS()`、RackPinion marker rewrite、native expected、focused tests 或发布闸门；不得把整个主线写成“已实现”。

索引关闭口径：本文件只表示工作步骤索引、矩阵文件名和轻量验收命令已经复核；S0-S6 仍由各自文件推进，不能因为本文件改名为 `【已实现】` 而跳过后续队列。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-16-36-【已实现】P8-ScrewRackPinionJoint-S0-声明口径与live基线复核.md` | 已实现 | 冻结支持声明、禁止声明和 current unsupported 基线 |
| S1 | `6-18-16-37-P8-ScrewRackPinionJoint-S1-FreeCAD源码候选矩阵.md` | 待执行 | 建立 Screw / RackPinion FreeCAD / cad-core source candidates |
| S2 | `6-18-16-38-P8-ScrewRackPinionJoint-S2-范围准入与blocker矩阵.md` | 待执行 | 将候选路由到 implementable unsupported、notCollected、releaseGate、nonGoal |
| S3 | `6-18-16-39-P8-ScrewRackPinionJoint-S3-SlidingAxis与swapJCS专项复审.md` | 待执行 | 收口 `slidingPartIndex()` / `swapJCS()` 共享前置 |
| S4 | `6-18-16-40-P8-ScrewRackPinionJoint-S4-RackPinionMarker重写专项复审.md` | 待执行 | 收口 RackPinion rack / pinion marker rewrite |
| S5 | `6-18-16-41-P8-ScrewRackPinionJoint-S5-NativeOracle与Capability专项复审.md` | 待执行 | 同步 fixtures、FreeCADCmd expected、focused tests 和 capabilities |
| S6 | `6-18-16-42-P8-ScrewRackPinionJoint-S6-Oracle实现与发布闸门.md` | 待执行 | 消费 blockers 并给出代码落点和验收命令 |

## 执行顺序

1. S0 已确认当前 `supported_joint_matrix` / `unsupported_joint_matrix`、Screw / RackPinion 的禁止声明和工作区状态。
2. 执行 S1 / S2，锁定 FreeCAD source authority，并把 shared sliding 前置、Screw pitch、RackPinion marker rewrite 和 nonGoal 分开。
3. 执行 S3，落地 `slidingPartIndex()` / `swapJCS()` 的 request-local 等价语义。
4. 执行 S4，落地 RackPinion 的 marker side detection 与 rack marker 旋转重写。
5. 执行 S5，增加 Screw / RackPinion c3m6 fixtures、FreeCADCmd expected、focused tests 和 capability 断言。
6. 执行 S6，关闭 blocker 后再提交；未关闭前不得改名为 `【已实现】`。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_screw_rackpinion_joint_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | seed 已列 Screw / RackPinion mapping、slidingPartIndex、RackPinion marker rewrite、当前 unsupported publication 和 test 落点 |
| `p8_screw_rackpinion_joint_scope_review_matrix.tsv` | 当前能力路由 | SRJ-SCOPE-002 / 003 / 004 / 005 / 006 为 implementable unsupported / notCollected / releaseGate |
| `p8_screw_rackpinion_joint_blocker_queue.tsv` | 发布前 blocker | shared sliding、Screw adapter、RackPinion marker、oracle、capability、nonGoal boundary 均待关闭 |
| `p8_screw_rackpinion_joint_non_goal_registry.tsv` | 非目标边界 | complex Distance、GUI/session、full transaction 不进入本包 |
| `p8_screw_rackpinion_joint_backend_gap_classification.tsv` | 分类与优先级 | 本包只推进 Screw / RackPinion |

## 状态纪律

- `supported` 只能在 build、focused tests、expected parity、capability/docs 同步全部通过后使用。
- 当前 Screw / RackPinion 是 `unsupportedImplementable`，不是 `backendGap`。
- complex Distance geometry 保持 `notCollected`；GUI/session lifecycle 保持 `nonGoal`。
- 不允许靠 JointType 名称存在来跳过 FreeCADCmd expected。
- 不允许恢复 representative fallback、adapter 层业务补丁或跨请求 solver session。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收由 S6 指定。

## 非目标

- 不实现完整 Distance geometry matrix。
- 不实现 GUI drag / postDrag、Reverse UI 或跨请求 solver session。
- 不修改 unrelated P5/P6/P7 packages。
- 不用 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 JointType 业务语义。
