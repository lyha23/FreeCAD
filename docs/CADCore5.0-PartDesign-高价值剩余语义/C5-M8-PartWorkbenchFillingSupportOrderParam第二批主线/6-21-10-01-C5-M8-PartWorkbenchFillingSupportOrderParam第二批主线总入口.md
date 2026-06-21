# C5-M8 Part Workbench Filling Support / Order / Param 第二批主线

本包承接 C5-M7 后的 Part Workbench Surface family 剩余项，但只打开 `Part.makeFilledFace(...)` / source-backed `Part::FilledFace` helper 这一条 API 边界。

范围边界：本包交付物只落在 `cad-core`、fixtures、tests、capabilities 和文档矩阵；FreeCAD `src/` 只作为语义依据读取，不在本包内修改上游源码来修复 helper oracle。若当前 `FreeCADCmd` 无法稳定采集 `surface` / `supports` / `orders` expected，后续 step 必须记录 source-backed known_gap 或 diagnostic-backed 边界。

收口状态：C5-M8 已完成 S0-S5。支持声明只覆盖 `Part.makeFilledFace(...)` request-local helper；native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native feature、cross-request mutable `Part.BRepOffsetAPI.MakeFilling` wrapper 和完整 Part surface family 均保持 non-goal。

## 目标

C5-M8 不做“单个 support/order case”，而是把同一 FreeCAD 调用链、同一 cad-core request DTO/API 边界、同一类 expected 能覆盖的代表场景放进同一轮：

- 复核第一批 Filling support：`Boundary=LinkSubList`、closed wire default、connected boundary edges default、invalid input diagnostics 和 `maker_history:filling` 必须继续成立。
- 补齐 `surface` / `supports` / `orders` 在 `Part.makeFilledFace(...)` helper 内的 request DTO、source resolution、OCCT builder 调用；若 native helper oracle 不稳定，只发布 source-backed known_gap / diagnostic-backed fixtures，不伪造 expected。
- 补齐 `degree`、`ptsOnCurve`、`numIter`、`anisotropy`、`tol2d`、`tol3d`、`tolG1`、`tolG2`、`maxDegree`、`maxSegments` 这一组非默认构造参数。
- 补齐同一 `TopoShape::makeElementFilledFace()` 中的非边界约束：额外 edge / wire、face support、vertex point constraint，并保持 source evidence 与 diagnostics 可定位。
- 对 compound optional case 与直接 `Part.BRepOffsetAPI.MakeFilling` wrapper 做 owner 判定：能证明同一 request-local DTO 的进入支持；不能证明则收敛为 stable diagnostic，不伪造持久 wrapper。

## 入口文件

- 方案：`6-21-10-01-C5-M8-PartWorkbenchFillingSupportOrderParam第二批方案.md`
- scope 矩阵：`矩阵/c5m8_filling_support_order_param_scope.tsv`
- fixture / oracle 矩阵：`矩阵/c5m8_filling_support_order_param_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m8_filling_support_order_param_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m8_filling_support_order_param_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m8_filling_support_order_param_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live guard | `c3m4/part-filling-closed-wire-default`、`part-filling-boundary-edges-default`、`part-filling-invalid-inputs`、`c4m1/part-filling-advanced-deferred` | S0 固定当前第一批 expected-backed / diagnostic-backed 基线，防止第二批扩大时破坏默认路径 |
| surface / support / order | `surface` 初始面、boundary edge + support face、boundary edge order C0/G1/G2、invalid target/order diagnostics | S1 已扩展 `FilledFaceSource` / request DTO / `makeElementFilledFaceFromSources()`；当前为 source-backed known_gap + diagnostic-backed，native helper expected 和 G2 stable geometry 后续关闭 |
| non-default params | constructor 参数、`SetConstrParam` / `SetResolParam` / `SetApproxParam` 等价字段、invalid range diagnostics | S2 已移除逐字段 deferred，改为 source-backed constructor params metadata；native helper geometry expected 因 FreeCADCmd explicit kwargs 退出 245 保持 known_gap，invalid params 为 diagnostic-backed |
| non-boundary constraints | 选定 boundary wire 后剩余 edge / wire、face、vertex 作为 non-boundary constraints；edge+support/order；point constraint | S3 已对齐 FreeCAD `IsBound=false` / face / vertex 分支；wire 与 face/point expected-backed，edge support/order 保持 native helper known_gap，diagnostics locatable |
| compound / wrapper boundary | compound optional source expansion；直接 `Part.BRepOffsetAPI.MakeFilling` wrapper 的 add/build/shape lifecycle 和 UV point-on-support 分支 | S4 已关闭：compound optional 为 expected-backed；direct wrapper / UV point-on-support 为 `unsupported_wrapper_lifecycle` diagnostic-backed，并保留 request-local DTO 删除条件 |
| capability closeout | docs、capability metadata、root matrices、remaining gaps | S5 已发布精确 support / known_gap / diagnostic / non-goal 边界，并清空本包队列 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现原生 FreeCAD `Part::FilledFace` DocumentObject；本包仍是 source-backed `Part.makeFilledFace(...)` helper。
- 不实现 Surface Workbench `Surface::Filling` feature、GUI TaskPanel、ViewProvider 或 command panel。
- 不把 `Part.BRepOffsetAPI.MakeFilling` 直接 Python wrapper 伪造成跨请求持久对象；除非它能落到同一 request-local DTO，否则只做 diagnostic owner。
- 不把 GeomPlate、Sweep advanced PipeShell、ProjectOnSurface、Loft complex profile family 或完整 Part surface family 混入本包。
- 不用 bbox、fixture 名、输出顺序或 adapter 后处理修剪替代 `BRepOffsetAPI_MakeFilling` / maker history / ElementMap 路径。
