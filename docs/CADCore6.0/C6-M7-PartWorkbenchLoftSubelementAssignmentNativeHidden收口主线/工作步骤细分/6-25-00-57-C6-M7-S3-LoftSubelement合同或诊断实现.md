# C6-M7-S3 LoftSubelement 合同或诊断实现

## 目标

按 S2 路由执行最小完整语义批次：要么新增 C6-M7 request-local Loft subelement product contract，要么强化 native-hidden diagnostic / narrowed gap evidence。

## 可能代码落点

- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`

## 产物

- 若实现 product contract：新增 `cad-core/fixtures/c6m7` 输入 / expected 或 product metadata，补 focused tests。
- 若保持 diagnostic：补 expected metadata、focused tests、delete condition 和 capability evidence。
- 更新 C6-M7 矩阵实际结果。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features -k loft
python3 -m unittest tests.test_expected_fixtures -k loft
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线
```

## 非目标

- 不做 PartDesign Loft 新语义。
- 不重开 complex profile family。
- 不改 Sweep / Filling / GeomPlate。
