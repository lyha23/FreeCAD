# WireJoiner 完整账本迁移方案

时间：2026-06-02 04:01。

## 背景

`06-02-02-37-cad-core临时诊断主路径偏移整改方案.md` 已经把明显的输出端修补收回到 `WireJoiner::getOpenWires()` 路径，并让 MVP、P5 Sketch、P6 Topology 验收通过。

但当前 `cad-core` 仍然只是阶段性对齐：

```text
FaceMaker InternalShape
  -> WireJoiner partial result-wire evidence
  -> getOpenWires()
  -> Sketch InternalShape / NamedShape
```

它还不是完整 FreeCAD `WireJoinerP` 账本。真正应该迁移的是：

```text
sourceEdgeArray
  -> EdgeInfo live list
  -> splitEdges()
  -> buildAdjacentList()
  -> findClosedWires()
  -> buildClosedWire()
  -> findTightBound()
  -> exhaustTightBound()
  -> compound / openWireCompound
  -> MapperHistory(aHistory)
  -> TopoShape ElementMap / NamedShape history
```

这份方案的目标，是把当前 partial evidence collector 替换成完整 `WireJoinerP::aHistory`、`findTightBound()` / `exhaustTightBound()` 生命周期，避免继续在 result-wire evidence 上叠 fixture 规则。

## FreeCAD 依据

主要源码：

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::buildAdjacentList()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findClosedWires()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getResultWires()`

关键 FreeCAD 语义短句 / 字段：

- `sourceEdgeArray`
- `sourceEdges`
- `aHistory`
- `EdgeInfo::iteration`
- `EdgeInfo::iteration2`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::superEdge`
- `WireInfo::vertices`
- `WireInfo::done`
- `WireInfo::purge`
- `openWireCompound`
- open export 条件：`iteration == -3 || (!wireInfo && iteration >= 0)`
- `shape.makeShapeWithElementMap(comp, MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op)`

## 当前 cad-core 差距

当前 `cad-core/src/geometry/wire_joiner.cpp` 已经有这些阶段性结构：

- `EdgeInfo` / `WireInfo` 的简化字段。
- `buildFinalEdgeOwnership()`。
- graph-cycle / bridge owner。
- `resultWireEvidence_`。
- `recordBoundedFaceClassifierProbe()`。
- `ledgerSummary()`。

这些结构能解释现有 fixture，但有三个本质缺口：

1. `EdgeInfo` 生命周期不是 FreeCAD 的生命周期。

   当前 owner 主要由 split edge graph 的 cycle / bridge 关系写出；FreeCAD 是通过 `findClosedWires()`、`findTightBound()`、`exhaustTightBound()` 多轮搜索写出。

2. `openWireCompound` 仍有 partial evidence collector。

   当前 result-wire evidence 已经收回 WireJoiner 内部，但它仍不是从真实 `EdgeInfo` final state 和 `aHistory` 自然导出的完整结果。

3. `aHistory` 尚未进入 topo 正式账本。

   当前 topo 已消费 FaceMaker history，但 WireJoiner 自己的 generated / modified / deleted history 还没有完整进入 `NamedShape.history` / `ElementMap`。

## 迁移目标

最终目标不是“再让 fixture 多过几个”，而是让 `cad-core` 的 WireJoiner 主路径具备以下性质：

```text
SketchObject / SketchInternalBuilder
  -> WireJoiner.addShape(source sketch edges)
  -> WireJoiner.Build()
  -> WireJoiner.getOpenWires()
  -> MapperHistory-equivalent
  -> NamedShape / ElementMap
```

验收口径：

- `SketchInternalBuilder` 不再知道 copied result-wire、partial overlap、T/cross/cycle 这类 result-wire 形态规则。
- `getOpenWires()` 只读 final `EdgeInfo` 字段和 source identity，不做几何形态猜测。
- `NamedShape` / `ElementMap` 只消费 WireJoiner history，不从 raw/internal 几何关系发明 WireJoiner split / deleted / generated history。
- 当前 `resultWireEvidence_`、graph-cycle owner、`ownerContributesToLedger` 这类过渡字段要么删除，要么降级为 diagnostic trace。

## 分层落点

### geometry

落点：

- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/src/geometry/wire_joiner.cpp`

职责：

- 迁移 `EdgeInfo` / `WireInfo` / `VertexInfo` / adjacent list / stack / edge set / wire set。
- 迁移 split、closed wire、tight bound、exhaust tight bound。
- 产出 `compound`、`openWireCompound` 和 WireJoiner history ledger。

### topo

落点：

- `cad-core/include/cad_core/topo/*`
- `cad-core/src/topo/*`

职责：

- 接收 WireJoiner history ledger。
- 转换为 `NamedShape.history`、`ElementMap` 和 stable subname 更新。
- 替换当前依赖 FaceMaker-only 或 exact alias 的 WireJoiner history 缺口。

### features

落点：

- `cad-core/src/geometry/sketch_internal_builder.cpp`
- `cad-core/src/features/sketch_object.cpp`

职责：

- 只表达 FreeCAD `SketchObject::buildInternals()` 调用顺序。
- 不再承载 result-wire 补边、split ownership、history 合成逻辑。

## 实施阶段

### 阶段 0：冻结基线和 trace

目标：迁移前先把当前过渡行为固化，防止边迁移边丢语义。

动作：

1. 保留当前通过的基线：
   - `python3 -m unittest tests/test_mvp.py`
   - `python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py`
2. 为 WireJoiner 增加只读 trace：
   - source edge count。
   - split edge count。
   - closed wire count。
   - final open export edge count。
   - generated / modified / deleted history entry count。
3. trace 只能进 diagnostics / metadata，不能参与 shape、subshape 或 history 决策。

完成条件：

- 当前 fixture 仍全绿。
- trace 能说明每个 InternalEdge 是 source、split、tight-bound consumed 还是 open export。

### 阶段 1：迁移 EdgeInfo / WireInfo 原始账本

目标：先补 FreeCAD 账本形状，不切输出。

动作：

1. 对齐 `EdgeInfo` 字段：
   - `edge`
   - `superEdge`
   - `p1`
   - `p2`
   - `mid`
   - `box`
   - `iStart`
   - `iEnd`
   - `iteration`
   - `iteration2`
   - `wireInfo`
   - `wireInfo2`
   - curve / parameter / linear metadata。
2. 对齐 `WireInfo` 字段：
   - `vertices`
   - `wire`
   - `done`
   - `purge`
3. 增加 `VertexInfo`：
   - 指向 `EdgeInfo`。
   - start / reversed 方向。
   - `pt()` / `ptOther()` 等价能力。
4. `addShape()` / `addOpenWire()` 只负责初始化 ledger，不做导出判断。

完成条件：

- `EdgeInfo` / `WireInfo` trace 能和 FreeCAD 字段一一对照。
- 当前 `buildFinalEdgeOwnership()` 仍可作为旧输出桥接，但不再扩展新规则。

### 阶段 2：迁移 splitEdges() 和 aHistory 初始能力

目标：让 split 结果和 history 从 WireJoiner 自己产出。

动作：

1. 迁移 `splitEdges()`：
   - 原 edge 到 split edge 的 modified history。
   - 被替换 edge 的 removed/deleted history。
   - `superEdge` 关系。
2. 建立 `WireJoinerHistoryLedger`：
   - generated。
   - modified。
   - deleted。
   - source-to-result edge map。
3. 不直接写 `NamedShape`，只在 geometry 层记录 history ledger。

完成条件：

- through-open-cutter、self/inter-edge split、circle overlap 的 split edge 能追溯 source edge。
- `topo` 还未消费前，输出行为不变。

### 阶段 3：迁移 buildAdjacentList() / findClosedWires()

目标：替换 graph-cycle 近似的基础。

动作：

1. 建立 FreeCAD 风格 adjacent list：
   - 每个 EdgeInfo 端点的 adjacency range。
   - `iStart` / `iEnd`。
   - tolerance 与 bbox 过滤。
2. 迁移 `_findClosedWires()` 搜索栈：
   - `vertexStack`
   - `stack`
   - `edgeSet`
   - `wireSet`
3. 迁移 `findClosedWires()`：
   - 初始 closed wire owner。
   - open edge iteration 标记。
   - 失败 wire 的 diagnostic。

完成条件：

- 单闭合 wire、nested hole/island、overlapping closed wires 的 closed wire ledger 稳定。
- 不再用 graph bridge 直接定义 tight-bound owner。

### 阶段 4：迁移 findTightBound()

目标：让 primary `wireInfo` 来自 FreeCAD branch search，而不是 bounded face 或 graph-cycle 推断。

动作：

1. 迁移 `findTightBoundByVertices()`：
   - `next == current` skip。
   - `next->iteration2 == iteration2` skip。
   - `next->iteration < 0` skip。
   - `isInside(*wireInfo, next->mid)` 判定。
2. 迁移 `findTightBoundSplitWire()`：
   - branch slicing。
   - splitWire vertices。
   - old wireInfo 到 new wireInfo 的 edge owner 转移。
3. 迁移 `findTightBoundUpdateVertices()`：
   - `wireInfo->done = true`。
   - done owner 向同 wire vertices 传播。
4. 当前 `ownerContributesToLedger` 应删除或降级为 diagnostic。

完成条件：

- T-junction、cross cutter、segmented cross cutter 的 primary owner 能由 branch search trace 解释。
- `wire_joiner_ledger.primary_owned_edge_info_count` 不再依赖 graph-cycle fallback。

### 阶段 5：迁移 exhaustTightBound()

目标：补齐 secondary `wireInfo2` 和 done wire 生命周期。

动作：

1. 迁移 `exhaustTightBoundUpdateWire()`。
2. 迁移 `exhaustTightBoundWithAdjacent()`。
3. 迁移 `exhaustTightBoundUpdateEdge()`。
4. 迁移 `exhaustTightBoundUpdateVertex()`。
5. 明确 `wireInfo2 && wireInfo2->done` 的 skip 语义。

完成条件：

- shared edge / partial overlap / multi-region cutter 的 secondary owner 能从 trace 解释。
- `secondary_owned_edge_info_count`、`exhaust_*` ledger 不再由 bounded-face classifier probe 写出。

### 阶段 6：切换 compound / openWireCompound 导出

目标：删除 partial result-wire evidence collector。

动作：

1. 按 FreeCAD `buildClosedWire()` 生成 `compound`。
2. 按 FreeCAD `build()` 生成 `openWireCompound`：

   ```text
   iteration == -3 || (!wireInfo && iteration >= 0)
   ```

3. `getOpenWires(noOriginal=true)` 只做 source shared-vertex purge。
4. 删除或降级：
   - `resultWireEvidence_`
   - `copiedResultWireGraphProbeForSketchInternals()`
   - graph-cycle owner 导出逻辑。

完成条件：

- T/cross/overlap result-wire 数量仍与 oracle 一致。
- dangling single bounded face 不再需要特殊 owner 可见性字段。
- `SketchInternalBuilder` 不知道 result-wire 复制细节。

### 阶段 7：接入 topo history

目标：把 `MapperHistory(aHistory)` 等价能力接入 `NamedShape` / `ElementMap`。

动作：

1. 新增 WireJoiner history public result：
   - source edge。
   - generated shapes。
   - modified shapes。
   - deleted shapes。
   - openWireCompound result edge。
2. 在 `topo` 新增 WireJoiner history consumer。
3. `NamedShape.history` 的 WireJoiner split/generated/deleted 只来自该 consumer。
4. 删除 raw/internal 几何采样式 WireJoiner history 兜底。

完成条件：

- through-open-cutter、dangling filtered edge、partial overlap 的 history 能追溯 WireJoiner ledger。
- `ElementMap` 不再靠 edge endpoint / curve sampling 发明 WireJoiner history。

### 阶段 8：切换 Sketch InternalShape 主路径

目标：让 Sketch 只保留 FreeCAD 调用顺序。

目标流程：

```text
FaceMakerBuildFace result
  -> WireJoiner.addShape(raw sketch edges)
  -> WireJoiner.getOpenWires(openWires, "SKF")
  -> result.makeElementCompound({result, openWires})
  -> NamedShape consumes FaceMakerHistory + WireJoinerHistory
```

动作：

1. `SketchInternalBuilder` 删除 WireJoiner 过渡参数。
2. `Profile` selection 只依赖 InternalFace 数量和 FreeCAD profile 语义。
3. mesh / subshape response 只读最终 InternalShape。

完成条件：

- `cad-core/src/features/sketch_object.cpp` 不出现 WireJoiner result-wire 推理。
- `cad-core/src/geometry/sketch_internal_builder.cpp` 不出现 partial overlap / T/cross/cycle 规则。

### 阶段 9：清理临时路径和文档验收

动作：

1. 删除或降级所有临时过渡项：
   - graph-cycle owner。
   - `resultWireEvidence_`。
   - `ownerContributesToLedger`。
   - partial evidence collector。
   - bounded-face classifier probe 对任何 lifecycle 字段的影响。
2. 更新 `06-02-02-37-cad-core临时诊断主路径偏移整改方案.md`：
   - 把 WireJoiner 剩余风险标记为已迁移。
   - 标明删除了哪些过渡路径。
3. 新增或更新专项测试说明。

完成条件：

- 代码中不再有“为了 T/cross/overlap/dangling fixture 复制 edge”的主路径规则。
- 文档能说明 FreeCAD 文件、函数、字段、cad-core 落点和验收命令。

## 验收矩阵

必须通过：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py
```

重点 fixture：

- `sketch-internal-face-through-open-cutter`
- `sketch-internal-face-branch-open-cutter`
- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-dangling-line`
- `sketch-internal-face-split-and-dangling`
- `sketch-internal-face-three-overlap-circles`
- `sketch-internal-face-arc-lens`
- `sketch-internal-face-cubic-figure8-bspline`

新增专项验收建议：

- self-intersection 与 inter-edge intersection 同时存在。
- closed bounded faces 与 open wires 同时存在。
- source edge 一对多 split fragments。
- shared edge 同时有 `wireInfo` 和 `wireInfo2`。
- `noOriginal=true` purge 只删除 source shared-vertex open wires。
- `aHistory` generated / modified / deleted 能被 `NamedShape` 消费。

## 禁止事项

迁移期间禁止：

- 在 `SketchObject`、`SketchInternalBuilder`、adapter 或 response 层按 fixture 名称分支。
- 在 `getOpenWires()` 中新增 midpoint、boundary-touch、same-coordinate endpoint 这类几何猜测规则。
- 用 InternalShape 结果反推 `wireInfo` / `wireInfo2`。
- 通过 raw/internal geometry sampling 发明 WireJoiner split/generated/deleted history。
- 为了保住当前测试，在 partial evidence collector 上继续叠 T/cross/overlap 特判。

如果某阶段短期必须保留旧桥接：

- 必须写明“临时桥接”。
- 必须写明删除条件。
- 必须只服务当前阶段切换，不允许继续扩展新形态规则。

## 风险与拆分建议

最大风险不是 OCCT 构造，而是身份账本。

建议不要一次性替换输出。正确拆分是：

```text
先补 ledger trace
  -> 再让 ledger 与现有输出并跑
  -> 再切 getOpenWires
  -> 再切 topo history
  -> 最后删 partial evidence
```

每个阶段都要能回答：

- FreeCAD 哪个函数写了这个字段？
- cad-core 哪个结构承接这个字段？
- 这个字段是否影响 shape / subshape / NamedShape / ElementMap？
- 如果影响，是否已经有 history 依据？

## 完成定义

这项迁移完成的标准是：

1. `WireJoiner::getOpenWires()` 的输出只来自真实 final `EdgeInfo` lifecycle。
2. `WireJoiner` 的 generated / modified / deleted history 进入 `topo` 消费路径。
3. `SketchInternalBuilder` 不再包含 result-wire 复制或形态判断。
4. 当前 partial evidence collector 被删除，或只剩完全不参与输出的 diagnostic。
5. MVP、P5 Sketch、P6 Topology 验收全部通过。
6. 方案文档更新为已实现，并记录剩余非阻塞差异。
