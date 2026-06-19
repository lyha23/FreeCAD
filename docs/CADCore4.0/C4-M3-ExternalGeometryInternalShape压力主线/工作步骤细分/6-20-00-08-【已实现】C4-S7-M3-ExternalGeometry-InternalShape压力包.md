# 【已实现】C4-S7 M3 ExternalGeometry / InternalShape 压力包

## 目标

扩展 ExternalGeometry projection / intersection 与 InternalShape 压力 case，保护 ReferenceShadow、ExternalGeo pool、FaceMaker、WireJoiner、ElementMap 的组合行为。

## 必读文件

- `docs/CADCore4.0/C4-M3-SketcherExternalGeometry总览/6-19-23-56-C4-M3SketcherExternalGeometry扩展方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
- `src/Mod/Sketcher/App/SketchObjectExternal.cpp`
- `src/Mod/Sketcher/App/ExternalGeometryExtension.cpp`
- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`
- `cad-core/src/part/face_maker.cpp`
- `cad-core/src/part/wire_joiner.cpp`
- `cad-core/src/part/topo_shape.cpp`

## 产物

- Expected-backed ExternalGeometry lifecycle rows：
  - `cad-core/fixtures/c4m3/sketch-external-internal-frozen-native-pool.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-detached-native-pool.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-missing-recovered.json`
- ReferenceShadow single subshape snapshot rows：
  - `cad-core/fixtures/c4m3/sketch-external-internal-frozen-brep-snapshot.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-reference-shadow-edge-stable.json`
- Expected-backed InternalShape stress rows：
  - `cad-core/fixtures/c4m3/sketch-external-internal-open-profile-empty.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-bounded-cross-cutters.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-self-intersection-bowtie.json`
  - `cad-core/fixtures/c4m3/sketch-external-internal-split-dangling-mixed.json`
- Locatable deferred diagnostics：
  - `missing_external_geometry_snapshot`：`sketch-external-internal-frozen-missing-snapshot-diagnostic.json`
  - `unsupported_reference_shadow_brep`：`sketch-external-internal-unsupported-reference-shadow-brep-diagnostic.json`
  - `subname_split_requires_reselect`：`sketch-external-internal-reference-shadow-brep-split-diagnostic.json`
  - `subname_deleted`：`sketch-external-internal-reference-shadow-brep-deleted-diagnostic.json`
  - `subname_resolve_ambiguous`：`sketch-external-internal-reference-shadow-ambiguous-diagnostic.json`
  - `deleted_stable_subname`：`sketch-external-internal-stable-deleted-diagnostic.json`

## 非目标

- 不把完整 BREP 作为建模输入。
- 不在 sketch executor 中猜 split ownership。
- 不恢复 WireJoiner 旧 fallback / candidate 字段。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

## 完成口径

ExternalGeometry / InternalShape 压力场景有 expected-backed fixture 或明确 deferred diagnostic；WireJoiner full ledger 不回退。新增能力声明位于 `cad-core/src/adapters/c_api/c_api.cpp` 的 `sketcher.external_internal_pressure`，并由 `tests.test_adapters` 固定无 full BREP state、无 backend ExternalGeometry session、无 sketch executor split ownership guessing、无 WireJoiner fallback/candidate 字段。
