# P8 DistanceTypeExtendedGeometry S3 RadiusPrimitive 证据专项复审

## 目标

先补 cad-core request-local DTO / JSON evidence 设计，再决定 native expected 和 C++ 映射。S3 重点是 primitive 与 radius evidence，不发布 ASMT support。

## 必须完成

- 设计 `JointConstraint` 中的 radius evidence 字段，至少能表达 edge radius、face radius、radius source side 和 scalar correction。
- 设计 `solverJointJson()` 输出，包含 `reference*_primitive`、`reference*_radius`、`distance_type`、`jcs_swapped_for_solver`。
- 复核 `classifyDistanceType()` 需要新增的 primitive cases：circle edge、cylinder/sphere/cone/torus face、curve edge。
- 对 unsupported / default branch 输出稳定 diagnostic 或 boundary evidence，避免 silent fallback。

## 验收

```bash
rg -n 'distanceType|reference.*primitive|radius|jcsSwappedForSolver|solverJointJson' cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp cad-core/src/assembly/assembly_utils.cpp
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 非目标

- 不先改 expected。
- 不绕过 DTO 在 adapter 或 tests 里拼字段。
