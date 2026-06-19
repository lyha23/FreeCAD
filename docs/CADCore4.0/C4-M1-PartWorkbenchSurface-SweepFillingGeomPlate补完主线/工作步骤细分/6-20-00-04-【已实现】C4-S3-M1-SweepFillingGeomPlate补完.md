# C4-S3 M1 Sweep / Filling / GeomPlate 补完

## 目标

推进 Sweep multi-profile / Linearize / advanced PipeShell、Filling advanced kwargs、GeomPlate advanced constraints。该步骤优先批量审计共用 surface helper 边界，再按 FreeCAD 调用链拆实现。

## 必读文件

- `docs/CADCore4.0/C4-M1-PartWorkbenchSurfaceFamily总览/6-19-23-54-C4-M1PartWorkbenchSurfaceFamily补完方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_source_candidates.tsv`
- `docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/Tools.cpp`
- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/part/part_geomplate.cpp`

## 产物

- Sweep / Filling / GeomPlate advanced fixture rows。
- 必要的 DTO / parser / executor / helper 更新。
- Diagnostics：unsupported advanced option 必须带 object、property、target、subname。
- Capability metadata 同步。

## 非目标

- 不把 Filling / GeomPlate helper 伪装成原生 DocumentObject。
- 不迁移 GUI GeomPlate feature。
- 不把 advanced PipeShell wrapper 混入 Hole internal PipeShell。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

advanced surface rows完成 expected-backed 支持或拆为带 next owner 的 deferred 行；C4-M1 scope matrix 不再只有宽泛 `full_surface_family`。

## 完成记录

- Sweep：新增 `part-sweep-multi-profile-linearize` expected-backed fixture；advanced wrapper 选项输出包含 object / property / target / subname 的 deferred diagnostic。
- Filling：新增 `part-filling-advanced-deferred` diagnostic fixture；Surface / Supports / Orders / non-default params 拆给 support/order/source-map owner。
- GeomPlate：新增 `part-geomplate-advanced-constraints` expected-backed fixture 和 `part-geomplate-advanced-deferred` diagnostic fixture；3D curve/point + approximation params 支持，initial surface / 2D / projected / wrapper 分支 deferred。
