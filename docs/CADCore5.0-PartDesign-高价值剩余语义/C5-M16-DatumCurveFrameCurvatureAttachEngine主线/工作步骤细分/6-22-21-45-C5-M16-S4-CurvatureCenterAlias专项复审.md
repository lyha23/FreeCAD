# C5-M16-S4 CurvatureCenter / Alias 专项复审

## 目标

复审 `Concentric` / `SectionOfRevolution` 的曲率中心语义，以及 DatumLine / DatumPoint aliases 如何复用 3D curve-frame helper。

## 工作内容

1. 记录 `Concentric` / `SectionOfRevolution` 的 curvature center 公式：`curvature = dd dot N / |d|^2`，base point 沿 N 偏移 `1 / curvature`。
2. 记录 infinite radius / undefined N failure。
3. 记录 `AxisOfCurvature -> RevolutionSection` 的 presuper rotation，避免 line executor 重写曲线帧逻辑。
4. 记录 `Normal -> FrenetTB`、`Binormal -> FrenetTN`、`CenterOfCurvature -> RevolutionSection` 的 alias contract。
5. 对 `TangentU/V` 继续保持 out-of-scope，除非 S1 证明其 source route。

## cad-core 合同

- `curvatureCenterPlacement()`
- `curveFrameAliasMode()`
- `applyDatumLineAliasPlacement()`
- `applyDatumPointAliasPlacement()`

名称只是建议；语义必须以 FreeCAD source 和 expected 为准。

## 完成条件

- `C5M16-BLK-301` 和 `C5M16-BLK-401` 具备可实现合同。
- alias fixture 覆盖 DatumLine 与 DatumPoint representative。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线
```
