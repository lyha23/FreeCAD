# M3 generated result-wire identity

## 目标

删除 `generatedOpenExportShapeForSketchInternals()` 这个 generated result-wire 过渡来源，用 FreeCAD `myShapesToReturn` / `aHistory` / final `EdgeInfo` 生命周期产出 result-wire identity。

M3 是当前最关键的未完成 milestone。M2 不能安全切换，核心原因就是 M3 还没完成。

M3 同时负责把 M1 留下的 `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 从“summary 后验归因”迁移到 rerun 拒绝分支的 causal blocker 统计。M3 不重新定义 M1 的 ownership 规则，不放宽 `idxVertex` / `stackPos` 顺序约束，也不靠输出数量反推 `wireInfo`。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getResultWires()`

关键字段：

- `myShapesToReturn`
- `aHistory`
- `EdgeInfo::wireInfo`
- `EdgeInfo::wireInfo2`
- `EdgeInfo::iteration`
- `openWireCompound`

## cad-core 落点

- `cad-core/src/geometry/wire_joiner.cpp::generatedOpenExportShapeForSketchInternals()`
- `cad-core/src/geometry/wire_joiner.cpp::buildFinalEdgeOwnership()`
- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/tests/test_p5_sketch.py`

## 当前基线

已完成：

- generated open-export 已从单一 bool 拆成 producer reason。
- producer reason 已进入 `wire_joiner_history_detail.open_export_history_entries`。
- `open_wire_compound_generated_*` 分类计数已进入 ledger。
- generated open-export bridge 已能追溯到被复制的现有 split `EdgeInfo`。

剩余 producer reason：

- `consumed_open_cutter_graph`
- `partial_junction_open_cutter`
- `closed_wire_cycle`
- `partial_shared_closed_wire`

仍未完成：

- 这些 reason 仍是过渡分类，不是真实 FreeCAD result-wire identity。
- `generated_open_export_edge_info_count` 仍标出 generated 过渡来源。
- `source_lineage_missing_open_export_edge_info_count` / `open_export_generated_missing_source_lineage_edge_count` 仍会标出 generated 缺口。
- repeated rerun live mutation 需要知道 generated result-wire identity 是否安全，但 generated open-export `EdgeInfo` 目前在 rerun 之后才 append；因此 rerun 阶段只能保守阻塞，`ledgerSummary()` 又只能事后按 generated edge 数量归因。

当前主路径：

```text
splitEdges()
  -> rebuildAdjacentList()
  -> assignClosedWireOwners()
  -> recordBranchSearchCandidates()
  -> recordTightBoundLifecycle()
  -> recordExhaustTightBoundLifecycle()
  -> recordBuildClosedWireRemovalLifecycle()
  -> recordRepeatedSplitExhaustRerunLifecycle()
  -> generatedOpenExportShapeForSketchInternals()
  -> append generated open-export EdgeInfo
  -> recordOpenWireCompoundLedger()
```

`repeated_split_exhaust_generated_identity_blocked_edge_info_count > 0` 不是 M1 owner lifecycle 缺失，而是 generated result-wire identity 尚未统一。

## 设计原则

1. generated result-wire 不能再作为游离的最终输出补边。
2. generated result-wire 必须绑定回已有 source `EdgeInfo`。
3. `findClosedWires()`、`findTightBound()`、`exhaustTightBound()` 仍使用 source edge / split edge 的生命周期状态。
4. `openWireCompound` / `getOpenWires()` 只在导出阶段消费 generated export override。
5. M3 不修改 M1 的 FreeCAD stack / vertexStack / edgeSet / wireSet 语义。

## 推荐结构与流程

在 `EdgeInfo` 上增加 export override，而不是 append 游离 generated `EdgeInfo`：

```cpp
struct EdgeInfo {
    TopoDS_Edge edge;

    // owner lifecycle 使用 edge；open export 使用 openExportOverride。
    std::optional<TopoDS_Edge> openExportOverride;

    bool generatedOpenExportEdge = false;
    std::string generatedOpenExportReason;
    bool generatedOpenExportSourceEdgeInfo = false;
    std::size_t generatedOpenExportSourceEdgeInfoIndex = 0;
    bool generatedOpenExportSourceEdgeInfoConsumed = false;
};
```

导出时使用：

```cpp
TopoDS_Wire EdgeInfo::exportWire() const
{
    if (openExportOverride) {
        return BRepBuilderAPI_MakeWire(*openExportOverride).Wire();
    }
    return wire();
}
```

先计算 generated export plan，再让 rerun gate 使用该 plan，最后才应用导出 override：

```text
computeGeneratedOpenExportPlan(finalInfo, boundedFaceShape, openEdges, closedWires)
  -> recordRepeatedSplitExhaustRerunLifecycle(finalInfo, boundedFaces, generatedPlan)
  -> applyGeneratedOpenExportPlan(finalInfo, generatedPlan)
  -> recordOpenWireCompoundLedger(finalInfo)
```

建议结构：

```cpp
struct GeneratedOpenExportBinding {
    std::size_t sourceEdgeInfoIndex = 0;
    TopoDS_Edge generatedEdge;
    std::string reason;
    bool consumedSourceEdgeInfo = false;
};

struct GeneratedOpenExportPlan {
    bool needed = false;
    bool identitySafe = false;
    std::vector<GeneratedOpenExportBinding> bindings;
};
```

rerun gate 不再扫描尚未 append 的 generated edge，而是使用 `GeneratedOpenExportPlan`：

```cpp
const bool generatedIdentityUnsafe = generatedPlan.needed && !generatedPlan.identitySafe;

const bool canApplyWithLiveReset =
    !canApplyWithoutReset
    && resettableAssignedEdges > 0U
    && (owner.branchSearchCandidateCount == 0U || !generatedIdentityUnsafe);
```

如果不能 live reset，则在实际拒绝分支记录 causal blocker：

```cpp
if (!canApplyWithoutReset && !canApplyWithLiveReset) {
    if (generatedIdentityUnsafe && resettableAssignedEdges > 0U) {
        info.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount += resettableAssignedEdges;
    }
    continue;
}
```

同时删除 `ledgerSummary()` 中按 `generatedOpenExportEdgeInfoCount` 事后反推 blocker 的逻辑。

## 必收切片

1. 按 producer reason 逐类替换，不再把所有 result-wire copy 混在一个 helper。
2. 对每类 reason 找到 FreeCAD 对应路径：
   - closed wire 被消费后的 result-wire。
   - partial junction 的 open cutter result。
   - cross / segmented-cross 的 bounded-result edge。
   - partial shared closed wire 的保留边。
3. 让 result-wire edge 从真实 `EdgeInfo` / `WireInfo` / `aHistory` 生命周期产出。
4. 删除或降级 `generatedOpenExportShapeForSketchInternals()`，使其不参与 shape 输出。
5. `repeated_split_exhaust_generated_identity_blocked_edge_info_count` 从 summary 后验统计迁移到 rerun 拒绝分支的 causal blocker 统计。
6. generated fixture 中 blocker 不再无条件等于 `generated_open_export_edge_info_count`，而是等于真实被 generated identity 阻塞的 rerun owner / edge 数。

## 边界

M3 只负责 result-wire identity 的来源。

M3 不负责：

- `getOpenWires()` shared-vertex purge 的最终切换。这属于 M2。
- 元素级 history 写入 `NamedShape`。这属于 M4。
- 在 `SketchInternalBuilder` 里复制 result-wire。M6 明确禁止。

## 非目标

- 不给 generated copy edge 人工补 `sourceEdgeIndices` 冒充真实 lineage。
- 不继续扩展 `generatedOpenExportShapeForSketchInternals()` 的几何形态规则。
- 不以 fixture 期望数量倒推 result-wire ownership。
- 不在 M3 中修改 `findTightBoundSplitWire()` 的 `idxVertex` / `stackPos` 顺序判断。

## 验收

完成条件：

- `generated_open_export_edge_info_count == 0`
- `open_wire_compound_generated_wire_info_count == 0`
- `source_lineage_missing_open_export_edge_info_count == 0`
- `open_export_generated_missing_source_lineage_edge_count == 0`
- `repeated_split_exhaust_generated_identity_blocked_edge_info_count == 0`
- 删除 `generatedOpenExportShapeForSketchInternals()` 后，T/cross/overlap result-wire 数量仍与 oracle 一致。
- `openWireCompound`、`NamedShape.history` 和 `ElementMap` 能消费同一套 source `EdgeInfo` identity。

重点 fixture：

- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-three-overlap-circles`
- `sketch-internal-face-arc-lens`
- `sketch-internal-face-bullseye`
