# FaceMaker/WireJoiner History Ledger 深模块重构方案

## 背景

本方案对应 `/tmp/architecture-review-20260623-162528.html` 中的 `Deepen the FaceMaker/WireJoiner history ledger` 候选项。报告判断是 `Strong / in-process`：`WireJoiner` 和 `FaceMaker` 已经有较深的几何实现，但 history ledger 的 seam 仍然偏浅，producer 内部账本细节泄漏到 topo、sketcher、响应调试字段和测试。

本方案已从 `my-chili3d/docs/重构` 移入 `/home/user/Chili3DProject/FreeCAD/docs/重构`。当前权威上下文是本仓库的 FreeCAD 源码与 `cad-core` 抽取实现；几何重算、FaceMaker、WireJoiner、NamedShape、ElementMap 的代码落点在：

- `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/face_maker.h`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/face_maker.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/wire_joiner.h`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/wire_joiner.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/internal_shape_history_ledger.h`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/internal_shape_history_ledger.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/topo_shape.h`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/topo_shape.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/sketcher/sketch_internal_builder.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/sketcher/sketch_internal_result.cpp`

本方案用于指导 `/home/user/Chili3DProject/FreeCAD/cad-core` 的后续实现；`my-chili3d` 只作为 `/cad/recompute` 的前端消费方，不复制几何 history 推理。

## FreeCAD 依据链

必须按 FreeCAD 的真实调用链迁移，不能从 fixture 输出倒推。

1. `/home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：
   - 先调用 `Part::FaceMakerBuildFace` 生成 `InternalShape` 的 bounded faces。
   - 再调用 `WireJoiner::getOpenWires(openWires, "SKF")` 追加未进入 closed face 的 open wires。

2. `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp`：
   - `splitSelfIntersecting()` 对自交 edge 做 pre-split。
   - `myPreSplitHistory->AddModified(edge, fi.Value())` 记录 original edge 到 fragment 的关系。
   - `splitAtIntersections()` 使用 `mySplitter` 记录 inter-edge split。

3. `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()`：
   - 先消费 `MapperHistory(myPreSplitHistory)`。
   - 再消费 `MapperMaker(mySplitter)`。
   - 这说明 bounded face / split edge 的命名来源是 producer history，不是后续几何采样。

4. `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`：
   - `WireJoinerP::splitEdges()` 记录 `aHistory->AddModified(split.intersectShape, newInfo.edge)`。
   - `WireJoinerP::build()` 在 closed wire / tight bound 消费 open edge 时记录 `aHistory->Remove(info.edge)`。
   - `WireJoinerP::getOpenWires()` 用 `MapperHistory(aHistory)` 和 `sourceEdges` 生成 open-wire result shape 的 ElementMap。

结论：`cad-core` 的正确 seam 应该是“FaceMaker/WireJoiner producer ledger 产出可发布的 ElementHistory / MapperHistory / ElementMap alias”，而不是让 topo 或 sketcher 读取 `EdgeInfo`、`WireInfo`、iteration、blocker、result-wire candidate、vmap replacement 等内部字段再二次判断。

## 当前问题

### 1. `InternalShapeHistoryLedger` public interface 仍然暴露 compatibility 形态

当前 `InternalShapeHistoryLedger` 的 public 入口包括：

- `historyEvents()`
- `internalShapeHistoryJson()`
- `compatibilityObjectFields()`
- `sketchInternalHistoryCompatibilityJson()`
- `sketchInternalHistoryStatus()`

这些方法混合了三类东西：可发布命名历史、调试诊断、旧响应兼容字段。调用方很容易拿 JSON 或 compatibility history 做业务判断，导致 module interface 和 implementation 几乎一样复杂。

### 2. `wire_joiner.cpp` 内部 producer ledger 过度外溢

`wire_joiner.cpp` 当前有大量只应属于 WireJoiner implementation 的概念：

- `ResultWireProducerKind`
- `ResultWireProducerState`
- `ResultWireBlocker`
- `OpenWireCompoundExportSource`
- `WireJoinerHistoryEvent`
- `WireJoinerVmapReplacementEvent`
- `WireJoinerLedgerSummary`
- `ResultWireProducerLedgerEntry`

这些概念本身有价值，但它们应该用于 WireJoiner 内部判断和诊断，不应该成为 topo module 消费 ElementMap 的必要语言。

### 3. `topo_shape.cpp` 正在消费 WireJoiner 解剖字段

`topo_shape.cpp` 中 `consumeSketchInternalWireJoinerProducerEvidence()` 读取 `SketchInternalWireJoinerOpenExportHistoryEntry` 的大量字段，并在 topo 层判断：

- open export 是否消耗 child-wire ownership。
- relation 取 event 还是 entry。
- noOriginal purge 是否 terminal。
- current-member vertex multiplicity 是否阻断。
- split/generated/deleted 如何变成 `ElementHistory`。

这些判断说明 topo 层已经学会了 WireJoiner 的内部状态机。按照 deep module 标准，这个 seam 太浅：调用方要理解 producer anatomy，才能消费命名历史。

### 4. `sketch_internal_result.cpp` 继续发布 ledger 兼容字段

`buildSketchInternalResult()` 在 `objectFields` 里合并 `historyLedger->compatibilityObjectFields()`。这会让 `/cad/recompute` 响应继续携带 WireJoiner/FaceMaker 的内部账本快照。调试字段可以保留，但它不能再是正式消费路径，也不能成为测试断言的主目标。

### 5. 测试容易锁死内部字段

只要测试断言 `open_wire_compound_*`、`result_wire_producer_*`、`wire_joiner_history_event_*` 这类字段，就会把 WireJoiner 内部实现冻结住。重构目标应把测试 surface 调整为：

- `element_map`
- `history`
- `mapper_history`
- `element_history_status`
- `SubList / StableSubList` 解析结果
- diagnostics code 的少量稳定分类

## 重构目标

把 FaceMaker/WireJoiner history ledger 深化成一个小 interface、大 implementation 的 module。

目标状态：

1. `FaceMaker` 和 `WireJoiner` 仍然保留内部账本和状态机，但只向外发布 producer history events。
2. `TopoShape / NamedShape` 不再读取 WireJoiner 的 `EdgeInfo`、`WireInfo`、producer state、blocker、vmap replacement 细节。
3. `InternalShapeHistoryLedger` 对外提供一个 publish seam：输入 raw/internal shape 与 internal element map，输出可写入 `NamedShape` 的 element map delta、element history、mapper history、status、diagnostics。
4. `sketch_internal_builder.cpp` 只负责按 FreeCAD 顺序合并 FaceMaker + WireJoiner ledger，不负责解释 ledger。
5. `sketch_internal_result.cpp` 只发布 `InternalShape` mesh/subshapes/NamedShape；debug JSON 如需保留，只来自 `diagnosticsJson()`，并且不作为业务字段。
6. 测试跨过新 seam，断言命名和引用恢复结果，而不是断言 producer 内部账本字段。

## 非目标

- 不改 `/cad/recompute` 的建模请求契约。
- 不在 `my-chili3d` 前端实现 FaceMaker、WireJoiner 或 ElementMap 推理；前端只消费后端发布的 mesh、subshapes、diagnostics 与命名结果。
- 不用几何形态、fixture 名称、split 顺序、source index 猜测替代 FreeCAD history。
- 不把 `FaceMaker` 和 `WireJoiner` 的内部状态机拆成很多 caller 可见的小接口。
- 不把 `InternalFaceN / InternalEdgeN / InternalVertexN` 的顺序差异当作硬失败；几何等价且输出稳定时仍按命名顺序差异处理。
- 不在本轮重写全部 `NamedShape` / `ElementMap` 系统；只收敛 Sketch InternalShape 的 FaceMaker/WireJoiner producer ledger。

## 建议 module 与 seam

### 外部 seam

深化后的外部 seam 建议留在 `part` 层：

```cpp
struct InternalShapeHistoryPublishInput
{
    std::string owner;
    TopoDS_Shape rawShape;
    TopoDS_Shape internalShape;
    nlohmann::json internalElementMap;
};

struct InternalShapeHistoryPublication
{
    std::map<std::string, std::string> elementMapAliases;
    std::vector<ElementHistory> elementHistory;
    std::vector<MapperHistoryEvent> mapperHistory;
    std::vector<std::string> elementHistoryStatus;
    nlohmann::json diagnostics;
};

class InternalShapeHistoryLedger
{
public:
    void merge(const InternalShapeHistoryLedger& other);
    bool empty() const;

    InternalShapeHistoryPublication publishForInternalShape(
        const InternalShapeHistoryPublishInput& input
    ) const;

    nlohmann::json diagnosticsJson() const;
};
```

命名可按实际代码调整，但约束是：consumer 只能看到 publication，不再看到 FaceMaker / WireJoiner 的内部 compatibility context。

### Producer 内部 seam

FaceMaker/WireJoiner 可以有 internal seams，但不要暴露给 topo：

- `FaceMakerHistoryBuilder`：把 pre-split、splitter、bounded face evidence 转成 ledger events。
- `WireJoinerHistoryBuilder`：把 `aHistory`、openWireCompound child、noOriginal purge、sourceEdges 转成 ledger events。
- `InternalShapeHistoryPublisher`：唯一负责 ledger events -> `ElementHistory / MapperHistoryEvent / elementMapAliases`。

建议文件落点：

```text
cad-core/include/cad_core/part/internal_shape_history_ledger.h
cad-core/src/part/internal_shape_history_ledger.cpp
cad-core/src/part/internal_shape_history_publisher.cpp
cad-core/src/part/internal_shape_history_publisher.h
cad-core/src/part/face_maker_history_ledger.cpp
cad-core/src/part/wire_joiner_history_ledger.cpp
cad-core/src/part/internal_shape_history_debug.cpp
```

其中 `*_publisher.h` 可以是 private header，放在 `src/part`；public header 只保留 `InternalShapeHistoryLedger` 和 publication 类型。

## 分阶段方案

### 阶段 0：固定当前基线与消费者清单

操作：

1. 在 `/home/user/Chili3DProject/FreeCAD/cad-core` 中列出所有 ledger 直接消费者：
   - `InternalShapeHistoryLedger`
   - `SketchInternalHistoryContext`
   - `wireJoinerOpenExportHistoryEntries`
   - `compatibilityObjectFields`
   - `sketch_internal_history`
2. 按用途分类：
   - producer 内部：保留但 private。
   - publish 命名历史：迁入 `InternalShapeHistoryPublisher`。
   - debug/diagnostics：降级为 `diagnosticsJson()`。
   - response compatibility：确认是否有前端消费；若无，计划删除或只保留稳定 summary。
3. 固定一组代表 fixture / case：
   - self-intersecting edge pre-split。
   - inter-edge split 生成多个 bounded faces。
   - closed faces + open wires 同时存在。
   - open wire `noOriginal` purge。
   - source edge 一对多 split。
   - deleted edge / vertex terminal history。

产出：

- 一张迁移表：`field / function -> producer private / publication / diagnostics / delete`。
- 不改行为。

### 阶段 1：定义 publication seam

操作：

1. 在 public header 引入 `InternalShapeHistoryPublication`。
2. 为 `NamedShape` 增加一个小的应用函数，或让 `namedShapeForSketchInternalShape()` 调用 publication：

```cpp
void applyInternalShapeHistoryPublication(
    NamedShape& namedShape,
    const InternalShapeHistoryPublication& publication
);
```

3. `publishForInternalShape()` 先内部调用现有 compatibility context，确保输出不变。
4. `topo_shape.cpp::consumeInternalShapeHistoryLedger()` 改为只消费 publication，不再直接展开 `SketchInternalHistoryContext`。

验收：

- `topo_shape.cpp` 不再出现 `SketchInternalWireJoinerOpenExportHistoryEntry`、`ResultWireProducer*` 相关逻辑。
- 现有 fixture 输出不应发生语义变化；若仅 debug JSON 字段顺序变化，记录为非语义差异。

### 阶段 2：收拢 FaceMaker history builder

操作：

1. 将 `addFaceMakerEvidenceToLedger()` 和 FaceMaker JSON/debug 转换拆开。
2. FaceMaker producer 只记录以下可发布事件：
   - source edge -> split internal edge。
   - source edge -> deleted terminal edge。
   - bounded face -> generated InternalFace，sources 为 outer boundary source edges。
3. `FaceMakerHistorySummary` 留在 producer/detail 层，不出现在 topo consumer。
4. 删除 topo 中对 FaceMaker summary count 的业务 fallback；如果没有具体 evidence，只输出 diagnostic status，不合成 history。

验收：

- bounded face 命名仍来自 producer evidence。
- `internal_element_map` 只作为 raw/internal edge/vertex alias 辅助，不决定 split/deleted history。

### 阶段 3：收拢 WireJoiner history builder

操作：

1. 在 WireJoiner 内部建立统一的 `WireJoinerPublishedHistoryEvent`，字段控制在：
   - `relation`: preserved / generated / modified / deleted / split / diagnostic。
   - `targetKind`: edge / vertex / wire。
   - `sourceEdgeIndices`。
   - `targetShape` 或 `targetOpenExportIndex`。
   - `diagnosticCode`。
2. `ResultWireProducerKind/State/Blocker` 只用于内部选择 producer，不进入 publication。
3. `noOriginal` purge 只发布 deleted terminal event 或 diagnostic，不把 purge 过程字段暴露给 topo。
4. vmap replacement、endpoint materialization、current-member vertex debt 只进入 debug diagnostics；能证明 source->target 的，转成 publication event；不能证明的，转成 diagnostic event。
5. `WireJoinerBuildResult` 保留：
   - `openWires`
   - `hasOpenWires`
   - `diagnostics`
   - `historyLedger`

删除或降级：

- `missingChildWireInvariant`
- `noOriginalPurged`
- `hasMapperHistoryEvidence`

这些信息若仍需要观察，进入 `diagnosticsJson().summary`，不作为 caller-facing 状态。

验收：

- `wire_joiner.h` 的 caller-facing result 不暴露 producer anatomy。
- `wire_joiner.cpp` 内部仍可保留复杂状态机，但 topo/sketcher 不 include、不匹配、不断言这些 enum。

### 阶段 4：迁出 `topo_shape.cpp` 的 Sketch Internal producer 消费逻辑

操作：

1. 新建 `internal_shape_history_publisher.cpp`，把以下逻辑从 `topo_shape.cpp` 搬走：
   - `consumeSketchInternalGeneratedFaceHistory`
   - `consumeSketchInternalTerminalHistory`
   - `consumeSketchInternalWireJoinerProducerEvidence`
   - `wireJoinerOpenExportEvidenceJson`
   - `appendSketchInternalMapperHistoryEvents` 中与 ledger detail 强相关的部分。
2. `topo_shape.cpp` 保留通用 `NamedShape`、ElementMap、maker history 消费能力。
3. `NamedShape` 不再保存完整 `sketchInternalHistory`；如确需 JSON 输出，只保存 publication diagnostics 或由 publisher 返回的 diagnostics。

目标边界：

- `topo_shape.cpp` 可以知道“这是 Sketch InternalShape history publication”。
- `topo_shape.cpp` 不知道“这个 open export 是 current member child producer，且 blocker 是 vertex multiplicity”。

### 阶段 5：响应与前端边界清理

操作：

1. 检查 `my-chili3d` 是否直接读取以下响应字段：
   - `sketch_internal_history`
   - `sketch_internal_history_status`
   - `internal_shape_history`
   - `wire_joiner_*`
   - `open_wire_compound_*`
2. 若没有生产消费，`/cad/recompute` response 中只保留：
   - `named_shapes[*].element_map`
   - `named_shapes[*].history`
   - `named_shapes[*].mapper_history`
   - `named_shapes[*].element_history_status`
   - diagnostics summary code。
3. 若仍有调试页消费，将调试页改为读取 `diagnostics.summary`，不要读取 producer 内部字段。

本阶段可以和前端 `BackendSubshapeIntent` / `CAD Reference Mapping` 方案并行，但不得把后端 history 推理搬到前端。

### 阶段 6：测试替换

新增或调整测试：

1. `test_p5_sketch.py`
   - 验证 InternalFace generated history。
   - 验证 InternalEdge split history。
   - 验证 open wire preserved / generated / deleted terminal history。

2. `test_p6_topology.py`
   - 验证 `StableSubList=InternalEdgeN` / `InternalFaceN` 经 ElementMap 或 terminal split 状态解析。
   - 验证 split source 出现多 target 时返回 split/reselect，不靠 debug field 判断。

3. `test_expected_fixtures.py`
   - 对代表 fixture 保持 named shape、subshape count、diagnostic code 稳定。

删除或改写：

- 直接断言 `open_wire_compound_*`、`result_wire_producer_*`、`wire_joiner_history_event_*` 的测试。
- 直接依赖 `sketch_internal_history` 全量 JSON 结构的测试。

## 风险与处理

### 风险 1：publication seam 一次切换太大

处理：第一轮让 `publishForInternalShape()` 包装现有 compatibility context，不先删字段；确认 fixture 稳定后，再分批私有化 detail struct。

### 风险 2：现有 fixture 依赖 debug JSON

处理：把 fixture 差异分为两类：

- 命名语义差异：必须修。
- debug 字段消失 / 重命名：只在确认没有生产消费后更新 expected。

### 风险 3：WireJoiner 内部状态机仍然很大

处理：本方案不是把 `wire_joiner.cpp` 小文件化，而是把内部状态机压到 module implementation。只要 caller-facing interface 变小，内部复杂度可以保留；后续再按 producer family 拆 private helper。

### 风险 4：删除 fallback 后暴露未迁移 gap

处理：不能回到几何猜测。缺 evidence 的路径输出 diagnostic-only publication，状态写入 `element_history_status`，并用 focused fixture 约束。

## 验收命令

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core-lib
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_mvp tests.test_expected_fixtures tests.test_feature_flows
```

前端消费边界检查：

```bash
cd /home/user/Chili3DProject/my-chili3d
rg -n "sketch_internal_history|internal_shape_history|wire_joiner_|open_wire_compound_|result_wire_producer_" src docs
```

不要求默认跑全量 FreeCAD 构建；涉及 FreeCAD oracle 或 expected 刷新时，必须使用同一套 FreeCAD / LibPack / OCCT 基线。

## 完成标准

1. `cad-core/include/cad_core/part/internal_shape_history_ledger.h` 的 public interface 不暴露 WireJoiner producer anatomy。
2. `topo_shape.cpp` 不直接读取 `SketchInternalWireJoinerOpenExportHistoryEntry` 等 WireJoiner detail 类型。
3. `wire_joiner.h` 的 `WireJoinerBuildResult` 只返回 open wires、diagnostics 和 history ledger，不返回内部 producer flags。
4. `sketch_internal_result.cpp` 不把 full compatibility ledger 合并进正式 `objectFields`；如需调试，只发布稳定 summary diagnostics。
5. fixture / tests 主要断言 `NamedShape` publication：`element_map`、`history`、`mapper_history`、`element_history_status`、引用解析结果。
6. 对缺失 producer evidence 的场景，输出 diagnostic-only history，不做几何猜测、不新增 fixture 特判。

## 后续实施建议

优先从阶段 1 开始，因为它能先建立深 module seam，后续阶段都只是把现有实现迁入 seam 内部。不要先在 `wire_joiner.cpp` 内继续加 blocker 或 fallback 字段；只要新增字段是为了让 topo 能判断 producer 状态，就说明方向仍然在扩大浅 interface。
