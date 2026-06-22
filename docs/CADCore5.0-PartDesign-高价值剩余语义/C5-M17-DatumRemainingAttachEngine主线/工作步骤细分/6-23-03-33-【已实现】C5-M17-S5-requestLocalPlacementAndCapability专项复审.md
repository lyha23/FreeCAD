# 【已实现】C5-M17-S5 requestLocalPlacement / Capability 专项复审

状态：`done_s5_release_gate_frozen`

## 目标

复核 conic landmark 的 request-local response、AttachmentOffset / MapReversed、writeback suggestions 和 capability release boundary。

## 必须保持的边界

- 后端只消费本次 request graph 的 support shape/subname/MapMode/AttachmentOffset/MapReversed。
- response 只返回 placement、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates` suggestions。
- 不保存 backend attachment session。
- 不把 complete BREP 当作前端或后端长期几何状态；`ReferenceShadow.brep` 仍只允许作为单个 referenced subshape snapshot 例外。
- S6 只能删除 expected-backed proven conic modes：`Directrix1/2`、`Asymptote1/2`、`Focus1/2`。

## 必做

1. 复核 `cad-core/src/adapters/c_api/c_api.cpp` capability supported / exact blocker 文案。
2. 确认 `Folding`、`IntersectionPoint`、`TangentU/V` 仍保留 exact blocker。
3. 更新 package/root validation matrix。
4. 写清 S6 release gate。

## S5 结论

- `cad-core/src/adapters/c_api/c_api.cpp` 当前 `datum_attach_engine_remaining_modes` 仍列出 `Folding`、`Directrix1/2`、`Asymptote1/2`、`TangentU/V`、`Focus1/2`、`IntersectionPoint`，S5 不删除任何 exact blocker。
- `cad-core/src/part_design/datum_attachment.h` 的 selected MapMode path 只消费本次 request graph 中的 support shape/subname、`MapMode`、`AttachmentOffset`、`MapReversed` / `Reverse` 和 path parameter；没有 backend attachment session。
- response 只允许返回 placement、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates` suggestions。`documentObjectUpdates` 只是给前端下一次 graph 写回 `AttachmentSupport` / `StableSubList` / `ShadowSub` / `ReferenceShadow` 的建议，不是后端持久状态。
- 不保存 complete BREP；`ReferenceShadow.brep` 只保留单个 referenced subshape snapshot，作为引用恢复证据例外，不能升级成完整对象或建模输入状态。
- S6 release gate：只有 FreeCADCmd expected、focused conic tests 和 adapter capability test 全部通过后，才允许从 `datum_attach_engine_remaining_modes` 删除 proven conic modes：`Directrix1/2`、`Asymptote1/2`、`Focus1/2`。
- `Folding`、`IntersectionPoint`、`TangentU/V` 必须继续保留 exact blocker 或 later package / source-audit 路由；S6 不得把 excluded families 发布为 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'datum_attach_engine_remaining_modes|Directrix1|Asymptote1|Focus1|Folding|IntersectionPoint|TangentU' cad-core/src/adapters/c_api/c_api.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
```

## 非目标

- 不改 C++。
- 不采集 expected。
- 不提前移除 exact blocker。
