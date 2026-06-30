# C12-M6 RuledSurface Wire/Wire 准入验证批次总入口

## 目标

验证当前 `Part::RuledSurface` wire/wire 支持是否满足正式准入条件，并裁决旧 `PARTSURF-BLOCK-005` 是应关闭为 current-supported、保留 blocker，还是另开 implementation candidate。

最终裁决：`wire_wire_admitted_current_supported`。C12-M6 已确认 collector / expected、request-local input schema、shell/topo provenance 三闸门全部成立，focused wire/wire test 与 capability smoke 通过；旧 PARTSURF deferred 行关闭为历史文档漂移，不再阻塞 current RuledSurface wire/wire。

## 当前假设

当前 live code / capability 已显示 wire/wire 可能已经实现：`part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`。C12-M6 必须核实该口径，不直接把旧 C3M4 `deferred-S2` 当成仍未实现。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=71dba06fb6`（`71dba06fb6 docs: 完成 C12-M5 S5 发布闸门`）。
- 起点 dirty boundary 只包含 `docs/CADCore12.0/README.md` 修改和未跟踪的 C12-M6 包。
- C12-M1..M5 队列均为空；C12-M6 S0 开始前队列为索引、S0、S1、S2、S3、S4、S5。
- `cad-core/build/cad-core capabilities` 显示 `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 含 `c4m1/part-ruled-surface-wire-wire`，`remaining_gaps=[]`，covered / request-local boundaries 均包含 `wire_wire_brepfill_shell`。
- 旧 deferred 继承口径冻结为 collector、input schema、shell/topo provenance 三项；下一步为 S1 source/current evidence 复核。

## S1 source/current evidence 复核

- S1 执行基线：`HEAD=2083c0cbdb`（`2083c0cbdb docs: 冻结 C12-M6 S0 live 基线`），起点 worktree clean。
- FreeCAD source-supported：`Part::RuledSurface` 声明 `Orientation`、`Curve1`、`Curve2`；`execute()` 调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())`；`TopoShape::makeElementRuledSurface()` 对 wire/wire 调用 `BRepFill::Shell` 并记录 source edge recovery comment。
- current-supported-candidate：cad-core executor/helper、`c4m1/part-ruled-surface-wire-wire` fixture/expected、focused test 和 capability 均已有 wire/wire landing。
- 旧 `PARTSURF-BLOCK-005` 在 S1 层面裁决为 `historical-deferred-doc-drift`；只关闭 `C12M6-BLOCKER-101/102`，collector/schema/provenance 仍由 S2/S3/S4 关闭。

## S2 collector / expected 准入验证

- S2 执行基线：`HEAD=af6477dd3b`（`af6477dd3b docs: 完成 C12-M6 S1 源码证据复核`），起点 worktree clean。
- `c4m1/part-ruled-surface-wire-wire` fixture 是 request-local `Part::RegularPolygon` wire producers + `Part::RuledSurface` graph；`Curve1` / `Curve2` 为 `App::PropertyLinkSub`，未携带 BREP、TopoDS、persistent `NamedShape` / `ElementMap` 或 mesh。
- checked-in expected 是 `FreeCADCmd oracle`，baseline 为 `freecad_version=1.2.0 revision 20260519`；当前 expected 未单独序列化 `occt_version` 字段。
- expected 记录 `occt_shell`、faces=4、edges=12、vertices=8、bbox、volume=10.5625、source curve object fields 和 Lower/Upper representative edge element-map 信号。
- collector `SUPPORTED_NATIVE_TYPES` 包含 `Part::RuledSurface`，native payload 从 FreeCAD `obj.Shape` 生成 geometry/object_fields；focused expected-backed unittest 已通过。
- S2 裁决为 `collector_expected_admitted`，只关闭 `C12M6-BLOCKER-201/202/203/204`；S3 input schema、S4 provenance 和 S5 publication 仍 open。

## S3 input schema 准入验证

- S3 执行基线：`HEAD=eb7a856b15`（`eb7a856b15 docs: 完成 C12-M6 S2 expected 准入验证`），起点 worktree clean。
- fixture schema 使用 `Part::RuledSurface` + `Curve1` / `Curve2` `App::PropertyLinkSub` value links + `Orientation=Automatic` + `recompute.objs=["RuledSurface"]`；`SubList` 缺省表示 whole wire object，capability 公开可选 `Curve1.SubList` / `Curve2.SubList`。
- `LowerWire` / `UpperWire` 为当前请求图内的 `Part::RegularPolygon` producers；fixture 无 BREP / TopoDS / persistent `NamedShape` / `ElementMap` / mesh / cache 字段。
- document/app parser 只解析 request graph、properties、link-sub 和 recompute targets；`part_ruled_surface.cpp` 从 `context.shapes` 读取本次 recompute source shape，并拒绝 no-edge 和非 edge/wire 输入。
- capability snapshot 包含 `source_shape_recomputed_from_document_graph` 与 `wire_wire_brepfill_shell`，未要求 full BREP transport。S3 裁决为 `input_schema_admitted`，只关闭 `C12M6-BLOCKER-301`；S4 provenance 和 S5 publication 仍 open。

## S4 shell/topo provenance 准入验证

- S4 执行基线：`HEAD=c9c58edd67`（`c9c58edd67 docs: 完成 C12-M6 S3 input schema 准入验证`），起点 worktree clean。
- FreeCAD `TopoShape::makeElementRuledSurface()` 的 wire/wire 分支调用 `BRepFill::Shell`，并在注释中记录 BRepFill 会修改输入 edge、需要恢复 source edge relation；current core 的对应落点是 `part_ruled_surface.cpp` 收集 source edge evidence、`topo_shape_expansion.cpp` 调用 `BRepFill::Shell(Wire, Wire)` 并写入 `part_ruled_surface:wire_wire_brepfill_shell`。
- checked-in expected 与 current focused test 覆盖 `occt_shell`、faces=4、edges=12、vertices=8、bbox tolerance、`volume=10.5625`、Lower/Upper representative Edge1 element-map 和 `wire_wire_brepfill_shell`。
- `assert_ruled_surface_source_edge()` 是 representative edge smoke helper；但 current legacy recompute 的 `mapper_history` 实际包含 LowerWire.Edge1..Edge4 与 UpperWire.Edge1..Edge4 共 8 条 source edge relation event，provenance 来自 Part executor / TopoShapeExpansion / NamedShape history，不是 adapter output guess。
- S4 裁决为 `provenance_admitted_current_supported_candidate`，关闭 `C12M6-BLOCKER-401`；当时 S5 publication gate 与 `C12M6-BLOCKER-501` 仍 open，不在 S4 发布 full surface family。

## S5 implementation / publication gate

- S5 执行基线：`HEAD=abb595fdbb`（`abb595fdbb docs: 完成 C12-M6 S4 provenance 准入验证`），起点 worktree clean。
- S2 / S3 / S4 三闸门全部关闭，最终出口为 `wire_wire_admitted_current_supported`。
- capability wording `supported_wire_wire_expected_backed` 准确描述 current support：只覆盖 `Part::RuledSurface` wire/wire expected-backed shell 与 source edge provenance，不扩展为 full Part surface family、ProjectOnSurface 或 CopyOnChange。
- 无 checked-in expected / current output mismatch；不创建 implementation 包，不改 C++、fixtures、expected、tests、adapters 或 capability source。
- `C12M6-BLOCKER-501`、`C12M6-CAT-006/007`、`C12M6-SCOPE-007/008` 已关闭；旧 `PARTSURF-BLOCK-005` / `PARTSURF-SCOPE-007` / `PARTSURF-FIX-005` 已由 C12-M6 evidence 关闭为 historical closed。

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 每步只关闭本步骤 owner 的矩阵行和 blocker；S2/S3/S4 三闸门全部通过前，不发布 `wire_wire_admitted_current_supported`。
3. 若发现 unrelated dirty work，不回退、不覆盖；只读取或暂停确认。
4. 不从 fixture 输出倒推业务逻辑；必须用 FreeCAD source、checked-in expected、current recompute 和 focused tests 共同裁决。
5. 本包默认只改 docs / matrices；只有 S5 明确分类为 `implementation_candidate_required` 后，才另开代码实现包。

## 顺序

- S0：live 基线与继承口径冻结。
- S1：FreeCAD 源码与旧 PARTSURF 证据复核。
- S2：collector / expected 准入验证。
- S3：request-local input schema 准入验证（已完成）。
- S4：shell/topo provenance 准入验证（已完成）。
- S5：implementation gate 与发布闸门（已完成，最终出口 `wire_wire_admitted_current_supported`）。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/矩阵/*.tsv
git diff --check
```
