# C6-M8 Part Workbench Surface Family Published Contract Closure 主线总入口

本文是 `docs/CADCore6.0` 下 C6-M8 实施主线。C6-M8 不是重新发明 surface family，也不是只做薄审计；它承接 C6-M4 到 C6-M7 的发布结果，把 Part Workbench surface family 的公开 CAD Core capability 合同做一次批量闭环。

## 目标

- 批量复核 `part_workbench.project_on_surface`、`ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 的 capability 状态。
- 裁决 `ProjectOnSurface` 当前 active `remaining_gaps` 与 `non_goals` 重叠的发布冲突。
- 对同一 DTO/API 边界下可实现的 gap，批量补 cad-core C++、fixtures、expected/product metadata、focused tests、capability 和 docs。
- 对不可实现或非 stateless CAD Core 目标，发布为 `non_goals` / `narrowed_gaps` / historical evidence，并写清 delete/reopen condition。
- 通过 focused tests、adapter assertion、阶段回归和 queue empty 作为 release gate。

## live 起点

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `HEAD=02798d9ca9`；`git log -1 --oneline` 为 `02798d9ca9 docs: 新增 C6-M8 表面族合同收口方案`。
- S0 开始时 `git -c core.quotepath=false status --short -uall` 为空。
- S1 开始时 `HEAD=f1f32b19c8`；`git log -1 --oneline` 为 `f1f32b19c8 文档：完成 C6-M8 S0 live 基线冻结`；`git -c core.quotepath=false status --short -uall` 为空。
- S2 开始时 `HEAD=f981730d1c`；`git log -1 --oneline` 为 `f981730d1c 文档：完成 C6-M8 S1 源码与矩阵复核`；`git -c core.quotepath=false status --short -uall` 为空。
- S3 开始时 `HEAD=72f173f682`；`git log -1 --oneline` 为 `72f173f682 文档：完成 C6-M8 S2 ProjectOnSurface 路由裁决`；`git -c core.quotepath=false status --short -uall` 为空。
- S4 开始时 `HEAD=d4a96dea91`；`git log -1 --oneline` 为 `d4a96dea91 docs: 完成 C6-M8 S3 发布口径收口`；`git -c core.quotepath=false status --short -uall` 为空。
- C6-M1 到 C6-M7 的 `工作步骤细分` 队列均返回空表；C6-M8 初始队列从 S0 开始，S0/S1 完成后推进到 S2，S2 完成后推进到 S3，S3 完成后推进到 S4，S4 完成后推进到 S5。
- S1 已完成 FreeCAD source authority、cad-core landing、fixtures/product fixtures/diagnostic evidence 和 adapter assertions 批量矩阵；S1 标记后队列推进到 S2。
- C6-M7 已发布 `part_workbench.loft.remaining_gaps=[]`；C6-M4/M5/M6 已分别发布 Sweep、Filling、GeomPlate product contract / narrowed gap / non-goal 状态。
- S2 已裁决 `project_on_surface` overlap：`gui_projection_task_panel` 是 `non_goal_frozen`；`unverified_advanced_branches` 拆为 expected-backed stateless DTO/API、GUI/session non-goal、native mapper hidden historical narrowed boundary、broad placeholder non-goal。
- adapter assertion 已按 S3 口径发布：`project_on_surface.remaining_gaps=[]`；`gui_projection_task_panel` 和 `gui_selection_camera_session` 保留在 `non_goals`；`native_project_on_surface_mapper_history_hidden_until_probe` 保留在 `request_local_boundaries` 与 `narrowed_gaps` historical evidence；宽泛 `unverified_advanced_branches` 不再进入 active gap 或 non-goal。S3 未改 `cad-core/src/part/*` executor，未新增或修改 fixtures / expected。`ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` active `remaining_gaps=[]` 且 S2/S3 复核与 fixtures、`narrowed_gaps`、`non_goals` 一致。

## Source authority

| family | source |
| --- | --- |
| ProjectOnSurface | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` |
| RuledSurface | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()` |
| Loft | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` |
| Sweep | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` |
| Filling | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` and `TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` |
| GeomPlate | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()` and `GeomPlate/BuildPlateSurfacePyImp.cpp` |

## Cad-core 落点

| owner | file |
| --- | --- |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| adapter assertion | `cad-core/tests/test_adapters.py` |
| expected/product tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py` |
| ProjectOnSurface | `cad-core/src/part/part_project_on_surface.cpp` |
| RuledSurface | `cad-core/src/part/part_ruled_surface.cpp` |
| Loft | `cad-core/src/part/part_loft.cpp` |
| Sweep | `cad-core/src/part/part_sweep.cpp` |
| Filling | `cad-core/src/part/part_filling.cpp` |
| GeomPlate | `cad-core/src/part/part_geomplate.cpp` |
| topo/history | `cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp` |
| collector | `cad-core/tools/collect_freecad_expected.py` |

## 工作步骤

| step | file | 目标 |
| --- | --- | --- |
| S0 | `工作步骤细分/6-25-10-54-【已实现】C6-M8-S0-live基线与surface-family状态冻结.md` | 已冻结 live baseline、queue、surface capability 当前状态和 root README 入口。 |
| S1 | `工作步骤细分/6-25-10-55-【已实现】C6-M8-S1-FreeCAD源码与capability批量矩阵.md` | 已批量复核 FreeCAD authority、cad-core 落点、fixtures/product fixtures/diagnostic evidence 和 adapter assertions。 |
| S2 | `工作步骤细分/6-25-10-56-【已实现】C6-M8-S2-准入路由与ProjectOnSurface裁决.md` | 已裁决 active/non-goal overlap：ProjectOnSurface GUI 为 non-goal，advanced placeholder 拆为 expected-backed / non-goal / historical narrowed boundary，未发现实现型 backend gap。 |
| S3 | `工作步骤细分/6-25-10-57-【已实现】C6-M8-S3-批量实现或发布口径收口.md` | 已按 S2 route 收口 capability、adapter assertion、docs/matrices；未改 executor、fixtures 或 expected。 |
| S4 | `工作步骤细分/6-25-10-58-【已实现】C6-M8-S4-fixtures-tests-capability-docs发布.md` | 已完成 capability、adapter assertion、C6-M8 docs、矩阵和 root README 发布一致性收口；未改 executor、fixtures 或 expected。 |
| S5 | `工作步骤细分/6-25-10-59-C6-M8-S5-阶段回归与release-gate.md` | 运行 build、focused regression、queue empty、TSV 和 diff checks，完成 release gate。 |

## 矩阵

| matrix | 用途 |
| --- | --- |
| `矩阵/c6m8_surface_family_publication_source_candidates.tsv` | FreeCAD / cad-core / test source authority。 |
| `矩阵/c6m8_surface_family_publication_scope_review_matrix.tsv` | scope 准入和状态。 |
| `矩阵/c6m8_surface_family_publication_backend_gap_classification.tsv` | backend gap / product contract / historical boundary 分类。 |
| `矩阵/c6m8_surface_family_publication_blocker_queue.tsv` | blocker、验证和关闭条件。 |
| `矩阵/c6m8_surface_family_publication_input_contract_matrix.tsv` | CapabilityResponse、DTO、expected/product metadata 字段合同。 |
| `矩阵/c6m8_surface_family_publication_oracle_fixture_matrix.tsv` | oracle、fixtures、expected/product metadata 批量路线。 |
| `矩阵/c6m8_surface_family_publication_non_goal_registry.tsv` | 非目标和 reopen condition。 |
| `矩阵/c6m8_surface_family_publication_validation_matrix.tsv` | 验收命令分层。 |

## 非目标

- 不声明 FreeCAD parity。
- 不做 GUI、ViewProvider、TaskPanel 或 Workbench session。
- 不引入 Rust / `opencascade-rs` 同步实现。
- 不把 historical native-hidden / crash / timeout evidence 伪装成 expected-backed。
- 不把 `full_part_surface_family` 写成已 parity；C6-M8 只发布当前 CAD Core contract closure。

## 当前结论

C6-M8 已完成 S0/S1/S2/S3/S4，当前队列从 S5 开始。S2 未发现 `backend_gap_requires_implementation`；S3 已完成 capability/test/docs publication assertion：移除 ProjectOnSurface active/non-goal overlap、保留 GUI non-goal、把 native mapper hidden 写为 historical narrowed boundary，并继续保护其它 surface family 的 `remaining_gaps=[]`。S4 已确认 capability、adapter assertion、fixtures/object_fields/diagnostics 发布引用、C6-M8 docs、矩阵和 root README 口径一致，且未新增 fixtures 或 expected。S5 作为 release gate；只有当 ProjectOnSurface 和其它 surface family 项的 `remaining_gaps`、`narrowed_gaps`、`non_goals`、fixtures 和 adapter assertions 都一致时，才能关闭本包。
