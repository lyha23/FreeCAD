# C11-M2 PartWorkbench Filling NativeHelper Parity 复开批次总入口

本文是 `docs/CADCore11.0` 下的 C11-M2 实施主线。主题是 Part Workbench `Part.makeFilledFace` / `BRepOffsetAPI_MakeFilling` native helper oracle 复开，不是继续 C11-M1 Sweep，也不是重做 C6-M5 product contract。

## 主线目标

- 复核当前 live capability：`part_workbench.filling.status=supported_expected_backed_plus_c6m5_product_contract_non_parity`，`remaining_gaps=[]`。
- 复开 C6-M5 保留的 six historical native helper evidence：Surface、Supports/Orders G1、Supports/Orders G2、PtsOnCurve/Anisotropy/TolG1/TolG2/MaxSegments/all params、non-boundary support/order。
- 若当前 FreeCADCmd 能稳定采集 `Part.makeFilledFace(...)` 的 `shape_summary` 与 request-local object fields，再把 native expected 与 C6-M5 current product contract 做 parity comparison。
- 若 native helper oracle 仍不可采，则发布 no-code retained non-parity release gate，不新增 C++、fixtures、expected 或 adapter fixup。

## 当前基线

- C11-M1 已关闭：Sweep Location overload 当前 FreeCAD `1.2.0 revision 20260519` / OCCT `7.8.1` 仍为 `notCollected`，C11-M1 发布 no-code retained non-parity gate。
- C6-M5 已关闭：`Part.makeFilledFace` 的 Surface、Supports/Orders、ExplicitParams 和 non-boundary support/order 已作为 CAD Core request-local product contract 发布，`remaining_gaps=[]`。
- C6-M5 未声明 FreeCAD parity；six native helper crash / timeout / notCollected evidence 保留在 `narrowed_gaps` / `historical_native_helper_evidence`。
- 本批次起点不假设 backend gap。只有 S3 得到 stable native oracle，且 S4 证明 current cad-core product contract 与 native expected 存在 request-local mismatch，才允许 S6 打开代码实现。

## 证明链条

```text
声明口径与 live capability
  -> FreeCAD helper / wrapper / current cad-core source authority
  -> scope review / blocker / nonGoal matrix
  -> FreeCADCmd native Part.makeFilledFace helper 复采集
  -> C6-M5 product contract 到 native parity comparison
  -> protocol / non-goal release boundary
  -> S6 oracle implementation or no-code release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| helper 参数合同 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` | 解析 `shapes`、`surface`、`supports`、`orders`、`degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments`，再调用 `TopoShape::makeElementFilledFace(...)`。 |
| Filling 构造主路径 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` | 创建 `BRepOffsetAPI_MakeFilling`，可 `LoadInitSurface(face)`，构造或寻找 boundary，按 edge / support / order / non-boundary source 调用 `Add(...)`，最后 `Build()` 并返回 element shape。 |
| direct wrapper 对照 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` | 暴露 `PyInit`、`LoadInitSurface`、`Add(...)`、`SetConstrParam`、`SetResolParam`、`SetApproxParam`、`Build`、`Shape`；本批次只把 wrapper 作为 helper oracle 对照，不把 mutable wrapper 生命周期作为 CAD Core 协议。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Part Filling executor | `cad-core/src/part/part_filling.cpp` | 解析 source-backed `Part::FilledFace` helper DTO、Surface / Supports / Orders / params / non-boundary fields、locatable diagnostics 和 product metadata。 |
| Filling builder | `cad-core/include/cad_core/part/topo_shape_expansion.h`、`cad-core/src/part/topo_shape_expansion.cpp` | 执行 request-local `BRepOffsetAPI_MakeFilling`，维护 `LoadInitSurface`、`Add(edge/support/order/isBound)`、non-boundary constraints 和 maker history。 |
| Capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `part_workbench.filling` covered / request_local_boundaries / field_boundaries / narrowed_gaps / remaining_gaps。 |
| Focused tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` | 锁定 C6-M5 product contract、retained helper evidence、diagnostics 和 capability publication。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-29-12-22-C11-M2-PartWorkbenchFillingNativeHelperParity复开批次方案.md` | 说明 C11-M2 背景、实施原则、S0-S6 拆分和验收分层。 |
| 工作步骤总入口 | `工作步骤细分/6-29-12-23-【已实现】C11-M2工作步骤总入口.md` | goal 队列索引；自身已完成，S0-S2 已关闭，S3-S6 待执行。 |
| S0 | `工作步骤细分/6-29-12-24-【已实现】C11-M2-S0-live基线与声明口径冻结.md` | 已冻结 live capability、dirty boundary、C11-M1/C6-M5 继承口径和 forbidden claims。 |
| S1 | `工作步骤细分/6-29-12-25-【已实现】C11-M2-S1-FreeCAD源码与helper候选矩阵.md` | 已复核 FreeCAD source、direct wrapper controls 和 current cad-core landings；`C11M2-BLOCKER-101` 关闭。 |
| S2 | `工作步骤细分/6-29-12-26-【已实现】C11-M2-S2-范围准入与blocker矩阵.md` | 已路由 scope / blocker / nonGoal / backend gap 分类，关闭 `C11M2-BLOCKER-201`，防止无 oracle 直接进 C++。 |
| S3 | `工作步骤细分/6-29-12-27-C11-M2-S3-FreeCADCmd原生FillingHelper复采集.md` | 复采集 Surface、Supports/Orders、ExplicitParams、non-boundary support/order native helper oracle。 |
| S4 | `工作步骤细分/6-29-12-28-C11-M2-S4-ProductContract到Parity升级审计.md` | 仅在 S3 stable oracle 存在时比较 C6-M5 current product contract 与 native expected。 |
| S5 | `工作步骤细分/6-29-12-29-C11-M2-S5-协议边界与non-goal复审.md` | 关闭 native DocumentObject、Surface Workbench GUI、persistent wrapper lifecycle、adapter fixup 等边界。 |
| S6 | `工作步骤细分/6-29-12-30-C11-M2-S6-Oracle实现与发布闸门.md` | 有 backend gap 则落 C++ / fixtures / focused tests / capability；否则发布 no-code retained non-parity gate。 |
| source candidates | `矩阵/c11m2_part_workbench_filling_native_helper_source_candidates.tsv` | FreeCAD / cad-core authority seed。 |
| scope review | `矩阵/c11m2_part_workbench_filling_native_helper_scope_review_matrix.tsv` | 范围、状态和 owner step。 |
| blocker queue | `矩阵/c11m2_part_workbench_filling_native_helper_blocker_queue.tsv` | S0 baseline blocker、`C11M2-BLOCKER-101` 与 `C11M2-BLOCKER-201` 已关闭；S3-S6 blocker 和关闭条件待后续步骤消费。 |
| oracle fixtures | `矩阵/c11m2_part_workbench_filling_native_helper_oracle_fixture_matrix.tsv` | retained C6-M5 evidence、S3 probe route 和 future expected gate。 |
| non-goal registry | `矩阵/c11m2_part_workbench_filling_native_helper_non_goal_registry.tsv` | 禁止声明和 reopen condition。 |
| backend gap classification | `矩阵/c11m2_part_workbench_filling_native_helper_backend_gap_classification.tsv` | implementation gate 分类。 |
| validation matrix | `矩阵/c11m2_part_workbench_filling_native_helper_validation_matrix.tsv` | 文档、oracle、focused tests 和 release gate 命令。 |

当前 S0-S2 已实现，S3-S6 仍是待执行状态；矩阵已写入 S1 source authority evidence 与 S2 route evidence，但 oracle、parity comparison、non-goal release boundary 和 S6 发布闸门仍未关闭。C11-M2 不修改 C11-M1 已关闭结论。
