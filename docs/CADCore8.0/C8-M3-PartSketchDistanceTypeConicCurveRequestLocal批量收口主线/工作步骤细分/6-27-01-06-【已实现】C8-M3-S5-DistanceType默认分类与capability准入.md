# 【已实现】C8-M3-S5 DistanceType 默认分类与 capability 准入

## 目标

查清 `part_workbench.conic_curves.remaining_gaps` 中的 `distance_type_default_todo`：它到底是实现缺口、capability publication stale gap、oracle blocker，还是 non-goal。S5 可打开 S6 implementation gate，但不直接实现。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=6808e4890c`
- `git log -1 --oneline`：`6808e4890c docs: 完成 C8-M3 S4 Sketcher conic 边界复核`
- S5 开始时 `git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列首项是本 S5 文件；S5 只裁决 DistanceType gate，不消费或关闭 S6。

## 裁决

`distance_type_default_todo` 的 S5 裁决为：`capability_publication_gap`。

原因：

- FreeCAD `getDistanceType()` 的 default / TODO 边界是 Assembly DistanceType solver boundary，不是 `PartConicCurveDTO` 或 Sketcher conic request-local 输入缺口。
- cad-core 当前已经按 FreeCAD typed reference 语义分类并显式暴露默认 / TODO 边界：`PointCurve` 进入 diagnostic boundary，`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 等代表 default cases 保持 `default_boundary_not_mapped`，不发布为 supported。
- `cad-core/tests/test_p8_features.py` 已覆盖 supported extended DistanceType、`PointCurve` diagnostic、以及四个 default/TODO representative cases；`cad-core/tests/test_adapters.py` 已断言 Assembly capability 中 `PointCurve`、`default_or_todo_boundaries` 与 `default_or_todo_branch_support` 的 publication。
- 当前唯一不一致是 `part_workbench.conic_curves.remaining_gaps` 仍把 `distance_type_default_todo` 当作 active gap。S6 不应实现 Assembly solver C++，只应同步 capability contract、docs 和 test assertion，让 conic capability 不再保留这个无解释 active gap。

S5 因此不打开 C++ implementation gate；S6 成功标准是 capability/docs/tests publication sync。

## FreeCAD 依据

| 入口 | S5 结论 |
| --- | --- |
| `src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` | FreeCAD enum 明确包含 `CurvePlane` / `CurveCylinder` / `CurveSphere` / `CurveCone` / `CurveTorus`、`PointCurve` 和 `Other`，默认分支是 Assembly DistanceType 语义。 |
| `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 只把 edge-edge 的 line / circle 映射为显式类型；源码在 ellipse / parabola / hyperbola edge-edge 处保留 TODO。edge-face 的非 line edge 进入 `Curve*`，vertex-edge 的非 line edge 进入 `PointCurve`，未匹配则 `Other`。 |
| `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointCurve` 映射到 `ASMTPointInPlaneJoint` + `offset`；default 分支映射到 `ASMTPlanarJoint` + `offset`。这证明 native 有默认行为，但不是已经接受为 cad-core supported solver branch。 |
| `src/Mod/Part/App/Attacher.h` | `rtConic`、`rtEllipse`、`rtParabola`、`rtHyperbola` 证明 conic reference typing 应来自 typed topology / OCCT primitive，不应靠字符串猜测。 |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp::processEdge2()` | `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 投影后构造 typed Part conic geometry；S4 已证明 Sketcher request-local conic 输入和 external-reference 不缺实现。 |

## current cad-core 复核

| 入口 | S5 结论 |
| --- | --- |
| `cad-core/src/assembly/joint_solver.cpp::classifyDistanceType()` | 基于 element kind 与 typed primitive 分类，不按 conic/default 字符串猜测；`PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 等边界会写入 request-local `distance_type` 与 mapping status。 |
| `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()` | 已映射 basic、13 个 supported extended cases 与 `PointCurve` 的 `ASMTPointInPlaneJoint` DTO；Ondsel adapter 仍通过 `point_curve_diagnostic_boundary` 把 `PointCurve` 保持 unsupported。 |
| `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` | 输出 `distance_type_mapping_status`、`distance_type_boundary`、primitive 和 scalar correction evidence，已有足够 publication evidence。 |
| `cad-core/src/runtime/capability_contract.cpp` | Assembly `distance_type_extended_geometry` 已发布 `deferred_diagnostic_cases=["PointCurve"]`、`default_or_todo_boundaries=[...]` 和 `non_goals=["PointCurve","default_or_todo_branch_support",...]`；但 Part `conic_curves.remaining_gaps` 仍列出 `distance_type_default_todo`。 |
| `cad-core/tests/test_p8_features.py` | 已覆盖 `PointCurve` diagnostic、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` default boundaries，以及 solver DTO mapping status。 |
| `cad-core/tests/test_adapters.py` | 已断言 Assembly capability 的 default/TODO publication；仍断言 Part conic capability 保留 `distance_type_default_todo`，这是 S6 需要同步的 publication assertion。 |

## S6 准入

| 项 | S5 结论 |
| --- | --- |
| verdict | `capability_publication_gap` |
| C++ landing | 不修改 `cad-core/src/assembly/joint_solver.cpp` 或 `cad-core/src/assembly/assembly_utils.cpp` 的 solver/classifier 主路径。 |
| capability landing | `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.conic_curves` publication：删除或重分类 `distance_type_default_todo`，并保留指向 Assembly DistanceType default/TODO boundary 的解释。 |
| test route | `cad-core/tests/test_p8_features.py` 作为现有 coverage evidence；S6 只需要更新 `cad-core/tests/test_adapters.py` 的 capability expectation。 |
| success criteria | `part_workbench.conic_curves.remaining_gaps` 不再保留无解释 `distance_type_default_todo`；Assembly `distance_type_extended_geometry` 继续发布 `PointCurve` 和 default/TODO branches 为 unsupported/non-goal；不关闭 `C8M3-BLOCKER-601`。 |
| delete / reopen condition | S6 同步后删除这个 active gap；只有产品批准把 FreeCAD TODO/default branch 作为 supported solver scope，或新增 expected-backed default/TODO solver branch 批次，才重新打开实现。 |

## 必须回写的矩阵行

- `C8M3-SCOPE-301`
- `C8M3-ORACLE-105`
- `C8M3-BG-101`
- `C8M3-BLOCKER-501`
- `C8M3-VAL-501`

## 矩阵回写

- `C8M3-SCOPE-301`：从 `backend_gap_candidate` 更新为 `capability_publication_gap`，说明 DistanceType default / conic reference 边界已有 current test coverage，缺的是 conic capability wording sync。
- `C8M3-ORACLE-105`：从 S5 gate 更新为 current tests / diagnostic expected already cover，无需新增 oracle；S6 只同步 publication。
- `C8M3-BG-101`：从 implementation gate 候选更新为 capability publication gap，不打开 Assembly solver C++。
- `C8M3-BLOCKER-501`：关闭为 `closed_S5_distance_type_default_classified_as_capability_publication_gap`，并记录 delete / reopen condition。
- `C8M3-VAL-501`：补充 S5 验收目的为证明 current DistanceType tests 与 capability assertion 已覆盖 gate。

## 验收标准

- `distance_type_default_todo` 有明确裁决：`backend_gap_candidate`、`capability_publication_gap`、`oracle_blocked` 或 `non_goal`。
- 如果是 `backend_gap_candidate`，必须列出 C++ landing、fixture/test route 和 success criteria。
- 如果是 `capability_publication_gap`，必须证明 current tests 已覆盖，不需要新增 C++。
- 如果保留 blocker，必须写 delete/reopen condition。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'DistanceType|getDistanceType|PointCurve|Other|distance_type_default_todo|distance_type_mapping_status|C8M3-BG-101|C8M3-SCOPE-301' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

## 验收结果

已按本步骤运行以下验证；`rg -n '[ \t]$' ...` 无输出，表示无 trailing whitespace 命中。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'DistanceType|getDistanceType|PointCurve|Other|distance_type_default_todo|distance_type_mapping_status|C8M3-BG-101|C8M3-SCOPE-301' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本文件已按原时间前缀重命名为 `6-27-01-06-【已实现】C8-M3-S5-DistanceType默认分类与capability准入.md`。

## 非目标

- 不实现 full Assembly solver。
- 不把 conic curve default 分类靠字符串猜测。
- 不修改下游 Rust。
