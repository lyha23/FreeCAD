# C5-M17-S3 ConicLandmark 合同复审

## 目标

冻结 conic landmark 的 DTO、placement、diagnostics 和 expected 字段，确保 S6 批量实现时按 FreeCAD 语义推进。

## 必须固化的语义

- `Asymptote1/2`：只接受 hyperbola edge；line base 为 hyperbola location，direction 为 `Asymptote1/2().Direction()`。
- `Directrix1/2`：ellipse/hyperbola 有两条 directrix；parabola 只有 `Directrix1`，`Directrix2` 必须 diagnostic。
- `Focus1/2`：ellipse/hyperbola 有两个 focus；parabola 只有 `Focus1`，`Focus2` 必须 diagnostic。
- support 必须是 edge/conic；wrong shape、non-conic edge、null edge 都应走 diagnostic，而不是 default placement。
- DatumLine / DatumPoint executor 只消费 placement，不复制 conic 业务规则。

## 必做

1. 设计 `partdesign-datum-conic-landmark-modes.json`，覆盖 ellipse / hyperbola / parabola 代表场景。
2. 设计 `partdesign-datum-conic-landmark-diagnostics.json`，覆盖 wrong support、non-conic edge、parabola second directrix/focus。
3. 明确 expected 字段：base/direction/point、map_mode、diagnostic_codes、documentObjectUpdates。
4. 更新 fixture/oracle matrix 和 validation matrix。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'mm1Asymptote|mm1Directrix|mm0Focus|Directrix1|Focus1|Asymptote1' src/Mod/Part/App/Attacher.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线
```

## 非目标

- 不实现代码。
- 不采集 expected。
- 不把 hyperbola-only / parabola-only failure 变成 supported。
