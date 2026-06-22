# C5-M16-S3 CurveProjection / FrenetFrame 专项复审

## 目标

复审并固化 edge/curve parameter、optional vertex projection、D1/D2、Frenet T/N/B 和 `NormalToPath` / `FrenetNB/TN/TB` 的 placement 合同。

## 工作内容

1. 记录 support order：edge/curve only、edge+vertex、vertex+edge。
2. 记录 parameter source：无 vertex 使用 `attachParameter`；有 vertex 用 `GeomAPI_ProjectPointOnCurve`。
3. 记录 D1 failure：zero derivative 必须 diagnostic，不 fallback 到 default placement。
4. 记录 D2 / Frenet normal failure：`FrenetTN/TB` 和后续 curvature modes 必须失败或留下 precise blocker。
5. 写入 S6 success / diagnostics fixture plan。

## cad-core 合同

- `resolveCurveFrameSupport()`
- `curveFrameParameter()`
- `frenetFrameAtParameter()`
- `normalToPathPlacement()`
- `frenetPlanePlacement()`

函数名只是建议，最终可以按现有 `datum_attachment.h` 风格调整；业务逻辑不得放到 adapter。

## 完成条件

- `C5M16-BLK-101` 和 `C5M16-BLK-201` 具备可实现合同。
- diagnostics fixture 覆盖 missing/wrong support、projection failure、zero derivative、undefined Frenet normal。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线
```
