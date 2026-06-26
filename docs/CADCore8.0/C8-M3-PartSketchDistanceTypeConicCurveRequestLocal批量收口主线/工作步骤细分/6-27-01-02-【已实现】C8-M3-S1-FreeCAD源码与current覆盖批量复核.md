# 【已实现】C8-M3-S1 FreeCAD 源码与 current 覆盖批量复核

## 目标

复核 conic curve 相关 FreeCAD source authority、current `cad-core` Part / Sketcher / Assembly 覆盖、fixtures、focused tests 和 capability active gaps。S1 不采 oracle，不改 C++。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=a91b4a9d6b`
- `git log -1 --oneline`：`a91b4a9d6b docs: 完成 C8-M3 S0 基线与范围冻结`
- S1 开始时 `git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列首项是本 S1 文件；S1 完成后下一 pending 必须是 S2。

## FreeCAD 依据

| 入口 | 必查函数 / 字段 | 复核重点 |
| --- | --- | --- |
| `src/Mod/Part/App/Geometry.cpp` | `GeomHyperbola/GeomParabola/GeomArcOf*::Save/Restore()` | 3D conic DTO 字段 |
| `src/Mod/Part/App/Geometry2d.cpp` | `Geom2dHyperbola/Geom2dParabola/Geom2dArcOf*::Save/Restore()` | 2D conic / Sketcher input |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | `GeomAbs_Hyperbola` / `GeomAbs_Parabola` branches | external geometry / projected conic |
| `src/Mod/Sketcher/App/Constraint.h` | `HyperbolaMajor` / `ParabolaFocus` 等 | solver-facing constraints，不等于 full solver implementation |
| `src/Mod/Assembly/App/AssemblyUtils.cpp` | `getDistanceType()` | default / curve reference classification |
| `src/Mod/Assembly/App/AssemblyObject.cpp` | `makeMbdJointOfType()` | DistanceType -> solver joint mapping |

## FreeCAD authority 结论

| 语义 | source authority | S1 结论 |
| --- | --- | --- |
| Part 3D Hyperbola | `src/Mod/Part/App/Geometry.cpp::GeomHyperbola::Save/Restore()`、`GeomArcOfHyperbola::Save/Restore()` | 3D Hyperbola 持久化 `MajorRadius`、`MinorRadius`、`AngleXU`；arc 追加 `StartAngle` / `EndAngle` 并用 `GC_MakeArcOfHyperbola(..., Standard_True)` 还原。 |
| Part 3D Parabola | `src/Mod/Part/App/Geometry.cpp::GeomParabola::Save/Restore()`、`GeomArcOfParabola::Save/Restore()` | 3D Parabola 持久化 `Focal`、`AngleXU`；arc 追加 `StartAngle` / `EndAngle` 并用 `GC_MakeArcOfParabola(..., Standard_True)` 还原。 |
| Part 2D conic | `src/Mod/Part/App/Geometry2d.cpp::Geom2dHyperbola/Geom2dParabola/Geom2dArcOf*::Save/Restore()` | 2D Hyperbola 持久化 `MajorRadius` / `MinorRadius`，2D Parabola 持久化 `Focal`，arc 使用 trim `u` / `v`；这是 Sketcher / surface 2D input 的字段来源。 |
| Sketcher external conic | `src/Mod/Sketcher/App/SketchObjectExternal.cpp::processEdge2()` | `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 分支把 projected / external edge 转成 `Part::GeomArcOfHyperbola` / `Part::GeomArcOfParabola` 风格的 construction geometry。 |
| Sketcher solver-facing names | `src/Mod/Sketcher/App/Constraint.h`、`SolverGeometryExtension.h` | `InternalAlignmentType` 包含 `HyperbolaMajor`、`HyperbolaMinor`、`HyperbolaFocus`、`ParabolaFocus`、`ParabolaFocalAxis`；只证明 solver-facing 状态名存在，不证明 CAD Core 已迁完整约束求解器。 |
| Assembly DistanceType | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`、`AssemblyObject.cpp::makeMbdJointOfType()` | `getDistanceType()` 对 line / circle / plane / cylinder / cone / torus / sphere 做显式分类，非 line/circle curve 会走 `PointCurve`、`Curve*` 或 `Other`；`makeMbdJointOfType()` 对 `PointCurve` 走 `ASMTPointInPlaneJoint`，default 走 planar joint。 |

## current cad-core 复核

必须检查：

- `cad-core/src/part/part_geometry_curve.cpp`
- `cad-core/src/sketcher/sketch_object_geometry.cpp`
- `cad-core/src/sketcher/sketch_object_operations.cpp`
- `cad-core/src/sketcher/sketch_object_external.cpp`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/fixtures/p8/part-*conic*` 及相关 expected

## current cad-core 覆盖结论

| 方向 | current 证据 | S1 判定 |
| --- | --- | --- |
| Part DTO producer | `cad-core/src/part/part_geometry_curve.cpp::PartConicCurveDTO` 解析 `curveKind`、`center`、`normal`、`xDirection`、`startAngle`、`endAngle`、`majorRadius` / `minorRadius` 或 `focal`，并生成 Hyperbola / Parabola finite edge metadata。 | 已有 current coverage，S3 只需复核 producer / consumer batch 是否足够。 |
| Part consumer | `cad-core/src/part/part_geometry_curve.cpp` request-local bridge 和 existing p8 expected 证明 `Part::Extrusion`、`Part::RuledSurface` 可消费 conic edge。 | 已有代表证据；S3 裁决是否扩展同类 consumer，不新增单 fixture churn。 |
| Sketcher conic input | `cad-core/src/sketcher/sketch_object_geometry.cpp` 解析 `ArcOfHyperbola` / `ArcOfParabola`；`sketch_object_operations.cpp` 用 OCCT arc maker 生成 profile edge。 | request-local conic input 已有 focused coverage；不等于 full solver support。 |
| Sketcher external conic | `cad-core/src/sketcher/sketch_object_external.cpp` 对 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 转为 conic arc construction geometry。 | external / projected conic 已有 focused coverage，S4 继续核边界。 |
| DistanceType | `cad-core/src/assembly/joint_solver.cpp::classifyDistanceType()` 对 basic 与 extended cases 分类；`resolveDistanceJointMapping()` 已映射 basic 和 13 个 extended radius-bearing cases，`PointCurve` 保持 diagnostic boundary，`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 等仍为 `default_or_todo_boundary`。 | `distance_type_default_todo` 不是 Part DTO 缺口；它是 S5 的 Assembly DistanceType publication / default-boundary gate。 |
| capability/tests | `cad-core/src/runtime/capability_contract.cpp` 发布 `part_workbench.conic_curves.status=done_part_geometry_curve_edge_consumer`，`cad-core/tests/test_adapters.py` 固化同一 capability contract。 | current `remaining_gaps` 仍是 `["gui_conic_edit","full_sketcher_solver_conic_constraints","distance_type_default_todo"]`。S1 不删除、不声明支持。 |

## existing fixtures / tests

| 证据层 | existing artifacts |
| --- | --- |
| Part conic p8 fixtures | `cad-core/fixtures/p8/part-hyperbola-edge.json`、`part-parabola-edge.json`、`part-conic-edge-invalid-params.json`、`part-conic-edge-extrusion.json`、`part-ruled-surface-conic-line.json` 及对应 expected。 |
| Part conic tests | `cad-core/tests/test_p8_features.py::test_p8_part_conic_geometry_curves_build_native_edges()`、`test_p8_part_conic_geometry_curves_feed_part_extrusion()`、`test_p8_part_ruled_surface_conic_edge_feeds_surface_executor()`。 |
| Sketcher conic p5 fixtures | `cad-core/fixtures/p5/sketch-hyperbola-arc-profile.json`、`sketch-parabola-arc-profile.json`、`sketch-conic-arcs-construction-filter.json`、`sketch-conic-arcs-external-geometry-projected.json`、`sketch-conic-arcs-external-geometry-native.json`、`sketch-invalid-conic-arc-params.json`。 |
| Sketcher conic tests | `cad-core/tests/test_p5_sketch.py::test_p5_conic_arc_profiles_are_supported_open_edges()`、`test_p5_conic_construction_arcs_are_ignored_for_profile()`、`test_p5_conic_external_geometry_projects_as_construction_curves()`、`test_p5_conic_native_external_geo_reuses_old_geometry()`、`test_p5_invalid_conic_arc_params_report_unsupported_geometry()`。 |
| DistanceType tests | `cad-core/tests/test_p8_features.py::test_c3m6_assembly_distance_type_fixtures_match_native_expected()`、`test_c3m6_assembly_distance_type_s6_supported_expected_batch_matches_native()` 和 `cad-core/tests/test_adapters.py` capability assertions。 |

## current capability gaps

`part_workbench.conic_curves.remaining_gaps` 当前仍为：

```json
["gui_conic_edit", "full_sketcher_solver_conic_constraints", "distance_type_default_todo"]
```

S1 结论：

- `gui_conic_edit` 继续是 GUI / frontend editor non-goal。
- `full_sketcher_solver_conic_constraints` 继续是完整 Sketcher solver non-goal；S1 只确认 solver-facing names 和 request-local input / diagnostics。
- `distance_type_default_todo` 继续交给 S5 裁决，不在 S1 提前删除。当前 evidence 指向 Assembly DistanceType default / publication boundary，而不是 Part conic DTO producer 缺口。

## 矩阵回写

- `c8m3_conic_requestlocal_source_candidates.tsv` 已补全 current DistanceType 与 capability/tests source rows，并把 Part / Sketcher / Assembly rows 指向可验证 source evidence。
- `c8m3_conic_requestlocal_scope_review_matrix.tsv` 已把 DistanceType scope 串到 current cad-core DistanceType row，把 capability scope 串到 tests / capability row。
- `c8m3_conic_requestlocal_non_goal_registry.tsv` 已保留 `gui_conic_edit` 与 `full_sketcher_solver_conic_constraints` 为 non-goal，不提升 supported。
- `c8m3_conic_requestlocal_blocker_queue.tsv` 已关闭 `C8M3-BLOCKER-101` 为 `closed_S1_source_authority_and_current_coverage_reviewed`。

## 必须回写的矩阵行

- `c8m3_conic_requestlocal_source_candidates.tsv`
- `c8m3_conic_requestlocal_scope_review_matrix.tsv`
- `c8m3_conic_requestlocal_non_goal_registry.tsv`
- `c8m3_conic_requestlocal_blocker_queue.tsv`

## 验收标准

- source candidates 覆盖 Part 3D conic、Part 2D conic、Sketcher external conic、Sketcher constraint names、Assembly DistanceType、current Part DTO、current Sketcher conic、current DistanceType、current capability/tests。
- S1 文档记录 current `part_workbench.conic_curves.remaining_gaps` 与 existing fixtures/tests。
- 没有把 `full_sketcher_solver_conic_constraints` 或 `gui_conic_edit` 提升为 supported。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'GeomHyperbola|GeomParabola|GeomArcOfHyperbola|GeomArcOfParabola|Geom2dHyperbola|Geom2dParabola|DistanceType|HyperbolaMajor|ParabolaFocus|PartConicCurveDTO|distance_type_default_todo' src/Mod/Part/App src/Mod/Sketcher/App src/Mod/Assembly/App cad-core/src cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-02-【已实现】C8-M3-S1-FreeCAD源码与current覆盖批量复核.md`。

## 验收结果

已按本步骤运行以下验证；未运行 FreeCAD oracle、fixture/expected collector、C++ build 或 Python tests。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'GeomHyperbola|GeomParabola|GeomArcOfHyperbola|GeomArcOfParabola|Geom2dHyperbola|Geom2dParabola|DistanceType|HyperbolaMajor|ParabolaFocus|PartConicCurveDTO|distance_type_default_todo' src/Mod/Part/App src/Mod/Sketcher/App src/Mod/Assembly/App cad-core/src cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本文件已按原时间前缀重命名为 `6-27-01-02-【已实现】C8-M3-S1-FreeCAD源码与current覆盖批量复核.md`，索引链接已更新。

## 非目标

- 不采 FreeCAD oracle。
- 不新增 fixtures / expected。
- 不改 C++ 或 tests。
