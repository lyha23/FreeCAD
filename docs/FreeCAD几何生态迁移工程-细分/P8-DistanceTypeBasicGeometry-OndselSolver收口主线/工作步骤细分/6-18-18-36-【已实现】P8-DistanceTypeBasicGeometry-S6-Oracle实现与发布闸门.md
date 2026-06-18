# 【已实现】P8 DistanceTypeBasicGeometry S6 Oracle 实现与发布闸门

## 目标

复核 S5 发布后的剩余边界：确认基础 `DistanceType` 已按 solver DTO / native class / scalar 字段 / capability 证据发布，并确认 radius-bearing、curve/default、GUI/session 和 persistent solver state 没有被误发布。

## 输入

- S3-S5 已关闭的 `DTC-BLOCK-001` 到 `DTC-BLOCK-006`
- `DTC-BLOCK-007` boundary protection
- S5 checked-in c3m6 basic DistanceType fixture / FreeCADCmd expected
- C ABI `basic_distance_type` / `distance_type_basic_geometry` publication
- 本包矩阵和上游 P8 AssemblySolver 矩阵

## 发布闸门结果

- `DTC-BLOCK-001..006` 均已由 S3-S5 证据关闭；S6 未发现需要新增 C++、fixture 或 expected 的 regression。
- `DTC-BLOCK-007` 已关闭：`DTC-SCOPE-008` 继续保持 radius-bearing `notCollected`，`DTC-SCOPE-009` 继续保持 curve/default、GUI/session 和 persistent solver state `nonGoal`。
- public capability 只发布 `basic_distance_type`，且 `distance_type_basic_geometry.supported` 仅包含 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`。
- S5 的 native oracle 结论只覆盖 solver DTO、resolved Ondsel class、`distance_ij` / `offset` 和 request-local `jcs_swapped_for_solver`；不声明完整 native subshape marker placement parity。

## blocker 关闭结果

| blocker | 结论 | 证据 |
| --- | --- | --- |
| `DTC-BLOCK-001` | 已由 S3 关闭 | solver JSON 暴露 `distance_type`、Reference1/2 element kind / primitive、request-local `jcs_swapped_for_solver` |
| `DTC-BLOCK-002` | 已由 S4 关闭 | `PointPoint` 零距离为 `ASMTSphericalJoint`，非零为 `ASMTSphSphJoint.distance_ij` |
| `DTC-BLOCK-003` | 已由 S4 关闭 | `LineLine` 为 `ASMTRevCylJoint.distance_ij`，`PointLine` 为 `ASMTCylSphJoint.distance_ij` |
| `DTC-BLOCK-004` | 已由 S4 关闭 | `PlanePlane` / `PointPlane` / `LinePlane` 分别为 `ASMTPlanarJoint.offset`、`ASMTPointInPlaneJoint.offset`、`ASMTLineInPlaneJoint.offset` |
| `DTC-BLOCK-005` | 已由 S5 关闭 | 7 个 c3m6 basic DistanceType fixture 与 FreeCADCmd expected 入库，focused test 对齐 solver DTO / class / scalar |
| `DTC-BLOCK-006` | 已由 S5 关闭 | C ABI capability 和上游 P8 matrix 发布 basic subset，并保留 radius / curve boundary |
| `DTC-BLOCK-007` | 已由 S6 关闭 | 本包 scope / backend gap / nonGoal / blocker matrix 保持 radius-bearing `notCollected`、curve/default 与 GUI/session `nonGoal` |

## scope 结论

| scope | S6 结论 |
| --- | --- |
| `DTC-SCOPE-002..005` | 保持 `supportedFoundation`；它们是分类与 Ondsel mapping 基础，不单独扩大支持声明 |
| `DTC-SCOPE-006` | 保持 `supported`；只表示 basic DistanceType native solver DTO / class / scalar oracle 已锁定 |
| `DTC-SCOPE-007` | 保持 `supported`；只发布 `basic_distance_type` 六类点 / 线 / 平面组合 |
| `DTC-SCOPE-008` | 保持 `notCollected`；`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` 需要第二批 radius package |
| `DTC-SCOPE-009` | 保持 `nonGoal`；`PointCurve`、`CurvePlane`、`Other`、GUI drag / postDrag / Reverse UI 和 persistent solver state 不进入本包 |

## 验收口径

本轮为文档 / 发布边界审计；未修改 C++，不要求构建或 focused runtime tests。最小验收为：

```bash
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
rg -n 'DTC-BLOCK-007|DTC-SCOPE-008|DTC-SCOPE-009|basic_distance_type|PointPoint|LineLine|PointLine|PlanePlane|PointPlane|LinePlane' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
```

## 非目标

- 不实现 radius-bearing DistanceType。
- 不实现 curve/default DistanceType。
- 不声明 full native subshape marker placement parity。
- 不实现 full Assembly transaction、GUI drag / postDrag、Reverse UI 或 persistent solver state。
