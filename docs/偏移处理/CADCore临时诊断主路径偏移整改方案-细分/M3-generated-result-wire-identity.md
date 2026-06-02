# M3 generated result-wire identity

## 目标

删除 `generatedOpenExportShapeForSketchInternals()` 这个 generated result-wire 过渡来源，用 FreeCAD `myShapesToReturn` / `aHistory` / final `EdgeInfo` 生命周期产出 result-wire identity。

M3 是当前最关键的未完成 milestone。M2 不能安全切换，核心原因就是 M3 还没完成。

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

## 必收切片

1. 按 producer reason 逐类替换，不再把所有 result-wire copy 混在一个 helper。
2. 对每类 reason 找到 FreeCAD 对应路径：
   - closed wire 被消费后的 result-wire。
   - partial junction 的 open cutter result。
   - cross / segmented-cross 的 bounded-result edge。
   - partial shared closed wire 的保留边。
3. 让 result-wire edge 从真实 `EdgeInfo` / `WireInfo` / `aHistory` 生命周期产出。
4. 删除或降级 `generatedOpenExportShapeForSketchInternals()`，使其不参与 shape 输出。

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

## 验收

完成条件：

- `generated_open_export_edge_info_count == 0`
- `open_wire_compound_generated_wire_info_count == 0`
- `source_lineage_missing_open_export_edge_info_count == 0`
- `open_export_generated_missing_source_lineage_edge_count == 0`
- 删除 `generatedOpenExportShapeForSketchInternals()` 后，T/cross/overlap result-wire 数量仍与 oracle 一致。

重点 fixture：

- `sketch-internal-face-cross-cutters`
- `sketch-internal-face-segmented-cross-cutter`
- `sketch-internal-face-t-cutter`
- `sketch-internal-face-three-overlap-circles`
- `sketch-internal-face-arc-lens`
- `sketch-internal-face-bullseye`
