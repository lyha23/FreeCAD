# C5-M8-S1 helper Surface / Support / Order 实现

## 目标

在同一 `Part.makeFilledFace(...)` source-backed request DTO 内实现 `Surface` / `Supports` / `Orders`，对齐 FreeCAD `LoadInitSurface`、`getSupport()`、`getOrder()` 和 boundary edge `maker.Add(edge, support, order, IsBound=true)`。

## 必读

- C5-M8 总入口、方案和局部矩阵。
- `src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- 扩展 Filling DTO / parser / core API，表达 initial surface、support face map 和 order map。
- 新增 expected-backed fixtures：`c5m8/part-filling-initial-surface-boundary`、`c5m8/part-filling-support-order-edge-face`。
- 新增 diagnostic fixture：`c5m8/part-filling-invalid-support-order`。
- 删除或收敛 `Surface` / `Supports` / `Orders` 的 broad `unsupported_property`，保留 target/subname 可定位诊断。
- 更新 capabilities、C5-M8 局部矩阵和本 step 状态。

## 非目标

- 不处理非默认 params。
- 不处理 non-boundary constraints。
- 不发布直接 `Part.BRepOffsetAPI.MakeFilling` wrapper。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore3.0 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
