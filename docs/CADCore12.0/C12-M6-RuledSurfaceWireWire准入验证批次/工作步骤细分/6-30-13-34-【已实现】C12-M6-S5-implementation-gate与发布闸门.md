# C12-M6 S5 implementation gate 与发布闸门【已实现】

## 目标

汇总 S0-S4，裁决 C12-M6 最终出口：关闭为 current-supported、只修文档发布、保留 validation blocker，或另开 implementation candidate。

## 输入条件

- S2 collector / expected 已裁决。
- S3 input schema 已裁决。
- S4 shell/topo provenance 已裁决。
- focused tests 和 capability snapshot 已记录。
- 旧 C3M4 `PARTSURF-BLOCK-005` 与 current capability 的冲突已分类。

## 出口

| 出口 | 条件 | 动作 |
| --- | --- | --- |
| `wire_wire_admitted_current_supported` | S2/S3/S4 全部通过，focused tests 通过，capability wording 准确 | 关闭 C12-M6 队列；可更新旧 PARTSURF docs 为历史已关闭；不改 C++。 |
| `publication_repair_required` | code/test/expected 成立，但 docs/capability/old matrix wording 不一致 | 只修发布口径和矩阵；不改几何实现。 |
| `retained_validation_blocker` | collector、schema 或 provenance 任一缺失 | 保留 blocker，写明缺口和重开条件；不实现。 |
| `implementation_candidate_required` | checked-in expected 与 current output 出现真实 mismatch，并定位到 Part executor / TopoShapeExpansion / topo provenance | 另开 implementation 包；本包不直接写代码。 |

## 必做验证

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次 docs/CADCore12.0/README.md
git diff --check
```

Focused code validation only when S2-S4 touched or challenged code/expected:

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```

## S5 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=abb595fdbb`。
- `git log -1 --oneline=abb595fdbb docs: 完成 C12-M6 S4 provenance 准入验证`。
- `git -c core.quotepath=false status --short -uall` 无输出，S5 起点为 clean。

## 三闸门复核

- S2 collector / expected 已关闭为 `collector_expected_admitted`：`c4m1/part-ruled-surface-wire-wire` 使用 request-local `Part::RegularPolygon` wire producers + `Part::RuledSurface`，checked-in expected 为 FreeCADCmd oracle，focused expected-backed test 已通过。
- S3 input schema 已关闭为 `input_schema_admitted`：输入只依赖 `DocumentObject graph`、`App::PropertyLinkSub` 和 `recompute.objs`；无 BREP、TopoDS、persistent `NamedShape` / `ElementMap`、mesh、cache 或 adapter shortcut。
- S4 shell/topo provenance 已关闭为 `provenance_admitted_current_supported_candidate`：current implementation 走 `BRepFill::Shell(Wire, Wire)`，写入 `part_ruled_surface:wire_wire_brepfill_shell`，并有 Lower/Upper source edge provenance；adapter 未补猜 ownership。

## 最终裁决

最终出口：`wire_wire_admitted_current_supported`。

理由：

- `supported_wire_wire_expected_backed` capability wording 与 S2/S3/S4 证据一致。
- focused wire/wire test 与 C API capability publication smoke 均通过，无 checked-in expected / current output mismatch。
- 不创建 implementation 包，不改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source。
- 旧 C3M4 `PARTSURF-BLOCK-005` / `PARTSURF-SCOPE-007` / `PARTSURF-FIX-005` 关闭为 historical closed / superseded by C12-M6 evidence，不再阻塞 current RuledSurface wire/wire。
- 本裁决只覆盖 `Part::RuledSurface` wire/wire；不发布 ProjectOnSurface、full Part surface family、SubShapeBinder CopyOnChange 或其它 surface family scope 为 supported。

## 输出

- `C12M6-BLOCKER-501` 关闭为 `closed_s5_admitted_current_supported`。
- `C12M6-CAT-006` 关闭为 `wire_wire_admitted_current_supported`。
- `C12M6-CAT-007` 关闭为 `implementation_candidate_not_required`。
- `C12M6-SCOPE-007` 关闭为 `closed_s5_capability_publication_admitted`。
- `C12M6-SCOPE-008` 关闭为 `closed_s5_no_implementation_candidate`。
- C12-M6 队列关闭，预期 `step_goal_queue.py` 只输出表头。

## 验证结果

- `step_goal_queue.py .../工作步骤细分 --format markdown`：只剩表头。
- C12-M6 与旧 C3M4 TSV 字段数检查：通过。
- `rg -n '[ \t]$' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次 docs/CADCore12.0/README.md`：无输出。
- `git diff --check`：通过。
- `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance`：通过。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke`：通过。
