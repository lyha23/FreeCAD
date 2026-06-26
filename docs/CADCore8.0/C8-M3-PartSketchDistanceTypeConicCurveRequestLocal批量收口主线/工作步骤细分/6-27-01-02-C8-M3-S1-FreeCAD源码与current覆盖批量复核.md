# C8-M3-S1 FreeCAD 源码与 current 覆盖批量复核

## 目标

复核 conic curve 相关 FreeCAD source authority、current `cad-core` Part / Sketcher / Assembly 覆盖、fixtures、focused tests 和 capability active gaps。S1 不采 oracle，不改 C++。

## FreeCAD 依据

| 入口 | 必查函数 / 字段 | 复核重点 |
| --- | --- | --- |
| `src/Mod/Part/App/Geometry.cpp` | `GeomHyperbola/GeomParabola/GeomArcOf*::Save/Restore()` | 3D conic DTO 字段 |
| `src/Mod/Part/App/Geometry2d.cpp` | `Geom2dHyperbola/Geom2dParabola/Geom2dArcOf*::Save/Restore()` | 2D conic / Sketcher input |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | `GeomAbs_Hyperbola` / `GeomAbs_Parabola` branches | external geometry / projected conic |
| `src/Mod/Sketcher/App/Constraint.h` | `HyperbolaMajor` / `ParabolaFocus` 等 | solver-facing constraints，不等于 full solver implementation |
| `src/Mod/Assembly/App/AssemblyUtils.cpp` | `getDistanceType()` | default / curve reference classification |
| `src/Mod/Assembly/App/AssemblyObject.cpp` | `makeMbdJointOfType()` | DistanceType -> solver joint mapping |

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

## 非目标

- 不采 FreeCAD oracle。
- 不新增 fixtures / expected。
- 不改 C++ 或 tests。
