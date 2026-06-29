# 【已实现】C11-M1 S1 FreeCAD 源码与 wrapper 候选矩阵

## 目标

复核 C11-M1 的源码 authority：native `Part::Sweep::execute()`、Python wrapper `BRepOffsetAPI_MakePipeShellPy::add()`、current `cad-core` executor / PipeShell builder / capability publication。S1 只更新 source candidate evidence，不升级 support 状态。

## live baseline

本轮 S1 执行基线：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=b6287cd6b1
git log -1 --oneline=b6287cd6b1 docs: 冻结 C11-M1 S0 live 基线
git -c core.quotepath=false status --short -uall=<clean>
```

S1 起点工作区干净；本步只允许产生
`docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次` 内的文档和矩阵状态变更。

## FreeCAD 依据

| 语义 | 源码 | 必须确认 |
| --- | --- | --- |
| native Sweep | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | 只读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，调用 `makeElementPipeShell()`。 |
| Location overload | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` | 第二签名为 `add(Profile, Location, WithContact=False, WithCorrection=False)`，Location 是 `TopoShapeVertexPy`。 |
| auxiliary / tolerance | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` 和 `setTolerance()` | 只作为 combined wrapper 依据；不证明 native DocumentObject direct properties。 |

## S1 源码审计结论

| ID | 结论 | 证据 | 后续口径 |
| --- | --- | --- | --- |
| `C11M1-SRC-001` | native `Part::Sweep` 只提供 DocumentObject baseline。 | `PartFeatures.cpp::Sweep::Sweep()` 定义 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`；`Sweep::execute()` 解析这些属性后调用 `result.makeElementPipeShell(...)`。 | 不把 `SectionOptions`、`AuxiliarySpine`、`Tolerance` 宣称为 native direct properties。 |
| `C11M1-SRC-002` | wrapper `add()` 存在 Location overload oracle 候选。 | `BRepOffsetAPI_MakePipeShellPy::add()` 先解析 `Profile/WithContact/WithCorrection`，再解析 `Profile/Location/WithContact/WithCorrection`；`Location` 要求 `TopoShapeVertexPy` 并调用 `Add(s, v, ...)`。 | S3 才能复采 native oracle；S1 不升级 parity。 |
| `C11M1-SRC-003` | auxiliary/tolerance 只证明 combined wrapper API 候选。 | `setAuxiliarySpine()` 要求 wire 并调用 `SetMode(TopoDS::Wire(...), curv, contact)`；`setTolerance()` 解析 `tol3d/boundTol/tolAngular` 并调用 `SetTolerance(...)`。 | 只在 located overload 可采后进入 combined comparison。 |
| `C11M1-SRC-004` | `.pyi` 暴露 public wrapper surface。 | `.pyi` 同时声明 `add(Profile, *, WithContact, WithCorrection)` 和 `add(Profile, Location, *, WithContact, WithCorrection)`，并记录 `setAuxiliarySpine()` / `setTolerance()`。 | 作为 wrapper oracle 候选，不作为 native DocumentObject property 证明。 |
| `C11M1-SRC-005` | current executor 是 C6-M4 comparison target。 | `executePartSweep()` 允许 request-local `SectionOptions`、`AuxiliarySpine`、`Tolerance`；`ProfilePlacement=AnchorLocationToSpineStart` 时发布 `cad_core_product_contract_non_parity` 和 `freecadcmd_location_overload_status=notCollected`。 | S4 仅在 S3 stable oracle 后比较 current output。 |
| `C11M1-SRC-006` | PipeShell builder 同时有 native OCCT overload 和 product placement 分支。 | `PipeShellSectionOption` 保存 `location/withContact/withCorrection/profilePlacement`；builder 在 non-product 分支调用 `Add(profile, location, ...)`，在 product 分支先把 profile anchor 平移到 spine start，再调用普通 `Add(profile, ...)` 并记录 `part_sweep:location_product_contract_profile_placement`。 | 若 S4 证明 mismatch，S6 才能改 builder；否则保留 current product contract。 |
| `C11M1-SRC-007` | live capability 仍是 non-parity publication。 | `part_workbench.sweep.status=supported_multi_profile_linearize_c6m4_product_contract_non_parity`，`narrowed_gaps` 保留 located / advanced combined wrapper blockers，`remaining_gaps=[]`。 | S6 才能根据 S3-S5 结论更新发布状态。 |
| `C11M1-SRC-008` | focused tests 锁定 historical guard 和 product contract。 | `test_p8_features.py` 锁定 c5m10 known-gap 与 c6m4 product fixtures；`test_expected_fixtures.py` 校验 wrapper metadata；`test_adapters.py` 校验 capability field boundaries、narrowed gaps 和空 `remaining_gaps`。 | 不删除 historical guards；current contract 只作为 comparison target。 |

## cad-core 依据

- `cad-core/src/part/part_sweep.cpp::executePartSweep()`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp::makeElementPipeShellFromSources()`
- `cad-core/src/runtime/capability_contract.cpp::part_workbench.sweep`
- `cad-core/tests/test_p8_features.py`、`test_expected_fixtures.py`、`test_adapters.py`

## 必须回写的矩阵行

- `C11M1-SRC-001..008`
- `C11M1-SCOPE-101..104`
- `C11M1-BLOCKER-101`

## S1 矩阵回写

- `c11m1_part_sweep_location_overload_source_candidates.tsv` 的 `C11M1-SRC-001..008` 已补为 S1 verified evidence，且每行保留 path、symbol、evidence、cad-core landing 和 next step。
- `c11m1_part_sweep_location_overload_scope_review_matrix.tsv` 的 `C11M1-SCOPE-101..104` 已标为 source-audit complete / comparison pending，不升级 support。
- `c11m1_part_sweep_location_overload_blocker_queue.tsv` 的 `C11M1-BLOCKER-101` 已关闭为 `closed_s1_source_audit_complete`。
- S1 未采 oracle、未运行 FreeCADCmd、未修改 `cad-core/src`、tests、fixtures、expected 或 collector，未创建 backend gap，未改 capability support。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Sweep::execute|makeElementPipeShell|Sections|Spine|Linearize|Transition' src/Mod/Part/App/PartFeatures.cpp
rg -n 'BRepOffsetAPI_MakePipeShellPy::add|Profile|Location|WithContact|WithCorrection|setAuxiliarySpine|setTolerance' src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi
rg -n 'ProfilePlacement|AnchorLocationToSpineStart|SectionOptions|WithContact|WithCorrection|Location overload|part_sweep:location_product_contract' cad-core/src/part/part_sweep.cpp cad-core/include/cad_core/part/topo_shape_expansion.h cad-core/src/part/topo_shape_expansion.cpp
rg -n 'part_workbench\\.sweep|product_contract_non_parity|freecadcmd_location_overload_status|part_sweep_located_profile_freecadcmd_wrapper_build_blocker' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
git diff --check
```

通过条件：

- source candidate TSV 中每个 `C11M1-SRC-*` 都有真实 path、symbol、evidence、cad-core landing 和 next step。
- S1 不把 property existence、wrapper signature 或 current product contract 升级为 FreeCAD parity。
- `C11M1-BLOCKER-101` 可关闭为 source audit complete。

S1 已按本节命令验收通过；文档 / source-audit 步未运行 cad-core build。

## 非目标

- 不重采 oracle。
- 不创建新的 fixture。
- 不修改 C++ 或 tests。
