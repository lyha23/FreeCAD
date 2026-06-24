# C6-M6-S3 G1CurveOnSurface 与 ProjectedCurve2d 合同实现或收窄

## 目标

批量处理同属 BuildPlateSurface constraint 路线的两个 gap：`g1_curve_on_surface_native_hidden_diagnostic_only` 和 `projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`。S3 要么实现 request-local product contract，要么把 blocker 收窄到更明确的输入、诊断和 delete condition。

## 必读

- S0/S1/S2 矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp`
- `cad-core/include/cad_core/part/part_geomplate.h`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c5m7/part-geomplate-g1-curve-on-surface.json`
- `cad-core/fixtures/c5m7/part-geomplate-projected-curve2d.json`
- `cad-core/fixtures/c5m13/part-geomplate-projected-curve2d-initial-surface.json`

## 产物

- 如可实现：新增 `cad-core/fixtures/c6m6` representative，并补 focused tests、expected 或 known_gap evidence。
- 如不可实现：保留 precise diagnostic / nativeOracleBlocked，并把旧 blocker 的 freecadcmd evidence 和 delete condition 写入矩阵。
- 更新 capability draft，但 S3 不删除 `remaining_gaps`，删除等待 S5/S6。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_oracle_fixture_matrix.tsv` 和 blocker queue。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features -k geomplate
python3 -m unittest tests.test_expected_fixtures -k geomplate
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
```

验收通过后，将本文重命名为 `6-24-19-57-【已实现】C6-M6-S3-G1CurveOnSurface与ProjectedCurve2d合同实现或收窄.md`。

## 非目标

- 不伪造 Adaptor3d_CurveOnSurface native expected。
- 不靠 bbox 或结果 shape 反推 2D surface placement。
- 不在 S3 处理 criteria setter 或 PlateSurface.Curves。
