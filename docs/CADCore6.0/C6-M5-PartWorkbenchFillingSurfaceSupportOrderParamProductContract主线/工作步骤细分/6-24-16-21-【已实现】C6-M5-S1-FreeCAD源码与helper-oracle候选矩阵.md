# 【已实现】C6-M5-S1 FreeCAD 源码与 helper-oracle 候选矩阵

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

## S1 结论

- FreeCAD source-backed 依据已复核：`makeFilledFace()` 声明 helper 字段并把 `BRepFillingParams` 交给 `TopoShape::makeElementFilledFace()`；builder 路径创建 `BRepOffsetAPI_MakeFilling`，按 `LoadInitSurface`、boundary wire、`Add(..., IsBound=true/false)`、face/order 和 point overload 构造结果。
- wrapper control 已复核：`BRepOffsetAPI_MakeFillingPyImp.cpp` 与 `.pyi` 暴露 constructor、`setConstrParam`、`setResolParam`、`setApproxParam`、`loadInitSurface`、`add`、`build`、`shape`；S1 只把它作为 source / diagnostic evidence，不把跨请求 mutable wrapper lifecycle 变成产品 API。
- cad-core 现状已复核：`part_filling.cpp` 只支持 request-local `Part.makeFilledFace` DTO，wrapper lifecycle 走 `unsupported_wrapper_lifecycle` 诊断；`topo_shape_expansion.cpp` 已有 request-local Filling builder；capability 仍保留六个 `remaining_gaps`。
- oracle 路由已复核：`c5m13` 参数子集与 `c5m12` non-boundary no support/order 是 expected-backed；`c5m8` surface、support/order、all params、non-boundary support/order 仍是 native helper blocked source-backed known gap；C6-M5 的新 fixture 路由留给 S2-S4。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'makeFilledFace|makeElementFilledFace|BRepOffsetAPI_MakeFilling|LoadInitSurface|SetApproxParam|SetResolParam|Add\\(' src/Mod/Part/App/AppPartPy.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp cad-core/src/part/part_filling.cpp cad-core/src/part/topo_shape_expansion.cpp
rg -n 'test_c5m8_part_filling|test_c5m13_part_filling|filling_.*known_gap|filling_.*expected_backed' cad-core/tests/test_p8_features.py cad-core/src/runtime/capability_contract.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
awk -F '\\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/矩阵/*.tsv
```

本文已按验收结果重命名为 `6-24-16-21-【已实现】C6-M5-S1-FreeCAD源码与helper-oracle候选矩阵.md`。

## 非目标

- 不把 wrapper lifecycle 变成产品 API。
- 不用当前不稳定 FreeCADCmd 结果改写 expected。
- 不做 C++ 实现。
