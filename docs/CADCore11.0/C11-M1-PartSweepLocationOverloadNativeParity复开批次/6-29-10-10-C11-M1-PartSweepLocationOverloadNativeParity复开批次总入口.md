# C11-M1 PartSweep LocationOverload NativeParity 复开批次总入口

本文是 `docs/CADCore11.0` 下的 C11-M1 实施主线。主题是 Part Workbench `Part::Sweep` / `BRepOffsetAPI_MakePipeShell` 的 located profile native oracle 复开，不是继续 C10-M4 CopyOnChange，也不是重做 C6-M4 product contract。

## 主线目标

- 复核当前 live capability：`part_workbench.sweep` 已发布 `supported_multi_profile_linearize_c6m4_product_contract_non_parity`，`remaining_gaps=[]`。
- 复开两个 historical narrowed evidence：located profile `add(Profile, Location, WithContact, WithCorrection)` 与 advanced combined `setAuxiliarySpine(); setTolerance(); add(Profile, Location, WithContact, WithCorrection)`。
- 若 native FreeCAD oracle 能稳定采集 `shape_summary`，把 C6-M4 product contract 与 current cad-core 输出做 parity comparison，并只在 mismatch 真实存在时进入 S6 C++ / expected / capability 实现。
- 若 native oracle 仍不可采，发布 no-code retained non-parity release gate，不新增 fixture 特判或 adapter 层修剪。

## 当前基线

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=a7df521d6a`（`a7df521d6a test: 同步 c3m6 Assembly expected 状态`），起点工作区干净；C10-M1 到 C10-M4 队列均已关闭。
- C10-M4 把 CopyOnChange 继续保留为 `known_gap_diagnostic` / `oracle_blocked`；CopyOnChange 不进入 C11-M1。
- C6-M4 队列已关闭；located profile 与 advanced combined 已作为 CAD Core product contract non-parity 发布，c5m10 expected 只保留 wrapper build blocker / request metadata evidence。
- 当前 live capability / adapter test 仍能追溯两个 narrowed wrapper blocker、`freecadcmd_location_overload_status=notCollected` 和 `part_workbench.sweep.remaining_gaps=[]`；C11-M1 只编辑本批次文档和矩阵，直到 S3/S4 证明 backend gap。

## 证明链条

```text
声明口径与 live capability
  -> FreeCAD native / wrapper source authority
  -> scope review / blocker / nonGoal matrix
  -> FreeCADCmd Location overload oracle 复采集
  -> product contract 到 parity comparison
  -> protocol / non-goal release boundary
  -> S6 oracle implementation or no-code release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| native Part::Sweep DocumentObject | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | 读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，然后调用 `result.makeElementPipeShell(...)`；不暴露 `SectionOptions`、`AuxiliarySpine`、`Tolerance` 这些 advanced wrapper 字段。 |
| located profile wrapper overload | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::BRepOffsetAPI_MakePipeShellPy::add()` | 公共签名包含 `add(Profile, WithContact=False, WithCorrection=False)` 和 `add(Profile, Location, WithContact=False, WithCorrection=False)`；Location overload 调用 OCCT `Add(s, v, ...)`。 |
| auxiliary wrapper | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` | 把 auxiliary wire 映射到 `SetMode(TopoDS::Wire, bool, contact)`；本批次只在 advanced combined comparison 中消费，不把 auxiliary 单独当 Location overload parity。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Part Sweep executor | `cad-core/src/part/part_sweep.cpp` | 解析 `SectionOptions`、`Location`、`ProfilePlacement`、`WithContact`、`WithCorrection`、AuxiliarySpine 和 Tolerance；当前保留 product-contract metadata 与 diagnostics。 |
| PipeShell builder | `cad-core/include/cad_core/part/topo_shape_expansion.h`、`cad-core/src/part/topo_shape_expansion.cpp` | 执行 native Location overload 或 product `AnchorLocationToSpineStart` placement；发布 `part_sweep:pipeshell_history` 与 location product contract status。 |
| Capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `part_workbench.sweep` supported / non-parity / narrowed_gaps / remaining_gaps / field_boundaries。 |
| Focused tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` | 锁定 c5m10 known-gap guard、c6m4 product contract、diagnostics 和 capability publication。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-29-10-10-C11-M1-PartSweepLocationOverloadNativeParity复开批次方案.md` | 说明 C11-M1 背景、实施原则、S0-S6 拆分和验收分层。 |
| 工作步骤总入口 | `工作步骤细分/6-29-10-11-【已实现】C11-M1工作步骤总入口.md` | goal 队列索引；自身已完成，S0-S6 仍待执行。 |
| S0 | `工作步骤细分/6-29-10-12-【已实现】C11-M1-S0-live基线与声明口径冻结.md` | 冻结 live capability、dirty boundary、forbidden claims。 |
| S1 | `工作步骤细分/6-29-10-13-【已实现】C11-M1-S1-FreeCAD源码与wrapper候选矩阵.md` | 已完成：复核 FreeCAD source、current cad-core source 和 source candidates，关闭 `C11M1-BLOCKER-101`。 |
| S2 | `工作步骤细分/6-29-10-14-【已实现】C11-M1-S2-范围准入与blocker矩阵.md` | 已完成：路由 scope / blocker / nonGoal / backendGap，关闭 `C11M1-BLOCKER-201`。 |
| S3 | `工作步骤细分/6-29-10-15-C11-M1-S3-FreeCADCmd原生LocationOverload复采集.md` | 复采集 native Location overload 与 advanced combined oracle。 |
| S4 | `工作步骤细分/6-29-10-16-C11-M1-S4-ProductContract到Parity升级审计.md` | 比较 c6m4 product contract 与 native oracle，判断是否有 implementation row。 |
| S5 | `工作步骤细分/6-29-10-17-C11-M1-S5-协议边界与non-goal复审.md` | 关闭 GUI、persistent wrapper lifecycle、adapter 修剪和 fixture 特判边界。 |
| S6 | `工作步骤细分/6-29-10-18-C11-M1-S6-Oracle实现与发布闸门.md` | 消费 S3-S5 结果；有 mismatch 则落代码，无 mismatch / notCollected 则发布 no-code gate。 |
| source candidates | `矩阵/c11m1_part_sweep_location_overload_source_candidates.tsv` | FreeCAD / cad-core authority seed。 |
| scope review | `矩阵/c11m1_part_sweep_location_overload_scope_review_matrix.tsv` | 范围、状态和 owner step。 |
| blocker queue | `矩阵/c11m1_part_sweep_location_overload_blocker_queue.tsv` | S0-S6 blocker 和关闭条件。 |
| non-goal registry | `矩阵/c11m1_part_sweep_location_overload_non_goal_registry.tsv` | 禁止声明和 reopen condition。 |
| backend gap classification | `矩阵/c11m1_part_sweep_location_overload_backend_gap_classification.tsv` | implementation gate 分类。 |
| validation matrix | `矩阵/c11m1_part_sweep_location_overload_validation_matrix.tsv` | 文档、oracle、focused tests 和 release gate 命令。 |

当前 S0、S1、S2 已实现，S3-S6 仍是待执行状态；S2 仅关闭范围准入和 blocker 路由，不是 oracle、C++ 或发布闸门结论。
