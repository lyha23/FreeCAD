# 【已实现】C5-M7 S0 live 基线与 scope 冻结

状态：`【已实现】`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
235377b59e

git log -1 --oneline
235377b59e fix: 修复 Revolve 同草图 InternalEdge 轴引用
```

本轮开始时工作区已有 C5-M7 package docs 与 C5 根矩阵未提交；S0 只在该 C5-M7 边界内补写 live 状态，不 reset/revert，不覆盖 unrelated dirty files。

```text
 M docs/CADCore5.0-PartDesign-高价值剩余语义/README.md
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_blocker_queue.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_fixture_oracle_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_non_goal_registry.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_scope_review_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_source_candidates.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_validation_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/
```

## S0 核查结论

- `cad-core/include/cad_core/part/part_geomplate.h` 当前 `PartGeomPlateSurfaceDTO` 只表达 `GeomPlateBuildParams`、`GeomPlateApproximationParams`、3D edge `GeomPlateCurveConstraintSource`、3D point `GeomPlatePointConstraintSource` 和 `sourceEvidence`。
- `cad-core/src/part/part_geomplate.cpp` 当前主路径只把 3D curve G0 constraints、3D point constraints、build params、advanced approximation params 交给 `GeomPlate_BuildPlateSurface` / `GeomPlate_MakeApprox`。
- `rejectDeferredGeomPlateAdvancedProperties()` 仍把 `InitialSurface`、`Surface`、`Curve2dOnSurface`、`ProjectedCurve2d`、`Point2dOnSurface`、`PlateSurfaceCurves` 作为 `unsupported_property` deferred diagnostic。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.geomplate` capability 仍发布 `supported_expected_backed_advanced_constraints_with_deferred_wrappers`，fixtures 只列 `c3m4/part-geomplate-curve-point-default`、`c3m4/part-geomplate-invalid-inputs`、`c4m1/part-geomplate-advanced-constraints`、`c4m1/part-geomplate-advanced-deferred`。
- `cad-core/tests/test_p8_features.py` 确认 `c3m4/part-geomplate-curve-point-default` 和 `c4m1/part-geomplate-advanced-constraints` 为 expected-backed；`c4m1/part-geomplate-advanced-deferred` 仍只验证 concrete deferred diagnostics，不是 supported。

## 冻结边界

| case / property | S0 live 状态 | 边界 |
| --- | --- | --- |
| `c3m4/part-geomplate-curve-point-default` | expected-backed | 3D G0 curve constraints + 3D point + default build / approximation metadata |
| `c4m1/part-geomplate-advanced-constraints` | expected-backed | explicit approximation params + source evidence；不代表 initial / G1 / 2D supported |
| `InitialSurface` / `Surface` | deferred diagnostic | S2 才能实现或保留 concrete blocker |
| `Curve2dOnSurface` / `ProjectedCurve2d` | deferred diagnostic | S3 才能实现或保留 concrete blocker |
| `Point2dOnSurface` | deferred diagnostic | S3 才能实现或保留 concrete blocker |
| `PlateSurfaceCurves` | deferred diagnostic | S4 判定 same-DTO support 或 wrapper lifecycle diagnostic |

## 目标

冻结 `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` 当前真实状态，确认哪些已经 expected-backed，哪些仍是 deferred diagnostic。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/6-21-00-43-C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/矩阵/c5m7_geomplate_surface_helper_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/矩阵/c5m7_geomplate_surface_helper_fixture_oracle_matrix.tsv`

## 工作

1. 复核 `cad-core/src/part/part_geomplate.cpp`、`cad-core/include/cad_core/part/part_geomplate.h`、`cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_p8_features.py`。
2. 记录 `c3m4/part-geomplate-curve-point-default`、`c4m1/part-geomplate-advanced-constraints` 的 supported 边界。
3. 记录 `InitialSurface`、`Surface`、`Curve2dOnSurface`、`ProjectedCurve2d`、`Point2dOnSurface`、`PlateSurfaceCurves` 的 deferred 事实。
4. 更新本包矩阵状态，不修改 unrelated dirty files。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现新 DTO。
- 不修改 C++。
- 不把 advanced-deferred fixture 改写成 supported。
