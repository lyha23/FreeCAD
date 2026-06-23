# WireJoiner Diagnostic Ledger 深模块重构方案

## 来源与结论

本方案来自 `/var/folders/5k/fms98vy54k18w9n0j5_53r400000gn/T/architecture-review-20260623-120740.html` 中的 Strong 候选项 `Hide WireJoiner's diagnostic ledger behind depth`。

当前排序判断：

- `Move the capability contract out of the adapter` 已实现，并已重命名为 `6-23-12-14-【已实现】CapabilityContract深模块重构方案.md`。
- `Collapse reference recovery into one deep module` 在本仓库已有 `6-23-11-37-【已实现】ReferenceResolution深模块重构方案.md`。
- 因此下一套方案应处理 HTML 中剩余的 Strong 候选：`Hide WireJoiner's diagnostic ledger behind depth`。

当前结论：`WireJoiner` 本身已经是有 depth 的 Part-layer state machine，但它的 public interface 暴露了太多 ledger、blocker、producer state 和 history detail。下一轮重构应把这些 diagnostic internals 收回 WireJoiner module 内部，让 Sketcher / topo / tests 消费稳定的 open-wire result、NamedShape history evidence 和 focused diagnostics，而不是继续依赖 ledger anatomy。

## 当前基线

当前 `cad-core/include/cad_core/part/wire_joiner.h` 已经公开了大量内部账本类型：

- `ResultWireProducerKind`
- `ResultWireProducerState`
- `ResultWireBlocker`
- `OpenWireCompoundExportSource`
- `WireJoinerHistoryEvent`
- `WireJoinerVmapReplacementEvent`
- `WireJoinerEndpointIdentityDebt`
- `ResultWireProducerIdentity`
- `ResultWireProducerLedgerEntry`
- `WireJoinerLedgerSummary`
- `WireJoinerOpenExportHistoryEntry`
- `WireJoinerHistorySummary`

这些类型本来描述的是 FreeCAD `WireJoinerP` 内部 `EdgeInfo` / `WireInfo` / `aHistory` / `openWireCompound` 生命周期，但现在已经漏到多个调用方：

- `cad-core/include/cad_core/sketcher/sketch_internal_builder.h` 直接 include `part/wire_joiner.h`，并在 `SketchInternalBuildResult` 中公开 `WireJoinerLedgerSummary` 和 `WireJoinerHistorySummary`。
- `cad-core/src/sketcher/sketch_internal_builder.cpp` 直接调用 `joiner.ledgerSummary()` / `joiner.historySummary()`。
- `cad-core/include/cad_core/sketcher/sketch_internal_result.h` 继续把 `part::WireJoinerLedgerSummary` / `part::WireJoinerHistorySummary` 暴露到 Sketch Internal Result 输入。
- `cad-core/src/sketcher/sketch_internal_result.cpp` 手写 `wireJoinerLedgerToJson()`、`wireJoinerHistoryDetailToJson()`，并逐字段把 `ResultWireProducer*` 投影到 object JSON 和 `SketchInternalHistoryContext`。
- `cad-core/tests/test_p5_sketch.py` 和 `cad-core/tests/test_adapters.py` 大量断言 `wire_joiner_ledger`、`wire_joiner_history_detail`、`result_wire_producer_*`、`open_wire_compound_current_member_split_ledger_*` 等字段。

这说明当前 module 的复杂度没有真正被 interface 收住。调用方和测试已经知道“为什么当前 child wire 没有发布”、“哪个 blocker 阻止了 result-wire producer”、“哪些 endpoint identity debt 还没还清”。这些事实有调试价值，但不应成为 Sketcher 的常规 interface。

## FreeCAD 依据

WireJoiner 的正确抽象应贴近 FreeCAD 的内部账本，而不是把账本平铺为调用方协议：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::add()`：维护 `sourceEdgeArray`、顶点合并和 vmap 语义，关键短句是 `Make sure coincident vertices are actually the same TopoDS_Vertex`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`：通过 `aHistory->AddModified(split.intersectShape, newInfo.edge)` 记录 split fragment history。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildClosedWire()`：通过 `aHistory->Remove(info.edge)` 记录 deleted / removed producer evidence。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findSuperEdgesUpdateFirst()`：维护 `superEdge`、`iteration`、`iteration2`、member suppression 和 current-member lifecycle。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`：把最终 `info.wire()` 加入 `openWireCompound`，这是 open-wire export 边界。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：消费 `MapperHistory(aHistory)`，并在 `noOriginal=true` 时按 source shared vertex 过滤 open children。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：调用 `Part::FaceMakerBuildFace` 后追加 `WireJoiner::getOpenWires()`，SketchObject 不应知道 WireJoiner ledger anatomy。

因此重构方向不是删除 ledger，也不是把它改成几何猜测，而是把 ledger 作为 WireJoiner module-local evidence，向外只发布结果、命名证据和有限 diagnostics。

## 目标 module

深化 `part/WireJoiner` module，新增或整理两个内部层次：

- `cad-core/include/cad_core/part/wire_joiner.h`：保留稳定 public API，只暴露 `WireJoiner`、open-wire result、history evidence package 和 focused diagnostics。
- `cad-core/src/part/wire_joiner.cpp`：继续持有 `EdgeInfo` / `WireInfo` / `ResultWireProducer*` / blocker / endpoint debt 等内部账本。
- 可新增 `cad-core/src/part/wire_joiner_diagnostics.cpp` 或在 `wire_joiner.cpp` 内部保留 module-local diagnostic projection，避免 JSON 投影散到 Sketcher。
- 可新增 `cad-core/include/cad_core/part/wire_joiner_result.h`，只放 caller 需要的 stable result 类型；不要把当前全部 ledger structs 重新搬到另一个 public header。

### public interface 应收敛到三类信息

1. open-wire result：
   - open wire shape / compound。
   - 是否被 noOriginal purge。
   - 是否有 missing child-wire invariant。

2. topo history evidence：
   - open export entries 中 topo/NamedShape 必须消费的 shape、source edge indices、relation、event id。
   - split / generated / deleted relation。
   - mapper history 所需的 child-wire evidence。

3. focused diagnostics：
   - human/debug JSON 可存在，但由 WireJoiner module 直接产出。
   - Sketcher 只挂载 `wire_joiner_diagnostics` 或兼容字段，不逐字段理解 producer/blocker。
   - 测试优先断言行为级 diagnostic code / summary，而不是数十个内部 counter。

重构后的 Sketcher 角色：

- `sketch_internal_builder.cpp` 仍按 FreeCAD 调用顺序创建 WireJoiner、add source/open wires、build、getOpenWires。
- `sketch_internal_builder.h` 不再在 result 中暴露 `WireJoinerLedgerSummary` / `WireJoinerHistorySummary` 全量结构。
- `sketch_internal_result.cpp` 不再逐字段翻译 WireJoiner ledger；只消费 WireJoiner module 给出的 topo history package 和 diagnostic JSON。
- `topo_shape.cpp` 继续消费 `SketchInternalHistoryContext`，但不依赖 `ResultWireProducer*` 的 public part-layer类型。

## 分层落点

- `part/wire_joiner.h`：缩小 public interface。保留 FreeCAD 调用顺序相关入口；把 internal ledger 类型迁入 `.cpp` 或 private detail header。
- `part/wire_joiner.cpp`：继续是 EdgeInfo / WireInfo / openWireCompound / aHistory lifecycle 的唯一实现地。
- `part/wire_joiner_diagnostics.*`（可选）：承接 diagnostic JSON projection、summary fields、compatibility field mapping。
- `sketcher/sketch_internal_builder.*`：只负责 SketchObject `buildInternals()` 调用顺序，不持有 ledger anatomy。
- `sketcher/sketch_internal_result.*`：只发布 Sketch InternalShape result package，不再维护 WireJoiner debug field map。
- `part/topo_shape.*`：继续负责 NamedShape / ElementMap / SketchInternalHistoryContext 消费，不从 output geometry 反推 WireJoiner ownership。
- `tests/test_p5_sketch.py`：从 ledger anatomy tests 逐步改成 behavior / topo evidence / focused diagnostic tests。

## 实施步骤

### S0：冻结当前行为面

先不改语义，只冻结这些必须保持稳定的输出：

- `InternalShape` 里的 open-wire carry-through 不改变。
- `Sketch.InternalShape` 的 NamedShape / ElementMap / history relation 不丢失。
- `wire_joiner_history_detail` 当前对 topo history 有用的 relation、source edge indices、split/generated/deleted event 继续存在，字段可先兼容保留。
- `wire_joiner_ledger` 当前用于定位 blocker 的 debug fields 第一轮先通过 module-local projection 保持输出，避免一次性改掉所有 Python assertions。
- 不改变 noOriginal purge、source lineage、current-member vertex multiplicity blocker、result-wire producer exported state 的行为。

本步建议先用当前 P5 focused tests 作为迁移前基线，记录哪些字段是“外部兼容字段”，哪些是“内部账本字段”。

### S1：定义 WireJoiner publish result

新增一个 caller-facing result package，替代调用方直接拿两个 summary：

- `TopoDS_Shape openWires` 或 optional shape。
- `WireJoinerHistoryEvidence`：只包含 topo / NamedShape 消费所需的 event、open export relation、source edge lineage 和 child-wire shape evidence。
- `nlohmann::json diagnostics` 或 `WireJoinerDiagnosticReport`：由 WireJoiner module 自己生成，调用方只转挂。
- 状态位：hasOpenWires、missingChildWireInvariant、noOriginalPurged、hasMapperHistoryEvidence。

public 类型必须在相邻注释中标注 FreeCAD 依据，至少指向 `WireJoinerP::build()`、`WireJoinerP::getOpenWires()` 和 `SketchObject::buildInternals()`。

### S2：把 JSON diagnostic projection 移入 WireJoiner module

把 `sketch_internal_result.cpp` 里的这些逻辑迁出：

- `resultWireProducerLedgerEntriesJson()`。
- `wireJoinerLedgerToJson()`。
- `wireJoinerHistoryDetailToJson()` 中只服务 debug JSON 的字段投影。
- `resultWireProducerKindName()` / `resultWireProducerStateName()` / `resultWireBlockerName()` 对 Sketcher 的直接依赖。

迁移后 Sketcher 只写：

- `result.objectFields["wire_joiner_diagnostics"] = input.wireJoinerResult.diagnosticsJson`。
- 短期兼容：如果必须保留旧字段名，可由 WireJoiner module 生成 `wire_joiner_ledger` / `wire_joiner_history_detail` JSON，Sketcher 只转发。

### S3：拆分 topo history evidence 与 debug ledger

把 `WireJoinerHistorySummary` 拆成两条语义路线：

- topo evidence：`SketchInternalHistoryContext` 需要的 stable entries，允许继续公开为小结构。
- debug ledger：producer kind/state/blocker、endpoint debt、counter matrix、diagnostic-only fields，留在 WireJoiner module 内部。

迁移要求：

- `SketchInternalHistoryContext` 不再引用 `part::ResultWireProducerIdentity` 或内部 blocker enum。
- topo evidence 只保留稳定字符串 / relation / source indices / event references。
- 如果 topo history 仍需要 producer state 作为 diagnostic metadata，先由 WireJoiner module 投影成 stable string，避免 public enum 泄漏。

### S4：缩小 public header

迁移完成后，`wire_joiner.h` 中应只保留：

- `class WireJoiner`。
- `WireJoinerBuildResult` / `WireJoinerOpenWireResult` / `WireJoinerHistoryEvidence` 等小型 public result。
- 必要的 opaque diagnostic accessor。

这些类型应移出 public header：

- `ResultWireProducerKind`
- `ResultWireProducerState`
- `ResultWireBlocker`
- `ResultWireProducerIdentity`
- `ResultWireProducerLedgerEntry`
- `WireJoinerEndpointIdentityDebt`
- 全量 `WireJoinerLedgerSummary`

短期如果无法一次迁完，可先放到 `src/part/wire_joiner_detail.h` 这类 private include，并确保只有 `src/part/*.cpp` 使用。

### S5：调整 Sketcher include 和 result package

- `sketch_internal_builder.h` 不再 include `cad_core/part/wire_joiner.h`，除非 public result 类型确实需要。
- `SketchInternalBuildResult` 改成持有 `WireJoinerBuildResult` 或 `WireJoinerHistoryEvidence`。
- `SketchInternalResultInput` 不再暴露 `WireJoinerLedgerSummary` / `WireJoinerHistorySummary`。
- `sketch_internal_builder.cpp` 仍可以 include WireJoiner public header，因为它是调用方；但不读取内部账本。

### S6：迁移 tests

测试分三层迁移：

- 保留少量 adapter compatibility tests，保证旧 JSON 字段暂时还存在或有清晰替代字段。
- 新增 focused behavior tests：open wire 是否输出、NamedShape history relation 是否进入 `Sketch.InternalShape`、noOriginal purge 是否生效、missing child-wire invariant 是否产生 diagnostic。
- 删除或降级 anatomy tests：不再让测试直接依赖几十个 `result_wire_producer_blocker_*` / `open_wire_compound_current_member_split_ledger_*` counter。

不要一次删掉全部字段断言。建议先把 `test_p5_sketch.py` 中公共 helper 改成检查 “WireJoiner diagnostic report contains stable summary”，再逐步删除对单个 counter 的硬绑定。

## 非目标

- 不改变 WireJoiner 几何算法、owner 传播、split、superEdge、tight-bound 或 noOriginal purge 语义。
- 不修复新的 internal-face parity case。
- 不把 WireJoiner ledger 改成 output geometry 猜测。
- 不删除 topo history evidence；只是把 debug ledger 和 topo evidence 分开。
- 不调整 `FaceMakerBuildFace`、`SketchObject` solver-facing 输入或 ProfileBased resolver。
- 不改变 `/cad/recompute` 外部契约；若旧 debug JSON 字段需要改名，另做兼容策略。

## 风险与控制

- 风险：把 topo history evidence 和 debug ledger 一起藏掉，导致 ElementMap / NamedShape history 退化。控制：S3 明确拆两条路线，topo evidence 必须保留。
- 风险：一次性删除 `wire_joiner_ledger` 字段导致大量测试失效但行为没问题。控制：先由 WireJoiner module 生成兼容 JSON，后续再瘦身测试。
- 风险：为了缩小 header，临时复制结构到 Sketcher。控制：所有 producer/blocker/endpoint debt 只允许在 `part/wire_joiner` module 内定义。
- 风险：隐藏 ledger 后排查 parity 变困难。控制：保留 module-local diagnostic report，并让 focused diagnostics 有 stable code / summary。
- 风险：把 WireJoiner result 做成巨型 DTO，只是换了名字。控制：deletion test：删除 debug JSON projection 后，Sketcher 仍能构造 InternalShape 和 NamedShape history。

## 验收命令

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check
```

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_p5_sketch.py tests/test_p7_features.py tests/test_feature_flows.py
python3 -m unittest tests/test_adapters.py
```

### 重型收口

仅在删除旧 debug JSON 字段、修改 NamedShape history evidence 或触碰 WireJoiner 几何输出时执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐顺序

优先执行 S0-S3。理由是当前最大的 friction 不是 WireJoiner 没有内部账本，而是账本已经成为 Sketcher 和 tests 的 interface。先把 diagnostic projection 和 topo evidence 拆开，再缩小 public header，可以在不改变几何语义的前提下提升 locality，并给后续 WireJoiner parity 修复留下更稳定的 test surface。
