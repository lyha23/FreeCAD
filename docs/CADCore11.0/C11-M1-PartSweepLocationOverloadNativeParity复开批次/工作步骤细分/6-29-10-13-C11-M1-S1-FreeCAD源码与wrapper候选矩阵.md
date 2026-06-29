# C11-M1 S1 FreeCAD 源码与 wrapper 候选矩阵

## 目标

复核 C11-M1 的源码 authority：native `Part::Sweep::execute()`、Python wrapper `BRepOffsetAPI_MakePipeShellPy::add()`、current `cad-core` executor / PipeShell builder / capability publication。S1 只更新 source candidate evidence，不升级 support 状态。

## FreeCAD 依据

| 语义 | 源码 | 必须确认 |
| --- | --- | --- |
| native Sweep | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | 只读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，调用 `makeElementPipeShell()`。 |
| Location overload | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` | 第二签名为 `add(Profile, Location, WithContact=False, WithCorrection=False)`，Location 是 `TopoShapeVertexPy`。 |
| auxiliary / tolerance | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` 和 `setTolerance()` | 只作为 combined wrapper 依据；不证明 native DocumentObject direct properties。 |

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

## 非目标

- 不重采 oracle。
- 不创建新的 fixture。
- 不修改 C++ 或 tests。
