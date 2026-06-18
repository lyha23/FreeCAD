# P8 DistanceTypeBasicGeometry S4 Ondsel Distance Joint 映射专项复审

## 目标

在 S3 classification 证据成立后，实现 FreeCAD `makeMbdJointDistance()` 的基础映射：不同 `DistanceType` 生成不同 Ondsel joint class，并写入 `distanceIJ` 或 `offset`。

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

## 必须回写的矩阵行

- `DTC-BLOCK-002`：`PointPoint` 零 / 非零关闭。
- `DTC-BLOCK-003`：`LineLine` / `PointLine` 关闭。
- `DTC-BLOCK-004`：`PlanePlane` / `PointPlane` / `LinePlane` 关闭。
- `DTC-SCOPE-003..005`：实现后从 `backendGap` 改为待 S5 oracle 验证的状态，不能直接 full publish。

## 验收标准

本步代码验收至少包括：

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
```

文档 / 矩阵验收：

```bash
rg -n 'DTC-BLOCK-002.*Closed|DTC-BLOCK-003.*Closed|DTC-BLOCK-004.*Closed' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_blocker_queue.tsv
rg -n 'radius|LineCircle|PlaneCylinder|PointCylinder' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_non_goal_registry.tsv
```

关闭条件：

- 不再把所有 `Distance` joint 固定映射为 `ASMTSphSphJoint`。
- 每个基础 DistanceType 都有 focused runtime assertion。
- `PointPoint` 零距离转 `ASMTSphericalJoint`，不能用 `distanceIJ=0` 代替。
- 不出现 fixture-name branch、bbox guessing、adapter 输出修正或 output sorting。

## 非目标

- 不发布 C ABI capability。
- 不采集 FreeCADCmd expected。
- 不处理 `LineCircle`、`CircleCircle`、`PlaneCylinder`、`CylinderSphere` 等半径类。
