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
