# C5-M8-S3 non-boundary constraints 实现

## 目标

补齐 FreeCAD `TopoShape::makeElementFilledFace()` 中 boundary wire 之外的 constraints：剩余 wire/edge 以 `IsBound=false` 加入，face 作为 support constraint 加入，vertex 作为 point constraint 加入。

## 必读

- C5-M8 方案与 `C5M8-ORC-301`。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp::add()`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`

## 产物

- 扩展 DTO/source model，表达 boundary 与 non-boundary source，不靠输出端猜测。
- 新增 expected-backed fixtures：`c5m8/part-filling-non-boundary-edge-support`、`c5m8/part-filling-non-boundary-face-point`、`c5m8/part-filling-non-boundary-wire`。
- 保留 invalid non-boundary source、missing target、invalid subshape 的 locatable diagnostics。
- 更新 tests、capability metadata、C5-M8 局部矩阵和本 step 状态。

## 非目标

- 不支持 UV point-on-support wrapper 分支；该分支留给 S4 owner 判定。
- 不实现 Surface Workbench `Surface::Filling`。
- 不用 geometry similarity 合成 source ownership。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore3.0 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
