# C6-M4-S1 FreeCAD 源码与 wrapper-oracle 候选矩阵

## 目标

复核 FreeCAD `Part::Sweep`、`BRepOffsetAPI_MakePipeShell` wrapper、C5-M13 probe、cad-core executor / low-level PipeShell 的权威边界，并把 S2-S6 需要消费的 source candidates 写入矩阵。S1 不做实现，不提升 capability。

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

## oracle 候选

| candidate | 当前处理 |
| --- | --- |
| c5m10 located known_gap | 作为 existing blocker guard，不能直接改成 expected-backed。 |
| c5m10 advanced combined known_gap | 作为 Location overload dependency guard。 |
| c6m4 located product fixture | S3/S5 新增，声明 CAD Core product contract，不伪装 FreeCAD parity。 |
| c6m4 combined product fixture | S4/S5 新增，必须依赖 located profile contract 已闭环。 |
| FreeCADCmd wrapper probe | S2 可复跑或记录不可跑原因；若仍失败，保留 notCollected 证据。 |

## 验收标准

通过条件：

- `source_candidates.tsv` 至少包含 `C6M4-SRC-001` 到 `C6M4-SRC-008`。
- 每个 source row 都有 `authority_type`、`evidence`、`cad_core_landing` 和 `next_step`。
- `source_candidates.tsv` 不把候选源码直接标成 supported。
- `non_goal_registry.tsv` 包含 Filling、Loft、Groove、FreeCAD parity、persistent wrapper lifecycle。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Sweep::execute|makeElementPipeShell|BRepOffsetAPI_MakePipeShellPyImp.cpp::add|SetTolerance|SetMode|SectionOptions|narrowed_gaps' src/Mod/Part/App cad-core/src cad-core/include docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
