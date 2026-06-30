# C12-M6 S4 shell/topo provenance 准入验证【已实现】

## 目标

验证 current wire/wire shell 输出是否具备足够的 topo provenance，能支撑 `supported_wire_wire_expected_backed`，而不是只匹配 topology counts 或 bbox。

## 必读来源

- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/fixtures/c4m1/expected/part-ruled-surface-wire-wire.freecad.json`
- `cad-core/tests/test_p8_features.py::assert_ruled_surface_source_edge`
- `cad-core/tests/test_p8_features.py::test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance`

## 操作

1. 复核 current implementation 是否进入 `BRepFill::Shell(Wire, Wire)` 分支，并在 named shape / history event 中记录 `part_ruled_surface:wire_wire_brepfill_shell`。
2. 核对 expected 中 `shape=occt_shell`、faces/edges/vertices、bbox、volume 和 `element_history_status_contains`。
3. 核对 source edge provenance 至少覆盖两个 source wires 的代表 edge，并明确是否只覆盖 Edge1 smoke 还是完整 wire edge family。
4. 若 provenance 只够 smoke，不足以关闭旧 blocker，S4 应输出 `retained_validation_blocker` 或 `publication_repair_required`，不得继续 overclaim full surface family。

## 裁决规则

- 几何等价但 source provenance 缺失，不算准入通过。
- provenance 不能在 adapter 输出端补猜；必须来自 Part executor / TopoShapeExpansion / named shape history。
- `InternalFaceN` / `EdgeN` 顺序差异可单列为非失败项，但 source edge 丢失或 shell/face 类型错误是失败。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```

## S4 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=c9c58edd67`。
- `git log -1 --oneline=c9c58edd67 docs: 完成 C12-M6 S3 input schema 准入验证`。
- `git -c core.quotepath=false status --short -uall` 无输出，S4 起点为 clean。

## FreeCAD source 复核结论

- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` 对两条 wire 输入进入 `BRepFill::Shell(TopoDS::Wire(S1), TopoDS::Wire(S2))`；edge/edge 才进入 `BRepFill::Face`。
- 同一函数在 `BRepFill` 之后的注释明确：`BRepFill::Face()` 和 `Shell()` 会修改原始输入 edge，且没有 API 提供输出 edge 关系，因此 FreeCAD 通过 shared-vertex 搜索恢复 source edge 到输出 edge 的关系，并交给 `makeShapeWithElementMap()`。
- 裁决：S4 不能只看 bbox / topology counts；source edge relation 是 admission 必须项。

## current implementation 复核结论

- `cad-core/src/part/part_ruled_surface.cpp::resolveRuledSurfaceCurve()` 从 `context.shapes` 和 source `NamedShape` 收集每条 source edge 的 stable edge names；whole-wire link fallback 为 `Object.EdgeN`。
- `cad-core/src/part/part_ruled_surface.cpp::executePartRuledSurface()` 把两条 source curve 和 edge evidence 传给 `makeElementRuledSurfaceFromCurves()`；该路径来自 Part executor，不是 adapter 输出端。
- `cad-core/src/part/topo_shape_expansion.cpp::makeElementRuledSurfaceFromCurves()` 在 `isWire` 分支调用 `BRepFill::Shell(Wire, Wire)`，随后对两个 source 调用 `addRuledSurfaceSourceRelations()`，并写入 `part_ruled_surface:wire_wire_brepfill_shell`。
- `addRuledSurfaceSourceRelation()` 写入 `elementMap[sourceEdge]=targetEdge`、`ElementHistoryKind::Modified` 和 `MapperHistoryEvent{maker_stage=ruled_surface_shared_vertex_relation, relation=modified, recoverability=resolved}`。
- adapter 复核：CLI / C API adapter 只调用 recompute / capabilities 协议转换；`rg` 未发现 adapter 侧对 `LowerWire.Edge1`、`UpperWire.Edge1` 或 RuledSurface provenance 做补猜。

## expected / current 复核结论

- checked-in expected 记录 `object_fields.shape=occt_shell`、`topology_counts={faces:4, edges:12, vertices:8}`、bbox tolerance、`volume=10.5625`、`element_history_status_contains=["part_ruled_surface:wire_wire_brepfill_shell"]`，以及 `LowerWire.Edge1` / `UpperWire.Edge1` representative element-map 信号。
- legacy recompute smoke 输出 `diagnostics=[]`、`shape=occt_shell`、faces=4、edges=12、vertices=8、`volume=10.5625`，并包含 `part_ruled_surface:wire_wire_brepfill_shell` 与 `history_consumed:generated_modified`。
- current `element_map` 中 `LowerWire.Edge1 -> Edge1`、`UpperWire.Edge1 -> Edge3`，对应 output elements 均为 `kind=edge`、`status=modified`，且 sources 保留原 source edge。
- current `mapper_history` 包含 LowerWire.Edge1..Edge4 与 UpperWire.Edge1..Edge4 共 8 条 `ruled_surface_shared_vertex_relation` event。

## provenance strength 裁决

- `assert_ruled_surface_source_edge()` 是 representative edge smoke helper：它只验证调用者传入的 source edge 是否进入 `element_map`、target 是否为 edge、target element sources 是否包含该 source edge。
- focused test 只显式断言 `LowerWire.Edge1` 和 `UpperWire.Edge1`，因此测试函数本身不能描述“所有 wire edge family”。
- 但 S4 不是只依赖 helper 名义；源码循环覆盖 source curve 的所有 edge，current legacy recompute 也显示 8 条 source edge mapper event。对 `c4m1/part-ruled-surface-wire-wire` admission 而言，provenance 足以关闭旧 blocker。
- 该裁决不等于 full Part surface family 支持，也不关闭 S5 publication gate。

## 验证结果

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
```

结果：`Ran 1 test in 0.107s`，`OK`。

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```

结果：`Ran 1 test in 0.099s`，`OK`。

## S4 裁决

- `C12M6-BLOCKER-401` 关闭为 `closed_s4`。
- `C12M6-CAT-005` 关闭为 `provenance_admitted_current_supported_candidate`。
- `C12M6-SCOPE-006` 关闭为 `closed_s4_provenance_admitted`。
- `c12m6_ruled_surface_wire_wire_provenance_matrix.tsv` 的 S4 行全部为 `passed_s4`。
- S5 publication gate / `C12M6-BLOCKER-501` 保持 open；下一步为 S5 implementation/publication gate。
