# C12-M6 RuledSurface Wire/Wire 准入验证批次

C12-M6 用于验证 `Part::RuledSurface` wire/wire 分支是否已经满足正式准入条件。

触发原因是旧 C3M4 PartSurface 主线把 `PARTSURF-BLOCK-005` 保留为 `deferred-S2`，理由是当时缺少 source-backed collector、cad-core input schema 和 shell/topo provenance 三项闭环。但当前 live code / capability 已出现 `supported_wire_wire_expected_backed`、`c4m1/part-ruled-surface-wire-wire` fixture、checked-in expected 和 focused tests。C12-M6 的任务是核实这组 current evidence 是否足够正式关闭旧 deferred blocker，或者是否需要文档发布修复 / 代码修复。

本包不是默认 C++ implementation 包。只有 S5 证明 checked-in expected 与 current output mismatch，且 mismatch 不能通过 docs/capability wording 修复时，才允许另开实现包。

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

## 入口

- 总入口：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次总入口.md`
- 方案：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前裁决目标

| 结果 | 含义 |
| --- | --- |
| `wire_wire_admitted_current_supported` | collector、schema、provenance 和 focused tests 均成立；关闭旧 deferred blocker，无新增 C++。 |
| `publication_repair_required` | current code/test 成立，但旧 C3M4/CADCore 文档或 capability wording 漂移；只做文档/metadata 修复。 |
| `implementation_candidate_required` | checked-in expected 与 current output 出现真实 mismatch，且来源可定位到 Part executor / TopoShapeExpansion / topo provenance；另开 implementation 包。 |
| `retained_validation_blocker` | 三项准入证据仍缺一项；保留 blocker，不实现、不发布 supported。 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
```
