# capabilities gap 对照表

本文件把当前 `cad_core_capabilities_json()` 暴露的能力和真实缺口对齐到 CAD Core 3.0 文档。它只记录当前基线和后续接管边界，不记录字段删除过程。

## 当前 capability 分组

| capability 分组 | 当前状态 | remaining gaps / 边界 |
| --- | --- | --- |
| `document` | FreeCAD 风格 `DocumentObject graph`、link property fields、ExternalGeometry flags、native ExternalGeo request-side pool、`elementReferenceUpdates` / `documentObjectUpdates` 已有结构化通道 | `external_geometry_lifecycle.remaining_gaps` 为空 |
| `link_transaction` | ShowElement、plain group child cache、ElementList / ElementCount owner sync、CopyOnChange transport、deep copy lifecycle 和 touched sync 已通过 update channel 表达 | remaining gaps 为空；请求 graph 仍 immutable，copy 后对象由前端保存到下一次请求 |
| `link_reference_lifecycle` | source object rename、label rename、nested Link/Group label、cross-document `FullSubList`、XLink document restore、hash mismatch、missing / pending / unloaded external document diagnostics 已有 first slice | remaining gaps 为空 |
| `topo_history` | mapper history core、producer matrix、ElementMap policy、child-map source range、ShapeFix / import / Part Offset / Offset2D / Section / DressUp / transformed / Hole / Loft ThruSections history 均已拆成具体 producer 或 diagnostic；WireJoiner full ledger 已接入 `MapperHistory(aHistory) -> ElementMap` | `topo_history.remaining_gaps` 为空 |
| `sketcher.solver` | conflict / redundancy / malformed / partial redundancy diagnostics、request-local DoF、dependent group metadata、常用几何关系 request-local 写回已覆盖 | remaining gaps 为空 |
| `part_workbench.offset` | `Part::Compound`、`Part::Offset`、`Part::Offset2D`、`Part::Thickness`、`Part::Section` 第一批 maker history 和 diagnostics 已覆盖 | remaining gaps 为空 |
| `part_workbench.conic_curves` | `Part.Hyperbola` / `Part.Parabola` geometry wrapper 对应请求级 `PartConicCurveDTO`，已覆盖有限 edge typed shape conversion、稳定 conic diagnostics、Hyperbola / Parabola edge -> `Part::Extrusion` -> `occt_face` consumer；conic edge 也可作为已验证 producer 进入 `Part::RuledSurface` edge/edge fixture | remaining gaps 明确保留 full Part surface family、ProjectionOnSurface、GUI conic edit、完整 Sketcher solver conic constraints、DistanceType default/TODO；不声明 `Part::Hyperbola` / `Part::Parabola` DocumentObject executor |
| `part_workbench.ruled_surface` | source-backed `Part::RuledSurface` executor 第一批已覆盖：`Curve1` / `Curve2` `App::PropertyLinkSub`、`Orientation=Automatic/Forward/Reversed`、edge/edge `BRepFill::Face`、source edge provenance 和四个 p8 fixtures | remaining gaps 保留 `wire_wire_brepfill_shell`、`projection_on_surface_source_audited_planned`、`full_part_surface_family`；不声明 full RuledSurface / full Part surface family |
| `part_workbench.loft` | source-backed `Part::Loft` executor 第一批已发布：`Sections=App::PropertyLinkList`、`Solid` / `Ruled` / `Closed` / `MaxDegree`、`BRepOffsetAPI_ThruSections`、`loft_thru_sections` maker history 和五个 c3m4 expected-backed fixtures | remaining gaps 保留 `Linearize=true` 后处理、face / vertex profile expected、复杂 profile family、Sweep / Filling / GeomPlate / PipeShell 和 full Part surface family；不声明完整 Part surface family |
| `part_design.body_chain` | BaseFeature / Group / Tip reroute、Origin / Datum relink、Add/Sub replay first slice 已覆盖 | remaining gaps 为空 |
| `part_design.pad_pocket` | `UpToShape` single target solid / face 与多面 LinkSubList 主路径覆盖；Pad / Pocket 成功 fixture 已覆盖，offset 多面和非 face selection 保留结构化失败诊断 | remaining gaps 为空；失败边界 diagnostics 为 `unsupported_property`、`unsupported_subshape_kind`、`invalid_subshape`、`missing_link_target` |
| `part_design.hole` | thread table、head cut resource、ModelThread、profile source mapper history、compound tool shape、subtractive cut history first slice 已覆盖 | remaining gaps 为空 |
| `assembly` | grounded Fixed / Revolute / Slider / Ball / Distance / Angle 走真实 Ondsel；placement writeback lifecycle 已覆盖；representative fallback 保留 | `ondsel_solver_adapter.status=covered_full`；`representative_solver_adapter.status=covered_representative` 只表示 fallback |
| `adapters` | CLI / C ABI / Worker / WASM / streaming mesh / binary mesh 复用同一 core recompute | remaining gaps 为空；adapter 不承载建模语义 |
| `object_metadata` | Pad / Pocket / Part::Extrusion taper history metadata 已为 `covered_full` | `NamedShape.element_map_status=history_partial` 只表达当前 maker history metadata，不恢复旧 local-history gap |
| `known_gaps` | 空 | 后续新增缺口必须拆成 producer、lifecycle、feature-family、JointType 或 adapter contract 项 |

## Sketcher conic arcs 发布口径

- P5CONIC 已发布 `ArcOfHyperbola` / `Part::GeomArcOfHyperbola` 与 `ArcOfParabola` / `Part::GeomArcOfParabola` 的 Sketcher profile / raw edge、construction 过滤、native `ExternalGeo` 和 projected `ExternalGeometry` TopoDS Edge 支持；证据落在 `cad-core/fixtures/p5`、`cad-core/fixtures/p5/expected`、`cad-core/tests/test_p5_sketch.py` 和 `cad-core/tests/test_diagnostics.py`。
- 当前 `cad_core_capabilities_json()` 的 `sketcher.solver` 仍只表达 solver-facing diagnostics / request-local geometry update；conic arcs 是 `SketchObject` geometry / external geometry 能力发布，不把完整 Sketcher solver 内部辅助几何 / conic 约束写成 supported。
- 保持非目标边界：GUI conic edit、完整 Sketcher solver 内部辅助几何 / 约束、未验证的完整 Part workbench conic surface family 均不属于本次 capability。

## Part Workbench conic geometry 发布口径

- PARTCONIC 已发布 `Part.Hyperbola` / `Part.Parabola` geometry wrapper 的 typed DTO / shape conversion：`cad-core/src/part/part_geometry_curve.cpp` 解析 `PartConicCurveDTO`，按 `Geometry.cpp::GeomHyperbola/GeomParabola/GeomArcOf*::Save/Restore()` 字段构造有限 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` edge，并保留 `Part.Hyperbola` / `Part.Parabola` metadata。
- 验收证据为 `cad-core/fixtures/p8/part-hyperbola-edge.json`、`part-parabola-edge.json`、`part-conic-edge-invalid-params.json`、`part-conic-edge-extrusion.json` 及对应 FreeCAD expected；`tests.test_diagnostics`、`tests.test_p8_features`、`tests.test_expected_fixtures` 保护 diagnostics、metadata、subshape / expected parity。
- consumer 只发布已验证的 edge-to-face 路径：`part-conic-edge-extrusion` 使用 Hyperbola / Parabola DTO edge 进入现有 `Part::Extrusion`，FreeCAD oracle 对应 `FeatureExtrusion.cpp::Extrusion::extrudeShape()` 的 `result.makeElementPrism(myShape, vec)` regular path。
- 不支持声明：`Part::Hyperbola` / `Part::Parabola` `DocumentObject` executor、完整 Part surface family、ProjectionOnSurface、GUI conic edit、完整 Sketcher solver conic constraints、DistanceType default/TODO、BREP / polyline / BSpline 替代 typed conic。`RuledSurface` 已从 PARTCONIC gap 拆到独立 `part_workbench.ruled_surface` capability，只覆盖 source-backed edge/edge 第一批。

## Part Workbench RuledSurface 发布口径

- `Part::RuledSurface` 已发布 source-backed `DocumentObject` executor 第一批；FreeCAD 依据是当前源码 `PartFeatures.cpp::RuledSurface::execute()` 读取 `Curve1` / `Curve2` 后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())`，以及 `TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` 在 edge 输入上调用 `BRepFill::Face`。
- cad-core 落点为 `cad-core/src/part/part_ruled_surface.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py` 和 `cad-core/src/adapters/c_api/c_api.cpp` capability metadata。
- 验收证据为 `cad-core/fixtures/p8/part-ruled-surface-line-line.json`、`part-ruled-surface-conic-line.json`、`part-ruled-surface-orientation-reversed.json`、`part-ruled-surface-invalid-input.json` 及对应 expected / focused tests；conic-line 只证明 link-resolved edge/edge geometry 可消费 `PartConicCurveDTO` edge，不声明 fake conic DocumentObject。
- 不支持声明：wire/wire `BRepFill::Shell`、非 edge/wire 自动提取 wire 的完整分支、`Part::ProjectOnSurface`、完整 Part surface family、Sweep / Filling / PipeShell / GeomPlate。

## Part Workbench Loft 发布口径

- `Part::Loft` 已发布 source-backed `DocumentObject` executor 第一批；FreeCAD 依据是当前源码 `PartFeatures.cpp::Loft::execute()` 读取 `Sections`、`Solid`、`Ruled`、`Closed`、`Linearize`、`MaxDegree`，再调用 `result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)`。
- cad-core 落点为 `cad-core/src/part/part_loft.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py` 和 `cad-core/src/adapters/c_api/c_api.cpp` capability metadata。
- 验收证据为 `cad-core/fixtures/c3m4/part-loft-two-section-surface.json`、`part-loft-solid.json`、`part-loft-ruled.json`、`part-loft-closed.json`、`part-loft-invalid-sections.json` 及对应 FreeCAD expected / focused tests；topo history 发布为 `loft_thru_sections`，覆盖 `GeneratedFace(s)`、`FirstShape()`、`LastShape()` 这类 `MapperThruSections` maker history。
- 不支持声明：`Linearize=true` 后处理、face / vertex profile expected、复杂 profile family、Sweep / Filling / GeomPlate / PipeShell、完整 Part surface family；invalid sections 只发布为 diagnostics / expected-backed failure boundary，不伪造 shape。

## WireJoiner capability 边界

| 项 | 当前 capability 状态 | 当前代码事实 | 后续目标 |
| --- | --- | --- | --- |
| `generated_open_export_bridge` | `covered_full` | open-export relation 由 `WireJoinerMapperHistoryProducerEvidence`、openWireCompound child-wire ownership 和 concrete `wire_joiner:open_export` mapper event 进入 topo；唯一 child-wire ElementMap alias 已覆盖，split / deleted / ambiguous 留在 mapper history / terminal history | remaining gaps 为空 |
| `purge_as_original_bridge` | `covered_full` | noOriginal 公开 candidate bridge 已删除；deleted relation 由 child-wire shared-source ledger、`MapperHistory(aHistory)` 消费路径和 terminal history 表达 | remaining gaps 为空 |
| `wire_joiner_open_export_element_map_unique_child_wire_alias` | covered full ledger | branch open-cutter 已能把唯一 source -> InternalEdge alias 写入 `Sketch.InternalShape.element_map` | 继续保持 ElementMap 只写唯一 target；split / deleted / ambiguous 不猜唯一 alias |
| `wire_joiner_history_event_ledger` | covered full ledger | history event 已记录 relation、source lineage、splitter lineage、actual noOriginal purge 和 Modified / Generated fragment 标记，并作为 `MapperHistory(aHistory)` 正式输入被 topo 消费 | remaining gaps 为空 |

## 收口规则

- 不恢复笼统 `complete_mapper_history`、`known_gaps`、旧 taper local-history gap 或旧 helper output gap。
- 不用空 `remaining_gaps` 伪装 full coverage；`covered_representative` 和 diagnostic-only boundary 必须明确写出边界。
- WireJoiner 后续只允许沿 FreeCAD `WireJoinerP::EdgeInfo / WireInfo / myShapesToReturn / MapperHistory / ElementMap` 路径维护，不恢复 producer bridge / candidate / result-slot 公开字段。
- `ElementMap` 只承接唯一 target alias；split / deleted / ambiguous relation 必须保留在 `MapperHistory`、terminal history 或 diagnostics 中。
- 所有新增 capability 必须有 FreeCAD 源码依据、cad-core 落点、fixture / test 或稳定 diagnostic。

## 验收入口

文档修改：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check -- docs/CADCore3.0
```

WireJoiner full ledger 回归优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```
