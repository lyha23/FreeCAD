# M2 openWireCompound / getOpenWires 导出

## 目标

把 open/result wire 导出切到 FreeCAD `openWireCompound` child-wire 边界。

M2 解决的是“哪些 WireJoiner final edge 应该进入 `getOpenWires()` 输出”。它不负责生成 result-wire identity，也不负责 topo history。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`

关键语义：

```text
builder.Add(openWireCompound, info.wire())
iteration == -3 || (!wireInfo && iteration >= 0)
source.findSubShapesWithSharedVertex(TopoShape(edge, -1))
```

## cad-core 落点

- `cad-core/src/geometry/wire_joiner.cpp::recordOpenWireCompoundLedger()`
- `cad-core/src/geometry/wire_joiner.cpp::WireJoiner::getOpenWires()`
- `cad-core/include/cad_core/geometry/wire_joiner.h`
- `cad-core/src/features/sketch_object.cpp`
- `cad-core/tests/test_p5_sketch.py`

## 当前基线

已完成：

- `recordOpenWireCompoundLedger()` 已为 final open-export `EdgeInfo` 保存 request-local child wire。
- `open_wire_compound_*` 计数已进入 `wire_joiner_ledger`。
- child-wire shared-vertex predicate 已能解释 dangling open line、internal-branch cutter、pad-dangling 的 `purgeAsOriginalOpenEdge` bridge。
- `open_wire_compound_purge_bridge_unmatched_wire_info_count == 0` 已成为重要回归信号。

仍未完成：

- 直接让 `getOpenWires()` 读取 child-wire ledger 会让 original open edge 泄漏。
- 直接应用 shared-vertex purge 会误删 cross / branch result-wire edge。
- 原因是当前 generated copy edge 的 vertex identity 还不是 FreeCAD `openWireCompound` / `aHistory` 产物。

## 必收切片

1. `getOpenWires()` 的输入边界改为真实 `openWireCompound` child-wire。
2. `noOriginal=true` purge 只执行 source shared-vertex predicate。
3. original open edge、split edge、generated result-wire edge 的 identity 不再依赖 `purgeAsOriginalOpenEdge`。
4. child-wire merge 不能改变 FreeCAD child ownership 语义。

## 边界

M2 可以使用 M1 的 final `EdgeInfo` 状态。

M2 不能补：

- generated result-wire identity。那属于 M3。
- `MapperHistory(aHistory)` 元素级映射。那属于 M4。
- Sketch 输出端 copied result-wire。那已经被 M6 禁止。

## 非目标

- 不在 `getOpenWires()` 中新增 midpoint、boundary-touch、same-coordinate 规则。
- 不为某个 fixture 单独调整 child-wire merge 顺序。
- 不把 shared-vertex predicate 当作 generated result-wire identity 的替代。

## 验收

必须同时满足：

- dangling open line、internal-branch cutter、pad-dangling 不泄漏 original open edge。
- T-junction、cross、branch result-wire edge 不被误删。
- `open_wire_compound_purge_bridge_unmatched_wire_info_count == 0`。
- 删除 `purgeAsOriginalOpenEdge` 后仍能通过 P5/P6 相关 fixture。

重点 fixture：

- `sketch-internal-face-dangling-line`
- `sketch-internal-face-split-and-dangling`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-segmented-cross-cutter`
