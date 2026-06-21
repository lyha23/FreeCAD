# C5-M12-S3 Loft complex profile expected recovery

状态：`done_C5M12-S3_loft_complex_profile_expected_recovery`

## 目标

关闭或精确收窄 `part_workbench.loft` 的 `complex_profile_family`：为复杂 profile / section family 增加 representative fixtures、FreeCAD expected 或 stable diagnostics，并补 focused tests 与 capability evidence。

## 必读

- `docs/CADCore3.0/capabilities-gap对照表.md` 的 Part Workbench Loft 发布口径。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- 新增 `cad-core/fixtures/c5m12/part-loft-complex-wire-face.json` 与 FreeCAD expected，覆盖 wire / face mixed profile representative。
- 新增 `cad-core/fixtures/c5m12/part-loft-complex-vertex-sketch-object.json` 与 FreeCAD expected，覆盖 whole sketch object / vertex representative。
- 新增 `cad-core/fixtures/c5m12/part-loft-subelement-assignment-diagnostic.json` 与 diagnostic-only expected，保留 `PropertyLinkList` native-hidden subelement blocker。
- 已更新 `C5M12-BLK-301`、`C5M12-SCOPE-301`、`C5M12-ORC-301` 与 root C5/C3 Loft capability 口径。

## 非目标

- 不把 PartDesign `FeatureLoft` 算入 `part_workbench.loft`。
- 不声明完整 Part surface family。
- 不做 output-side bbox/order fixup。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```
