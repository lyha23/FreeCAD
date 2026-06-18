# 【已实现】P8 DistanceTypeBasicGeometry S3 ReferenceElement 分类与 JCS 顺序专项复审

## 目标

复审并实现基础 DistanceType 所需的 request-local reference classification：元素类型、基础几何 primitive、`DistanceType` 结果和 `swapJCS` 等价的 DTO ordering。S3 不发布 solver support，只为 S4 映射提供稳定输入。

## live 基线

2026-06-18 live 复核通过，S3 关闭 request-local classification 与 JSON evidence；S4 Ondsel joint class 映射、S5 native oracle / capability 和 S6 发布闸门仍保持待执行。

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`15ba4de3c1`
- `git log -1 --oneline`：`15ba4de3c1 docs: 完成P8 DistanceType S2矩阵路由复核`
- `git -c core.quotepath=false status --short -uall`：开始复核时无输出，工作区干净。
- `step_goal_queue.py .../6-18-18-33-P8-DistanceTypeBasicGeometry-S3-ReferenceElement分类与JCS顺序专项复审.md --format markdown`：开始复核时 S3 为首个 pending，S4-S6 仍 pending。

## FreeCAD 依据

| 语义 | FreeCAD 入口 | S3 落点 |
| --- | --- | --- |
| 元素类型读取 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 第 160-371 行读取 `Reference1` / `Reference2` 的 `Vertex`、`Edge`、`Face` | `cad-core/src/assembly/joint_solver.cpp` 从 `JointConstraint.reference1/2` 的 target shape / subshape 建立 request-local evidence |
| line / face primitive | 同上调用 `isEdgeType(..., GeomAbs_Line)` 与 `isFaceType(..., GeomAbs_Plane)` | `BRepAdaptor_Curve` / `BRepAdaptor_Surface` 判断 `line` / `plane`，只把 point / line / plane 基础组合提升为 S3 evidence |
| JCS 顺序 | 同上在 point-line、point-face、line-face 等分支调用 `swapJCS(joint)` | 只交换 in-memory `JointConstraint.reference1/2`，并输出 `jcs_swapped_for_solver`，不持久修改 DocumentObject graph |

## 关键实现结论

- `cad-core/include/cad_core/assembly/joint_solver.h` 增加 `JointConstraint::distanceType`，以及 `AssemblyJointReference::elementKind` / `primitive`。
- `cad-core/src/assembly/joint_solver.cpp` 在 `buildAssemblySolveRequest()` 内对 `Distance` 进行 request-local 分类，覆盖 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`。
- `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` 输出 `distance_type`、`reference1_element_kind`、`reference1_primitive`、`reference2_element_kind`、`reference2_primitive`、`jcs_swapped_for_solver`。
- `cad-core/tests/test_p8_features.py` 新增 `test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto`，用临时请求覆盖 6 类基础 DistanceType，并断言 solver DTO order 与原始 joint object order 不同步写回。

## 矩阵回写

- `DTC-BLOCK-001` 已关闭：focused test 证明 solver JSON 暴露 `distance_type`、primitive 和 `jcs_swapped_for_solver`，且 graph 未被修改。
- `DTC-SCOPE-002` 已从 `backendGap` 改为 `supportedFoundation`：仅代表 S3 分类输入已具备，不代表 S4 solver class / scalar field 映射或 S5 oracle 已发布。
- `DTC-BG-001` 已关闭：剩余代码差距转入 `DTC-BG-002..004` 的 S4 Ondsel Distance mapping。

## 验收

本轮通过：

```bash
cmake --build build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
rg -n 'distanceType|distance_type|jcsSwappedForSolver|jcs_swapped_for_solver|reference.*element|primitive' cad-core/include/cad_core/assembly cad-core/src/assembly
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

## 非目标与剩余风险

- 未创建或切换 Ondsel Distance joint class；`ASMTSphericalJoint`、`ASMTRevCylJoint`、`ASMTPlanarJoint` 等映射仍由 S4 处理。
- 未采集 FreeCAD expected，未新增 c3m6 native oracle，未发布 capability。
- radius-bearing、curve/default、GUI/session 和 persistent solver state 仍按 nonGoal / second batch 边界保留。
