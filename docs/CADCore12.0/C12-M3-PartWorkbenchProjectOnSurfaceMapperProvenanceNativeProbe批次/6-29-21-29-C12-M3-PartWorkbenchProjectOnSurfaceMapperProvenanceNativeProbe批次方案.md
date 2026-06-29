# C12-M3 Part Workbench ProjectOnSurface Mapper Provenance Native Probe 批次方案

## 背景

C12-M2 S5 对 ProjectOnSurface 的结论是：原生 `FeatureProjectOnSurface` 能构造 geometry，但 source-backed mapper/history 仍不可见；S6 因此发布 `native_hidden` 和 `no_code_oracle_blocked_gate`。C5-M9 的 ProjectOnSurface fixtures 当前保留 source-backed known-gap expected，文件本身也标明需要等 native mapper history expected 替换。

C12-M3 不再扩大到 Sweep、Filling、GeomPlate 或 Loft。它只沿 ProjectOnSurface provenance 这一条线追问：是否能通过 FreeCAD 原生 `TopoShapePy` / `TopoShape` / `PropertyTopoShape` API，在不依赖 GUI session 或跨请求 native state 的前提下，导出源 subelement 到目标 Edge/Wire/Face 的稳定 history。

## 方法

1. 冻结 C12-M2 S6 后的 live baseline、队列状态和 no-code 禁止项，确认 C12-M3 不是 implementation 包。
2. 从 `FeatureProjectOnSurface.cpp`、`TopoShapePyImp.cpp`、`TopoShapeExpansion.cpp`、`PropertyTopoShape.cpp`、C5-M9 expected 和当前 cad-core landing 建立 source candidate matrix。
3. 做范围准入：edge/wire split、face rebuild、all-compound/height/offset、invalid diagnostic 是否属于 request-local mapper provenance；凡依赖持久 native document、GUI session、完整 BREP 或 output guessing 的行直接挡住。
4. 复用 C12-M2 S3 harness 思路，但固定 C12-M3 provenance artifact：输入 fixture、FreeCAD/OCCT runtime、object result shape summary、history API 调用、source endpoint、target endpoint、request-local judgement、classification 和 current comparison path。
5. S4 只采或阻断原生可观测性：尝试 object result shape、intermediate projected wire/face、`getElementHistory`、`mapShapes`、`mapSubElement`、ElementMap save/load 等 request-local API。若仍得到 `None` 或只能靠顺序/bbox 猜测，分类为 `native_hidden_retained`。
6. S5 只比较 expected-ready row。没有 stable native provenance artifact 时，不运行 current mismatch 结论，不改 expected，不改 tests。
7. S6 发布结果：no-code retained、current-covered，或另开 implementation package 的最小授权边界。

## 本包不做什么

- 不新增或修改 `cad-core` C++。
- 不刷新 `cad-core/fixtures/c5m9/expected/*.freecad.json` 为 native-supported 状态。
- 不新增测试断言、adapter 字段、capability wording 或 frontend mock。
- 不从 EdgeN 顺序、bbox、result topology 数量或 fixture 名称推断 provenance。
- 不把 FreeCAD GUI / Workbench / cross-request native document state 变成 CAD Core 产品边界。
- 不把 native-hidden、collector bug、TypeError、timeout 或 crash 升级成 backend gap。

## 矩阵职责

| matrix | purpose |
| --- | --- |
| source candidates | 记录 ProjectOnSurface / TopoShape history source authority、当前 C5-M9 expected 和 cad-core landing。 |
| scope review | 判断每个 provenance axis 是否属于 request-local 产品边界。 |
| blocker queue | 跟踪 schema、FreeCADCmd、native-hidden、collector、current comparison 和 product boundary blocker。 |
| non-goal registry | 固化 GUI/session/persistent geometry/output guessing 等明确不进入 CAD Core 的行为。 |
| backend gap classification | 只在 S6 才允许把 expected-ready mismatch 行升级为 implementation candidate。 |
| probe matrix | 记录 S4 native provenance probe 的输入、artifact 命名和通过标准。 |
| validation matrix | 记录 docs/TSV/queue 验收、S4 probe 和 S5 comparison 验收。 |

## 后续分流

- 若 S4 仍为 native-hidden：关闭为 no-code retained，只保留 future native API / collector blocker。
- 若 S4 expected-ready 但 S5 current-covered：更新分类为 current-covered，不开实现包。
- 若 S4 expected-ready 且 S5 current mismatch：S6 只建议另开 C12-M4 implementation 包，落点优先是 `cad-core/src/part/part_project_on_surface.cpp`、`cad-core/include/cad_core/part/topo_shape_mapper.h` 和 `cad-core/src/topo` 的 mapper/history API，而不是 adapter 或 output 修剪。

## 验收

创建阶段只做文档验收。运行步骤时，S4 可在本机 FreeCADCmd 环境采集 native probe；若 sandbox 报 Qt / processor 限制，只能记录为 sandbox runtime limitation，不能当作 FreeCAD expected 失败。
