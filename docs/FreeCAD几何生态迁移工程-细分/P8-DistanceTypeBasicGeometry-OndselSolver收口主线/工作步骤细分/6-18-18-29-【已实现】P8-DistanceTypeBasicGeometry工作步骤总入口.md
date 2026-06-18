# P8 DistanceTypeBasicGeometry 工作步骤总入口

## 目标

把 `Distance` JointType 的基础 `DistanceType` 几何映射从 scalar-only `ASMTSphSphJoint` 推进到 FreeCAD `makeMbdJointDistance()` 等价的 request-local Ondsel 子集，覆盖 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`。

当前收口状态：S0 到 S6 均为待执行。已建立初始矩阵骨架，但尚未完成 oracle、实现或发布闸门；不得把整个主线写成“已实现”。

## 入口校验状态

2026-06-18 已校验：本文作为后续 S0-S6 队列入口已成立，步骤索引、执行顺序、状态纪律、矩阵闸门和通用验收均与当前文件树一致。该状态只表示本入口文件已完成；S0-S6、oracle、cad-core 实现和发布闸门仍保持待执行。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-18-30-P8-DistanceTypeBasicGeometry-S0-声明口径与live基线复核.md` | 待执行 | 冻结支持声明、禁止声明、状态字典和 current scalar-only 基线 |
| S1 | `6-18-18-31-P8-DistanceTypeBasicGeometry-S1-FreeCAD源码候选矩阵.md` | 待执行 | 建立 DistanceType FreeCAD / cad-core source candidates |
| S2 | `6-18-18-32-P8-DistanceTypeBasicGeometry-S2-范围准入与blocker矩阵.md` | 待执行 | 将候选路由到 backendGap、notCollected、releaseGate、nonGoal |
| S3 | `6-18-18-33-P8-DistanceTypeBasicGeometry-S3-ReferenceElement分类与JCS顺序专项复审.md` | 待执行 | 复审 Reference element 分类、primitive 判断和 request-local `swapJCS` |
| S4 | `6-18-18-34-P8-DistanceTypeBasicGeometry-S4-OndselDistanceJoint映射专项复审.md` | 待执行 | 复审基础 DistanceType 到 Ondsel joint class、`distanceIJ`、`offset` 的映射 |
| S5 | `6-18-18-35-P8-DistanceTypeBasicGeometry-S5-NativeOracle与Capability专项复审.md` | 待执行 | 复审 fixtures、FreeCADCmd expected、focused tests 和 capability publication |
| S6 | `6-18-18-36-P8-DistanceTypeBasicGeometry-S6-Oracle实现与发布闸门.md` | 待执行 | 消费 blockers，落代码，完成发布边界复核 |

## 执行顺序

1. S0 先确认当前 `Distance` solver baseline、禁止声明和工作区状态。
2. S1 锁定 FreeCAD source authority，把基础点 / 线 / 平面、半径类、曲线类、当前 cad-core gap 分开。
3. S2 把所有候选路由到 scope matrix、blocker queue、nonGoal registry 和 backend gap 分类。
4. S3 落 `JointConstraint` / request builder 的基础 element classification 与 JCS ordering 证据，不发布 capability。
5. S4 落基础 DistanceType 到 Ondsel joint 的 request-local adapter 映射和 JSON 证据。
6. S5 增加 c3m6 fixtures、FreeCADCmd expected、focused runtime tests 和 C ABI capability docs。
7. S6 复核所有 blocker，确认 radius-bearing / curve / GUI-session 仍未被误发布。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_distance_type_basic_geometry_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | Seed 已列出 DistanceType enum、classification、makeMbdJointDistance、cad-core scalar-only gap、collector 和 capability 路由 |
| `p8_distance_type_basic_geometry_scope_review_matrix.tsv` | 当前能力路由 | 基础 DistanceType 为 backendGap / notCollected / releaseGate；半径类保持 notCollected；曲线和 GUI/session 为 nonGoal |
| `p8_distance_type_basic_geometry_blocker_queue.tsv` | 发布前 blocker | S3-S6 必须依次关闭 classification、mapping、oracle、capability 和边界保护 |
| `p8_distance_type_basic_geometry_non_goal_registry.tsv` | 非目标边界 | radius-bearing DistanceType、curve/default、GUI/session、persistent solver state 不进入本包 |
| `p8_distance_type_basic_geometry_backend_gap_classification.tsv` | 分类与优先级 | 第一批代码目标是基础 point / line / plane DistanceType |

## 状态纪律

- `supported` 只能在 build、focused tests、FreeCADCmd expected parity、capability/docs 同步全部通过后使用。
- `backendGap` 必须同时有 FreeCAD source authority 和当前 cad-core mismatch evidence。
- `notCollected` 只能表示 native oracle 尚未采集；一旦 fixture / expected 路径明确，S6 必须把它变成代码落点或 release gate。
- 半径类 DistanceType 不能在本包中靠常量、bbox、shape 名称、fixture 名称或 adapter 输出修正实现。
- `swapJCS` 在 CAD Core 中只能是 request-local solver DTO ordering，不得持久修改 DocumentObject graph。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收由 S6 指定。

## 非目标

- 不实现 `getEdgeRadius()` / `getFaceRadius()` 相关半径类 DistanceType。
- 不实现 curve / conic / default DistanceType。
- 不实现 GUI drag / postDrag、Reverse UI 或跨请求 solver session。
- 不修改 unrelated P5/P6/P7/P8 JointType packages。
- 不用 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 DistanceType 业务语义。
