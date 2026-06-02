# M7 fixture / oracle 验收闸门

## 目标

把 fixture 和 oracle 用作验收证据，而不是业务语义来源。

M7 不直接实现功能。它固定失败归因顺序，防止某个 fixture 失败后又回到 executor、Sketch 输出层或 response 层叠形态规则。

## 失败归因顺序

遇到 fixture 或 oracle 不一致时，按以下顺序定位：

1. Oracle 是否可信，FreeCAD probe 是否真实执行。
2. FaceMaker result / WireJoiner result 的几何结果是否等价。
3. WireJoiner `EdgeInfo` / `WireInfo` / `openWireCompound` 身份账本是否完整。
4. `MapperHistory(aHistory)` / `NamedShape` / `ElementMap` 是否完整消费。
5. Sketch / Profile / ExternalGeometry 调用顺序是否越界补规则。
6. 是否只是 stable subname 命名顺序差异。

只有归因到具体 milestone 后，才允许改对应层代码。

## 当前重点 fixture

WireJoiner / Sketch internal face：

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

Reference / profile：

- Profile.SubList 指向多个 InternalFace 的选择 case。
- ReferenceShadow 单 subshape drift / split / ambiguous / deleted case。

## 必收诊断字段

WireJoiner 回归字段：

- `temporary_result_wire_edge_info_count`
- `graph_fallback_assigned_edge_info_count`
- `graph_secondary_owner_edge_info_count`
- `generated_open_export_edge_info_count`
- `source_lineage_missing_open_export_edge_info_count`
- `open_wire_compound_generated_wire_info_count`
- `open_wire_compound_purge_bridge_wire_info_count`
- `open_wire_compound_purge_bridge_unmatched_wire_info_count`

Topo / history 回归字段：

- `wire_joiner_history`
- `wire_joiner_history_detail`
- `element_history_status`
- `reference_recovery`
- `reference_recovery_reason`

## 非目标

- 不把当前 `cad-core` 输出冻结成 oracle，除非已经验证 FreeCAD 行为。
- 不因为 InternalEdge / InternalVertex 命名顺序不同直接判失败；几何等价且输出稳定时，单独归类为命名顺序差异。
- 不在测试里写会反向定义业务语义的 fixture 特判。

## 验收命令

默认相关范围：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py
git diff --check
```

涉及 Profile / ReferenceShadow 时追加：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests/test_feature_flows.py tests/test_p7_features.py
```

文档类拆分不需要构建。

## 完成定义

M7 自身没有一次性完成状态。它是长期验收闸门。

每个 milestone 标记完成时，必须在对应文档里留下：

- FreeCAD 依据。
- `cad-core` 落点。
- 最终通过的验收命令。
- 剩余 known gap。
- 不再允许回退的旧桥接或旧规则。
