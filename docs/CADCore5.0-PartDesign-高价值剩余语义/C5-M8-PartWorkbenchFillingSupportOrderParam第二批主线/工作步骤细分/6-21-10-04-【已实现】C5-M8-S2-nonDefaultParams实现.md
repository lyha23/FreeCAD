# 【已实现】C5-M8-S2 non-default params 实现

状态：`done_cad_core_source_backed_known_gap`

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
- 新增 source-backed known_gap fixture：`c5m8/part-filling-non-default-params`。
- 新增 invalid params diagnostic fixture：`c5m8/part-filling-param-diagnostics`。
- 在结果 metadata 中保留 params evidence，避免后续 regression 只看 bbox/volume。
- 更新 C5-M8 局部矩阵、capability metadata 和本 step 状态。

## 实现结果（2026-06-21）

- `cad-core/src/part/part_filling.cpp` 已删除逐字段 non-default deferred guard，改为 `readFillingParams()` 统一解析 `Degree`、`PtsOnCurve`、`NumIter`、`Anisotropy`、`Tol2d`、`Tol3d`、`TolG1`、`TolG2`、`MaxDegree`、`MaxSegments`。
- `cad-core/include/cad_core/part/topo_shape_expansion.h` 将参数结构命名为 `FilledFaceParams`，`makeElementFilledFaceFromSources()` 继续把同一批参数传给 `BRepOffsetAPI_MakeFilling` constructor。
- result metadata 同时保留 `default_params` 基线、实际 `params` 和 `params_source=Part.makeFilledFace constructor kwargs`，避免只靠 bbox / volume 判断参数是否生效。
- `cad-core/fixtures/c5m8/part-filling-non-default-params.json` 覆盖完整非默认参数批次；对应 expected 只记录 known_gap，因为本机 `FreeCADCmd` 默认 no-kwargs Filling collector 稳定，但 explicit params helper collector 退出 `245`，不能冻结 native geometry expected。
- `cad-core/fixtures/c5m8/part-filling-param-diagnostics.json` 与 expected 固定 `Degree`、`Tol3d`、`Anisotropy` 三类 invalid 参数的 locatable `invalid_parameter` diagnostics。
- `c4m1/part-filling-advanced-deferred` 中的 `NonDefaultParamsDeferred` 已从 deferred error 变成 source-backed success；`SupportsDeferred` / `OrdersDeferred` 仍保持 S1 的具体 invalid diagnostics。
- capability metadata 与 C5-M8 局部矩阵已关闭 `C5M8-BLK-201`；remaining gap 改为 `non_default_params_native_helper_expected`，删除条件是 FreeCADCmd explicit params helper oracle 稳定返回。

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
