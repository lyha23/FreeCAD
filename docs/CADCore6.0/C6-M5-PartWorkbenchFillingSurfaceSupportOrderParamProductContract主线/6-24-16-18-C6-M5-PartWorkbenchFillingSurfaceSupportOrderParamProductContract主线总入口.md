# C6-M5 Part Workbench Filling Surface / SupportOrder / Param Product Contract 主线总入口

本文是 `docs/CADCore6.0` 下的 C6-M5 实施主线。C6-M1 到 C6-M4 已关闭队列；下一批最小完整语义批次选取 `part_workbench.filling` 中同属 `Part.makeFilledFace` helper 的 Surface、Supports/Orders、显式构造参数和 non-boundary support/order 缺口。

## 主线目标

- 把当前 `part_workbench.filling` 的 6 个 `remaining_gaps` 拆成可执行的 CAD Core product contract：不等待不稳定的 FreeCADCmd helper oracle 作为唯一前置。
- 保留 FreeCAD 依据：`Part.makeFilledFace`、`TopoShape::makeElementFilledFace()` 和 `BRepOffsetAPI_MakeFilling` 的参数 / surface / support / order / non-boundary 语义。
- 产出 request-local DTO、fixture、focused tests、capability contract 和发布闸门；不声明 FreeCAD parity，不扩成完整 Surface Workbench 或 full Part surface family。

## 当前基线

- `cad-core/src/runtime/capability_contract.cpp` 中 `part_workbench.filling` 当前状态是 `supported_expected_backed_with_c5m13_param_subset_closeout`。
- 已覆盖 Boundary 默认、closed wire / connected edges、默认参数、Degree / NumIter / Tol2d+Tol3d / MaxDegree expected-backed 子集、non-boundary edge no support/order、compound optional boundary、wrapper lifecycle diagnostics。
- 剩余缺口集中在 `filling_surface_native_helper_blocker`、`filling_support_order_g1_native_helper_blocker`、`filling_support_order_g2_native_helper_blocker`、`filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker`、`filling_params_all_native_helper_blocker`、`filling_non_boundary_support_order_native_helper_blocker`。

## 证明链条

```text
live capability baseline
  -> FreeCAD source and wrapper entry review
  -> helper oracle blocker classification
  -> CAD Core request-local product contract
  -> fixtures and focused tests
  -> capability/docs publication
  -> stage/heavy release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| helper 参数合同 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` | 解析 `shapes`、`surface`、`supports`、`orders`、`degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments`。 |
| Filling 构造主路径 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` | 创建 `BRepOffsetAPI_MakeFilling`，加载初始 surface，寻找或构造 boundary，添加 wire / edge / face / vertex constraints，最后返回 element shape。 |
| wrapper 对照 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` | 暴露 `LoadInitSurface`、`Add(edge, face, order, isBound)`、`SetConstrParam`、`SetResolParam`、`SetApproxParam`、`Build`、`Shape`；仅作语义参照，不作为跨请求状态合同。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| part executor | `cad-core/src/part/part_filling.cpp` | 解析 `Part::FilledFace` helper DTO、诊断、object fields 和 fixture metadata。 |
| geometry bridge | `cad-core/src/part/topo_shape_expansion.cpp` | 执行 request-local `BRepOffsetAPI_MakeFilling`，维护 Filling builder diagnostics 和 maker history。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 covered / narrowed_gaps / remaining_gaps / non_goals。 |
| tests | `cad-core/tests/test_p8_features.py` | 约束 Filling expected-backed、source-backed known gap、diagnostics 和后续 C6-M5 product fixtures。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-24-16-19-【已实现】C6-M5工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-24-16-20-【已实现】C6-M5-S0-live基线与Filling剩余gap边界复核.md` | 冻结 live baseline、声明边界和剩余 gap 字典。 |
| S1 | `工作步骤细分/6-24-16-21-【已实现】C6-M5-S1-FreeCAD源码与helper-oracle候选矩阵.md` | 复核 FreeCAD source / wrapper / helper oracle 候选。 |
| S2 | `工作步骤细分/6-24-16-22-【已实现】C6-M5-S2-Filling合同与oracle复采集.md` | 把原生 helper blocker 转成 product contract 准入矩阵。 |
| S3 | `工作步骤细分/6-24-16-23-C6-M5-S3-SurfaceAndSupportOrder产品合同实现.md` | 实现 Surface 与 Support/Order 产品合同。 |
| S4 | `工作步骤细分/6-24-16-24-C6-M5-S4-ExplicitParams与nonBoundarySupportOrder实现.md` | 实现显式参数与 non-boundary support/order 产品合同。 |
| S5 | `工作步骤细分/6-24-16-25-C6-M5-S5-fixtures-tests-capability-docs发布.md` | 补 fixture、focused tests、capability 和文档状态。 |
| S6 | `工作步骤细分/6-24-16-26-C6-M5-S6-阶段回归与release-gate.md` | 阶段回归、heavy 收口和 release gate。 |
| source candidates | `矩阵/c6m5_filling_surface_support_order_param_source_candidates.tsv` | FreeCAD / cad-core 依据候选。 |
| scope review | `矩阵/c6m5_filling_surface_support_order_param_scope_review_matrix.tsv` | 范围准入与状态字典。 |
| input contract | `矩阵/c6m5_filling_surface_support_order_param_input_contract_matrix.tsv` | DTO 字段、诊断和输出合同。 |
| oracle fixtures | `矩阵/c6m5_filling_surface_support_order_param_oracle_fixture_matrix.tsv` | 既有 expected、blocked oracle 与 C6-M5 新 fixture 路由。 |
| blocker queue | `矩阵/c6m5_filling_surface_support_order_param_blocker_queue.tsv` | 需要实现或关闭的 blocker。 |
| backend gap | `矩阵/c6m5_filling_surface_support_order_param_backend_gap_classification.tsv` | backendGap 分类与优先级。 |
| non-goals | `矩阵/c6m5_filling_surface_support_order_param_non_goal_registry.tsv` | 明确不做内容。 |
| validation | `矩阵/c6m5_filling_surface_support_order_param_validation_matrix.tsv` | 本轮、阶段、重型验收命令。 |

当前 S0 已完成 live 基线与 Filling 剩余 gap 边界复核，S1 已完成 FreeCAD source / wrapper / helper oracle 候选矩阵复核，S2 已把 Surface、Supports/Orders、显式参数和 non-boundary support/order 锁定为 implementation-ready 合同与 fixture 路由；S3-S6 仍是待执行状态。矩阵已经写入 S0/S1/S2 证据，但不是业务 C++、oracle 几何或发布闸门结论。

## 非目标

- 不声明 native FreeCAD `Part::FilledFace` DocumentObject。
- 不实现 Surface Workbench GUI / native feature。
- 不引入跨请求持久 `BRepOffsetAPI_MakeFilling` wrapper 或 BREP 状态。
- 不把 C6-M5 扩大到 full Part surface family、GeomPlate、Loft、Sweep 或 Groove。
