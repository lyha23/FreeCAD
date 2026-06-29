# 【已实现】C11-M2 S1 FreeCAD 源码与 helper 候选矩阵

## 目标

复核 `Part.makeFilledFace` helper、`TopoShape::makeElementFilledFace()` builder、direct `BRepOffsetAPI_MakeFilling` wrapper controls 和 current cad-core Filling implementation。S1 只做 source authority，不运行 FreeCADCmd，不升级 support。

## live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `e09c742bc0` |
| `git log -1 --oneline` | `e09c742bc0 docs: 完成 C11-M2 S0 基线冻结` |
| `git -c core.quotepath=false status --short -uall` | 无输出，S1 起点工作区干净。 |

## S1 完成结论

- `C11M2-SRC-001..010` 已从 seed 改为 S1 verified evidence，均写入 source path、symbol、semantic axis、cad-core landing 和下一步 owner。
- FreeCAD helper 源码确认 `makeFilledFace` 的 kwd_list 声明 `surface`、`supports`、`orders` 和全部 Filling params，并调用 `TopoShape::makeElementFilledFace(shapes, params, op)`；当前可见代码段里 `pySurface` 只落入局部 `surface`，未看到写回 `params.surface`，因此该项只作为 S3 runtime helper probe 的 source caveat，不在 S1 升级成 backend gap。
- `TopoShape::makeElementFilledFace()` 已确认 `BRepOffsetAPI_MakeFilling`、可选 `LoadInitSurface`、boundary wire 路由、boundary/non-boundary `Add(...)`、face/order、point constraint、`Build()` 和 `makeElementShape()` 主路径。
- direct `BRepOffsetAPI_MakeFilling` wrapper 已确认 `PyInit`、`SetConstrParam`、`SetResolParam`、`SetApproxParam`、`loadInitSurface`、`add`、`build`、`shape`；mutable wrapper lifecycle 与 UV point-on-support branch 仍进入 S5 non-goal。
- current cad-core executor / builder / capability / adapter tests 已确认 request-local product contract、object_fields、`unsupported_wrapper_lifecycle`、`supported_expected_backed_plus_c6m5_product_contract_non_parity`、`remaining_gaps=[]` 和 six historical native helper evidence。
- `C11M2-BLOCKER-101` 已关闭；`C11M2-SCOPE-101..203` 只标记 source authority verified，S2/S3/S5 未关闭；C6-M5 product contract 未升级为 parity。

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
rg -n '[ \t]$' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次 docs/CADCore11.0/README.md
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
