# C3M4 Part Workbench Surface Loft 收口方案

## 目标

实现 cad-core 必迁后端主线中的 `Part::Loft`：source-backed executor、FreeCAD expected、ThruSections maker history、focused tests 和 capability 发布。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::Loft`：属性为 `Sections`、`Solid`、`Ruled`、`Closed`、`Linearize`、`MaxDegree`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`：读取 `Sections`，用 `getTopoShape(... ResolveLink | Transform)` 取 profile，转成 `IsSolid` / `IsRuled` / `IsClosed`，调用 `result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`：使用 `BRepOffsetAPI_ThruSections`、`SetMaxDegree()`、`AddVertex()` / `AddWire()`、`CheckCompatibility(Standard_True)`、`MapperThruSections` 和 `makeShapeWithElementMap()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeLoft()`：Python low-level API 同样调用 `TopoShape().makeElementLoft()`，可作为 oracle 构造辅助，但发布口径应以 `Part::Loft` DocumentObject 为主。

## 首批范围

- 两个或多个 section wire/edge 的 loft。
- `Solid=false/true`、`Ruled=false/true`、`Closed=false/true`、`MaxDegree`。
- `Linearize=false` 为第一批；`Linearize=true` 只在 source/oracle 明确后纳入。
- MapperThruSections / generated face history 不得只靠 final shape parity。

## 非目标

- 不做 GUI TaskPanel。
- 不把 taper history 的 ThruSections 支持直接等同于 `Part::Loft` 支持。
- 不做 fixture 名或 bbox 特判。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
