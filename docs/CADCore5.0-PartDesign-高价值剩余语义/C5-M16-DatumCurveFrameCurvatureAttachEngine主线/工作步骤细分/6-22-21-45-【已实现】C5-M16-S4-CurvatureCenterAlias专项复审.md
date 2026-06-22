# 【已实现】C5-M16-S4 CurvatureCenter / Alias 专项复审

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

## S4 冻结结论

`C5M16-BLK-301` 的可实现合同：

- FreeCAD 在 `mmRevolutionSection` / `mmConcentric` 中先复用 S3 的 Frenet math：`T = d.Normalized()`，`N = dd - T * (dd dot T)`，`B = T x N`。
- `SectionOfRevolution` 先设置 `SketchNormal = T.Reversed()`、`SketchXAxis = N.Reversed()`；`Concentric` 先设置 `SketchNormal = B`、`SketchXAxis = T`。
- 之后 curvature center 才覆盖 base：若 `N.Magnitude() == 0.0`，必须报 `path has infinite radius of curvature` / undefined curvature diagnostic，不能用 bbox、circle center 或 world axis 猜测。
- 曲率公式固定为 `curvature = dd.Dot(N) / pow(d.Magnitude(), 2)`，base point 固定为 `p + N * (1 / curvature)`；S6 expected 需覆盖 curved edge / circle representatives。

`C5M16-BLK-401` 的可实现合同：

- `AttachEngineLine` 的 `modeRefTypes` 复用 3D branch：`mm1AxisCurv -> mmRevolutionSection`、`mm1Binormal -> mmFrenetTN`、`mm1Normal -> mmFrenetTB`。
- `AxisOfCurvature` 计算时映射为 `mmRevolutionSection`，并额外应用 presuper placement，把 line convention 从 Z 方向旋到 Y 方向；alias expected 必须记录 source 3D mode 与最终 line direction。
- `Binormal` 只映射到 `FrenetTN`，`Normal` 只映射到 `FrenetTB`；不能在 `datum_line.cpp` 复制 T/N/B 或 curvature 业务规则。
- `AttachEnginePoint` 的 `mm0CenterOfCurvature` 复用 `mmRevolutionSection`；point executor 只消费 shared placement 并按点形状 convention 输出 base / placement。
- cad-core 边界：alias helper 只能调用 shared 3D curve-frame / curvature helper，再做 line / point shape convention 或 presuper transform；`datum_line.cpp`、`datum_point.cpp` 不承载曲线帧业务判断。

## S6 fixture plan

Success fixture `c51m5/partdesign-datum-curve-frame-modes.json` 至少覆盖：

- curved edge / circle 上的 `Concentric` 与 `SectionOfRevolution`，校验 curvature center base、normal、X axis 和 map mode。
- line aliases：`AxisOfCurvature`、`Normal`、`Binormal`，记录 source 3D mode、line direction 和 presuper transform 效果。
- point alias：`CenterOfCurvature`，记录 source 3D mode、point base 和 placement。

Diagnostics fixture `c51m5/partdesign-datum-curve-frame-diagnostics.json` 至少覆盖：

- straight line 或等效 `N == 0` 触发 infinite radius / undefined curvature diagnostic。
- `Concentric` / `SectionOfRevolution` 不允许在 undefined N 时退回 bbox、circle center assumption、world axes 或 default placement。

## 状态结论

- `C5M16-BLK-301` / `C5M16-BLK-401` 已具备可实现合同，仍是 `pending_native_oracle`，不声明 supported。
- `TangentU/V`、`OnEdge/Tangent` 等非本包 release 项继续 out-of-scope，除非后续 live blocker/source route 单独证明。
- 下一步进入 S5 request-local placement / capability 专项复审；C5-M15 S6 未关闭前，M16 S6 仍不得发布 capability 或移除 exact blocker。

## 完成条件

- `C5M16-BLK-301` 和 `C5M16-BLK-401` 具备可实现合同。
- alias fixture 覆盖 DatumLine 与 DatumPoint representative。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线
```
