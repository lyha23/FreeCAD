# C4-M1 Sweep / Filling / GeomPlate 补完方案

## 目标

按 FreeCAD 调用链补 Sweep advanced、Filling advanced helper、GeomPlate advanced helper。继续区分 helper 与原生 DocumentObject，不迁移 GUI feature。

## 范围

- Sweep / PipeShell：multi-profile、Linearize、advanced wrapper、spine / section resolver。
- Filling：surface、supports、orders、non-default params、non-boundary constraints。
- GeomPlate：initial surface、G1 / projected 2D / 2D point、custom criteria、`Part.PlateSurface.Curves` wrapper。

## 当前收口

- Sweep：multi-profile + `Linearize=true` 已按 `Sweep::execute -> makeElementPipeShell -> Linearize` expected-backed；advanced PipeShell wrapper 选项保留 locatable deferred diagnostic。
- Filling：`Surface` / `Supports` / `Orders` / non-default params 不伪装为 supported；当前按 `makeFilledFace` wrapper 风险和 `makeElementFilledFace` support/order/source-map 缺口拆到 deferred diagnostic。
- GeomPlate：3D curve/point constraints 和非默认 approximation 参数已 expected-backed；initial surface、2D/projected constraints、`PlateSurface.Curves` wrapper 保留 locatable deferred diagnostic。

## 非目标

- 不把 Filling / GeomPlate helper 发布为 GUI feature。
- 不声明 full surface family。
- 不把 Hole internal PipeShell 混入 Sweep capability。
