# 【已实现】C5-M13 Part Workbench Surface Narrowed Blocker Recovery 主线

状态：`done_C5-M13`

C5-M13 承接 C5-M12 已收口后的剩余 Part Workbench surface narrowed blockers。C5-M12 已关闭 broad `part_sweep_wrapper_expected_collector`、Loft `complex_profile_family` broad gap，并为 Sweep support、Loft complex representatives、Filling non-boundary edge no-support/order 建立 expected-backed 证据；本包只处理 C5-M12 留下的精确 blocker，不重开完整 Part surface family。

本包批次边界是同一个 capability contract：已有 Part Workbench surface helper / wrapper 能力中，因 FreeCADCmd/native helper 调用方式、shape 参数构造或 Python wrapper 暴露限制导致的 narrowed expected blockers。它可以跨 Sweep / Filling / GeomPlate，但每个子项必须保留自己的 FreeCAD 调用链、DTO/API 边界和 expected/delete condition，不允许把失败归因为 broad surface family。

## 目标

- 冻结 C5-M12 后真实 remaining blockers：Sweep located / combined、Filling surface/support/order/G2/params/non-boundary support-order、GeomPlate G1 / ProjectedCurve2d，以及明确的 diagnostic-only non-goals。
- 批量复核 blocker 根因：区分 cad-core collector 参数构造错误、FreeCADCmd 调用方式错误、OCCT runtime failure、FreeCAD wrapper 未暴露 / NotImplemented、以及产品非目标。
- 对能修复的场景批量补 FreeCADCmd oracle、cad-core collector / helper 实现、fixtures、expected、focused tests 和 capability/docs。
- 对仍不能修复的场景保留精确 narrowed blocker，写清复现命令、错误文本、未采字段、下一批删除条件。
- 保持边界：不声明完整 Part surface family，不把 GUI / native DocumentObject / persistent wrapper lifecycle 混入支持声明。

## 入口文件

- 方案：`6-22-04-02-【已实现】C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery方案.md`
- scope 矩阵：`矩阵/c5m13_surface_narrowed_blocker_recovery_scope.tsv`
- source / DTO / oracle 合同：`矩阵/c5m13_surface_narrowed_blocker_recovery_source_dto_oracle_contract.tsv`
- fixture / oracle 矩阵：`矩阵/c5m13_surface_narrowed_blocker_recovery_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m13_surface_narrowed_blocker_recovery_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m13_surface_narrowed_blocker_recovery_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m13_surface_narrowed_blocker_recovery_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live narrowed blocker guard | C5-M12 后 `part_workbench.sweep/filling/geomplate` remaining gaps 与 expected blocker files | S0 冻结当前 blocker、delete condition、non-goal 和不重开 Loft broad gap的边界 |
| blocker root-cause matrix | C5-M12 probe evidence + new FreeCADCmd probe variants | S1 把每个 blocker 分类为 collector-fixable、native-runtime blocker、native-hidden diagnostic-only 或 product non-goal |
| Sweep wrapper location / combined | `add(Profile, Location, WithContact, WithCorrection)` 与 combined auxiliary/location/tolerance | S2 尝试修复 location vertex / call order / helper construction；可采则 expected-backed，不可采则保留更窄 blocker |
| Filling native helper expected | `Surface`、`Supports`、`Orders`、G2、non-default params、non-boundary support/order | S3 修复 `Part.makeFilledFace(...)` helper collector 和 shape argument 构造，批量替换可采 expected |
| GeomPlate native oracle | G1 curve-on-surface、ProjectedCurve2d、criteria / PlateSurface wrapper边界 | S4 修复可采 G1/projected helper；不可采路径只保留 native-hidden / NotImplemented / RuntimeError blocker |
| capability docs closeout | expected、tests、capability、C3/C5 docs | S5 同步 root/package matrices，队列为空后关闭 C5-M13 |

## 最终收口状态

- Sweep：`part_sweep_located_profile_freecadcmd_wrapper_build_blocker`、`part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 保留为 `add(Profile, Location, WithContact, WithCorrection)` Location overload build-stage `OCCError: NCollection_Array1::Value` blocker；combined 依赖 Location overload，no-location controls 已可 build。
- Filling：`Degree`、`NumIter`、`Tol2d+Tol3d`、`MaxDegree` 单字段 representatives 已 expected-backed；`Surface`、support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params、non-boundary support/order 保留 precise blocker。
- GeomPlate：`ProjectedCurve2d + InitialSurface` 已 expected-backed；无 `InitialSurface` ProjectedCurve2d 保留 `Geom_RectangularTrimmedSurface::V1==V2` blocker；G1 curve-on-surface native-hidden/NotImplemented、curve criteria setters NotImplemented、`Part.PlateSurface.Curves` wrapper lifecycle SIGSEGV/non-goal 边界保留。
- Loft：C5-M12 已关闭 broad `complex_profile_family`，C5-M13 未重开 Loft；完整 Part surface family、GUI/native DocumentObject、persistent wrapper lifecycle 和 cad-core-output-derived expected 仍为非目标。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```

## 非目标

- 不声明完整 Part surface family。
- 不新增 GUI / TaskPanel / ViewProvider 支持。
- 不把 helper / wrapper expected 当成 upstream native DocumentObject executor。
- 不用 cad-core recompute 输出、bbox 或 fixture 名称反推 FreeCAD expected。
- 不把 C5.1 PartDesign exact blockers 混入 Part Workbench surface package。
- 不修改 FreeCAD upstream source 来绕过 oracle blocker。
