# PARTCONIC-S3 conic 曲线 edge 构造实现

## 目标

按 S1/S2 的 DTO 和 oracle 结论，实现 Part geometry Hyperbola / Parabola finite edge 构造、解析、diagnostics、collector 和 focused tests。

## 必读

- S1/S2 已实现后的所有矩阵。
- `cad-core/src/part/primitive_feature.cpp`
- `cad-core/src/part/part_feature_support.*`
- `cad-core/include/cad_core/part/part_feature.h`
- `cad-core/src/runtime/feature_registry.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_diagnostics.py`
- `src/Mod/Part/App/Geometry.cpp`

## 工作内容

1. 在 Part/geometry 层实现 Hyperbola / Parabola finite edge helper，代码注释写明 FreeCAD 源文件、类/函数和关键字段短句。
2. 实现 S1 冻结的请求解析/DTO，不伪造 FreeCAD 不存在的 `TypeId`。
3. 添加 valid Hyperbola / valid Parabola fixtures 和 FreeCAD expected。
4. 添加 invalid 参数 diagnostics，禁止靠 OCCT failure 文本作为唯一口径。
5. 确保 result 仍保留 `GeomAbs_Hyperbola` / `GeomAbs_Parabola`，不转换成 BSpline/polyline。

## S3 live 结论

- `cad-core/src/part/part_geometry_curve.cpp` / `.h` 新增请求级 `PartConicCurveDTO` helper，支持 top-level `partGeometryCurve` 单对象或数组 payload；adapter 仅在该 payload 出现时分流，不改 `feature_registry`，不注册 fake `Part::Hyperbola` / `Part::Parabola`。
- Hyperbola / Parabola 均按 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp` 的 `GeomHyperbola` / `GeomArcOfHyperbola` / `GeomParabola` / `GeomArcOfParabola` Save/Restore 字段构造有限 edge，并使用 `GC_MakeArcOf* (..., Standard_True)` 语义。
- 新增 `part-hyperbola-edge`、`part-parabola-edge`、`part-conic-edge-invalid-params` fixtures；valid expected 由 FreeCADCmd `1.2.0 revision 20260519` 采集，invalid fixture 只断 stable diagnostics，不写成功 expected。
- `tests.test_p8_features` 断言 valid edge 的 `curve_kind`、`curve_type`、`part_geometry_type`、`Edge1` 与 expected parity；`tests.test_diagnostics` 断言 invalid 参数稳定诊断码。
- S3 关闭 `PARTCONIC-BLOCK-003/004/005`；S4 consumer 与 S5 capability publication 仍保持 open。

## 非目标

- 不做 Part consumer 扩展，S4 单独处理。
- 不改 Sketcher solver。
- 不改 unrelated Part primitive expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_diagnostics tests.test_p8_features
```

完成后把本文件重命名为 `6-19-17-05-【已实现】PARTCONIC-S3-conic曲线edge构造实现.md`。
