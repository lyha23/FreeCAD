# C6-M8 Part Workbench Surface Family Published Contract Closure 方案

## 背景

C6-M7 发布后，`part_workbench.loft.remaining_gaps=[]`，C6-M4/M5/M6 也分别把 Sweep、Filling、GeomPlate 的 active gaps 清空或转为 `narrowed_gaps` / non-goal / historical evidence。现在剩余风险不是单个 Loft fixture，而是 `part_workbench` surface family 对外 capability 合同是否一致：哪些是 expected-backed，哪些是 CAD Core request-local product contract non-parity，哪些只是 historical native evidence，哪些明确不是 stateless CAD Core 目标。

当前需要重点裁决的是 `project_on_surface`：`gui_projection_task_panel` 和 `unverified_advanced_branches` 同时出现在 `remaining_gaps` 与 `non_goals`。这不能靠一句“审计”结束，必须走一轮可验证的 published contract closure。

## 本轮做什么

- S0：冻结 live baseline、C6-M1 到 C6-M7 queue、`cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py` 中 Part Workbench surface capability 当前状态。
- S1：批量复核 FreeCAD / cad-core authority：`ProjectOnSurface`、`RuledSurface`、`Loft`、`Sweep`、`Filling`、`GeomPlate` 的调用链、DTO/API、expected fixture 和 capability tests。
- S2：做准入路由。对每个 candidate 判断为 `expected_backed_closed`、`cad_core_product_contract_non_parity`、`historical_narrowed_gap`、`non_goal_frozen` 或 `backend_gap_requires_implementation`。
- S3：若 S2 发现实现型 backend gap，按同一 owner 的 DTO/API 边界批量补 cad-core C++、fixtures、expected/product metadata、focused tests 和 capability。若 S2 判定只是发布口径冲突，也必须通过 adapter assertion、matrix 和 docs 证明，不允许只删字符串。
- S4：发布 capability/docs：更新 `capability_contract.cpp`、`test_adapters.py`、C6-M8 矩阵、`docs/CADCore6.0/README.md`，明确 surface family 的公开合同和非目标边界。
- S5：release gate：运行 build、focused suites、TSV 字段检查、queue empty 和 diff check；把已完成步骤标 `【已实现】`，只在验证通过后关闭 C6-M8。

## 最小完整语义批次

C6-M8 的最小批次不是一个 fixture，也不是纯文档。它至少包含以下代表面：

| family | 当前发布面 | C6-M8 批量检查 |
| --- | --- | --- |
| `project_on_surface` | expected-backed published slice plus active/non-goal overlap | 裁决 GUI 和 advanced branches；若 advanced branch 是 stateless candidate，批量补代表 fixtures 和 tests；若不是，冻结为 non-goal / narrowed boundary。 |
| `ruled_surface` | expected-backed `Part::RuledSurface` | 复核 `remaining_gaps=[]`、fixture 列表、source authority 和 adapter assertion。 |
| `loft` | expected-backed plus C6-M7 product contract non-parity | 复核 selected subelement contract、native-hidden evidence、`remaining_gaps=[]` 和 non-goal wording。 |
| `sweep` | expected-backed plus C6-M4 product contract non-parity | 复核 located profile / advanced combined historical wrapper evidence 和 `remaining_gaps=[]`。 |
| `filling` | expected-backed plus C6-M5 product contract non-parity | 复核 Surface / Supports / Orders / params / non-boundary product contract 和 native helper blockers。 |
| `geomplate` | expected-backed plus C6-M6 product contract non-parity | 复核 G1 CurveOnSurface、ProjectedCurve2d、criteria diagnostic、PlateSurface non-goal 和 `remaining_gaps=[]`。 |

## 批量 oracle / fixture 策略

- expected-backed 项优先使用现有 checked-in expected fixtures 和 `tests.test_expected_fixtures` 证明，不重新采集已稳定 expected。
- 如果 S1/S2 发现同一 owner 下缺少代表 fixture 或 expected metadata，S3 必须批量补同一 DTO/API 边界下的多个代表场景；不能只补一个最容易过的 case。
- 对 native-hidden、FreeCADCmd crash/timeout/notCollected、wrapper lifecycle 类边界，不伪造 FreeCAD expected；只能保留 historical evidence，并在 CAD Core product fixture 中标 `freecad_native_expected=false` 或等价字段。
- C6-M8 不把 GUI task panel、persistent Python wrapper lifecycle、full FreeCAD parity 或跨请求状态列入实现目标。

## 代码和文档落点

| 方向 | 文件 |
| --- | --- |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| adapter capability assertions | `cad-core/tests/test_adapters.py` |
| surface executors / helpers | `cad-core/src/part/part_project_on_surface.cpp`、`cad-core/src/part/part_ruled_surface.cpp`、`cad-core/src/part/part_loft.cpp`、`cad-core/src/part/part_sweep.cpp`、`cad-core/src/part/part_filling.cpp`、`cad-core/src/part/part_geomplate.cpp` |
| link parsing / DTO | `cad-core/src/app/property_links.cpp`、`cad-core/include/cad_core/part/part_feature.h` |
| topo / history | `cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp` |
| expected collector | `cad-core/tools/collect_freecad_expected.py` |
| focused tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` |
| C6-M8 docs | `docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线`、`docs/CADCore6.0/README.md` |

## FreeCAD 依据

| family | FreeCAD source |
| --- | --- |
| ProjectOnSurface | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` |
| RuledSurface | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()` |
| Loft | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` |
| Sweep | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` |
| Filling | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`、`/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` |
| GeomPlate | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()`、`/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp` |

## 验收分层

- 本轮短跑：queue 输出、source/capability grep、C6-M8 TSV 字段数检查、`git diff --check`。
- 实现短跑：若 S3 改 C++，至少跑对应 owner 的 `tests.test_p8_features` / `tests.test_expected_fixtures` filter 和 adapter capability focused test。
- 阶段回归：`cmake --build build`，`python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`。
- 重型收口：只有修改 `topo_shape_expansion`、ElementMap/history 主路径、collector expected 语义或批量 expected 文件时，才补跑 topology / broader expected suites。

## 非目标

- 不声明 FreeCAD parity 或 full Part surface family parity。
- 不实现 GUI task panel、ViewProvider、Workbench session、persistent Python wrapper lifecycle。
- 不把 C6-M4/M5/M6/M7 已冻结的 historical native evidence 改写成 native expected。
- 不引入 Rust / `opencascade-rs` 同步实现；本包只处理当前 FreeCAD/cad-core C++ 仓库。
- 不清理 unrelated dirty work，不改 build 产物。

## 结论

推荐执行 C6-M8。它的核心是把 Part Workbench surface family 的公开 capability 合同做成一次真正的 closure：ProjectOnSurface 的 active/non-goal overlap 必须被裁决；RuledSurface、Loft、Sweep、Filling、GeomPlate 的 published evidence 必须批量复核；任何可实现 gap 都必须按同一 DTO/API 边界补 code + fixtures + tests + capability/docs + release gate，而不是只写审计文档。

