# C6-M5-S1 FreeCAD 源码与 helper-oracle 候选矩阵

## 目标

复核 Filling 相关 FreeCAD 源码和 cad-core 现有实现，把可支撑 C6-M5 的源码依据、helper oracle 候选、wrapper control 和实现落点写入矩阵。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFilling.pyi`

## cad-core 依据

- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p8_features.py`

## 分类规则

- `source_backed`：FreeCAD 源码明确，但当前 native helper oracle 崩溃、timeout 或 construction error。
- `expected_backed`：已有稳定 expected fixture 或 focused test。
- `diagnostic_backed`：产品合同是诊断，不是几何结果。
- `non_goal`：超出 request-local CAD Core helper DTO。

## 必须回写的矩阵行

- `SRC-001` 到 `SRC-008`：补齐源码依据和 cad-core 落点。
- `ORC-001` 到 `ORC-301`：标明已有 expected、blocked oracle 和 C6-M5 计划 fixture。
- `VAL-101`：记录 S1 focused source scan。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'makeFilledFace|makeElementFilledFace|BRepOffsetAPI_MakeFilling|LoadInitSurface|SetApproxParam|SetResolParam|Add\\(' src/Mod/Part/App/AppPartPy.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp cad-core/src/part/part_filling.cpp cad-core/src/part/topo_shape_expansion.cpp
rg -n 'test_c5m8_part_filling|test_c5m13_part_filling|filling_.*known_gap|filling_.*expected_backed' cad-core/tests/test_p8_features.py cad-core/src/runtime/capability_contract.cpp
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
```

验收通过后，将本文重命名为 `6-24-16-21-【已实现】C6-M5-S1-FreeCAD源码与helper-oracle候选矩阵.md`。

## 非目标

- 不把 wrapper lifecycle 变成产品 API。
- 不用当前不稳定 FreeCADCmd 结果改写 expected。
- 不做 C++ 实现。
