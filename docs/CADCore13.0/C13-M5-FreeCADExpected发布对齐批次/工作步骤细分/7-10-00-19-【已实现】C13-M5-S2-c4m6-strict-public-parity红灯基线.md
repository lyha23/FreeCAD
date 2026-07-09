# 【已实现】C13-M5 S2 c4m6 strict public parity 红灯基线

## 目标

把 `c4m6` 当前 strict diff 从人工观察变成机器可复现的红灯基线，并把每个差异归到具体实现或协议决策。

## 必做

1. 用 S1 comparator 生成 `c4m6` strict report。
2. 对每个 diff 建立 owner：
   - object publication set -> `runtime/topo_naming_state.cpp`。
   - mapperHistory publication -> `runtime/topo_naming_state.cpp` / `part/topo_shape.cpp`。
   - request hash mismatch policy -> `runtime/topo_naming_state.cpp` / `runtime/recompute.cpp`。
   - Link compound diagnostics/results -> `runtime/recompute.cpp` stableSubname diagnostics。
3. 把现有 `tests.test_topo_naming_state_response` 的前端最低合同断言保留为 consumer smoke，同时新增 strict expected parity 断言。
4. 明确哪些差异是必须实现，哪些需要协议裁决。

## 实现结果

- `cad-core/tools/compare_freecad_expected.py` 已把每个 strict diff 扩展为可追责红灯条目，字段包含 `owner`、`owner_step`、`decision`、`freecad_authority`、`next_action`、`close_condition`。
- `c4m6` strict report 仍保持红灯，不把 known gap 当 passed：9 cases，2 green / 7 red。
- 当前红灯 summary 稳定为：diagnostics=14、results=14、results.subshapes=1、topoNamingState.objects=13、topoNamingState.elementMap=1、topoNamingState.mapperHistory=328、topoNamingState.subshapes=449、geometry.numeric=2。
- 当前 decision 分组稳定为：`runtime_publication_gap`=470、`mapper_history_publication_gap`=328、`stable_subname_diagnostic_policy`=13、`hash_mismatch_policy`=6、`protocol_decision_required`=5。
- owner 归属已进入 report：object/subshape/elementMap 发布缺口归 `cad-core/src/runtime/topo_naming_state.cpp`；mapperHistory 归 `cad-core/src/runtime/topo_naming_state.cpp` / `cad-core/src/part/topo_shape.cpp`；hash mismatch 归 `cad-core/src/runtime/topo_naming_state.cpp` / `cad-core/src/runtime/recompute.cpp`；stableSubname diagnostic / Link result 归 `cad-core/src/runtime/recompute.cpp`。
- `tests.test_topo_naming_state_response` 继续保留 consumer smoke；`tests.test_freecad_expected_public_parity` 已新增当前 c4m6 strict classified red baseline 断言，确认所有 red diff 都已分类。

## 非目标

- 不把 `c4m6` 以外 phase 直接纳入红灯。
- 不通过修改 expected 让 strict report 变绿。
- 不用 raw `:H...` hash 判断失败。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/compare_freecad_expected.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response tests.test_freecad_expected_public_parity
```
