# C5-M13-S3 Filling native helper expected 收口记录

状态：`done_C5M13-S3_filling_helper_expected`

## 结论

- S3 只把 S1 已证明可采的 `Part.makeFilledFace(...)` constructor 参数子集晋级 expected-backed：`Degree`、`NumIter`、`Tol2d+Tol3d`、`MaxDegree`。
- 新增 `cad-core/fixtures/c5m13` 四个 representatives，并由 FreeCADCmd collector 写入 expected；每个 expected 都包含 `shape_summary` 与 `object_fields.params`。
- `Surface`、boundary support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params、non-boundary support/order 继续保留 blocker；未把 `cad-core/fixtures/c5m8/part-filling-non-default-params.json` 全量升级为 supported。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 直接接收 `degree/ptsOnCurve/numIter/anisotropy/tol*/max*` kwargs。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` 用这些参数构造 `BRepOffsetAPI_MakeFilling` 并执行 filling。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp::PyInit()/setConstrParam()/setResolParam()/setApproxParam()` 是同一底层参数边界。

## 新增 expected-backed 子集

- `cad-core/fixtures/c5m13/part-filling-param-degree-only.json`
- `cad-core/fixtures/c5m13/part-filling-param-num-iter-only.json`
- `cad-core/fixtures/c5m13/part-filling-param-tol2d-tol3d-only.json`
- `cad-core/fixtures/c5m13/part-filling-param-max-degree-only.json`

## 保留 blockers

- `surface`：`filling_surface_only` 仍为 FreeCADCmd signal termination / process exit 139；删除条件是 `surface=` helper 返回稳定 `shape_summary` 和 initial-surface metadata。
- support/order：G1 仍返回 TypeError/control-byte payload，G2 仍返回 `UnicodeDecodeError`；删除条件是 G1/G2 representatives 都返回可解码 expected。
- crash 参数：`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params 仍为 signal termination；删除条件是所有显式参数子集都可稳定返回 expected。
- non-boundary support/order：C5-M12 no-support/order control 继续 expected-backed，support/order variant 仍为 signal termination。

## 验收

- `cd cad-core && python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported`：通过。
- `cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c5m13_part_filling_param_subsets_are_expected_backed tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_c5m13_filling_param_expected_metadata_matches_s3_boundaries tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_c5m12_filling_geomplate_expected_metadata_matches_s4_boundaries`：通过。
