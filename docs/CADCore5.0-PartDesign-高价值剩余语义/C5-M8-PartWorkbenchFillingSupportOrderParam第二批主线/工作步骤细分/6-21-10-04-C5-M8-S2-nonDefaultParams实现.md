# C5-M8-S2 non-default params 实现

## 目标

把 Filling 非默认参数作为同一 OCCT builder constructor 参数批次实现：`Degree`、`PtsOnCurve`、`NumIter`、`Anisotropy`、`Tol2d`、`Tol3d`、`TolG1`、`TolG2`、`MaxDegree`、`MaxSegments`。

## 必读

- C5-M8 方案与 `C5M8-ORC-201`。
- `src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp::PyInit()`、`setConstrParam()`、`setResolParam()`、`setApproxParam()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`

## 产物

- 移除当前逐字段非默认参数 deferred 主路径。
- 新增 expected-backed fixture：`c5m8/part-filling-non-default-params`。
- 新增 invalid params diagnostic fixture：`c5m8/part-filling-param-diagnostics`。
- 在结果 metadata 中保留 params evidence，避免后续 regression 只看 bbox/volume。
- 更新 C5-M8 局部矩阵、capability metadata 和本 step 状态。

## 非目标

- 不改变 S1 的 support/order 语义。
- 不处理 non-boundary constraints。
- 不为直接 wrapper 创建跨请求 builder 对象。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore3.0 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
