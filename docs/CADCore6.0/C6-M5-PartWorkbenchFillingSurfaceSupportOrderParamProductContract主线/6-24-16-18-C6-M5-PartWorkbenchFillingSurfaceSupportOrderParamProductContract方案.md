# C6-M5 Part Workbench Filling Surface / SupportOrder / Param Product Contract 方案

## 背景

C6-M4 已把 Part Workbench Sweep located profile / combined PipeShell product contract 收口。下一批不应继续拉长 Sweep，而应进入 `part_workbench.filling`：这里的剩余问题同属 `Part.makeFilledFace` helper，且当前 capability 已经把 expected-backed 子集、source-backed known gap、diagnostic-backed 生命周期和 non-goal 边界写清，适合做一个最小完整语义批次。

## 本轮做什么

- S0：复核 live capability、tests 和 docs，冻结 `part_workbench.filling` 当前 6 个 `remaining_gaps`。
- S1：复核 `AppPartPy.cpp::makeFilledFace()`、`TopoShapeExpansion.cpp::makeElementFilledFace()`、`BRepOffsetAPI_MakeFillingPyImp.cpp` 与 cad-core 现有落点。
- S2：判断哪些行仍只能保留为 `notCollected` / `native_helper_blocker`，哪些行必须转成 CAD Core request-local product contract。
- S3：实现 Surface / Supports / Orders 的产品合同，补 fixtures 和 focused tests。
- S4：实现显式参数与 non-boundary support/order 的产品合同，补 diagnostics 和 focused tests。
- S5：更新 capability contract、docs、fixture registry 和矩阵状态。
- S6：执行阶段回归和 heavy 收口，只有证据通过后才允许移除对应 `remaining_gaps`。

## 关键边界

- CAD Core product contract 可以覆盖 request-local 输入与确定性输出，但不能写成 FreeCAD helper parity。
- 既有 FreeCADCmd SIGSEGV、timeout、ConstructionError 是 helper oracle blocker 证据；它们不能继续阻塞可实现的 CAD Core 产品字段。
- wrapper 只作为源码语义参照；不把 `Part.BRepOffsetAPI.MakeFilling` 的 mutable lifecycle 暴露为前后端长期状态。
- 输出侧不得用 fixture 名称、几何类型猜测、bbox/面积排序、adapter 层修补来关闭缺口。

## 代码落点

| 方向 | 文件 |
| --- | --- |
| DTO / executor | `cad-core/src/part/part_filling.cpp` |
| Filling builder | `cad-core/src/part/topo_shape_expansion.cpp` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| tests | `cad-core/tests/test_p8_features.py` |
| fixture / expected | `cad-core/fixtures/p8` 或后续 C6 fixture 分组 |

## 验收分层

- 本轮短跑：本步骤相关 `rg`、focused unittest、`git diff --check`、TSV 字段数检查。
- 阶段回归：`cmake --build build` 加 P8 / expected / adapter focused suites。
- 重型收口：补跑 topology / P8 / expected / adapter heavy suites；仅 S6 或发布前要求。

## 结论

推荐立即进入 C6-M5。它是 C6-M4 后最集中的产品价值批次：同一 FreeCAD helper、同一 cad-core executor、同一 capability surface，能一次性关闭 Filling 的 Surface、Support/Order、explicit params 和 non-boundary support/order 合同缺口。
