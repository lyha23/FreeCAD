# C11-M2 S1 FreeCAD 源码与 helper 候选矩阵

## 目标

复核 `Part.makeFilledFace` helper、`TopoShape::makeElementFilledFace()` builder、direct `BRepOffsetAPI_MakeFilling` wrapper controls 和 current cad-core Filling implementation。S1 只做 source authority，不运行 FreeCADCmd，不升级 support。

## FreeCAD 依据

| 轴 | 需要复核的源码 | 必须确认的短句或字段 |
| --- | --- | --- |
| helper DTO | `src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` | `shapes`、`surface`、`supports`、`orders`、`degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments`。 |
| builder path | `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` | `BRepOffsetAPI_MakeFilling`、`LoadInitSurface`、boundary construction、`Add(edge, support, order, IsBound)`、non-boundary wire/edge/face/vertex constraints。 |
| direct wrapper | `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` | `PyInit`、`setConstrParam`、`setResolParam`、`setApproxParam`、`loadInitSurface`、`add`、`build`、`shape`。 |
| current executor | `cad-core/src/part/part_filling.cpp` | request-local DTO、unsupported wrapper lifecycle diagnostics、Surface / Supports / Orders / params / non-boundary object_fields。 |
| current builder | `cad-core/src/part/topo_shape_expansion.cpp` | `makeElementFilledFaceFromSources`、Filling builder evidence、maker history。 |
| publication | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_adapters.py` | `supported_expected_backed_plus_c6m5_product_contract_non_parity`、`remaining_gaps=[]`、six retained evidence。 |

## 候选路由规则

- FreeCAD source 可以成为 S3 oracle candidate，但不能单独证明 support。
- current cad-core product fixtures 只能成为 S4 comparison target。
- direct wrapper control 只作为 helper oracle 对照；wrapper lifecycle 默认进入 S5 non-goal。
- historical c5m8/c5m12/c5m13 evidence 是 retained input；不得被 S1 改写成 C11-M2 stable expected。

## 必须回写的矩阵行

- `C11M2-SRC-001..010`
- `C11M2-SCOPE-101..203`
- `C11M2-BLOCKER-101`
- `C11M2-VAL-101..103`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'makeFilledFace|makeElementFilledFace|BRepOffsetAPI_MakeFilling|LoadInitSurface|SetConstrParam|SetResolParam|SetApproxParam|Add\\(' src/Mod/Part/App/AppPartPy.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp
rg -n 'Part.makeFilledFace|makeElementFilledFaceFromSources|surface_source_status|support_order_source_status|params_source_status|non_boundary_support_order_status|unsupported_wrapper_lifecycle' cad-core/src/part/part_filling.cpp cad-core/src/part/topo_shape_expansion.cpp
rg -n 'part_workbench\\.filling|supported_expected_backed_plus_c6m5_product_contract_non_parity|historical_native_helper_evidence|remaining_gaps' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
git diff --check
```

通过条件：

- `C11M2-SRC-001..010` 均有 source path、symbol、semantic axis、evidence 和 landing。
- Surface、Supports/Orders、ExplicitParams、non-boundary support/order 分别有 S3/S4 owner。
- direct wrapper lifecycle 与 UV point-on-support branch 没有被路由为 implementation support。
- 未运行 FreeCADCmd，未新增 expected，未改 C++。

## 非目标

- S1 不生成 probe 输出。
- S1 不创建 `cad-core/fixtures/c11m2`。
- S1 不把 C6-M5 product contract 升级为 parity。
