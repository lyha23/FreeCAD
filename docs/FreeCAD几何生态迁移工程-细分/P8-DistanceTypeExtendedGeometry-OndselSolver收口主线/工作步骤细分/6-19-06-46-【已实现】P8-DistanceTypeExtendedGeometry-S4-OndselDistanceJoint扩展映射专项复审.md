# P8 DistanceTypeExtendedGeometry S4 OndselDistanceJoint 扩展映射专项复审【已实现】

## 目标

对 extended DistanceType 的 ASMT class、`distanceIJ` / `offset` 和 radius scalar correction 做 FreeCAD 等价映射设计。S4 可写 C++ DTO / mapping，但不发布 capability。

## live 基线

- 复核仓库：`pwd=/Users/li/Chili3DProject/FreeCAD`，本轮开始 `HEAD=28b13586a1`，最新提交为 `28b13586a1 assembly: 完成P8扩展DistanceType S3证据层`。
- 复核开始时工作区仅见 unrelated `AGENTS.md` dirty；本步骤未编辑或暂存 `AGENTS.md`。
- S3 已提供 `reference*.radius`、`scalarCorrection`、primitive、mapping status 和 default boundary evidence；S4 只消费这些 request-local 字段做 ASMT 映射，不采 oracle、不改 checked-in expected、不发布 capability。

## 已完成

- `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()` 已按 FreeCAD `AssemblyObject.cpp::makeMbdJointDistance()` 显式 switch 映射 extended cases：
  - `LineCircle` / `CircleCircle` -> `ASMTRevCylJoint.distanceIJ = Distance + scalarCorrection`。
  - `PlaneCylinder` -> `ASMTLineInPlaneJoint.offset = Distance + scalarCorrection`。
  - `PlaneSphere` -> `ASMTPointInPlaneJoint.offset = Distance + scalarCorrection`。
  - `PlaneTorus` / `TorusTorus` -> `ASMTPlanarJoint.offset = Distance`。
  - `CylinderCylinder` / `CylinderTorus` -> `ASMTRevCylJoint.distanceIJ = Distance + scalarCorrection`。
  - `CylinderSphere` / `TorusSphere` -> `ASMTCylSphJoint.distanceIJ = Distance + scalarCorrection`。
  - `SphereSphere` -> `ASMTSphSphJoint.distanceIJ = Distance + scalarCorrection`。
  - `PointCylinder` -> `ASMTCylSphJoint.distanceIJ = Distance + scalarCorrection`。
  - `PointSphere` -> `ASMTSphSphJoint.distanceIJ = Distance + scalarCorrection`。
  - `PointCurve` -> `ASMTPointInPlaneJoint.offset = Distance`。
- Mapped extended cases now expose `distance_type_mapping_status=mapped_s4_extended` and `distance_type_boundary=extended_mapping_pending_s5_oracle`，明确 S5 native oracle 仍是 parity 闸门。
- `PointLine` 继续保持已采 native parity 口径：`ASMTLineInPlaneJoint.offset`，未回退到 FreeCAD source 中的 `ASMTCylSphJoint.distanceIJ`。
- `PlaneCone`、`LineCylinder` 等 line-surface、`CurvePlane` 等 curve-face 和 `Other` 保持 `default_boundary_not_mapped`，不写 `solver_joint_class`、`distance_ij` 或 `offset`。
- `cad-core/tests/test_p8_features.py::test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto` 已扩展 synthetic coverage：edge circle、face radius、torus/sphere、`PointCurve` 和 default no-mapping 均有断言。

## blocker 结论

- `DTE-BLOCK-003` / `DTE-BLOCK-004` 的 S4 mapping 部分已完成，但 native expected 和 parity 仍由 S5/S6 关闭。
- `DTE-BLOCK-005`、`DTE-BLOCK-006`、`DTE-BLOCK-007`、`DTE-BLOCK-008` 保持 open：torus/sphere oracle、default/curve boundary、expected 采集和 capability 发布仍不能被 S4 代替。
- 本步骤没有新增 supported capability，也没有删除任何 `notCollected` / `oracleFirst` / `defaultBoundary` 结论。

## 验收

```bash
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto
rg -n 'LineCircle|CircleCircle|PlaneCylinder|CylinderSphere|PointCurve|solverJointClass|distanceIJ|offset' cad-core/src/assembly/joint_solver.cpp cad-core/tests/test_p8_features.py
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 非目标

- 不改 capability supported matrix。
- 不修改 checked-in expected 适配 cad-core。
- 不关闭 S5/S6 native oracle、fixture parity 或 publication gate。
