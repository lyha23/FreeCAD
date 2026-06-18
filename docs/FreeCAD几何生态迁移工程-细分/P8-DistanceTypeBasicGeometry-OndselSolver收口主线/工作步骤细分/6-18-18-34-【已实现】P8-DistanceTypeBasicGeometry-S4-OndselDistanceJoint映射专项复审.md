# 【已实现】P8 DistanceTypeBasicGeometry S4 Ondsel Distance Joint 映射专项复审

## 目标

在 S3 classification 证据成立后，实现 FreeCAD `makeMbdJointDistance()` 的基础映射：不同 `DistanceType` 生成不同 Ondsel joint class，并写入 `distanceIJ` 或 `offset`。

## live 基线

2026-06-18 live 复核通过，S4 关闭基础 DistanceType 到 Ondsel joint class / scalar field 的 request-local mapping；S5 native oracle / capability 和 S6 发布闸门仍保持待执行。

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`ad5f87179e`
- `git log -1 --oneline`：`ad5f87179e feat: 完成P8 DistanceType S3引用分类`
- `git -c core.quotepath=false status --short -uall`：开始复核时无输出，工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：开始复核时 S4 为首个 pending，S5-S6 仍 pending。

## FreeCAD 依据

| DistanceType | FreeCAD 映射 | cad-core 输出证据 |
| --- | --- | --- |
| `PointPoint` distance < `Precision::Confusion()` | `ASMTSphericalJoint` | `solver_joint_class=ASMTSphericalJoint` |
| `PointPoint` distance > 0 | `ASMTSphSphJoint.distanceIJ = Distance` | `solver_joint_class=ASMTSphSphJoint`、`distance_ij` |
| `LineLine` | `ASMTRevCylJoint.distanceIJ = Distance` | `solver_joint_class=ASMTRevCylJoint`、`distance_ij` |
| `PointLine` | `ASMTCylSphJoint.distanceIJ = Distance` | `solver_joint_class=ASMTCylSphJoint`、`distance_ij` |
| `PlanePlane` | `ASMTPlanarJoint.offset = Distance` | `solver_joint_class=ASMTPlanarJoint`、`offset` |
| `PointPlane` | `ASMTPointInPlaneJoint.offset = Distance` | `solver_joint_class=ASMTPointInPlaneJoint`、`offset` |
| `LinePlane` | `ASMTLineInPlaneJoint.offset = Distance` | `solver_joint_class=ASMTLineInPlaneJoint`、`offset` |

## 范围

- `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()`：`jointType == "Distance"` 必须分派到 DistanceType-specific helper。
- `cad-core/include/cad_core/assembly/joint_solver.h`：必须能承载 resolved class 和 scalar field evidence。
- `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()`：必须输出 focused evidence，便于 tests 和 expected 对比。
- `cad-core/tests/test_p8_features.py`：必须覆盖每一类基础映射。

## 关键实现结论

- `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 增加 request-local `solverJointClass`、`distanceIJ`、`offset`，与 S3 `distanceType` 一起承载 S4 映射证据。
- `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()` 按 FreeCAD `makeMbdJointDistance()` 对基础 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` 写入 resolved class 与 scalar field。
- `cad-core/src/assembly/joint_solver.cpp::makeOndselDistanceJoint()` 不再把已分类的基础 `Distance` 固定映射为 `ASMTSphSphJoint`；无基础 `distanceType` 的旧 scalar Distance fixture 仍走旧 `ASMTSphSphJoint.distanceIJ` fallback，避免误发布半径 / curve 支持。
- `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` 输出 `solver_joint_class`、`distance_ij` 或 `offset`，供 focused tests 与后续 expected 对比。
- `cad-core/tests/test_p8_features.py::test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto` 已覆盖 PointPoint 零 / 非零以及五类基础 point / line / plane 映射。

## 必须回写的矩阵行

- `DTC-BLOCK-002` 已关闭：focused test 证明 `PointPoint` 零距离为 `ASMTSphericalJoint`，非零为 `ASMTSphSphJoint.distanceIJ`。
- `DTC-BLOCK-003` 已关闭：focused test 证明 `LineLine` 为 `ASMTRevCylJoint.distanceIJ`，`PointLine` 为 `ASMTCylSphJoint.distanceIJ`。
- `DTC-BLOCK-004` 已关闭：focused test 证明 `PlanePlane` / `PointPlane` / `LinePlane` 分别为 `ASMTPlanarJoint.offset`、`ASMTPointInPlaneJoint.offset`、`ASMTLineInPlaneJoint.offset`。
- `DTC-SCOPE-003..005` 已从 `backendGap` 改为 `supportedFoundation`：只代表 S4 request-local runtime mapping 已具备，不代表 S5 native expected 或 capability 已发布。
- `DTC-BG-002..004` 已关闭；native expected / capability 剩余闸门继续由 `DTC-BG-005..006` 承接。

## 验收标准

本轮通过：

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
rg -n 'DTC-BLOCK-002.*Closed|DTC-BLOCK-003.*Closed|DTC-BLOCK-004.*Closed' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_blocker_queue.tsv
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

## 关闭条件复核

- 已满足：已分类的基础 `Distance` joint 不再固定映射为 `ASMTSphSphJoint`。
- 已满足：每个基础 DistanceType 都有 focused runtime assertion。
- 已满足：`PointPoint` 零距离转 `ASMTSphericalJoint`，没有用 `distanceIJ=0` 代替。
- 已满足：实现未引入 fixture-name branch、bbox guessing、adapter 输出修正或 output sorting。

## 非目标与剩余风险

- 未发布 C ABI capability，S5/S6 前不得把本包声明为 full supported。
- 未采集 FreeCADCmd expected，S5 仍需补 c3m6 fixtures / expected / collector check。
- 未处理 `LineCircle`、`CircleCircle`、`PlaneCylinder`、`CylinderSphere` 等半径类，仍按 second batch / nonGoal 边界保留。
