# C8-M3-S4 Sketcher Conic 输入与 solver 边界复核

## 目标

复核 Sketcher ArcOfHyperbola / ArcOfParabola 的 request-local 输入、profile、external-reference 和 solver-facing 状态边界。S4 不能把 full Sketcher solver conic constraints 当作本轮实现目标。

## FreeCAD 依据

| 入口 | 复核重点 |
| --- | --- |
| `src/Mod/Sketcher/App/GeoList.cpp` | conic geometry type recognition |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | Hyperbola / Parabola external geometry conversion |
| `src/Mod/Sketcher/App/SketchAnalysis.cpp` | conic analysis helper |
| `src/Mod/Sketcher/App/SolverGeometryExtension.h` | solver-facing conic parameter status |
| `src/Mod/Sketcher/App/Constraint.h` | Hyperbola / Parabola constraint names |

## current cad-core 复核

- `cad-core/src/sketcher/sketch_object_geometry.cpp`
- `cad-core/src/sketcher/sketch_object_operations.cpp`
- `cad-core/src/sketcher/sketch_object_external.cpp`
- `cad-core/tests/test_p5_sketch.py`
- 相关 sketch fixtures / p5 expected

## 必须回写的矩阵行

- `C8M3-SCOPE-201`
- `C8M3-SCOPE-202`
- `C8M3-ORACLE-104`
- `C8M3-NG-002`
- `C8M3-BLOCKER-401`

## 验收标准

- 明确区分 request-local Sketcher conic input / profile support 与 full solver constraints。
- 若 current `cad-core` 已支持 ArcOfHyperbola / ArcOfParabola input，记录 tests/fixtures；若缺 focused evidence，进入 S6。
- `full_sketcher_solver_conic_constraints` 必须有 non-goal / reopen condition，不能作为无解释 active gap 保留。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'ArcOfHyperbola|ArcOfParabola|HyperbolaMajor|HyperbolaMinor|ParabolaFocus|ParabolaFocalAxis|SolverGeometryExtension|full_sketcher_solver_conic_constraints|C8M3-SCOPE-20|C8M3-NG-002' src/Mod/Sketcher/App cad-core/src/sketcher cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-05-【已实现】C8-M3-S4-SketcherConic输入与solver边界复核.md`。

## 非目标

- 不实现完整 solver DOF / constraint solve。
- 不把 solver-facing metadata 当成 solved constraint support。
- 不新增 GUI behavior。
