# C6-M6 Part Workbench GeomPlateSurface Remaining Gap Product Contract 主线总入口

本文是 `docs/CADCore6.0` 下的 C6-M6 实施主线。当前已完成 S0 live 基线冻结、S1 source / wrapper / oracle 候选矩阵、S2 remainingGap 准入路由、S3 G1 / ProjectedCurve2d 合同实现或收窄和 S4 criteria / wrapper boundary 冻结，S5 到 S6 仍 pending。执行时必须从 `工作步骤细分/` 的 live queue 逐步推进，不能跳过实现、fixture、capability 和 release gate 直接删除 `remaining_gaps`。

## 目标

- 复核并收口 `part_workbench.geomplate` 的 4 个 active `remaining_gaps`。
- 对可产品化的 request-local 输入建立 CAD Core product contract，并补 fixture / focused tests / capability evidence。
- 对 FreeCAD wrapper 或 native oracle 仍不可用的能力，保留更精确的 diagnostic / non-goal / delete condition。
- 发布时仍保持 source-backed geometry helper 口径，不声明 FreeCAD parity 或 full Part surface family。

## live 起点

- S2 起点 HEAD：`05dbaf0c53 docs: 完成 C6-M6 S1 GeomPlate 源码候选矩阵`。
- `pwd=/Users/li/Chili3DProject/FreeCAD`；S2 起点工作区干净。
- C6-M5 queue：`step_goal_queue.py` 返回空表。
- C6-M1 到 C6-M5 queue：均返回空表。
- `part_workbench.geomplate.status=supported_expected_backed_projected_initial_surface_with_curve_wrapper_diagnostics`。
- active `remaining_gaps`：
  - `g1_curve_on_surface_native_hidden_diagnostic_only`
  - `projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`
  - `curve_constraint_criteria_setters_not_implemented`
  - `platesurface_curves_wrapper_lifecycle`

## FreeCAD source authority

| owner | source |
| --- | --- |
| helper execution | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()` |
| BuildPlateSurface wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp` |
| curve constraint wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp` |
| point constraint wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp` |
| PlateSurface wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp` |
| GeomPlateSurface wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp` |

## cad-core 落点

| owner | file |
| --- | --- |
| DTO / executor | `cad-core/include/cad_core/part/part_geomplate.h`、`cad-core/src/part/part_geomplate.cpp` |
| oracle collector | `cad-core/tools/collect_freecad_expected.py` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| fixture | `cad-core/fixtures/c6m6` |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` |

## 工作步骤

| step | file | 目标 |
| --- | --- | --- |
| S0 | `工作步骤细分/6-24-19-54-【已实现】C6-M6-S0-live基线与GeomPlate剩余gap边界复核.md` | 已冻结 live capability、旧结论和 4 个 gap 边界。 |
| S1 | `工作步骤细分/6-24-19-55-【已实现】C6-M6-S1-FreeCAD源码与wrapper-oracle候选矩阵.md` | 已建立 source / wrapper / oracle 候选矩阵。 |
| S2 | `工作步骤细分/6-24-19-56-【已实现】C6-M6-S2-remainingGap准入与产品合同路由.md` | 已把 gap 路由到 implementationReady / nativeOracleBlocked / diagnosticOnly / nonGoal / releaseGate。 |
| S3 | `工作步骤细分/6-24-19-57-【已实现】C6-M6-S3-G1CurveOnSurface与ProjectedCurve2d合同实现或收窄.md` | 已批量处理 G1 curve-on-surface 与无 InitialSurface ProjectedCurve2d。 |
| S4 | `工作步骤细分/6-24-19-58-【已实现】C6-M6-S4-CriteriaSetter与PlateSurfaceCurves边界实现或nonGoal冻结.md` | 已冻结 curve criteria setter 与 PlateSurface.Curves 边界。 |
| S5 | `工作步骤细分/6-24-19-59-C6-M6-S5-fixtures-tests-capability-docs发布.md` | 发布 fixtures、tests、capability 和 docs。 |
| S6 | `工作步骤细分/6-24-20-00-C6-M6-S6-阶段回归与release-gate.md` | 阶段回归和 heavy release gate。 |

## 矩阵

| matrix | 用途 |
| --- | --- |
| `矩阵/c6m6_geomplate_remaining_gap_source_candidates.tsv` | FreeCAD / cad-core 候选来源。 |
| `矩阵/c6m6_geomplate_remaining_gap_scope_review_matrix.tsv` | scope 准入和当前状态。 |
| `矩阵/c6m6_geomplate_remaining_gap_backend_gap_classification.tsv` | backend gap / diagnostic / non-goal 分类。 |
| `矩阵/c6m6_geomplate_remaining_gap_blocker_queue.tsv` | blocker 和关闭条件。 |
| `矩阵/c6m6_geomplate_remaining_gap_input_contract_matrix.tsv` | request / diagnostic 字段合同。 |
| `矩阵/c6m6_geomplate_remaining_gap_oracle_fixture_matrix.tsv` | representative、fixture 和 expected 路由。 |
| `矩阵/c6m6_geomplate_remaining_gap_non_goal_registry.tsv` | 明确不实现项和重开条件。 |
| `矩阵/c6m6_geomplate_remaining_gap_validation_matrix.tsv` | 验收命令分层。 |

## 非目标

- 不做 GUI GeomPlate feature、TaskPanel、ViewProvider。
- 不创建 fake native `Part::GeomPlate` DocumentObject。
- 不引入 persistent `PlateSurface` wrapper state 或 cross-request geometry cache。
- 不扩大到 Filling、Loft、Sweep、ProjectOnSurface 或 full Part surface family。
- 不修改上游 FreeCAD source 来让 probe 通过。

## 当前结论

C6-M6 是 C6-M5 之后的下一条可执行主线。S2 已把 4 个 active remaining gap 路由成两个后续批次：

- S3：`G1 curve-on-surface` 已发布 request-local source-backed product contract 和 C6-M6 representative；`ProjectedCurve2d without InitialSurface` 已保留 `V1==V2` nativeOracleBlocked，并新增 no-InitialSurface representative guard，明确 cad-core output 不能作为 FreeCAD expected。
- S4：`curve criteria setter` 已保留 `unsupported_curve_criteria` locatable diagnostic、source authority 和 delete condition；`PlateSurface.Curves wrapper lifecycle` 已保留 `unsupported_wrapper_lifecycle` / `nonGoal`、SIGSEGV evidence 和 reopen condition。未新增 C6-M6 wrapper fixture，因为 FreeCAD 仍无 request-local safe contract。

最终能否删除某个 gap，必须由 S3/S4 的代码、fixture、focused tests 和 S5/S6 的 capability / release gate 同步证明。
