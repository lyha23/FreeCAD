# P8 DistanceTypeExtendedGeometry S4 OndselDistanceJoint 扩展映射专项复审

## 目标

对 extended DistanceType 的 ASMT class、`distanceIJ` / `offset` 和 radius scalar correction 做 FreeCAD 等价映射设计。S4 可写 C++ DTO / mapping，但不发布 capability。

## 必须完成

- 显式 switch cases 必须按 FreeCAD source 成批映射：edge circle、face radius、torus/sphere、point cylinder/sphere、point curve。
- `makeOndselDistanceJoint()` 只能消费 resolver 产出的 `solverJointClass` 和 scalar fields；不得按 fixture 名称分支。
- default / TODO branch 必须有明确处理：native oracle 证明后 supported，不能证明则 diagnostic-only 或 nonGoal。
- PointLine 既有 native parity `ASMTLineInPlaneJoint` 口径不得回退。

## 验收

```bash
rg -n 'LineCircle|CircleCircle|PlaneCylinder|CylinderSphere|PointCurve|solverJointClass|distanceIJ|offset' cad-core/src/assembly/joint_solver.cpp cad-core/tests/test_p8_features.py
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 非目标

- 不改 capability supported matrix。
- 不修改 checked-in expected 适配 cad-core。
