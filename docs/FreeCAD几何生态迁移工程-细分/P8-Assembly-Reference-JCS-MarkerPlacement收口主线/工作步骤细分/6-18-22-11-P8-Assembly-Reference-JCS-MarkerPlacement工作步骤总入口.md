# P8 Assembly Reference / JCS MarkerPlacement 工作步骤总入口

## 目标

把 P8 Assembly solver 已发布子集中，Joint `Reference1/2` + `Placement1/2` 的 subshape JCS marker placement 从当前 connector-only baseline 推进到 FreeCAD `handleOneSideOfJoint()` 等价语义。本包按最小完整语义批次推进，覆盖 ordinary dispatch、object/subshape global transform、part-local transform、offsetPlc、Vertex / Edge / Face oracle、mixed swap/current value、special rewrite regression 和 capability publication。

当前收口状态：本文件是执行索引。S0 已完成 live 基线复核；S1 已完成 FreeCAD 源码候选矩阵复核；S2 已完成范围准入与 blocker 矩阵复核；S3-S6 尚未执行，不得把本方案写成已支持。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-22-12-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S0-声明口径与live基线复核.md` | 已实现 | 冻结 supported claim、禁止声明和 current connector-only marker baseline |
| S1 | `6-18-22-13-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S1-FreeCAD源码候选矩阵.md` | 已实现 | 建立 `makeMbdJoint()` / `handleOneSideOfJoint()` / `getRackPinionMarkers()` / `getJointCurrentValue()` source candidates |
| S2 | `6-18-22-14-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S2-范围准入与blocker矩阵.md` | 已实现 | 路由 14 个 scope 和 10 个 blocker，明确哪些是 backendGap、notCollected、releaseGate、nonGoal |
| S3 | `6-18-22-15-P8-Assembly-Reference-JCS-MarkerPlacement-S3-MarkerPlacementResolver专项复审.md` | 待执行 | 设计 cad-core 统一 marker resolver、diagnostic、swap sync、offsetPlc 边界和 fallback audit |
| S4 | `6-18-22-16-P8-Assembly-Reference-JCS-MarkerPlacement-S4-NativeOracle与代表fixture专项复审.md` | 待执行 | 批量采集 Vertex / Edge / Face / mixed / current value / special regression native expected |
| S5 | `6-18-22-17-P8-Assembly-Reference-JCS-MarkerPlacement-S5-实现与focused-parity.md` | 待执行 | 切换 resolver 主路径，跑 focused parity，证明真实 Ondsel marker path 消费 resolver 输出 |
| S6 | `6-18-22-18-P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md` | 待执行 | 发布 capability/docs，关闭 MP-BLOCK-001..010，确认 excluded axes 未被误发布 |

## 执行顺序

1. S0 已确认当前 `jointReference()` connector-only baseline、已有 P8 object-level supported baseline、DistanceType S6 DTO / class / scalar 边界和工作区状态。
2. S1 已完成 FreeCAD / cad-core 源码候选扫描，候选不等于 supported。
3. S2 已把已复核候选分类为 `supportedBaseline`、`backendGap`、`notCollected`、`releaseGate` 或 `nonGoal`，并确认 10 个 blocker 足以阻止单 fixture 推进。
4. S3 复审 marker resolver 设计：统一计算 moving part local marker，不按 fixture 名称、JointType、bbox 或 subshape 顺序补输出。
5. S4 批量采集 object / vertex / edge / face / mixed / current value / special rewrite representative expected。
6. S5 落 C++、fixtures、focused tests；保持 RackPinion / Screw 特例不回退，并证明 `addConstraintToOndselAssembly()` 消费 resolver 输出。
7. S6 回写 C ABI capabilities、P8 AssemblySolver / DistanceType docs 和矩阵边界。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_marker_placement_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | 20 条 candidates，S1 已复核；候选不等于 supported |
| `p8_marker_placement_scope_review_matrix.tsv` | 当前能力路由 | S2 确认 14 个 scope：`supportedBaseline=1`、`backendGap=4`、`notCollected=5`、`releaseGate=3`、`nonGoal=1` |
| `p8_marker_placement_blocker_queue.tsv` | 发布前 blocker | S2 确认 10 个 blocker 覆盖 ordinary/global/part-local/oracle/mixed/real Ondsel/special/capability/boundary/fallback；S3-S6 依次消费 |
| `p8_marker_placement_non_goal_registry.tsv` | 非目标边界 | S2 确认 radius-bearing、curve/default、GUI/session、persistent solver state、connector-only shortcut 排除与 reopen condition |
| `p8_marker_placement_backend_gap_classification.tsv` | 分类与优先级 | S2 确认 12 类分类，ordinary/global/part-local/mixed 是 P0；oracle、special、publication 是 P1；non-goal boundary 是 P2 |

## 状态纪律

- `supported` 只能在 build、focused tests、FreeCADCmd expected parity、capability/docs 同步后使用。
- `backendGap` 必须同时有 FreeCAD source authority 和当前 cad-core mismatch evidence。
- `notCollected` 表示需要 native oracle，但尚未采集或入库。
- `nonGoal` 必须写清 reopen condition。
- 不允许把 radius-bearing DistanceType、curve/default、GUI/session、persistent solver state 合入本包。
- 不允许用 fixture 名称、bbox、shape 数量、subshape 输出顺序或 adapter 输出修正推断 marker placement。
- 不允许只采一个 oracle case、只补一个 fixture 或只修一个 DistanceType 代表；若必须拆分，必须说明 FreeCAD 调用链分叉、oracle 阻塞或跨模块风险，并写清下一批次范围。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收由 S5 / S6 指定。

## 非目标

- 不实现 radius-bearing DistanceType。
- 不实现 curve/default DistanceType。
- 不实现 GUI drag / postDrag、Reverse UI 或跨请求 solver session。
- 不修改 unrelated P5/P6/P7 packages。
