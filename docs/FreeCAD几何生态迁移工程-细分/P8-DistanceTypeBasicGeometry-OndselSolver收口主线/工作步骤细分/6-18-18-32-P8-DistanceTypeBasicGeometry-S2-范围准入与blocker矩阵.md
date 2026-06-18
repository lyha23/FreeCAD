# P8 DistanceTypeBasicGeometry S2 范围准入与 blocker 矩阵

## 目标

把 S1 source candidates 路由到 scope review、blocker queue、nonGoal registry 和 backend gap classification，让 S3-S6 可以直接消费实现项。

## 分类规则

| 分类 | 准入条件 | 示例 |
| --- | --- | --- |
| `supportedBaseline` | 当前 cad-core 已支持，且不由本包修改 | scalar Distance JointType 存在，但不代表 DistanceType 已支持 |
| `backendGap` | FreeCAD 有明确语义，cad-core 当前不等价，且本包能闭环 | `PointPoint` 零 / 非零、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` |
| `notCollected` | 需要 native oracle 但尚未采集 | c3m6 expected、radius-bearing DistanceType |
| `releaseGate` | 实现后必须同步才能发布 | C ABI capability、docs / matrix publication |
| `nonGoal` | 本包不做且有 reopen 条件 | curve/default、GUI/session、persistent solver |

## blocker 路由

| blocker | 消费步骤 | 关闭条件 |
| --- | --- | --- |
| `DTC-BLOCK-001` | S3 | `JointConstraint` / JSON 能暴露基础 DistanceType、element classification 和 JCS ordering |
| `DTC-BLOCK-002` | S4 | `PointPoint` 零 / 非零映射到正确 Ondsel joint |
| `DTC-BLOCK-003` | S4 | `LineLine` / `PointLine` 映射到正确 Ondsel joint |
| `DTC-BLOCK-004` | S4 | `PlanePlane` / `PointPlane` / `LinePlane` 映射到正确 Ondsel joint |
| `DTC-BLOCK-005` | S5 | fixtures 和 FreeCADCmd expected 存在并通过 check |
| `DTC-BLOCK-006` | S5 / S6 | capability、tests、docs / matrices 同步 |
| `DTC-BLOCK-007` | S6 | radius-bearing / curve / GUI/session 未被误发布 |

## 必须回写的矩阵行

- `p8_distance_type_basic_geometry_scope_review_matrix.tsv`：所有 `DTC-SCOPE-*` 状态必须有效。
- `p8_distance_type_basic_geometry_blocker_queue.tsv`：所有 backendGap / releaseGate 都必须有 blocker。
- `p8_distance_type_basic_geometry_non_goal_registry.tsv`：所有 nonGoal 必须有用户 / 协议行为和 reopen 条件。
- `p8_distance_type_basic_geometry_backend_gap_classification.tsv`：所有 P1 项必须有下一轮代码落点。

## 验收标准

```bash
rg -n 'backendGap|notCollected|releaseGate|nonGoal|supportedBaseline' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_scope_review_matrix.tsv
rg -n 'DTC-BLOCK-00[1-7]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_blocker_queue.tsv
rg -n 'DTC-NG-00[1-4]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_non_goal_registry.tsv
rg -n 'DTC-BG-00[1-8]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_backend_gap_classification.tsv
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

完成条件：

- 每个 scope 都路由到有效状态。
- 每个 blocker / nonGoal / backendGap row 都能追溯到 scope 或 exclusion reason。
- 不允许没有 FreeCAD authority 的 `backendGap`。
- 不允许把 `notCollected` 当成长期文档状态；S6 必须给出采集或延后路径。

## 非目标

- 不把半径类 DistanceType 并入第一批。
- 不做代码变更。
- 不做发布声明。
