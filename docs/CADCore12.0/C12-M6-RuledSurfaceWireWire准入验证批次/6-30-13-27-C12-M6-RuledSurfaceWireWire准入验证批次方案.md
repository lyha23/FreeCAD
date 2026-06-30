# C12-M6 RuledSurface Wire/Wire 准入验证批次方案

## 背景

C3M4 PartSurface RuledProjection 主线已经实现 `Part::RuledSurface` edge/edge 第一批，并把 wire/wire 保留为 `PARTSURF-BLOCK-005 = deferred-S2`。旧裁决要求补齐三项证据后才能重开：source-backed collector、cad-core input schema、shell/topo provenance。

当前 live code 已出现新的证据：`part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`，focused test 断言 `occt_shell`、`part_ruled_surface:wire_wire_brepfill_shell` 和 source edge provenance。C12-M6 因此不是从零实现 wire/wire，而是验证这组 current evidence 是否足够让旧 deferred blocker 正式准入。

S5 已完成发布闸门，最终出口为 `wire_wire_admitted_current_supported`。三闸门均通过，旧 C3M4 deferred 行关闭为 historical closed；本包不创建 implementation package，不改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source。

## 设计原则

| 原则 | 含义 |
| --- | --- |
| 先验 source authority | 以 `PartFeatures.cpp::RuledSurface::execute()` 和 `TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()` 为准。 |
| 三闸门同时成立 | collector、input schema、shell/topo provenance 必须全部通过；缺一项不发布 supported。 |
| 不用 adapter 特例补洞 | RuledSurface 必须走 source-backed `Part::RuledSurface` executor 和 TopoShapeExpansion helper。 |
| expected/current 双证据 | checked-in FreeCAD expected 与 current recompute/focused tests 都要能复核。 |
| 文档漂移单独处理 | 旧 C3M4 deferred 行可能只是历史未回写；不要把文档漂移误判为 C++ bug。 |

## 最小完整语义批次

wire/wire 不应拆成单个输出 shape 检查，因为 FreeCAD 同一调用链同时包含：

- `Curve1` / `Curve2` `App::PropertyLinkSub` source-backed 取形。
- edge/wire normalization 与 wire 输入准入。
- `Orientation=Automatic/Forward/Reversed` 语义。
- `BRepFill::Shell(Wire, Wire)` shell 构造。
- BRepFill 修改输入 edge 后的 source edge provenance 恢复。
- capability / adapter tests 对 supported wording 的公开口径。

因此 C12-M6 一次性验证 source、collector、schema、provenance 和发布闸门；S5 只在三项证据全部成立后关闭旧 deferred blocker。

## 关键问题

1. checked-in `c4m1/part-ruled-surface-wire-wire` expected 是否确实来自 FreeCADCmd source-backed `Part::RuledSurface` object，而不是 cad-core 自证？
2. fixture input 是否只使用 DocumentObject graph、`PropertyLinkSub` 和 recompute target，不携带 BREP、TopoDS 或 adapter-only shortcut？
3. current `TopoShapeExpansion` 是否真正走 `BRepFill::Shell(Wire, Wire)`，并把 shell 输出的 element history / source edge relation 写入 named shape？
4. focused tests 和 capability JSON 是否足以支撑 `supported_wire_wire_expected_backed`，还是只覆盖了过窄的 Edge1 smoke？
5. 若旧 C3M4 `PARTSURF-BLOCK-005` 与 current capability 冲突，应修文档，还是创建 C++ implementation candidate？S5 已裁决为历史 deferred 文档漂移：由 C12-M6 evidence 关闭，不创建 C++ implementation candidate。

## 最终出口

- `wire_wire_admitted_current_supported`：collector / expected、input schema、shell/topo provenance、focused wire/wire test 和 adapter capability smoke 均成立。
- `publication_repair_required` 未采用：只需把旧 C3M4 deferred 行改为 historical closed，不需要改 capability source。
- `retained_validation_blocker` 未采用：三项准入证据无缺项。
- `implementation_candidate_required` 未采用：没有 checked-in expected / current output mismatch。
- non-goals 保持：ProjectOnSurface、full Part surface family、SubShapeBinder CopyOnChange、full BREP transport 和 persistent NamedShape / ElementMap cache 不因本包被发布为 supported。

## 交付物

- `README.md`
- `6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次总入口.md`
- `工作步骤细分/`
- `矩阵/c12m6_ruled_surface_wire_wire_source_candidates.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_scope_review_matrix.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_collector_validation_matrix.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_input_schema_matrix.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_provenance_matrix.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_backend_gap_classification.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_blocker_queue.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_non_goal_registry.tsv`
- `矩阵/c12m6_ruled_surface_wire_wire_validation_matrix.tsv`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次 docs/CADCore12.0/README.md
git diff --check
```
