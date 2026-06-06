# WireJoiner Bridge 彻底删除方案

## 1. 目标

删除 CAD Core WireJoiner 当前用于 generated open export 和 purge-as-original 的 helper bridge，让 open wire export ownership 主路径来自 FreeCAD WireJoiner 内部账本。

这里的“删除 WireJoiner bridge”不是删除 `WireJoiner` 本体，而是删除这些临时桥接职责：

- `openExportOverride`
- `purgeAsOriginalOpenEdge`
- `LegacyHelperShapeStillUsed`
- 以及继续新增 helper reason 来修补导出结果的做法

`helperOpenExportOverride*` 仍可作为诊断字段记录历史 helper reason 和 producer ledger 来源，但不得再承载旧 helper shape 输出主路径。

完成后，generated open export 和 source edge ownership 应来自：

- `EdgeInfo`
- `WireInfo` / `wireInfo2`
- `iteration` / `iteration2`
- `superEdge`
- `sourceEdgeArray`
- `aHistory`
- `myShapesToReturn`
- `openWireCompound`
- `MapperHistory(aHistory)`

## 2. 非目标

- 不新增 fixture-specific pruning。
- 不在 sketch executor、adapter 或结果导出层按几何类型猜 source ownership。
- 不把 helper bridge 改名后继续作为主路径。
- 不重写 WireJoiner 本体之外的 Feature / Sketch 语义来掩盖账本缺口。

## 3. FreeCAD 依据入口

开工前必须复核这些本地源码入口：

| FreeCAD 源码 | 需要提取的语义 |
| --- | --- |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()` | WireJoiner 主状态机、closed/open wire 构造和最终输出 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()` | split 后 source edge 到 fragment 的 history |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()` | closed wire 对 EdgeInfo / WireInfo ownership 的消耗 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()` | tight bound 查找与 wireInfo/wireInfo2 关系 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()` | ownership exhaust 生命周期 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()` | final open export gate 和 `noOriginal` 语义 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp` 中 `MapperHistory(aHistory)` | WireJoiner history 到 topo naming 的传播 |

现有 `docs/偏移处理/06-02-04-01-WireJoiner完整账本迁移方案.md` 是本方案的前置输入。若两个文档口径冲突，以 FreeCAD 源码复核后的账本迁移口径为准，并同步修正文档。

## 4. 当前 bridge 基线

当前 CAD Core bridge 主要服务两个已知原因：

| helper reason | 当前作用 | 正确替代来源 |
| --- | --- | --- |
| `partial_shared_closed_wire` | 在部分 source edge 已进入 closed wire 时，仍给 generated open export 提供临时 ownership | `WireInfo` / `wireInfo2` exhaust 生命周期、`myShapesToReturn` |
| `closed_wire_cycle` | closed wire 成环后，用 helper 标记 open export 或 purge 原始边 | `aHistory` generated/removed source map、`openWireCompound` child-wire ownership |

这些 helper 只能解释当前 bridge 为什么存在，不能作为后续继续扩展的理由。

## 4.1 本轮代码复核

已复核 FreeCAD 调用链：

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`：先 `splitEdges()`，再 `findSuperEdges()` / closed wire / tight bound / exhaust 账本，最终按 `info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)` 把 `info.wire()` 加入 `openWireCompound`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`：对 split fragment 写 `aHistory->AddModified(split.intersectShape, newInfo.edge)`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()`：消费 EdgeInfo / WireInfo owner，并通过 `aHistory->Remove(info.edge)` 记录被 closed wire 消耗的 source。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()` / `exhaustTightBound()`：使用 `wireInfo` / `wireInfo2`、`iteration2` 和 owner transfer 账本决定 shared edge 生命周期。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：`noOriginal=true` 时用 `sourceEdgeArray` 构造 source compound，再通过 `MapperHistory(aHistory)` 传播到 `TopoShape::makeShapeWithElementMap()`。
- FaceMaker 路径：`FaceMaker.cpp::postBuild()` 消费 `MapperHistory(myPreSplitHistory)`；`FaceMakerBuildFace.cpp::Build_Essence()` 通过 `myShapesToReturn` 返回 bounded faces，后续与 WireJoiner open wires 一起进入 Sketch InternalShape。

当前 CAD Core 代码状态：

- 已有正式账本：`cad-core/include/cad_core/part/wire_joiner.h` 和 `cad-core/src/part/wire_joiner.cpp` 已包含 `EdgeInfo`、`WireInfo`、`wireInfo2`、`iteration` / `iteration2`、`superEdge`、`openWireCompoundWires`、`WireJoinerOpenExportHistoryEntry`、`ResultWireProducerIdentity` 和 `resultWireProducerLedgerEntries`。
- source / `aHistory` 已证明的导出 shape 已拆到 `producerOpenExportShape`；`sourceEdges_` / `sourceEdgeLedgerEdges_` 已拆成原始 `sourceEdgeArray` 等价集合和 FreeCAD mutable `sourceEdges` 等价集合，`addSourceEdge()` 会用临时 `BRepBuilderAPI_MakeWire` 账本替换 source edge 重合顶点，split lineage 和 `source_vertex_replacement_*` evidence 消费 mutable ledger。旧 `openExportOverride` 字段已从代码主路径删除。
- 本轮继续删除主路径 bridge：`recordOpenWireCompoundLedger()` 的 child-wire 输出改为 producer 优先，并新增 `open_wire_compound_result_slot_vertex_evidence_wire_info_count` / `result_slot_vertex_evidence_output` 诊断固定最终是否仍消费 result-slot；P5 覆盖的 generated open export 计数为 0。`purgeAsOriginalOpenEdge` 字段已删除，noOriginal purge 候选由 `sourceEdgeArrayOriginalOpenEdgeCandidate()` 从 `!splitFromInputEdge`、`sourceVertexIdentity` 和非 helper producer 状态实时计算，再与 child-wire shared-source gate 共同决定是否跳过。
- 因此本轮 capability 改为 `generated_open_export_bridge.status=covered_main_path`、`purge_as_original_bridge.status=covered_main_path`。这不是 `covered_full`：`helperOpenExportOverride*` 和 `resultSlotVertexEvidenceEdge` 仍作为诊断 / producer ledger 解释字段保留。

## 4.2 本轮删除尝试结论

已验证不能只把 `getOpenWires()` 的最终 purge 改成 FreeCAD 表面谓词来删除 bridge。

FreeCAD 依据：

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：先 `result.makeElementFace(edges.getSubTopoShapes(TopAbs_WIRE), ..., "Part::FaceMakerBuildFace")`，再 `joiner.addShape(edges)` 和 `joiner.getOpenWires(openWires, "SKF")`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`：`sourceEdges.insert(sourceEdgeArray.begin(), sourceEdgeArray.end())`，split / closed-wire / tight-bound 账本运行后才按 `info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)` 生成 `openWireCompound`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：`noOriginal=true` 时先从 `sourceEdgeArray` 建 source compound，再对 `openWireCompound` 的 child wire 执行 shared-vertex purge，最后用 `MapperHistory(aHistory)` 调用 `makeShapeWithElementMap()`。

本轮验证：

- 试验性把 CAD Core `WireJoiner::getOpenWires()` 改为只在最终 `liveWires` 阶段按“每条 edge 至少共享一个 source vertex” purge，并移除 `childWire.purgeBridge` / `edgeInfo.purgeAsOriginalOpenEdge` 的主路径影响。
- `cmake --build build --target cad-core cad_core_ffi` 通过。
- `python3 -m unittest tests.test_p5_sketch tests.test_p6_topology` 出现 6 个 P5 open-cutter 失败：`through-open-cutter`、`branch-open-cutter`、`cross-cutters`、`segmented-cross-cutter`、`t-cutter` 的 internal edge / vertex 数量下降，`open_cutter_fragments` 的 split history entry 也从至少 2 个降到 1 个。

结论：

- 当前 CAD Core 的最终 child-wire 分组、open edge add() 顶点替换、source vertex identity 和 FreeCAD `WireJoinerP::add()` / `splitEdges()` 后的 `sourceEdgeArray` 关系仍未完全等价；直接在最终导出层套 FreeCAD purge 谓词会误删合法 split fragment。
- 本轮没有采用该错误方向，而是删除 `purgeAsOriginalOpenEdge` 独立字段，把同一判断收敛到 `sourceEdgeArrayOriginalOpenEdgeCandidate()`：只用 split/source identity ledger 先判断“是否仍是原始 source edge 候选”，再由 `openWireCompound` child-wire 的 shared-source gate 决定实际 noOriginal skip。
- 该路径保持 P5 open-cutter fixture 数量和 split history 稳定，同时避免继续扩展输出端 pruning。

## 4.3 观测字段基线

bridge 删除前保留这些 diagnostic-only evidence，用于验收 source vertex identity / replacement 是否已经具备删除 purge bridge 的前置条件：

- `open_export_history_entries[].source_vertex_identity`
- `open_export_history_entries[].source_vertex_identity_any`
- `open_export_history_entries[].source_vertex_identity_all`
- `open_export_history_entries[].source_vertex_replacement_source_edge_indices`
- `open_export_history_entries[].source_vertex_replacement_endpoints`
- `open_export_history_entries[].source_vertex_replacement_identity`
- `sketch_internal_history.wire_joiner_open_export_history_entries[]` 中同步保留同名字段

这些字段直接来自当前 `EdgeInfo::sourceVertexIdentity` 和 `EdgeInfo` 的 source vertex replacement evidence，用于固定每个 open export child edge 的 source vertex identity / replacement 证据。FreeCAD 依据是 `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::add()` 中 “Make sure coincident vertices are actually the same TopoDS_Vertex” 的临时 wire replacement 逻辑；CAD Core 落点是 `cad-core/include/cad_core/part/wire_joiner.h::EdgeInfo`、`WireJoinerOpenExportHistoryEntry`、`cad-core/src/part/wire_joiner.cpp::initializeEdgeInfo()`、`cad-core/src/sketcher/sketch_object.cpp` 和 `cad-core/src/part/topo_shape.cpp`。P5 回归已把 entry-level 计数与 ledger 级 `source_identity_open_export_*` 计数绑定，后续替换 `purgeAsOriginalOpenEdge` 时必须先证明 child-wire 分组和这些端点 replacement 与 FreeCAD `sourceEdgeArray` 等价。

## 4.4 本轮二次复核结论

2026-06-06 继续复核后确认，不能把 result-wire producer identity 完成直接等同为 `covered_full`，但本轮已把旧 bridge 主路径删除：

- `sketch-internal-face-through-open-cutter` 的 open export 已无 helper override，`openWireCompound` child ledger 只记录 split lineage 与 source vertex identity。
- `sketch-internal-face-t-cutter` / `cross-cutters` 的 helper entries 已全部带 `result_wire_producer_state=ExportedWithoutHelper`；本轮改为 producer 优先输出后，`open_wire_compound_result_slot_vertex_evidence_wire_info_count=0`，说明 result-slot 没有再作为最终 child-wire 输出形状。
- `sketch-internal-face-branch-open-cutter` 仍存在 unmatched `purge_bridge` 诊断：candidate 为 true，但 `sourceSharedVertexPurgeMatch=false`，当前不会删输出。`sketch-internal-face-dangling-line` 覆盖 matched purge candidate，说明 noOriginal 语义仍被保留；区别是该 candidate 不再来自 `purgeAsOriginalOpenEdge` 字段，而是由 source/split ledger 计算。

本轮没有继续叠加 purge 规则。旧 `openExportOverride` / `purgeAsOriginalOpenEdge` 字段删除后，后续工作只剩诊断命名和更完整的 FreeCAD `myShapesToReturn` / child-wire identity 覆盖；不得再把 helper reason 当作输出修补入口。

## 4.5 producer export shape 字段拆分

本轮已删除 `openExportOverride` 字段，但没有删除 bridge：

- 新增 `EdgeInfo::producerOpenExportShape`：只承载已经由 source lineage / `aHistory->Remove(info.edge)` / same-lineage sidecar 证明的 producer-owned export shape。
- 新增 `EdgeInfo::sourceVertexReplacement*` evidence：记录每个 open export endpoint 对应的 `sourceEdgeArray` edge/endpoint，以及是否已是同一 `TopoDS_Vertex`；该证据只进入 runtime/topo history JSON，不参与 `openWireCompound` 或 `getOpenWires(noOriginal)` 主路径。
- 新增 `WireJoiner::sourceEdgeLedgerEdges_`：`sourceEdges_` 保持 FreeCAD `sourceEdgeArray` 等价用途，继续作为 `getOpenWires(noOriginal)` 的原始 source purge 依据；`sourceEdgeLedgerEdges_` 对齐 FreeCAD `WireJoinerP::build()` 中先从 `sourceEdgeArray` 填充、再被 `WireJoinerP::add()` 更新的内部 `sourceEdges`。`buildFinalEdgeOwnership()` 用它作为 split lineage source，`initializeEdgeInfo()` 仍用原始 `sourceEdges_` 计算 purge-facing `sourceVertexIdentity`，只用 mutable ledger 计算 `source_vertex_replacement_*` evidence。
- 新增 `EdgeInfo::hasOpenExportShape()`：open export gate、`openWireCompound` child ledger、ledger summary 和 `getOpenWires()` 在判断“是否存在替代导出 shape”时使用它；当前包含 `producerOpenExportShape` 或作为诊断定位的 `resultSlotVertexEvidenceEdge`。最终 child-wire 输出走 producer 优先，P5 断言 `open_wire_compound_result_slot_vertex_evidence_wire_info_count=0`。
- `applyHelperOpenExportOverridePlan()` 中 `useSourceEdgeExportShape` 与 same-lineage sidecar source shape 已写入 `producerOpenExportShape`；root/current-member result-slot seed 不再写入旧 `openExportOverride`，而是只通过 `resultSlotVertexEvidenceEdge` 保留 request-local vertex evidence。它尚未变成真实 `openWireCompound` child producer。
- 试把 open wire 入账 edge 也按 mutable ledger canonicalize 时，`sketch-internal-face-branch-open-cutter` 的 internal edge / vertex 数量从 FreeCAD expected 的 10 / 11 降为 8 / 8，说明 open edge vmap replacement 不能只靠 source ledger 局部替换，否则会提前触发 noOriginal purge；该尝试已撤回。
- 试删 `resultSlotVertexEvidenceEdge` 后，P5 cross / segmented-cross / T cutter 和 three-overlap closed-cycle fixture 的 helper ledger / producer identity 计数下降，说明该 field 仍需作为诊断定位证据保留，不能直接删除。
- 已验证 `cmake --build build`、`python3 -m unittest tests.test_p5_sketch` 通过；阶段收口仍建议跑 `tests.test_p6_topology`。

因此当前 bridge 口径已更新：`generated_open_export_bridge.status=covered_main_path`，`purge_as_original_bridge.status=covered_main_path`。保留条件是：`helperOpenExportOverride*` / `resultSlotVertexEvidenceEdge` 只能作为诊断或 producer ledger 解释字段，`result_slot_vertex_evidence_output` 必须持续为 false。

## 5. 目标账本映射

| FreeCAD 账本/字段 | CAD Core 目标职责 |
| --- | --- |
| `sourceEdgeArray` | 保存原始 source edge identity，支持 noOriginal / purge 判断 |
| `aHistory` | 保存 generated、modified、removed source edge 到 fragment/result 的历史 |
| `myShapesToReturn` | 保存最终需要返回的 wire/edge result identity |
| `openWireCompound` | 表达 open wire child ownership，而不是靠导出层猜测 |
| `EdgeInfo::wireInfo` | primary owner 和 closed/open 消耗状态 |
| `EdgeInfo::wireInfo2` | secondary owner 和 shared edge lifecycle |
| `EdgeInfo::iteration` / `iteration2` | final export gate 和 ownership exhaust 判断 |
| `EdgeInfo::superEdge` | split fragment 到源边/合并边的关系 |
| `MapperHistory(aHistory)` | 把 WireJoiner history 传播到 NamedShape / ElementMap |

最终 open export 判断必须从上述账本自然推出。旧反思文档中的关键约束仍然有效：输出端 pruning 不是主路径，final open export 应受 FreeCAD `EdgeInfo` 状态机约束，而不是靠结果形状后处理。

## 6. 实施切片

### A. Bridge inventory 和诊断冻结

- 统计当前 `generated_open_export_bridge`、`purge_as_original_bridge`、helper reason、helper fields 的出现位置。
- 增加诊断只用于观测，不新增 helper 决策。
- 明确本轮不允许新增 helper reason。

验收：当前 bridge 使用点和剩余原因可被 `rg` 查清；没有新增 fixture-specific reason。

### B. 完整 source / split history

- 补齐 `sourceEdgeArray` 到 split fragment 的映射。
- 补齐 `aHistory` 的 generated / modified / removed 关系。
- 对 splitter 失败、source edge 未 split、source edge 一对多 fragment 保持结构化 history。

验收：source edge 到 fragment/result 的映射不依赖导出层猜测。

### C. myShapesToReturn 和 openWireCompound ownership

- 迁移 `myShapesToReturn` 等价结构。
- 让 open wire child ownership 从 `openWireCompound` / result-wire identity 进入 CAD Core。
- 删除用 helper 标记“这个 generated edge 应当导出”的主路径。

验收：generated open export 可以从 result-wire identity 推导。

### D. EdgeInfo / WireInfo exhaust 生命周期

- 完整实现 `wireInfo` / `wireInfo2` 的 primary / secondary owner 消耗。
- 实现 `iteration` / `iteration2` 对 final open export gate 的约束。
- 实现 shared edge 被 closed wire 消耗后的状态迁移。

验收：`partial_shared_closed_wire` 不再需要 helper reason。

### E. purge-as-original 替换

- 用 `sourceEdgeArray`、`aHistory` 和 `openWireCompound` child ownership 判断原始边是否应保留。
- 删除 `purgeAsOriginalOpenEdge` 主路径。
- 如果必须保留诊断字段，只能标记“这个旧 bridge 曾经会 purge”，不能影响导出结果。

验收：`closed_wire_cycle` 不再需要 helper purge marker。

### F. MapperHistory 到 ElementMap

- 把 WireJoiner `aHistory` 消费到 `MapperHistory` 等价结构。
- 让 NamedShape / ElementMap 从 history 获取 InternalEdge / InternalVertex source trace。
- 不在 sketch executor 中补 source edge 猜测。

验收：如果几何已经一致，stable subname 和 internal element 差异必须归类到 history propagation，而不是输出端修正。

### G. 删除 bridge fields

- 删除或降级 `helperOpenExportOverride`。
- 删除 `openExportOverride` 主路径。
- 删除 `purgeAsOriginalOpenEdge`。
- 删除 `LegacyHelperShapeStillUsed` 对 capability 的有效影响。
- 更新 CADCore3 gap 表：bridge 不再作为 accepted implementation。

验收：`rg` 只能在文档、历史说明或 diagnostic-only 路径中看到旧字段名，不能在 open export ownership 主路径中看到。

## 7. 验收矩阵

必须覆盖的 case：

| Case | 期望 |
| --- | --- |
| self-intersection split | fragment history 完整，open export 不靠 helper |
| inter-edge intersection split | source 一对多 fragment 可追溯 |
| bounded faces + open wires | closed wire 消耗和 open wire 导出同时正确 |
| partial shared closed wire | 不再生成 `partial_shared_closed_wire` helper reason |
| closed wire cycle | 不再生成 `closed_wire_cycle` purge helper |
| splitter failure | 保留原 edge 语义，输出 diagnostic，不靠 fixture fallback |
| MapperHistory propagation | InternalEdge / InternalVertex source trace 稳定 |
| naming order difference | 几何等价且顺序稳定时归类为命名顺序差异，不算硬失败 |

阶段回归命令：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch
python3 -m unittest tests.test_p6_topology
```

若修改会影响 CADCore3 capability / adapters，再补：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_p8_features
```

代码改动后的轻量检查：

```bash
git diff --check -- cad-core docs/CADCore3.0 docs/偏移处理
```

## 8. 完成条件

- generated open export ownership 来自真实 WireJoiner 账本。
- purge-as-original 不再由 helper marker 驱动。
- helper fields 不再参与主路径决策。
- `generated_open_export_bridge.status` 和 `purge_as_original_bridge.status` 不再作为 covered 口径存在。
- P5/P6 topology 和 sketch fixture 通过，剩余差异只允许是已记录的命名顺序差异或 unsupported case。
