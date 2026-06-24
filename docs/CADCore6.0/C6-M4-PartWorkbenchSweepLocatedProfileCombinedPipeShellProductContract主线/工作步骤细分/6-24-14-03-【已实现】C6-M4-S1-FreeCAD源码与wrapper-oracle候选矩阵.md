# 【已实现】C6-M4-S1 FreeCAD 源码与 wrapper-oracle 候选矩阵

## 目标

复核 FreeCAD `Part::Sweep`、`BRepOffsetAPI_MakePipeShell` wrapper、C5-M13 probe、cad-core executor / low-level PipeShell 的权威边界，并把 S2-S6 需要消费的 source candidates 写入矩阵。S1 不做实现，不提升 capability。

## S1 live baseline

- live repo：`/home/user/Chili3DProject/FreeCAD`
- S1 live HEAD：`571f1061d1`
- S1 live last commit：`571f1061d1 docs: 完成 C6-M4 S0 live 基线复核`
- 起始工作区：`git -c core.quotepath=false status --short -uall` 无输出。
- 队列入口：`step_goal_queue.py` 在本步骤执行前显示 S1-S6 pending，S1 是当前首个未实现步骤。

## FreeCAD authority

| source | 必查内容 |
| --- | --- |
| `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | native DocumentObject 只读取标准 Sweep 属性。 |
| `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()` | 标准 PipeShell maker history 与 `Add(profile)` 路径。 |
| `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` | `add(Profile, Location, WithContact, WithCorrection)` overload。 |
| `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` | auxiliary spine wrapper call。 |
| `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setTolerance()` | tolerance triple wrapper call。 |
| `src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi` | Python API public shape。 |

## cad-core authority

| source | 必查内容 |
| --- | --- |
| `cad-core/src/part/part_sweep.cpp` | `SectionOptions` parsing、metadata、known_gap publication。 |
| `cad-core/include/cad_core/part/topo_shape_expansion.h` | `PipeShellSectionOption` / `PipeShellOptions` public DTO。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | `makeElementPipeShellFromSources()` call order、`Add(profile, vertex, ...)`、maker history。 |
| `cad-core/src/runtime/capability_contract.cpp` | published covered fields、narrowed gaps、remaining gaps。 |
| `cad-core/tests/test_p8_features.py` | current known_gap guard and future assertion landing. |

## S1 复核结论

| authority | S1 结论 |
| --- | --- |
| native `Part::Sweep` | `Sweep::execute()` 只消费 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，然后调用 `result.makeElementPipeShell(...)`。这只能作为标准 Sweep baseline，不能推出 native advanced direct properties。 |
| FreeCAD PipeShell core | `TopoShape::makeElementPipeShell()` 走 `makeElementWires()`、`SetMode(isFrenet)`、`SetTransitionMode()`、可选 `SetTolerance()`、`Add(profile)`、`Build()`、可选 `MakeSolid()`，最后由 `makeElementShape()` 消费 maker history。 |
| Python wrapper | `BRepOffsetAPI_MakePipeShellPyImp.cpp` 与 `.pyi` 都公开 `add(Profile, WithContact, WithCorrection)` 和 `add(Profile, Location, WithContact, WithCorrection)`；located overload 明确要求 vertex `Location` 并调用 `Add(s, v, ...)`。 |
| auxiliary/tolerance | `setAuxiliarySpine()` 只证明 wrapper 可把 auxiliary wire 映射到 `SetMode(TopoDS::Wire, bool, contact)`；`setTolerance()` 只证明三元 tolerance 映射到 `SetTolerance()`。两者单独不是 C6-M4 的 remaining blocker。 |
| cad-core DTO / builder | `PipeShellSectionOption` 已有 `location/hasLocation/withContact/withCorrection`；`makeElementPipeShellFromSources()` 已按 section option 选择 `Add(profile, location, ...)` 或 no-location overload，并记录 `part_sweep:pipeshell_history`。 |
| cad-core executor / capability | `executePartSweep()` 已解析 `SectionOptions`、`AuxiliarySpine`、`Tolerance` 并保留 diagnostics；capability 仍把 `SectionOptions[].Location/WithContact/WithCorrection` 与 `advanced_combination` 留在 `narrowed_gaps/remaining_gaps`，不得在 S1 提升为 supported。 |
| focused guard | `test_p8_features.py` 仍断言 c5m10 located / combined 为 build-stage `NCollection_Array1::Value` known_gap、`request_metadata_only`，并且不发布 `named_shapes`。 |

## oracle 候选

| candidate | 当前处理 |
| --- | --- |
| c5m10 located known_gap | 作为 existing blocker guard，不能直接改成 expected-backed。 |
| c5m10 advanced combined known_gap | 作为 Location overload dependency guard。 |
| c6m4 located product fixture | S3/S5 新增，声明 CAD Core product contract non-parity，不伪装 FreeCAD parity。 |
| c6m4 located diagnostics fixture | S3/S5 新增，覆盖 missing/invalid/non-vertex Location 与 malformed bool，不能 fallback 到 no-location Add。 |
| c6m4 combined product fixture | S4/S5 新增，必须依赖 located profile contract 已闭环，并保持 auxiliary/tolerance no-location control 不是 blocker 的口径。 |
| FreeCADCmd wrapper probe | S2 可复跑或记录不可跑原因；若仍失败，保留 notCollected 证据。 |

## 矩阵冻结结果

- `c6m4_sweep_located_profile_combined_source_candidates.tsv` 已冻结 `C6M4-SRC-001` 到 `C6M4-SRC-012`；每行均有 `authority_type`、`evidence`、`cad_core_landing`、`next_step`，且没有把源码候选标成 supported。
- `c6m4_sweep_located_profile_combined_oracle_fixture_matrix.tsv` 保留 c5m10 known_gap guard，新增 `C6M4-ORC-103` 对齐 malformed bool diagnostics；planned rows 均标明 non-parity product/diagnostic contract。
- `c6m4_sweep_located_profile_combined_input_contract_matrix.tsv` 明确 located profile 与 combined contract 只能在 S3/S5、S4/S5 后作为 CAD Core product contract non-parity 发布，不允许提前删除 remaining gaps。
- `c6m4_sweep_located_profile_combined_non_goal_registry.tsv` 保留 FreeCAD parity、native advanced direct properties、persistent wrapper lifecycle、Filling、Loft、Groove、GUI 和 output fixups 非目标。

## 验收标准

通过条件：

- `source_candidates.tsv` 至少包含 `C6M4-SRC-001` 到 `C6M4-SRC-008`，本轮实际冻结到 `C6M4-SRC-012`。
- 每个 source row 都有 `authority_type`、`evidence`、`cad_core_landing` 和 `next_step`。
- `source_candidates.tsv` 不把候选源码直接标成 supported。
- `non_goal_registry.tsv` 包含 Filling、Loft、Groove、FreeCAD parity、persistent wrapper lifecycle。
- 总入口、README、工作步骤总入口均将 S1 标为已实现，后续队列从 S2 开始。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Sweep::execute|makeElementPipeShell|BRepOffsetAPI_MakePipeShellPyImp.cpp::add|SetTolerance|SetMode|SectionOptions|narrowed_gaps' src/Mod/Part/App cad-core/src cad-core/include docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
