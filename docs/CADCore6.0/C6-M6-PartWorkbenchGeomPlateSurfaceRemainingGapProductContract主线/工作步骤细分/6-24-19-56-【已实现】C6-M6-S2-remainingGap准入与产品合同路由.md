# 【已实现】C6-M6-S2 remainingGap 准入与产品合同路由

## 目标

把 S0/S1 的 4 个 remaining gap 路由为可执行范围。S2 的核心是决定每项能否进入 CAD Core request-local product contract；不能实现的项必须变成更窄的 diagnostic / nativeOracleBlocked / nonGoal，而不是继续保留模糊 gap。

## 准入规则

| 状态 | 准入条件 | 下一步 |
| --- | --- | --- |
| `implementationReady` | CAD Core 已有 DTO/执行路径，FreeCAD source 支持 request-local 语义，fixture 可表达输入输出 | S3 / S4 C++ |
| `nativeOracleBlocked` | FreeCADCmd helper 或 wrapper 仍不能稳定采 expected，但 CAD Core 可保留 source-backed evidence | S3 / S4 收窄 blocker |
| `diagnosticOnly` | FreeCAD wrapper 明确 `NotImplementedError` 或不可执行 | S4 诊断 fixture |
| `nonGoal` | 需要 persistent wrapper、GUI 或 native DocumentObject | S4 / S5 non-goal freeze |
| `releaseGate` | 代码已实现，只待 capability / regression 发布 | S5 / S6 |

## 产物

- 更新 `矩阵/c6m6_geomplate_remaining_gap_scope_review_matrix.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_backend_gap_classification.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_input_contract_matrix.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_oracle_fixture_matrix.tsv`。
- 明确 S3 批次和 S4 批次，不把单个 fixture 当作整个语义批次。

## S2 收口结论

- `G1_curve_on_surface` 路由为 `implementationReady`：FreeCAD `Tools.cpp` 有 `Adaptor3d_CurveOnSurface` 到 `GeomPlate_CurveConstraint` G1 的 request-local source path，cad-core 已有 `CurveOnSurface` DTO 和 `addCurveOnSurfaceConstraint` 落点；S3 需要发布产品合同批次、代表性 fixture/tests、capability evidence 和 native-hidden delete condition。
- `ProjectedCurve2d_without_InitialSurface` 路由为 `nativeOracleBlocked`：带 `InitialSurface` 的 ProjectedCurve2d 已有 expected-backed 子集，但 no-InitialSurface native helper 仍是 `Geom_RectangularTrimmedSurface::V1==V2`；S3 必须保留该 blocker 或给出明确 request-local contract 与删除条件。
- `curve_criteria_setters` 路由为 `diagnosticOnly`：FreeCAD `CurveConstraintPyImp.cpp` 的 `setG0Criterion`、`setG1Criterion`、`setG2Criterion` 明确 `NotImplementedError`；S4 保留 `unsupported_curve_criteria` diagnostic，不把 PointConstraint criteria setter 误推广到 curve support。
- `PlateSurface_Curves_wrapper_lifecycle` 路由为 `nonGoal`：`PlateSurfacePyImp.cpp` 的 `Curves` 解析仍是 TODO，`GeomPlateSurface` persistence 函数抛 `NotImplementedError`，既有 wrapper probe 为 SIGSEGV；S4 只冻结 `unsupported_wrapper_lifecycle` / non-goal，不实现 persistent wrapper lifecycle。
- `capability_publication` 路由为 `releaseGate`：只有 S3/S4 的 fixture、focused tests、diagnostic/non-goal registry 和 S5/S6 capability/release gate 同步证明后，才允许删除对应 capability `remaining_gaps`。

S3 批次固定为 `G1 curve-on-surface` 与 `ProjectedCurve2d without InitialSurface`；S4 批次固定为 `curve criteria setter` 与 `PlateSurface.Curves wrapper lifecycle`。任一批次都不能用单个 fixture 代替完整语义族。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
rg -n 'implementationReady|nativeOracleBlocked|diagnosticOnly|nonGoal|releaseGate|C6M6' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
git diff --check -- docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
```

验收通过后，本文已重命名为 `6-24-19-56-【已实现】C6-M6-S2-remainingGap准入与产品合同路由.md`。

## 非目标

- 不因为字段存在就删除 capability `remaining_gaps`。
- 不采集或改写 expected。
- 不实现 GUI / native DocumentObject / persistent wrapper 生命周期。
- 不改 C++ executor、不新增 fixture、不推进 S3 到 S6。
