# P8 DistanceTypeBasicGeometry 工作步骤总入口

## 目标

把 `Distance` JointType 的基础 `DistanceType` 几何映射从 scalar-only `ASMTSphSphJoint` 推进到 FreeCAD `makeMbdJointDistance()` 等价的 request-local Ondsel 子集，覆盖 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`。

当前收口状态：S0 已完成声明口径与 live scalar-only 基线复核；S1 已完成 FreeCAD 源码候选矩阵复核；S2 已完成范围准入与 blocker 矩阵复核；S3 已完成 request-local reference classification；S4 已完成基础 Ondsel Distance mapping；S5 已完成 native oracle、focused fixtures/tests 和 C ABI capability 发布；S6 已完成发布边界审计并关闭 `DTC-BLOCK-007`。已建立 basic DistanceType solver DTO / class / scalar 证据，但 full native subshape marker placement parity 仍不能作为本包已支持声明。

## 入口校验状态

2026-06-18 已校验：本文作为后续队列入口已成立，步骤索引、执行顺序、状态纪律、矩阵闸门和通用验收均与当前文件树一致。S6 已确认本包只发布 basic DistanceType 六类点 / 线 / 平面组合；半径类仍为后续 `notCollected`，曲线和 GUI/session 仍为 `nonGoal`。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-18-30-【已实现】P8-DistanceTypeBasicGeometry-S0-声明口径与live基线复核.md` | 已实现 | 冻结支持声明、禁止声明、状态字典和 current scalar-only 基线 |
| S1 | `6-18-18-31-【已实现】P8-DistanceTypeBasicGeometry-S1-FreeCAD源码候选矩阵.md` | 已实现 | 建立 DistanceType FreeCAD / cad-core source candidates |
| S2 | `6-18-18-32-【已实现】P8-DistanceTypeBasicGeometry-S2-范围准入与blocker矩阵.md` | 已实现 | 将候选路由到 backendGap、notCollected、releaseGate、nonGoal |
| S3 | `6-18-18-33-【已实现】P8-DistanceTypeBasicGeometry-S3-ReferenceElement分类与JCS顺序专项复审.md` | 已实现 | 复审 Reference element 分类、primitive 判断和 request-local `swapJCS` |
| S4 | `6-18-18-34-【已实现】P8-DistanceTypeBasicGeometry-S4-OndselDistanceJoint映射专项复审.md` | 已实现 | 复审基础 DistanceType 到 Ondsel joint class、`distanceIJ`、`offset` 的映射 |
| S5 | `6-18-18-35-【已实现】P8-DistanceTypeBasicGeometry-S5-NativeOracle与Capability专项复审.md` | 已实现 | 复审 fixtures、FreeCADCmd expected、focused tests 和 capability publication |
| S6 | `6-18-18-36-【已实现】P8-DistanceTypeBasicGeometry-S6-Oracle实现与发布闸门.md` | 已实现 | 关闭 `DTC-BLOCK-007`，确认 radius-bearing / curve / GUI-session 未被误发布 |

## 执行顺序

1. S0 先确认当前 `Distance` solver baseline、禁止声明和工作区状态。
2. S1 锁定 FreeCAD source authority，把基础点 / 线 / 平面、半径类、曲线类、当前 cad-core gap 分开。
3. S2 把所有候选路由到 scope matrix、blocker queue、nonGoal registry 和 backend gap 分类。
4. S3 落 `JointConstraint` / request builder 的基础 element classification 与 JCS ordering 证据，不发布 capability。
5. S4 落基础 DistanceType 到 Ondsel joint 的 request-local adapter 映射和 JSON 证据。
6. S5 已增加 c3m6 fixtures、FreeCADCmd expected、focused runtime tests 和 C ABI capability docs。
7. S6 已复核剩余发布 blocker，确认 radius-bearing / curve / GUI-session 仍未被误发布。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_distance_type_basic_geometry_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | S1 已复核 DistanceType enum、classification、makeMbdJointDistance、cad-core scalar-only gap、collector 和 capability 路由 |
| `p8_distance_type_basic_geometry_scope_review_matrix.tsv` | 当前能力路由 | `DTC-SCOPE-002..005` 已为 `supportedFoundation`；`DTC-SCOPE-006..007` 已完成 basic DistanceType solver oracle 与 capability 发布；半径类保持 notCollected；曲线和 GUI/session 为 nonGoal |
| `p8_distance_type_basic_geometry_blocker_queue.tsv` | 发布前 blocker | `DTC-BLOCK-001..007` 已关闭；边界保护仍限制本包只发布 basic DistanceType |
| `p8_distance_type_basic_geometry_non_goal_registry.tsv` | 非目标边界 | radius-bearing DistanceType、curve/default、GUI/session、persistent solver state 不进入本包 |
| `p8_distance_type_basic_geometry_backend_gap_classification.tsv` | 分类与优先级 | `DTC-BG-001..006` 已关闭；剩余开放项是半径类第二批与曲线 / GUI nonGoal 边界 |

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
