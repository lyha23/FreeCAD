# C12-M6 S5 implementation gate 与发布闸门

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
