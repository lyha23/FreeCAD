# 【已实现】C12-M16 S1 FreeCAD split history 与 current reselect 复核

## 目标

复核 FreeCAD split history source authority 与 cad-core current split/reselect 行为，确认 C12-M16 的 red path 和 C++ 落点。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`c59b84e43f`。
- `git log -1 --oneline`：`c59b84e43f docs: 关闭 C12-M16 S0 live 基线`。
- `git -c core.quotepath=false status --short -uall`：无输出。

## 必读文件

- `../README.md`
- `../7-4-19-52-C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次方案.md`
- `../矩阵/c12m16_split_fragment_identity_source_matrix.tsv`
- `../矩阵/c12m16_split_fragment_identity_scope_matrix.tsv`
- `src/Mod/Part/App/FaceMakerBuildFace.cpp`
- `src/Mod/Part/App/FaceMaker.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Sketcher/App/SketchObject.cpp`
- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`
- `cad-core/src/sketcher/sketch_internal_result.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/tests/test_p5_sketch.py`

## FreeCAD source authority

### FaceMakerBuildFace pre-split

- `src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::splitSelfIntersecting()` 先在单条自相交 edge 上按参数拆 fragments，再对每个 fragment 执行 `myPreSplitHistory->AddModified(edge, fi.Value())`。
- 同函数在 pre-split 发生后把 result 写入 `myPreSplitCompound`，为后续 `postBuild()` 的中间 ElementMap 准备 shape。
- `Build_Essence()` 的顺序是 `splitSelfIntersecting(edges, plane)`，再 `splitAtIntersections(edges)`；因此 self-intersection history 与 splitter history 是两段链，不能在输出端按 fragment 顺序补猜。

### FaceMaker postBuild chaining

- `src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()` 先 `myTopoShape.mapSubElement(mySourceShapes)`。
- 若 `myPreSplitHistory` 存在，先用 `MapperHistory(myPreSplitHistory)` 和 `myPreSplitCompound` 构造 `preSplitShape`，并把它作为 splitter 的 source。
- 若 `mySplitter.IsDone()`，再用 `MapperMaker(mySplitter)` 对 `mySplitter.Shape()` 调 `makeShapeWithElementMap()`，最后 `myTopoShape.mapSubElement(splitInputShape)`。
- 结论：FreeCAD 的正确语义是 history -> ElementMap -> mapped name 传播；C12-M16 不能只在 `edgeSegments[]` 或 `subshapes[]` 后处理里生成 `g<ID>:splitN`。

### WireJoiner split bookkeeping

- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::EdgeInfo` 保存 `edge`、`superEdge`、`wireInfo` / `wireInfo2`、`iteration` / `iteration2` 等闭合 wire 状态。
- `sourceEdgeArray` / `sourceEdges` 是 WireJoiner 的输入来源集合；`build()` 先把 source edge 加进内部 edges，再按 `doTightBound || doSplitEdge` 执行 `splitEdges()`。
- `splitEdges()` 里新 fragment 进入 edges 后执行 `aHistory->AddModified(split.intersectShape, newInfo.edge)`；`removedEdge -> newInfo.edge` 记录在当前源码里是注释掉的。
- `getOpenWires()` / `getResultWires()` 都通过 `shape.makeShapeWithElementMap(..., MapperHistory(aHistory), {sourceEdges...})` 发布历史。
- 结论：WireJoiner 的第一轮实现要消费已有 history 和 source sidecar，不能假设所有 new fragment 都能直接从 removed source edge 反查；无法唯一归属时应继续 diagnostic。

### Sketch internal alias

- `src/Mod/Sketcher/App/SketchObject.cpp::buildInternals()` 先用 `Part::FaceMakerBuildFace` 生成 internal faces，再用 `WireJoiner` 追加 open wires。
- `getInternalElementMap()` 只对 `InternalVertex` / `InternalEdge` 与 raw `Vertex` / `Edge` 做 `findSubShapesWithSharedVertex(... CheckGeometry | SingleResult)` 双向映射。
- `convertSubName()` 对 internal name 依赖 `InternalShape.getShape().getMappedName()`。
- 结论：`InternalEdgeN` 可在 shared-vertex 单结果或 internal mapped name 有证据时继承 source-backed identity；`InternalFaceN` 的 stable fragment identity 仍必须来自 FaceMaker / WireJoiner history，而不是 raw sketch executor 猜测。

### Mapper authority

- `src/Mod/Part/App/TopoShapeExpansion.cpp::MapperMaker::modified/generated()` 包装 OCCT maker 的 `Modified()` / `Generated()`。
- `MapperHistory::modified/generated()` 包装 `BRepTools_History::Modified()` / `Generated()`。
- `TopoShape::makeElementShape()` 和 FaceMaker/WireJoiner 都把 mapper 交给 `makeShapeWithElementMap()`。
- 结论：C12-M16 的 C++ seam 应在 sketch/history/topo 账本层形成 fragment ledger，再由 runtime response/reference 透传。

## cad-core current behavior

- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h` 当前只有 `RawSketchEdgeIdentity { indexed, source }` 和 `RawSketchEdgeIdentityLedger { edges, stableCount, fallbackCount }`，没有 fragment token、source-to-many 列表或 fragment recovery 字段。
- `buildRawSketchEdgeIdentityLedger()` 只用 `rawEdge.IsSame(sourceEdges[index])` 和可选 source-order fallback 找一个 source；`rawSketchEdgeIdentityObject()` 的 `byStableSubname` 只写普通 `g<ID> -> EdgeN`。
- `publishRawSketchEdgeIdentity()` 会把 `sourceStableSubname`、`stableSubname`、`sourceGeometryId`、`identityStatus` 写到 mesh/subshapes；`recompute.cpp` response publisher 也会透传这些字段。也就是说 response seam 已可承载字段，但上游没有 durable split ledger 产出。
- `reference_resolution.cpp::sourceStableSubnameForReference()` 只接受 `g` 后全数字的 raw sketch stable name；`g305:split1` 不会进入 raw edge identity 解析路径。
- `rawSketchSourceIdentityForStable()` 只查 `raw_edge_identity.byStableSubname[stableSubname]`，因此当前无法把 `g<ID>:splitN` 解析到 current fragment。
- `tests/test_p5_sketch.py` 已有 `g305:split1` 用于 internal face `StableSubList` / `ShadowSub` / `ReferenceShadow` 刷新与恢复，也已有 ReferenceShadow BREP split 场景返回 `subname_split_requires_reselect`。
- 同测试文件中的 open-wire raw identity 仍是普通 `g<ID> -> EdgeN`，例如 `g102`、`g100001`、`g100005`；这不是 split fragment durable ledger。

## diagnostic 与 ledger 缺口

已有 diagnostic / recovery：

- `split_stable_subname`：ExternalGeometry stable subname 命中当前 ElementMap split 状态时报告。
- `subname_split_requires_reselect`：ReferenceShadow/BREP recovery 发现旧 subshape split 成多个候选时报告。
- `subname_deleted`、`subname_resolve_ambiguous`、`deleted_stable_subname`：删除、歧义和 raw stable 缺失都有现成诊断路径。
- `g305:split1` 当前可作为 internal face 的 stable name 通过 `ShadowSub` / `ReferenceShadow` 恢复，但它不是 raw edge split fragment ledger 的通用产物。

缺少 durable fragment ledger 的 split 场景：

- 一个 source geometry id 对应多个 current raw/internal edge fragment 时，`raw_edge_identity.byStableSubname` 无法发布 `g<ID>:split1..N`。
- `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity.byIndexed/byStableSubname` 尚未共享同一个 source->fragment 账本。
- `StableSubList=["g<ID>:splitN"]` 不能通过 raw sketch identity 直接解析到 current fragment；当前只能依赖 ShadowSub、ReferenceShadow 或 split diagnostic。
- fragment count drift、old split token missing、geometry kind drift、missing source id fallback 还没有以 fragment ledger 为中心的 red/green 覆盖。

## S2 red path

- 用一个 source geometry id 产生多个 current fragments，断言 response 发布 `g<ID>:split1..N`，且 `edgeSegments[]`、`subshapes[]`、`raw_edge_identity.byIndexed`、`raw_edge_identity.byStableSubname` 一致。
- 覆盖旧 `StableSubList=["g<ID>:split1"]` 能解析到当前 fragment；缺失、count drift 或 kind drift 时继续输出 explicit diagnostic，不按输出顺序重绑。
- internal alias 只在 FaceMaker/WireJoiner history 或 `getInternalElementMap()` 可证明时继承 fragment identity。
- 无 source geometry id 或无法唯一归属的 split fragment 不发布 durable token，只保留 fallback/diagnostic。

## 本步改动边界

- 更新 S1 正文，记录 FreeCAD split history/source authority 与 cad-core current reselect 缺口。
- 更新 source / scope / blocker / validation 矩阵。
- 未修改 C++、fixtures、expected、tests、adapters。
- 未运行 build、FreeCADCmd 或 oracle collector。

## 关闭条件

- FreeCAD split history source authority 已写入 source matrix。
- current reselect / missing ledger 行为已写入 scope matrix。
- S2 red tests 可直接从 S1 证据落地。
- 本步骤已重命名为 `【已实现】`，队列下一项进入 S2。

## 非目标

- 不修改 C++。
- 不新增 fixture。
- 不运行重型 build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'myPreSplitHistory|mySplitter|MapperHistory|MapperMaker|splitSelfIntersecting|getInternalElementMap|getMappedName' src/Mod/Part/App/FaceMakerBuildFace.cpp src/Mod/Part/App/FaceMaker.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Sketcher/App/SketchObject.cpp
rg -n 'g305:split|subname_split_requires_reselect|raw_edge_identity|byStableSubname|ReferenceShadow|split' cad-core/src/sketcher cad-core/src/runtime cad-core/tests/test_p5_sketch.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```
