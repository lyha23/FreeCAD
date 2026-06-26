# 【已实现】C8-M3-S4 Sketcher Conic 输入与 solver 边界复核

## 目标

复核 Sketcher ArcOfHyperbola / ArcOfParabola 的 request-local 输入、profile、external-reference 和 solver-facing 状态边界。S4 不能把 full Sketcher solver conic constraints 当作本轮实现目标。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=a1638a28ab`
- `git log -1 --oneline`：`a1638a28ab docs: 完成 C8-M3 S3 Part conic 批量复核`
- S4 开始时 `git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列首项是本 S4 文件；S4 只关闭 Sketcher request-local boundary，不消费 S5/S6。

## FreeCAD 依据

| 入口 | S4 结论 |
| --- | --- |
| `src/Mod/Sketcher/App/GeoList.cpp` | `GeomArcOfHyperbola` / `GeomArcOfParabola` 拥有 start / end / mid 点索引，属于 Sketcher geometry input 边界。 |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | `processEdge2()` 对 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 投影结果构造 `Part::GeomArcOfHyperbola` / `Part::GeomArcOfParabola` construction geometry。 |
| `src/Mod/Sketcher/App/SketchAnalysis.cpp` | `PointConstraints::addGeometry()` 分别识别 Hyperbola / Parabola arc，并只记录 start / end 端点约束分析。 |
| `src/Mod/Sketcher/App/SolverGeometryExtension.h/.cpp` | `ArcOfHyperbola` / `ArcOfParabola` 暴露 solver-facing parameter status；`notifyAttachment()` 记录 Hyperbola 5 个参数、Parabola 4 个参数。 |
| `src/Mod/Sketcher/App/Constraint.h`、`SketchObjectConstraints.cpp` | `HyperbolaMajor`、`HyperbolaMinor`、`HyperbolaFocus`、`ParabolaFocus`、`ParabolaFocalAxis` 是 internal alignment / solver-facing 名称，不等于完整约束求解支持。 |

## current cad-core 证据

| 边界 | 证据 |
| --- | --- |
| request-local input parse | `cad-core/src/sketcher/sketch_object_geometry.cpp` 解析 `ArcOfHyperbola` / `Part::GeomArcOfHyperbola` 的 `center`、`majorRadius`、`minorRadius`、`angle`、`startAngle`、`endAngle`，解析 `ArcOfParabola` / `Part::GeomArcOfParabola` 的 `center`、`focal`、`angle`、`startAngle`、`endAngle`。 |
| profile filtering | `profileHyperbolaArcs()` 与 `profileParabolaArcs()` 只把非 construction conic arc 进入 profile；construction conic arc 不参与 profile edge。 |
| profile edge build | `cad-core/src/sketcher/sketch_object_operations.cpp` 用 `GC_MakeArcOfHyperbola` / `GC_MakeArcOfParabola` 构造 OCCT edge，并保留 FreeCAD `Restore()` 字段依据注释。 |
| external projected conic | `cad-core/src/sketcher/sketch_object_external.cpp` 从 projected edge 的 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 生成 construction `SketchHyperbolaArc` / `SketchParabolaArc`。 |
| native old external geometry | `sketch_object_external.cpp` 会把 `ExternalGeo` 里的 native Hyperbola / Parabola arc 作为旧 external geometry 复用，并按 flags 设置 construction / missing / detached 状态。 |

## p5 fixture / test 覆盖

| 场景 | 证据 | S4 裁决 |
| --- | --- | --- |
| Hyperbola request-local profile input | `cad-core/fixtures/p5/sketch-hyperbola-arc-profile.json` + `expected/sketch-hyperbola-arc-profile.freecad.json`；expected 记录 `raw_edge_count=1`、`profile_ready=false`、`shape=occt_sketch_shape`。 | covered |
| Parabola request-local profile input | `cad-core/fixtures/p5/sketch-parabola-arc-profile.json` + `expected/sketch-parabola-arc-profile.freecad.json`；expected 记录 `raw_edge_count=1`、`profile_ready=false`、`shape=occt_sketch_shape`。 | covered |
| construction filter | `sketch-conic-arcs-construction-filter.json` 同时包含 construction Hyperbola / Parabola arc 和闭合矩形；expected 记录 `raw_edge_count=4`、`profile_ready=true`、`internal_counts.faces=1`。 | covered |
| projected external reference | `sketch-conic-arcs-external-geometry-projected.json` 的 `SourceSketch` 同时提供 Hyperbola / Parabola Edge1 / Edge2；expected 记录目标 `Sketch.external_geometry_count=2`、`external_curve_count=2`、`construction_count=2`。 | covered |
| native ExternalGeo old geometry | `sketch-conic-arcs-external-geometry-native.json` + `test_p5_conic_native_external_geo_reuses_old_geometry()` 固定 missing old conic external geometry 复用：无 diagnostics、`edge_count=0`、`external_geometry_count=2`、`external_curve_count=2`、`missing=2`。 | focused-test-backed |
| invalid request-local conic params | `sketch-invalid-conic-arc-params.json` + `test_p5_invalid_conic_arc_params_report_unsupported_geometry()` / `test_diagnostics.py` 固定 invalid conic 输入报告 `unsupported_geometry`。 | diagnostics-covered |

`test_p5_conic_arc_profiles_are_supported_open_edges()` 同时跑 Hyperbola 与 Parabola profile，不是单 fixture；construction、projected external 和 native external 也都是 Hyperbola + Parabola 同批覆盖。S4 因此不新增 fixtures、expected、C++ 或 Python test，也不把任何 missing request-local evidence route 到 S6。

## solver 边界裁决

- `full_sketcher_solver_conic_constraints` 发布为 `non_goal`：FreeCAD 的 `Constraint.h` 与 `SolverGeometryExtension` 只证明 conic solver-facing names / parameter status 存在，不证明 `cad-core` 已支持完整 Sketcher solver DOF / conic constraint solve。
- S4 不声明 `HyperbolaMajor`、`HyperbolaMinor`、`HyperbolaFocus`、`ParabolaFocus`、`ParabolaFocalAxis` 可求解；这些名称只能作为 future solver package 的 reopen 入口。
- reopen condition：只有当 Sketcher solver 迁移 scope 明确扩大到完整 conic constraint solve，并有新的 solver package / architecture 文档批准时，才重开该 non-goal。
- `C8M3-BLOCKER-501` 与 `C8M3-BLOCKER-601` 不在 S4 关闭；capability publication 仍等 S5/S6。

## 矩阵回写

- `C8M3-SCOPE-201`：从 `oracle_candidate` 更新为 `already_supported`，证据为 existing p5 Hyperbola / Parabola profile、construction filter、projected external、native ExternalGeo 和 diagnostics tests。
- `C8M3-SCOPE-202`：保持 `non_goal`，补 S4 solver-facing / full solver 边界和 reopen condition。
- `C8M3-SCOPE-203`：从 `oracle_candidate` 更新为 `already_supported`，证据为 projected external expected 与 native ExternalGeo focused test。
- `C8M3-ORACLE-103`：关闭为 existing p5 expected / focused-test-backed Sketcher conic request-local batch。
- `C8M3-ORACLE-104`：关闭为 S4 published non-goal，不声明 full solver support。
- `C8M3-BG-102`：关闭为 no Sketcher request-local backend gap。
- `C8M3-BG-201` / `C8M3-NG-002`：发布 full solver non-goal 和 reopen condition。
- `C8M3-BLOCKER-401`：关闭为 `closed_S4_sketcher_conic_request_local_boundary_reviewed`。

## 验收标准

- 明确区分 request-local Sketcher conic input / profile support 与 full solver constraints。
- 证明 current p5 fixtures/tests 已覆盖 ArcOfHyperbola + ArcOfParabola input、profile、construction、projected external 和 native old external geometry。
- `full_sketcher_solver_conic_constraints` 有 non-goal / reopen condition，不作为无解释 active gap 保留。
- 不关闭 `C8M3-BLOCKER-501` 或 `C8M3-BLOCKER-601`。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'ArcOfHyperbola|ArcOfParabola|HyperbolaMajor|HyperbolaMinor|ParabolaFocus|ParabolaFocalAxis|SolverGeometryExtension|full_sketcher_solver_conic_constraints|C8M3-SCOPE-20|C8M3-NG-002' src/Mod/Sketcher/App cad-core/src/sketcher cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本轮未修改 `cad-core/fixtures`、`cad-core/tests` 或 C++，因此不运行 `cd cad-core && python3 -m unittest tests.test_p5_sketch tests.test_diagnostics`。

## 验收结果

已按本步骤运行以下验证；`rg -n '[ \t]$' ...` 无输出，表示无 trailing whitespace 命中。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'ArcOfHyperbola|ArcOfParabola|HyperbolaMajor|HyperbolaMinor|ParabolaFocus|ParabolaFocalAxis|SolverGeometryExtension|full_sketcher_solver_conic_constraints|C8M3-SCOPE-20|C8M3-NG-002' src/Mod/Sketcher/App cad-core/src/sketcher cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本文件已按原时间前缀重命名为 `6-27-01-05-【已实现】C8-M3-S4-SketcherConic输入与solver边界复核.md`。

## 非目标

- 不实现完整 solver DOF / constraint solve。
- 不把 solver-facing metadata 当成 solved constraint support。
- 不新增 GUI behavior。
- 不关闭 `C8M3-BLOCKER-501` 或 `C8M3-BLOCKER-601`。
