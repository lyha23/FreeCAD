# P8 DistanceTypeExtendedGeometry S3 RadiusPrimitive 证据专项复审【已实现】

## 目标

先补 cad-core request-local DTO / JSON evidence 设计，再决定 native expected 和 C++ 映射。S3 重点是 primitive 与 radius evidence，不发布 ASMT support。

## live 基线

- 复核仓库：`pwd=/Users/li/Chili3DProject/FreeCAD`，本轮开始 `HEAD=19ed87acea`，最新提交为 `19ed87acea docs: 完成P8扩展DistanceType S2范围冻结`。
- 复核开始时工作区仅见 unrelated `AGENTS.md` dirty；本步骤未编辑或暂存 `AGENTS.md`。
- S0-S2 已冻结 basic baseline、完整 remaining enum 覆盖和 blocker ownership；本步骤只消费 `DTE-BLOCK-002`，不关闭 S4-S6 blocker。

## 已完成

- `cad-core/include/cad_core/assembly/joint_solver.h` 已在 request-local DTO 上增加 `reference*.radius` / `radiusSource`、`scalarCorrection`、`scalarCorrectionSource`、`radiusSourceSide`、`distanceTypeMappingStatus` 和 `distanceTypeBoundary`。
- `cad-core/src/assembly/joint_solver.cpp::classifyDistanceType()` 已按 FreeCAD `getDistanceType()` 顺序扩展 circle edge、cylinder / sphere / cone / torus face、curve edge 和 `Other` 分类：edge-edge 先 line 再 circle，face-face 先 plane / cylinder / cone / torus / sphere，face 在 point/edge 前，edge 在 point 前。
- `getEdgeRadius()` / `getFaceRadius()` 语义已转成证据字段：circle edge 返回 circle radius；cylinder/sphere face 返回 radius；line/curve/plane/cone/torus 等返回 0，且不依赖 fixture 名称或 expected 输出。
- `solverJointJson()` 已输出 `reference*_element_kind`、`reference*_primitive`、`reference*_radius`、`reference*_radius_source`、`scalar_correction`、`scalar_correction_source`、`radius_source_side`、`distance_type_mapping_status` 和 `distance_type_boundary`。
- extended/default DistanceType 在 S4 前不写 `solver_joint_class`、`distance_ij` 或 `offset`；已分类但未映射的 Distance joint 返回 `pending_s4_mapping` 或 `default_boundary_not_mapped`，避免落入 ASMT fallback。未分类 object-level Distance 保留既有 basic fallback，保持现有行为。
- `cad-core/tests/test_p8_features.py::test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto` 已覆盖 edge circle、PlaneCylinder、PointSphere、TorusSphere、PlaneCone、CurvePlane 和 `Other` default boundary，并断言 extended cases 不发布 `solver_joint_class`。

## blocker 结论

- `DTE-BLOCK-002` 已满足关闭条件：radius evidence 已可见于 request-local DTO / JSON，且没有修改 DocumentObject graph。
- `DTE-BLOCK-003..008` 保持 open：edge-circle / face-radius ASMT mapping、torus/sphere native oracle、default/curve boundary、expected 采集和 capability 发布仍由 S4-S6 处理。

## 验收

```bash
rg -n 'distanceType|reference.*primitive|radius|jcsSwappedForSolver|solverJointJson' cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp cad-core/src/assembly/assembly_utils.cpp
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 非目标

- 不先改 expected。
- 不绕过 DTO 在 adapter 或 tests 里拼字段。
- 不发布 capability。
- 不实现 S4 ASMT extended mapping。
