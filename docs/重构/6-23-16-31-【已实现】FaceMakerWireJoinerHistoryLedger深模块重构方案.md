# FaceMaker/WireJoiner History Ledger 深模块重构方案

## 实现状态

已实现。当前落点是 `InternalShapeHistoryLedger` request-local 深模块：Sketcher 只传 ledger，Topo 通过 ledger 消费 history，兼容 JSON 由 ledger 生成；旧 `SketchInternalHistoryContext` 和 producer-specific evidence 结构已从 public header 收回到 `src/part` private detail。

## 来源与结论

本方案来自 `/tmp/architecture-review-20260623-162528.html` 的 Strong 候选项 `Deepen the FaceMaker/WireJoiner history ledger`。该报告把它列为 top recommendation，原因是同一处 producer ledger 泄漏同时影响 `SketchObject` publication、`FaceMaker`、`WireJoiner`、`NamedShape`、`ElementMap`、reference recovery 和 fixture assertions，而且仍处在本仓库既定的 geometry/topo module map 内。

当前结论：上一份 `6-23-13-13-【已实现】WireJoinerDiagnosticLedger深模块重构方案.md` 已经把 `WireJoiner` diagnostic ledger 的一部分收回到 `WireJoinerBuildResult` 后面，但本轮报告指出的是更深一层问题：`FaceMaker` 与 `WireJoiner` 的 producer history evidence 仍以各自内部形态流入 Sketcher、topo 和测试。下一轮重构应新增一个 Part 层 `InternalShapeHistoryLedger` 深模块，让调用方消费“命名历史事件”，而不是消费 FaceMaker/WireJoiner 的账本 anatomy。

## 当前基线

当前代码已经比旧状态收敛了一步：

- `cad-core/include/cad_core/part/wire_joiner.h` 只公开 `WireJoinerBuildResult`，并把 `compatibilityLedger`、`compatibilityHistoryDetail` 和 `diagnostics` 留在 WireJoiner module 侧生成。
- `cad-core/src/part/wire_joiner.cpp::WireJoiner::Impl::buildResult()` 已经把内部 `WireJoinerLedgerSummary` / `WireJoinerHistorySummary` 投影成 `WireJoinerBuildResult`。
- `cad-core/src/sketcher/sketch_internal_result.cpp` 不再直接逐字段生成 WireJoiner ledger JSON，只转挂 `wireJoinerResult` 中的兼容 JSON。

但剩余外露面仍然很大：

- `cad-core/include/cad_core/part/face_maker.h` 公开 `FaceMakerHistorySummary`、`FaceMakerEdgeHistoryEvidence`、`FaceMakerBoundedFaceHistoryEvidence` 等 producer 形态。
- `cad-core/include/cad_core/part/topo_shape.h` 公开 `SketchInternalHistoryContext`，其中同时塞入 FaceMaker summary、WireJoiner open-export event、vmap replacement、endpoint debt、result-wire producer state、child-wire ownership、split fragment fallback 等字段。
- `cad-core/src/sketcher/sketch_internal_result.cpp::sketchInternalHistoryContext()` 手动把 `FaceMakerHistorySummary` 和 `WireJoinerBuildResult::historyEvidence` 拼成 topo context。
- `cad-core/src/part/topo_shape.cpp` 的 `consumeSketchInternalGeneratedFaceHistory()`、`consumeSketchInternalTerminalHistory()`、`consumeSketchInternalWireJoinerProducerEvidence()` 和 `sketchInternalHistoryToJson()` 仍然知道大量 producer 内部字段。
- `cad-core/tests/test_p5_sketch.py` 仍有大量 assertion 绑定 `wire_joiner_history_detail`、`wire_joiner_open_export_history_entries`、`result_wire_producer_*`、`open_wire_compound_current_member_split_ledger_*` 等 anatomy。

这说明当前 seam 仍然偏浅：Sketcher 负责拼接 producer evidence，topo 负责解释 producer anatomy，tests 负责固定账本细节。以后每补一条 FaceMaker/WireJoiner history relation，都可能同时修改 producer、Sketcher、topo JSON、NamedShape history 和 Python assertions。

## FreeCAD 依据

目标抽象应贴近 FreeCAD 的调用链，而不是贴近当前 DTO 字段：

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：先调用 `Part::FaceMakerBuildFace`，再追加 `WireJoiner::getOpenWires()`，Sketcher 只关心最终 `InternalShape` publication。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()`：消费 `MapperHistory(myPreSplitHistory)` 和 `MapperMaker(mySplitter)`，再把 bounded face / edge history 写入 Part 层命名流程。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`：把最终 `info.wire()` 加入 `openWireCompound`。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：用 `MapperHistory(aHistory)` 把 open-wire compound 和 source edges 带入 `TopoShape::makeShapeWithElementMap()`。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`：先 `mapSubElement(shapes)`，再消费 mapper history；topo 层不应从 bbox、几何类型、输出顺序或 fixture 名称反推 ownership。

因此 cad-core 的正确方向是：`FaceMaker` 和 `WireJoiner` 仍然负责产生 history evidence，但 evidence 要在 Part 层收敛成一个统一的 request-local ledger；`Sketcher` 只转交 ledger；`TopoShape` / `ElementMap` 只消费稳定命名历史事件。

## 目标 module

新增一个 Part 层深模块：

- public header：`cad-core/include/cad_core/part/internal_shape_history_ledger.h`
- implementation：`cad-core/src/part/internal_shape_history_ledger.cpp`
- 可选 private header：`cad-core/src/part/internal_shape_history_ledger_detail.h`

建议 public interface 控制在少量类型和函数内：

- `InternalShapeHistoryLedger`
  - request-local ledger，代表 `Sketch.InternalShape` 从 raw sketch edge 到 internal face/edge/vertex 的历史。
  - 保存 stable relation、source element、target shape evidence、target element kind、producer tag 和 focused diagnostic code。
  - 不暴露 `WireInfo`、`EdgeInfo`、vmap、result-wire producer state、blocker enum、endpoint debt counter。
- `InternalShapeHistoryEvent`
  - relation：`Preserved` / `Generated` / `Modified` / `Deleted` / `Split` / `DiagnosticOnly`。
  - producer：`FaceMakerBuildFace` / `WireJoinerOpenWires`。
  - source：`EdgeN` 或 source edge index 列表。
  - target：`TopoDS_Shape` evidence 加 element kind；由 topo 在当前 `InternalShape` 内解析成 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`。
  - stage：稳定诊断标签，例如 `facemaker:pre_split`、`facemaker:splitter`、`wire_joiner:open_export`。
- `InternalShapeHistoryDiagnostics`
  - summary / code / compatibility JSON 生成入口。
  - 允许保留旧 `facemaker_history`、`wire_joiner_history_detail`、`sketch_internal_history` 字段，但由 ledger module 统一生成，调用方只转挂。

核心 seam：

```cpp
InternalShapeHistoryLedger mergeInternalShapeHistory(
    const std::optional<FaceMakerHistorySummary>& faceMaker,
    const std::optional<WireJoinerBuildResult>& wireJoiner
);

void consumeInternalShapeHistoryLedger(
    NamedShape& namedShape,
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape,
    const nlohmann::json& internalElementMap,
    const InternalShapeHistoryLedger& ledger
);
```

第一版可以先用现有 `FaceMakerHistorySummary` 和 `WireJoinerBuildResult` 做输入，以降低迁移风险；收口完成后，再把 FaceMaker/WireJoiner 的 public result 改成直接输出 ledger fragment，最终删除 `SketchInternalHistoryContext` 和 producer-specific topo structs。

## 分层落点

- `part/face_maker.*`：继续实现 FaceMakerBuildFace 几何与 history 采集。短期保留 `FaceMakerHistorySummary`，但新增到 `InternalShapeHistoryLedger` 的投影函数；中期把 public `FaceMakerHistorySummary` 缩小或迁入 private detail。
- `part/wire_joiner.*`：继续实现 `EdgeInfo` / `WireInfo` / `aHistory` / `openWireCompound` 生命周期。`WireJoinerBuildResult` 中的 `historyEvidence` 改为 ledger fragment，不再直接依赖 `SketchInternalHistoryContext`。
- `part/internal_shape_history_ledger.*`：承接 FaceMaker/WireJoiner producer evidence merge、stable event normalization、compatibility JSON 和 diagnostic summary。
- `sketcher/sketch_internal_builder.*`：保持 FreeCAD 调用顺序，只收集 `faceResult`、`wireJoinerResult`，不拼 topo context。
- `sketcher/sketch_internal_result.*`：`SketchInternalResultInput` 改为持有 `std::optional<part::InternalShapeHistoryLedger>`；object fields 中的 debug / compatibility 字段由 ledger module 生成。
- `part/topo_shape.*`：`namedShapeForSketchInternalShape()` 消费统一 ledger，不再暴露或解释 `SketchInternalWireJoinerOpenExportHistoryEntry`、`SketchInternalFaceMakerBoundedFaceEvidence` 这类 producer DTO。
- `tests/test_p5_sketch.py`、`tests/test_p7_features.py`：测试改成断言命名结果、stable history event、diagnostic code 和兼容字段存在，不再把 producer counter 当作长期 interface。
- `cad-core/CMakeLists.txt`：新增 `src/part/internal_shape_history_ledger.cpp`。

## 实施步骤

### S0：字段分类与冻结

先不改行为，只把现有字段按用途分类并写入相邻注释或临时清单：

- 命名必需：source edge、target shape、relation、target kind、bounded face outer boundary、open export target、deleted/split/generated event。
- 诊断必需：`missing_child_wire_invariant`、`no_original_purged`、target not found、vertex multiplicity blocked。
- 兼容输出：旧 `facemaker_history`、`wire_joiner_history_detail`、`sketch_internal_history` 中测试仍读取的字段。
- producer internals：vmap replacement detail、endpoint debt counters、result-wire producer state/blocker、wire info / edge info indexes。

冻结规则：本轮重构不改变 `InternalShape` 拓扑、不改变 `NamedShape.elementMap`、不改变旧 debug JSON 的存在性；只改变这些字段由哪个 module 生成和谁能读取。

### S1：新增 `InternalShapeHistoryLedger`

新增 ledger public 类型和从现有 summary 构造 ledger 的 adapter：

- `ledger.addFaceMakerEvidence(const FaceMakerHistorySummary&)`
- `ledger.addWireJoinerEvidence(const WireJoinerBuildResult&)`
- `ledger.events()` 或 `ledger.historyEvents()` 返回稳定命名事件。
- `ledger.diagnosticsJson()` / `ledger.compatibilityJson()` 统一生成 debug 字段。

此步先允许 adapter 读取旧 producer summary。目标是引入新 seam，不急着删除旧结构。

验收重点：

- `sketch_internal_result.cpp::sketchInternalHistoryContext()` 的逻辑可以搬进 ledger module。
- 新旧 JSON 和 `NamedShape.sketchInternalHistory` 暂时保持等价。
- 不引入 geometry matching fallback。

### S2：让 Sketcher 只传 ledger

修改 `SketchInternalBuildResult` 和 `SketchInternalResultInput`：

- 移除或降级 `faceMakerHistory` 与 `wireJoinerResult` 在 Sketcher result interface 中的直接暴露。
- `buildSketchInternals()` 在调用 `FaceMaker` 与 `WireJoiner` 后构造 `InternalShapeHistoryLedger`。
- `buildSketchInternalResult()` 只把 ledger 传给 `namedShapeForSketchInternalShape()`，并转挂 ledger 生成的 compatibility object fields。

Sketcher 的长期角色只剩两个：

- 复刻 FreeCAD `SketchObject::buildInternals()` 的调用顺序。
- 发布 `InternalShape`、mesh、subshapes、object fields。

它不再理解 FaceMaker 的 bounded-face evidence，也不再理解 WireJoiner 的 open-export child-wire anatomy。

### S3：把 topo 消费改成 event dispatcher

在 `part/topo_shape.cpp` 内新增统一消费入口：

- `consumeInternalShapeHistoryLedger(...)`
- 根据 `InternalShapeHistoryEvent.targetKind` 和 `targetShape` 在当前 `internalShape` 中解析目标元素。
- 根据 relation 写入 `ElementHistory`、`MapperHistoryEvent`、`elementMap` 或 diagnostic status。

迁移策略：

- 第一阶段可让旧 `consumeSketchInternalGeneratedFaceHistory()`、`consumeSketchInternalTerminalHistory()`、`consumeSketchInternalWireJoinerProducerEvidence()` 改为 ledger event 的内部 helper。
- 第二阶段删除 producer-specific helper 参数，把 `SketchInternalHistoryContext` 从 `topo_shape.h` public header 中移除或缩成 compatibility-only detail。

验收重点：topo 消费的是“source -> target element relation”，不是 `openWireCompoundOwnerWireInfo2`、`resultWireProducerState`、`currentMemberSplitLedgerOutputVertexDebt` 等 producer anatomy。

### S4：把 compatibility JSON 收到 ledger module

把这些 JSON 投影从 Sketcher / topo 移到 `internal_shape_history_ledger.cpp`：

- `facemaker_history`
- `facemaker_history_status`
- `wire_joiner_history_detail`
- `wire_joiner_history`
- `sketch_internal_history`
- history event summary counters

兼容策略：

- 短期保持旧字段名，避免一次性破坏 adapter / Python assertions。
- 新增稳定字段，例如 `internal_shape_history`，只包含 event count、producer tags、relation summary、diagnostic codes 和 target-found 状态。
- 后续测试先迁到 `internal_shape_history`，再逐步删除对旧 counter 的硬绑定。

### S5：缩小 public headers

完成 S1-S4 后收缩 public interface：

- `face_maker.h` 不再公开完整 `FaceMakerHistorySummary` anatomy；若仍需短期过渡，放到 private detail header 或只在 `src/part/*.cpp` 可见。
- `wire_joiner.h` 中 `WireJoinerBuildResult` 不再包含 `SketchInternalHistoryContext`。
- `topo_shape.h` 删除或私有化：
  - `SketchInternalWireJoinerEndpointIdentityDebt`
  - `SketchInternalWireJoinerVmapReplacementEvent`
  - `SketchInternalWireJoinerOpenExportHistoryEntry`
  - `SketchInternalWireJoinerHistoryEvent`
  - `SketchInternalFaceMaker*Evidence`
  - 全量 `SketchInternalHistoryContext`

public topo header 最终只需要知道 `InternalShapeHistoryLedger` 或一个小型 forward-declarable handle。

### S6：测试迁移

测试分三层迁移：

- 行为测试：`InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 数量、`NamedShape.elementMap`、`mapperHistory`、`elementHistoryStatus` 不退化。
- 稳定 ledger 测试：断言 `internal_shape_history` 的 producer、relation、source/target、diagnostic code。
- 兼容测试：保留少量旧字段存在性和关键 summary assertion，避免外部协议突然断裂。

应逐步减少这些 assertion 的权重：

- `result_wire_producer_*`
- `open_wire_compound_*_wire_info_count`
- `open_wire_compound_current_member_split_ledger_*`
- `wire_joiner_history_detail.open_export_history_entries[*]` 的内部 counter

## 非目标

- 不改变 `FaceMakerBuildFace`、`WireJoiner` 几何算法、split、superEdge、tight-bound、noOriginal purge 或 ShapeFix 行为。
- 不修复新的 fixture parity；本方案只调整 history ledger seam。
- 不从 fixture 输出倒推 ownership，不新增 bbox、几何类型、输出顺序或 fixture 名称分支。
- 不删除 `ElementMap` / `NamedShape` 所需的 history evidence。
- 不立即删除旧 debug JSON 字段；删除应作为后续兼容性清理。
- 不把 `InternalShapeHistoryLedger` 做成新的巨型 DTO；producer internals 仍应留在 producer module 内部。

## 风险与控制

- 风险：把 topo 必需 evidence 和 debug detail 一起藏掉，导致 `ElementMap` 退化。控制：S0 明确字段分类，命名必需事件必须保留为 stable event。
- 风险：新 ledger 只是把 `SketchInternalHistoryContext` 换名。控制：deletion test：删除 producer-specific structs 后，topo 仍能通过 source/target/relation 事件完成 NamedShape history。
- 风险：为了保持兼容 JSON，把旧 counter 重新暴露成 public interface。控制：compatibility JSON 只由 ledger module 生成，C++ caller 不读取这些字段。
- 风险：FaceMaker 和 WireJoiner history 语义被过早合并，丢失 producer 顺序。控制：event 保留 `producer`、`stage` 和 stable diagnostic code，但不暴露内部账本结构。
- 风险：测试迁移后排查变难。控制：保留 focused diagnostics，例如 `target_not_found`、`no_original_purged`、`vertex_multiplicity_blocked`、`missing_child_wire_invariant`。

## 验收命令

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
git diff --check
```

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
```

### 重型收口

仅在删除旧 compatibility JSON、改 `NamedShape` history 语义或触碰 `FaceMaker` / `WireJoiner` 几何输出时执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐执行顺序

优先执行 S0-S3。原因是当前最大问题不是缺少 FaceMaker/WireJoiner 内部账本，而是账本已经成为 Sketcher、topo 和 tests 的接口。先建立 `InternalShapeHistoryLedger` 这个深模块，再把 Sketcher 和 topo 改成通过 stable event 消费历史，可以在不改变几何语义的前提下提升 locality，并给后续 ElementMap / MapperHistory parity 修复提供更稳定的测试面。
