# C12-M6 S1 FreeCAD 源码与旧 PARTSURF 证据复核【已实现】

## 目标

复核 FreeCAD `Part::RuledSurface` 的真实调用链，并把旧 C3M4 deferred evidence 与当前 cad-core landing 做一次对照，确认 S2-S4 应验证的最小闭环。

## 必读来源

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::RuledSurface`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`
- `cad-core/src/part/part_ruled_surface.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- 旧 C3M4 `part_surface_*` 矩阵。

## 操作

1. 用 `rg` 复核 `OrientationEnums`、`Curve1`、`Curve2`、`res.makeElementRuledSurface(shapes, Orientation.getValue())`。
2. 用 `rg` 复核 FreeCAD `makeElementRuledSurface()` 的 edge/wire normalization、`BRepFill::Face`、`BRepFill::Shell` 和 source edge recovery comment。
3. 对照 current `cad-core`：Part executor、TopoShapeExpansion helper、capability source、focused tests 和 checked-in fixture 是否都有 wire/wire landing。
4. 更新 source candidates 与 backend classification：区分 source-supported、current-supported-candidate、historical-deferred-doc-drift 和 true backend gap。

## 裁决规则

- 若 FreeCAD source 不支持 wire/wire，则 S2-S5 全部改为 no-code docs repair，不允许 implementation。
- 若 FreeCAD source 支持 wire/wire，但 cad-core current landing 缺一项，则保留到 S2/S3/S4 分类。
- 不能把旧 `deferred-S2` 本身当作 current backend gap；必须用当前 expected/current evidence 判断。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "RuledSurface::execute|makeElementRuledSurface|BRepFill::Shell|BRepFill::Face|Both BRepFill" src/Mod/Part/App/PartFeatures.cpp src/Mod/Part/App/TopoShapeExpansion.cpp
rg -n "Part::RuledSurface|wire_wire_brepfill_shell|part-ruled-surface-wire-wire" cad-core/src cad-core/tests cad-core/fixtures
git diff --check
```

## S1 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=2083c0cbdb`。
- `git log -1 --oneline=2083c0cbdb docs: 冻结 C12-M6 S0 live 基线`。
- `git -c core.quotepath=false status --short -uall` 无输出，S1 起点为 clean。

## FreeCAD source 复核结论

- `src/Mod/Part/App/PartFeatures.h::Part::RuledSurface` 声明 `Orientation`、`Curve1`、`Curve2`、`execute()` 和 `mustExecute()`，是 source-backed `DocumentObject`，不是 DTO-only helper。
- `src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()` 的 `OrientationEnums` 为 `Automatic`、`Forward`、`Reversed`。
- `src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()` 逐个读取 `Curve1` / `Curve2`，通过 `ResolveLink | Transform` 和必要的 `NeedSubElement` 获得 `TopoShape`，随后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())`。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` 接受 edge 或 wire；非 edge/wire 输入按单 wire、单 edge 或 `makeElementWires()` 归一化；edge/wire 混合时把 edge 转成 wire。
- 同一函数在 edge/edge 分支调用 `BRepFill::Face`，在 wire/wire 分支调用 `BRepFill::Shell`。
- 同一函数注释明确 `BRepFill::Face()` 和 `Shell()` 会修改原始输入 edge，且没有 API 提供输出 edge 关系，因此 FreeCAD 通过 shared-vertex 搜索恢复 source edge 到输出 edge 的关系，再交给 `makeShapeWithElementMap()`。

裁决：FreeCAD source-supported。旧 PARTSURF 不能再用“FreeCAD source 不支持 wire/wire”解释。

## current landing 复核结论

- `cad-core/src/part/part_ruled_surface.cpp::executePartRuledSurface()` 已按 FreeCAD 调用链解析 `Orientation`、`Curve1`、`Curve2`，只接受 edge/wire，随后调用 `makeElementRuledSurfaceFromCurves()`。
- `cad-core/src/part/topo_shape_expansion.cpp::makeElementRuledSurfaceFromCurves()` 已复制输入、处理 edge/wire 混合归一化、支持 `Automatic` / `Forward` / `Reversed`，并在 wire 输入时调用 `BRepFill::Shell`，同时写入 `part_ruled_surface:wire_wire_brepfill_shell`。
- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json` 使用 request-local `DocumentObject graph`：两个 `Part::RegularPolygon` wire producer 加一个 `Part::RuledSurface`，`Curve1` / `Curve2` 为 `App::PropertyLinkSub`，没有 BREP、TopoDS、NamedShape 或 ElementMap 输入。
- `cad-core/fixtures/c4m1/expected/part-ruled-surface-wire-wire.freecad.json` 声明 `FreeCADCmd oracle from part-ruled-surface-wire-wire.json`，对象为 `Part::RegularPolygon, Part::RegularPolygon, Part::RuledSurface`，并记录 `occt_shell`、`faces=4`、`edges=12`、`vertices=8`、`LowerWire.Edge1` / `UpperWire.Edge1` provenance 和 `part_ruled_surface:wire_wire_brepfill_shell`。
- `cad-core/tests/test_p8_features.py::test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance()` 断言 `occt_shell`、source curve、representative source edge relation、`wire_wire_brepfill_shell`，并与 checked-in expected 比较。
- `cad-core/src/runtime/capability_contract.cpp` 当前公开 `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`，`remaining_gaps=[]`，covered / request-local boundaries 均包含 `wire_wire_brepfill_shell`。

裁决：current-supported-candidate。S1 只证明 current code/test/capability 已有 wire/wire landing；collector trust、schema boundary 和 provenance strength 仍分别由 S2、S3、S4 关闭。

## 旧 PARTSURF 复核结论

- 旧 `PARTSURF-BLOCK-005` 的 `deferred-S2` 理由是当时缺少 collector path、cad-core input schema、shell/topo provenance checks。
- 旧 `PARTSURF-SCOPE-007` 和 `PARTSURF-FIX-005` 同样把 wire/wire 晋升条件限定为 collector、input schema、provenance validation 三项，而不是 FreeCAD source 不支持。
- S1 复核后，旧 PARTSURF deferred 在 source/current landing 层面归类为 `historical-deferred-doc-drift`；它仍作为 S2/S3/S4 的验证条件保留，不再作为 current backend gap 或代码实现理由。

## 输出

- `C12M6-BLOCKER-101` 关闭为 FreeCAD source authority verified。
- `C12M6-BLOCKER-102` 关闭为 old PARTSURF deferred reconciled。
- `c12m6_ruled_surface_wire_wire_source_candidates.tsv`、`backend_gap_classification.tsv`、`scope_review_matrix.tsv`、`blocker_queue.tsv`、`validation_matrix.tsv` 的 S1 owner 行已更新。

## 下一步

S2 只处理 collector / expected gate：复核 `c4m1/part-ruled-surface-wire-wire` fixture 与 checked-in expected 是否确实 source-backed、稳定、可复现，并运行 focused expected-backed test。S2 不改 input schema、provenance publication 或 C++ implementation；S3/S4 仍分别保留 request-local schema 和 shell/topo provenance 闸门。
