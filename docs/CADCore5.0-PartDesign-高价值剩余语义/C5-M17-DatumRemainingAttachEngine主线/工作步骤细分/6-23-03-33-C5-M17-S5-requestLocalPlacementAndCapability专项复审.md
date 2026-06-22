# C5-M17-S5 requestLocalPlacement / Capability 专项复审

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
