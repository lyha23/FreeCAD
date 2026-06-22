# C5-M16-S5 requestLocalPlacement / Capability 专项复审

## 目标

复核 C5-M16 的 request-local placement response、AttachmentOffset / MapReversed / MapPathParameter、writeback suggestions 和 capability release boundary。

## 工作内容

1. 确认 curve-frame helpers 只消费本次 request graph 中的 support shape 和 subname。
2. 确认 response 只能返回 placement、diagnostics、documentObjectUpdates / elementReferenceUpdates suggestions。
3. 确认 `ReferenceShadow.brep` 例外不扩展成完整 BREP 状态。
4. 确认 release gate 只移除本包 proven modes。
5. 确认 excluded family 仍留在 exact blocker 或 non-goal。

## capability 发布边界

| 项 | S6 允许移除 | 条件 |
| --- | --- | --- |
| `FrenetNB/TN/TB` | 是 | expected-backed modes + diagnostics 通过 |
| `Concentric` / `SectionOfRevolution` | 是 | curvature center expected-backed 或 precise blocker split |
| `AxisOfCurvature` / `Normal` / `Binormal` / `CenterOfCurvature` | 是 | alias expected-backed |
| `Folding` | 否 | 独立 fold-angle package |
| conic landmarks | 否 | 独立 conic landmark package |
| `IntersectionPoint` | 否 | source route 不清 |
| `TangentU/V` | 否 | 非本包 proven owner |

## 完成条件

- `C5M16-BLK-501` 具备 release gate 合同。
- adapter test expected list 能从本包 proven modes 精确删除，不影响 excluded modes。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
```
