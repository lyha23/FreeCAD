# 【已实现】LOFT-S1 executor 与 fixture 实现

已新增 `Part::Loft` source-backed executor、CMake/registry/header、FreeCAD expected collector native type、fixtures、expected 和 focused tests。实现消费 `Sections` link list 与 `Solid/Ruled/Closed/MaxDegree`，并通过 `BRepOffsetAPI_ThruSections` / `namedShapeForThruSectionsHistory()` 保护 ThruSections maker history。

## 完成范围

- `Part::Loft` executor 从 DocumentObject `Properties.Sections` 解析 `App::PropertyLinkList`，读取 `Solid` / `Ruled` / `Closed` / `MaxDegree`，`Linearize=true` 保持 deferred diagnostic。
- `makeElementLoftFromSources()` 对齐 FreeCAD `prepareProfiles()`、`BRepOffsetAPI_ThruSections`、`SetMaxDegree()`、closed profile duplication、`CheckCompatibility(Standard_True)` 与 profile separation 检查。
- 新增 `part-loft-two-section-surface`、`part-loft-solid`、`part-loft-ruled`、`part-loft-closed`、`part-loft-invalid-sections` fixtures 与 expected。
- Focused tests 覆盖 source-backed sections、solid/ruled/closed/maxDegree、invalid diagnostics 和 ThruSections maker history，不只比较 final shape parity。

## 验证

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
python3 -m unittest tests.test_adapters
```

剩余发布动作进入 `LOFT-S2`：更新 capability / adapter 发布口径，不提前宣称 `Linearize=true`。
