# 【已实现】P5P6-S5 FaceMaker / WireJoiner history producer 专项复审

## 目标

P5P6-S5 单独裁决 FaceMaker / WireJoiner 的 history producer 主路径和 fallback 删除边界。它只做文档与矩阵复审，不写 C++、不采 FreeCAD oracle、不改 fixture expected，也不把 summary-only 或 geometry-match fallback 伪装成发布完成。

## live 基线复核

| 项 | 复核结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `97205127d3` |
| `git log -1 --oneline` | `97205127d3 docs: 完成 P5P6 S4 生命周期复审` |
| 初始非本步 dirty | `AGENTS.md`、`DESIGN.md`、`cad-core/CMakeLists.txt` 已在本步骤开始时存在 dirty；S5 不编辑、不暂存、不提交这些文件。 |

## FreeCAD 依据

| 源码入口 | S5 使用的语义 |
| --- | --- |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::Build()` / `postBuild()` | `Build()` 先重置 `myPreSplitHistory`、执行 `Build_Essence()`、收集 `myShapesToReturn`，再调用 `postBuild()`；`postBuild()` 先 `mapSubElement(mySourceShapes)`，再用 `MapperHistory(myPreSplitHistory)` 和 `MapperMaker(mySplitter)` 生成可命名的中间结果，最后按外环 edge history 给 face 命名。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::splitSelfIntersecting()` | 自交 edge 会被拆成 fragments，并通过 `myPreSplitHistory->AddModified(edge, fragment)` 记录 original-to-fragment；该账本必须进入 producer history，不能只保留 stage/count summary。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::splitAtIntersections()` / `Build_Essence()` | `mySplitter` 对 inter-edge intersection 做 split，`BOPAlgo_BuilderFace` 产出 bounded areas 并填充 `myShapesToReturn`；splitter generated/modified/deleted 关系属于 FaceMaker producer，不属于 sketch executor 猜测。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::EdgeInfo` / `WireInfo` | `EdgeInfo` 保存 `superEdge`、`iteration`、`iteration2`、`wireInfo`、`wireInfo2`；`WireInfo` 保存 ordered vertices、wire、face、done/purge。这些是 open wire ownership 的内部账本。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::splitEdges()` / `buildClosedWire()` | split fragment 通过 `aHistory->AddModified(...)` 记录，closed/tight-bound 消费通过 `aHistory->Remove(...)` 记录；history producer 必须从账本和 `aHistory` 来。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::build()` / `getOpenWires()` | `build()` 只把最终 `iteration == -3 || (!wireInfo && iteration >= 0)` 的 open child 加进 `openWireCompound`；`getOpenWires(noOriginal=true)` 先过滤全部匹配 source 的原始 child wire，再调用 `makeShapeWithElementMap(comp, MapperHistory(aHistory), sourceEdges, op)`。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.h` | `WireJoiner` 公开 `Modified()`、`Generated()`、`IsDeleted()` 都委托 `aHistory`；cad-core producer 与 topo 消费必须保持同一 history 语义。 |

## cad-core live evidence

| evidence | S5 结论边界 |
| --- | --- |
| `cad-core/src/sketcher/sketch_object.cpp` | Sketch consumer 已把 FaceMaker / WireJoiner summary 和 event ledger 放进 `Sketch.InternalShape` history context，但注释明确 topo 不得从 raw/internal geometry 推断 WireJoiner split/generated/deleted。 |
| `cad-core/src/app/element_map.cpp::internalElementMapForSketch()` | 当前仍用 raw/internal vertex、edge 几何匹配生成基础 map；它只能覆盖简单同一几何，不能替代 FaceMaker/WireJoiner concrete producer。 |
| `cad-core/src/part/topo_shape.cpp` | `facemaker_history:summary_only` 被明确标为 diagnostic only；`wire_joiner_current_member_vertex_multiplicity_blocked` 被明确标为 terminal diagnostic，不改写 openWireCompound child topology。 |
| `cad-core/include/cad_core/part/wire_joiner.h` / `cad-core/src/part/wire_joiner.cpp` | WireJoiner 已记录 child-wire event ledger、openWireCompound ownership、vmap replacement 和 current-member split ledger，但 `currentMemberSplitLedgerVertexCandidate` 仍等待 ElementMap / MapperHistory vertex multiplicity parity。 |
| `cad-core/tests/test_p5_sketch.py` / `cad-core/tests/test_adapters.py` | 已覆盖 `wire_joiner_history:element_map`、`summary_only` diagnostic、fallback removal facts 和 current-member blocker diagnostic；这些是 S5 裁决证据，不等于 S6 已关闭 producer gaps。 |

## S5 裁决

| 范围 | S5 状态 | 裁决 |
| --- | --- | --- |
| `P5P6-SCOPE-009` FaceMaker concrete history producer | `backendGap` | 保持并细化为 FaceMaker concrete history producer backendGap。`facemaker_history:summary_only` 只能作为 diagnostic，不能合成 terminal split/deleted/generated history，不能从 `internalElementMapForSketch()` 缺口或 raw/internal 几何采样反推 source-to-target。 |
| `P5P6-SCOPE-010` WireJoiner current-member vertex multiplicity / openWireCompound child-wire parity | `backendGap` | 保持并细化为 WireJoiner current-member vertex multiplicity 与 openWireCompound child-wire parity backendGap。已有 child-wire event ledger 可作为 producer evidence，但 current-member one-to-many vertex multiplicity 不唯一时必须保持 `wire_joiner_current_member_vertex_multiplicity_blocked` stable diagnostic。 |
| `P5P6-SCOPE-011` Sketch InternalShape main path | `releaseGate` | 保持 releaseGate。Sketch consumer 已接收 producer context，但完整切换必须等 S6 关闭 FaceMaker/WireJoiner producer gaps 后审计；不得提前宣布 fallback retired。 |
| `P5P6-SCOPE-012` `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` | `unsupported` | 保持 S4 unsupported diagnostic。除非 S6 同时给出 FreeCAD evidence 和 ElementMap-backed InternalShape stable selector support，否则不得升级。 |
| `P5P6-SCOPE-013` fallback deletion and fixture-specific rule audit | `releaseGate` | 保持 releaseGate。S5 只裁决删除边界：summary-only、geometry-match internal map、diagnostic-only branch、source/split identity fallback 都不能成为发布主路径；是否删除或临时保留等 S6 实现后审计。 |

## S6 可执行 blocker

| blocker | FreeCAD source 账本 | cad-core 缺口 | 允许的诊断边界 | 关闭条件 |
| --- | --- | --- | --- | --- |
| `P5P6-BLOCK-003` FaceMaker concrete producer | `myPreSplitHistory`、`myPreSplitCompound`、`mySplitter`、`myShapesToReturn`、`FaceMaker::postBuild()` 的 `MapperHistory` / `MapperMaker` 链 | stage/count summary 还不能为所有 source edge 产出 concrete source-to-target events；`internalElementMapForSketch()` 仍是几何匹配基础 map | ambiguous / missing concrete producer 时只能输出 `facemaker_history:summary_only` 或 producer-missing diagnostic，不生成 terminal history | pre-split / splitter 可喂入 `MapperHistoryEvent` / `ElementMap`；可恢复项有唯一 source-to-target，歧义项只有稳定 diagnostic；不再靠 summary-only 关闭可恢复 case |
| `P5P6-BLOCK-004` WireJoiner vertex multiplicity / child-wire parity | `EdgeInfo`、`WireInfo`、`wireInfo2`、`iteration2`、`superEdge`、`openWireCompound`、`aHistory`、`getOpenWires(noOriginal)` | current-member split ledger 的 vertex multiplicity 仍不能证明所有 recoverable child-wire endpoint；openWireCompound child-wire parity 仍需和 `MapperHistory(aHistory)` 完整对齐 | one-to-many 或 vertex multiplicity 不唯一时保持 `wire_joiner_current_member_vertex_multiplicity_blocked`，不得 output pruning 或按 source index / split order 选目标 | recoverable openWireCompound child wire 由 producer ledger + `aHistory` 唯一映射到 ElementMap；不可恢复一对多保留 stable diagnostic，且无可恢复 case 被 blocker gate 住 |
| `P5P6-BLOCK-006` fallback retirement audit | FaceMaker / WireJoiner producer history 必须先于 Sketch InternalShape consumer 和 resolver | `facemaker_summary_only`、geometry-match `internalElementMapForSketch()`、diagnostic-only candidate、source/split identity fallback 仍需发布前审计 | 临时 fallback 只能带 FreeCAD 依据、适用边界、删除条件和 diagnostic；不得继续叠加 fixture 名、几何类型、source index、split 顺序、输出排序、面积/长度猜测 | S6 实现后搜索并删除或正式标注所有 fallback；无 unexplained summary-only / geometry-match / fixture-shaped fallback 作为主路径 |

## 矩阵回写

- `p5p6_scope_review_matrix.tsv`：`P5P6-SCOPE-009/010` 保持 `backendGap` 并细化 producer / diagnostic 边界；`P5P6-SCOPE-011/013` 保持 `releaseGate`；`P5P6-SCOPE-012` 保持 `unsupported`。
- `p5p6_backend_gap_classification.tsv`：仍只保留 FaceMaker concrete producer 和 WireJoiner vertex multiplicity / child-wire parity 两条 P0 backendGap；summary-only 不得代替 producer。
- `p5p6_blocker_queue.tsv`：`P5P6-BLOCK-003/004/006` 作为 S6 可执行项，写清 FreeCAD source 账本、cad-core 缺口、允许诊断边界和关闭条件。
- 主线入口和步骤总览只把 S5 标为已实现；S6 仍是待执行发布闸门。

## 验收

```bash
rg -n "FaceMaker|postBuild|myPreSplitHistory|mySplitter|myShapesToReturn|WireJoiner|EdgeInfo|WireInfo|openWireCompound|iteration2|superEdge|aHistory|noOriginal|getOpenWires" src/Mod/Part/App/FaceMaker.cpp src/Mod/Part/App/FaceMakerBuildFace.cpp src/Mod/Part/App/WireJoiner.cpp src/Mod/Part/App/WireJoiner.h
rg -n "facemaker_history|summary_only|wire_joiner_current_member_vertex_multiplicity_blocked|internalElementMapForSketch|fallback|InternalShape" cad-core/src cad-core/include cad-core/tests
python3 - <<'PY'
import csv
from pathlib import Path
root = Path('docs/FreeCAD几何生态迁移工程-细分/P5P6-ExternalGeometry-TopoNaming联合主线/矩阵')
with (root / 'p5p6_scope_review_matrix.tsv').open(newline='') as f:
    scope = {row['scope_id']: row for row in csv.DictReader(f, delimiter='\t')}
expected = {
    'P5P6-SCOPE-009': 'backendGap',
    'P5P6-SCOPE-010': 'backendGap',
    'P5P6-SCOPE-011': 'releaseGate',
    'P5P6-SCOPE-013': 'releaseGate',
}
for sid, status in expected.items():
    assert scope[sid]['current_status'] == status, (sid, scope[sid]['current_status'])
assert scope['P5P6-SCOPE-012']['current_status'] == 'unsupported'
with (root / 'p5p6_blocker_queue.tsv').open(newline='') as f:
    blockers = {row['blocker_id']: row for row in csv.DictReader(f, delimiter='\t')}
for bid in ['P5P6-BLOCK-003', 'P5P6-BLOCK-004', 'P5P6-BLOCK-006']:
    row = blockers[bid]
    assert row['scope_id'] in scope, row
    assert row['next_step'] and row['close_condition'], row
PY
git diff --check
```

## 非目标

- 不写 C++。
- 不采 FreeCAD oracle。
- 不改 fixture expected。
- 不扩大为完整 Sketcher solver。
- 不在 adapter 或导出层做 output pruning。
- 不让 summary 永久替代 producer。
- 不新增按 fixture 名、几何类型、source index、split 顺序、输出排序、面积或长度猜测的实现路线。
