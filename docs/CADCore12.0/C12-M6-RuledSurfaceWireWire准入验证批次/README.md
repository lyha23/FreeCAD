# C12-M6 RuledSurface Wire/Wire 准入验证批次

C12-M6 用于验证 `Part::RuledSurface` wire/wire 分支是否已经满足正式准入条件。

触发原因是旧 C3M4 PartSurface 主线把 `PARTSURF-BLOCK-005` 保留为 `deferred-S2`，理由是当时缺少 source-backed collector、cad-core input schema 和 shell/topo provenance 三项闭环。但当前 live code / capability 已出现 `supported_wire_wire_expected_backed`、`c4m1/part-ruled-surface-wire-wire` fixture、checked-in expected 和 focused tests。C12-M6 的任务是核实这组 current evidence 是否足够正式关闭旧 deferred blocker，或者是否需要文档发布修复 / 代码修复。

本包不是默认 C++ implementation 包。只有 S5 证明 checked-in expected 与 current output mismatch，且 mismatch 不能通过 docs/capability wording 修复时，才允许另开实现包。

S5 已完成最终发布闸门，出口为 `wire_wire_admitted_current_supported`。S2 collector / expected、S3 input schema、S4 shell/topo provenance 三闸门均通过，focused wire/wire test 与 C API capability publication smoke 通过；未发现 checked-in expected / current output mismatch，不创建 implementation 包，不修改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source。

## S0 live 冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=71dba06fb6`。
- `git log -1 --oneline=71dba06fb6 docs: 完成 C12-M5 S5 发布闸门`。
- 起点 dirty boundary 只包含 `docs/CADCore12.0/README.md` 修改和未跟踪的 C12-M6 包；未发现 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source 改动。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 / C12-M5 `工作步骤细分` 队列均只输出表头。
- C12-M6 S0 开始前队列为索引、S0、S1、S2、S3、S4、S5；索引只承担导航职责，随 S0 一并标记为 `【已实现】` 后，下一步为 S1 source/current evidence 复核。
- `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`，`remaining_gaps=[]`，covered 包含 `wire_wire_brepfill_shell` / `wire_edge_provenance` / `expected_backed_fixtures`，request-local boundaries 包含 `wire_wire_brepfill_shell`。
- 旧 `PARTSURF-BLOCK-005`、`PARTSURF-SCOPE-007`、`PARTSURF-FIX-005` 的 deferred 条件冻结为：source-backed collector、cad-core input schema、shell/topo provenance。

## S1 source/current 复核

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=2083c0cbdb`（`2083c0cbdb docs: 冻结 C12-M6 S0 live 基线`），起点 `status --short -uall` 无输出。
- FreeCAD `PartFeatures.cpp::RuledSurface::execute()` source-backed 读取 `Curve1` / `Curve2` / `Orientation` 后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())`。
- FreeCAD `TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` source-supported：edge/edge 调用 `BRepFill::Face`，wire/wire 调用 `BRepFill::Shell`，并记录 BRepFill 修改输入 edge 后需要 shared-vertex relation recovery。
- current cad-core 已有 executor/helper/fixture/expected/focused test/capability wire/wire landing，因此旧 PARTSURF deferred 在 S1 层面归类为 `historical-deferred-doc-drift`，不是 current backend implementation gap。
- S1 只关闭 `C12M6-BLOCKER-101/102`；S2 collector/expected、S3 input schema、S4 shell/topo provenance 仍是正式 admission gate。

## S2 collector / expected 准入

- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=af6477dd3b`（`af6477dd3b docs: 完成 C12-M6 S1 源码证据复核`），起点 `status --short -uall` 无输出。
- fixture 使用 request-local `DocumentObject graph`：`LowerWire` / `UpperWire` 为 `Part::RegularPolygon` wire producers，`RuledSurface` 为 `TypeId=Part::RuledSurface`，`Curve1` / `Curve2` 为 `App::PropertyLinkSub`，输入未携带 BREP、TopoDS、persistent `NamedShape` / `ElementMap` 或 mesh。
- checked-in expected 的 `reference` 是 `FreeCADCmd oracle from part-ruled-surface-wire-wire.json; objects: Part::RegularPolygon, Part::RegularPolygon, Part::RuledSurface`；`freecad_version=1.2.0 revision 20260519`，当前 expected 未序列化独立 `occt_version` 字段。
- expected 记录 `shape=occt_shell`、`topology_counts={faces:4, edges:12, vertices:8}`、bbox、`volume=10.5625`、`object_fields` source curves / orientation / status，以及 `LowerWire.Edge1` / `UpperWire.Edge1` element-map 信号。
- collector 支持 `Part::RuledSurface` native type，`object_expected_payload()` 走 `ruled_surface_payload()` 从 FreeCAD native `obj.Shape` 生成 geometry/object_fields；S2 未刷新 expected。
- focused unittest `tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance` 通过。S2 关闭 `C12M6-BLOCKER-201/202/203/204`；当时下一步为 S3 input schema gate，S4 provenance strength 和 S5 publication gate 仍保持 open。

## S3 input schema 准入

- S3 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=eb7a856b15`（`eb7a856b15 docs: 完成 C12-M6 S2 expected 准入验证`），起点 `status --short -uall` 无输出。
- fixture payload 为 request-local `DocumentObject graph`：`Objects[2].TypeId=Part::RuledSurface`，`Curve1.value=LowerWire`，`Curve2.value=UpperWire`，`Orientation.value=Automatic`，`recompute.objs=["RuledSurface"]`。`SubList` 在该 fixture 中缺省，表示 whole wire object；capability 仍公开 `Curve1.SubList` / `Curve2.SubList` 可选 key。
- `LowerWire` / `UpperWire` 由当前请求图中的 `Part::RegularPolygon` 重算；fixture 无 `BREP`、`TopoDS`、persistent `NamedShape` / `ElementMap`、mesh 或旧 shape cache 字段。
- 当前 document 层实际落点为 `cad-core/src/app/document.cpp`、`document_object.cpp`、`property.cpp`、`property_links.cpp`；只解析 `Objects`、properties、link-sub 和 `recompute.objs`，不引入后端 session cache。
- `part_ruled_surface.cpp` 只接受 `Curve1`、`Curve2`、`Orientation`，从 `context.shapes` 解析本次 recompute source shape，并拒绝 missing link、multiple subnames、no-edge 和非 edge/wire 输入；未发现 fixture name branch、adapter shortcut 或 compound hack。
- capability snapshot 的 `request_local_boundaries` 包含 `source_shape_recomputed_from_document_graph` 和 `wire_wire_brepfill_shell`，payload wording 不要求 full BREP transport。S3 关闭为 `input_schema_admitted`；S4 provenance gate 和 S5 publication gate 继续 open。

## S4 shell/topo provenance 准入

- S4 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c9c58edd67`（`c9c58edd67 docs: 完成 C12-M6 S3 input schema 准入验证`），起点 `status --short -uall` 无输出。
- FreeCAD `TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` 的 wire/wire 分支调用 `BRepFill::Shell(TopoDS::Wire(S1), TopoDS::Wire(S2))`；相邻注释明确 `BRepFill::Face()` 和 `Shell()` 会修改原始 input edges，需要通过 shared-vertex 搜索重建 source edge relation 后交给 element map。
- current `cad-core/src/part/part_ruled_surface.cpp` 从 source `NamedShape` / selected curve 收集 source edge evidence，并把两条 curve 传给 `makeElementRuledSurfaceFromCurves()`；`cad-core/src/part/topo_shape_expansion.cpp` 在 `isWire` 分支调用 `BRepFill::Shell(Wire, Wire)`，随后对两个 source 调用 `addRuledSurfaceSourceRelations()` 并写入 `part_ruled_surface:wire_wire_brepfill_shell`。
- checked-in expected 记录 `shape=occt_shell`、faces=4、edges=12、vertices=8、bbox tolerance、`volume=10.5625`、`element_history_status_contains=["part_ruled_surface:wire_wire_brepfill_shell"]`，以及 `LowerWire.Edge1` / `UpperWire.Edge1` 的 representative element-map 信号。
- current legacy recompute smoke 输出 `diagnostics=[]`，`RuledSurface.shape=occt_shell`，faces=4、edges=12、vertices=8，`volume=10.5625`；`element_history_status` 包含 `part_ruled_surface:wire_wire_brepfill_shell` 和 `history_consumed:generated_modified`。
- `assert_ruled_surface_source_edge()` 本身是按传入 source edge 做 representative smoke；focused test 只显式断言 `LowerWire.Edge1` 和 `UpperWire.Edge1`。但 current output 的 `mapper_history` 包含 LowerWire.Edge1..Edge4 与 UpperWire.Edge1..Edge4 共 8 条 `ruled_surface_shared_vertex_relation` / `modified` / `resolved` event，因此本 fixture 的 source edge relation 不只是 adapter 输出端猜测。
- CLI / C API adapter 只做 recompute / capabilities 协议转换；RuledSurface provenance 命中点在 Part executor、TopoShapeExpansion、NamedShape serialization 和 capability contract，未发现 adapter 侧补猜 `LowerWire.Edge1` / `UpperWire.Edge1`。
- focused unittest 与 C API capability publication smoke 均通过。S4 裁决为 `provenance_admitted_current_supported_candidate`，关闭 `C12M6-BLOCKER-401`；当时 S5 publication gate 仍保持 open，不在 S4 发布 full surface family 或关闭 `C12M6-BLOCKER-501`。

## S5 implementation / publication gate

- S5 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=abb595fdbb`（`abb595fdbb docs: 完成 C12-M6 S4 provenance 准入验证`），起点 worktree clean。
- 三闸门全部成立：S2 为 `collector_expected_admitted`，S3 为 `input_schema_admitted`，S4 为 `provenance_admitted_current_supported_candidate`。
- capability wording `supported_wire_wire_expected_backed` 与 current evidence 一致：只发布 `Part::RuledSurface` wire/wire shell、source edge provenance、request-local graph/schema 和 expected-backed fixture，不发布 full Part surface family、ProjectOnSurface 或 CopyOnChange。
- focused wire/wire test 与 adapter capability smoke 均通过，无 current mismatch。
- 最终出口为 `wire_wire_admitted_current_supported`；`C12M6-BLOCKER-501`、`C12M6-CAT-006/007`、`C12M6-SCOPE-007/008` 已关闭；旧 C3M4 `PARTSURF-BLOCK-005` / `PARTSURF-SCOPE-007` / `PARTSURF-FIX-005` 改为 historical closed / superseded by C12-M6 evidence。

## 入口

- 总入口：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次总入口.md`
- 方案：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 最终裁决结果

| 结果 | 含义 |
| --- | --- |
| `wire_wire_admitted_current_supported` | 已发布：collector、schema、provenance 和 focused tests 均成立；关闭旧 deferred blocker，无新增 C++。 |
| `publication_repair_required` | 未采用：current code/test 成立，C12-M6 仅需把旧 C3M4 deferred 行改为历史关闭，不需要 capability source 修复。 |
| `implementation_candidate_required` | 未采用：checked-in expected 与 current output 无真实 mismatch。 |
| `retained_validation_blocker` | 未采用：collector、schema、provenance 三项准入证据均已关闭。 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
```
