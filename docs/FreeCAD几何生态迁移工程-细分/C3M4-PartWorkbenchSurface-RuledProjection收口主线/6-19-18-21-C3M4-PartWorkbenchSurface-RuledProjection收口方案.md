# C3M4 Part Workbench Surface RuledSurface / ProjectionOnSurface 收口方案

## 当前基线

- 方案时间：2026-06-19。
- 当前仓库：`/Users/li/Chili3DProject/FreeCAD`。
- S2 live HEAD：`7d1776d1d0`（`7d1776d1d0 docs: 完成PARTSURF S1源码裁决`）。
- 脏工作区边界：当前仍有既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp` 改动；另有其它 Surface / CADCore 主线未跟踪 docs。S2 只更新本 RuledProjection 主线 docs/矩阵/步骤文件，不暂存、不回退、不覆盖这些既有改动。
- S0 已冻结边界：PARTCONIC `工作步骤细分` 队列为空；PARTSURF 队列在 S0 重命名后推进到 S1；full Part surface family 和 `Part::ProjectOnSurface` 仍不得发布为 supported。
- S1 裁决边界：`Part::RuledSurface` 是 source-backed `DocumentObject`；cad-core 落点是 Part executor + `TopoShapeExpansion` 等价 helper + topo provenance，不是 adapter 特例。S3 第一批默认只纳入 edge/edge；wire/wire 只有在 S2 oracle/input 证明可控时才扩入；`Part::ProjectOnSurface` 只做 S4 裁决，禁止混入 S3。
- S2 fixture/oracle 结论：S3 required fixtures 固定为 `part-ruled-surface-line-line`、`part-ruled-surface-conic-line`、`part-ruled-surface-orientation-reversed`、`part-ruled-surface-invalid-input`。fixture JSON 必须表达 `DocumentObject Type=Part::RuledSurface`，通过 `Properties.Curve1` / `Properties.Curve2` / `Properties.Orientation` 和 link/subname 输入，不允许 adapter 特例直接输出 face。FreeCAD expected collector 优先创建 source-backed `Part::RuledSurface` object；若 conic-line 只能用 `Part.makeRuledSurface()`，等价边界只覆盖 `RuledSurface::execute()` link resolve 后进入 `makeElementRuledSurface` 的 edge/edge geometry，不覆盖 link/subname diagnostics 或 topo provenance。wire/wire 因缺少 collector/input/provenance 验收而 defer；`Part::ProjectOnSurface` 仅保留 S4 candidate。
- S3 实现结论：`cad-core` 已实现 source-backed `Part::RuledSurface` edge/edge 第一批，覆盖 `Curve1` / `Curve2` App::PropertyLinkSub、`Orientation=Automatic/Forward/Reversed`、`BRepFill::Face`、源 edge 到输出 edge provenance、四个 p8 fixtures/expected/tests 和 collector native/fallback 路径。`wire/wire` 仍 deferred；`ProjectOnSurface` 当时 routed-S4，S4 已进一步拆入独立主线。
- S4 裁决结论：选择 split-later 出口 B，不在本主线实现 `Part::ProjectOnSurface`。原因是当前 collector 未启用 `Part::ProjectOnSurface` native type、`set_property()` 未支持普通 `App::PropertyLinkSubList`，cad-core 未有 `part_project_on_surface` executor / CMake / registry / projection named-shape 策略；即使第一批仅做 `Mode=Edges Height=0 Offset=0`，也需要先在独立主线闭合 collector、input schema、diagnostics 和 topo provenance 裁决。本主线 S5 发布口径固定为 `Part::RuledSurface` supported，`Part::ProjectOnSurface` source-audited / planned。
- S5 发布结论：`cad-core/src/adapters/c_api/c_api.cpp` 已新增 `part_workbench.ruled_surface` 精确 capability；CADCore3.0 文档和 oracle 队列只发布已验证的 `Part::RuledSurface` edge/edge 第一批。PARTCONIC 的 `conic_curves` 不再把 `ruled_surface` 作为 gap；它只声明 conic edge 可进入 `Part::RuledSurface` consumer，不声明 fake `Part::Hyperbola` / `Part::Parabola` DocumentObject。`wire/wire`、`ProjectOnSurface` 和 full Part surface family 仍为 remaining gaps / planned。
- 上一轮 PARTCONIC 已收口：`Part.Hyperbola` / `Part.Parabola` geometry wrapper 已通过 `PartConicCurveDTO` 输出有限 edge，并已验证 Hyperbola / Parabola edge 可进入 `Part::Extrusion` consumer 输出 `occt_face`。
- 新主线定位：把 PARTCONIC 明确保留的 full Part surface family / RuledSurface / ProjectionOnSurface gap 拆成 source-backed Part Workbench surface 主线；第一实现批次优先 `Part::RuledSurface`，`Part::ProjectOnSurface` 先做源码裁决和 fixture 分批，不和 RuledSurface 直接混成一个实现任务。

## 为什么接这条主线

- PARTCONIC 的发布口径只覆盖 typed conic edge 与 `Part::Extrusion` edge-to-face consumer，没有覆盖 FreeCAD Part Workbench 的曲面对象。
- FreeCAD 当前源码里存在 source-backed `Part::RuledSurface` `DocumentObject`，属性和 execute 路径清晰，且核心几何落在 `TopoShape::makeElementRuledSurface()`；这是最适合作为第一批 Part surface executor 的入口。
- `Part::ProjectOnSurface` 也是 source-backed `DocumentObject`，但它同时包含 support face、projection list、mode filter、height/offset、project face/wire、wire repair、face rebuild 和 solid 分支。它适合作为同主线内的 S4 裁决对象，不适合在第一批和 RuledSurface 混写。
- S4 已把 `Part::ProjectOnSurface` 拆到 `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/`，避免 S5 把未实现的 projection family 和 RuledSurface edge/edge 混成一个 supported 能力。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::RuledSurface`：声明 `Orientation`、`Curve1`、`Curve2`，并覆盖 `execute()` / `mustExecute()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()`：属性组为 `"Ruled Surface"`，`OrientationEnums` 为 `Automatic` / `Forward` / `Reversed`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`：依次读取 `Curve1` / `Curve2`，通过 `Part::Feature::getTopoShape(... ShapeOption::ResolveLink | ShapeOption::Transform)` 或 `NeedSubElement` 获取 shape，错误短句包括 `"No shape linked."`、`"Not exactly one sub-shape linked."`、`"Invalid link."`，随后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())` 并写入 `Shape`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：要求两个输入，错误短句包括 `"Wrong number of input shapes"`、`"Null input shape"`、`"Input shape has no edge"`、`"Input shape forms more than one wire"`；接受 edge / wire，非 edge/wire 时从输入提取 single wire / single edge 或 `makeElementWires()`；edge/wire 类型不一致时把 edge 转 wire。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：`Automatic` orientation 通过两条曲线端点附近采样和三角面法向点积判断是否反转第二条曲线；`Reversed` 直接反转第二条曲线。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：edge 分支调用 `BRepFill::Face(Edge, Edge)`，wire 分支调用 `BRepFill::Shell(Wire, Wire)`；由于 OCCT 会修改输入边，FreeCAD 用 `findSubShapesWithSharedVertex()` 找回输出边并 `resetElementMap()`，最后 `makeShapeWithElementMap(res.getShape(), Mapper(), edges, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface`：声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`，并有 `createProjectedWire()`、`projectFace()`、`projectWire()`、`fixWire()`、`createFaceFromParametricWire()`、`createSolidIfHeight()` 等内部步骤。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`：读取 support face 与 projection shapes，按 `Direction` 投影，经过 `filterShapes()` 后写 `Shape`，并恢复原 `Placement`；相关错误短句包括 `"No support face specified"`、`"Expect exactly one support face"`、`"Number of objects and sub-names differ"`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectWire()`：使用 `BRepProj_Projection(wire, supportFace, dir)`，取距离参考形状最近的 projected wire，再拆成 edge 输出。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectFace()`：对 face wires 投影、`fixWire()`，再可能通过 surface parametric wire rebuild face，并在 `Height > 0 && Mode == All` 时生成 prism solid；`filterShapes()` 还会按 `"All"` / `"Faces"` / `"Edges"` 改变输出集合。

## cad-core 当前落点

- `cad-core/src/runtime/feature_registry.cpp` 当前已注册 `Part::RuledSurface`；`Part::ProjectOnSurface` 仍未注册，S4 已裁决为后续独立主线。
- `cad-core/include/cad_core/part/part_feature.h` 当前已有 `executePartRuledSurface()` 声明；实现位于 `cad-core/src/part/part_ruled_surface.cpp`，adapter 未做 face 特例。
- `cad-core/src/part/part_geometry_curve.cpp` 已能创建 request-local Hyperbola / Parabola edge，并允许 `partGeometryCurveConsumers` 中的 `Part::Line` / `Part::RuledSurface` 通过同一 executor 消费 conic edge；没有注册假的 `Part::Hyperbola` / `Part::Parabola` DocumentObject。
- S3 落点：`cad-core/src/part/topo_shape_expansion.*` 暴露 `makeElementRuledSurfaceFromEdges()` 承接 edge/edge `BRepFill::Face`、orientation 和 shared-vertex source edge relation；`cad-core/src/part/part_ruled_surface.cpp` 负责 property/link/diagnostic/metadata；`cad-core/src/topo` 通过 named shape element_map/history 输出 provenance。
- S3 collector 落点：`cad-core/tools/collect_freecad_expected.py` 已加入 `Part::RuledSurface` native type 支持；line-line 和 orientation-reversed 走 native expected；conic-line 走 `Part.makeRuledSurface()` fallback，expected reference 标明只覆盖 link resolve 后 edge/edge geometry。
- S5 发布落点：`cad-core/src/adapters/c_api/c_api.cpp` 暴露 `part_workbench.ruled_surface`，`cad-core/tests/test_adapters.py` 断言四个 fixture、edge/edge 边界、wire/wire / ProjectOnSurface / full family gap，以及 conic consumer 不代表 fake conic DocumentObject。
- S4 collector/input 缺口：`cad-core/tools/collect_freecad_expected.py` 尚未把 `Part::ProjectOnSurface` 加入 `SUPPORTED_NATIVE_TYPES`，且 `set_property()` 只支持 `App::PropertyLinkSubListHidden`，未支持 `ProjectOnSurface::Projection` 需要的普通 `App::PropertyLinkSubList`；因此本主线不采集 projection expected、不新增 projection fixture。

## 最小完整语义批次

第一批不追求 full Part surface family，默认只实现 source-backed `Part::RuledSurface` 的 edge/edge 分支闭环：

- `Part::RuledSurface` `DocumentObject` executor 注册、属性解析、`Curve1` / `Curve2` link-sub shape acquisition、`Orientation` enum 解析已完成。
- Edge / edge 输入输出 `TopoDS_Face`，覆盖 `Automatic`、`Forward`、`Reversed` 三种 orientation 的可验证行为已完成。
- `part-ruled-surface-conic-line` 使用上一轮 PARTCONIC 的 conic edge producer，证明 conic edge 不是只能进入 `Part::Extrusion`，也能进入 `Part::RuledSurface`。
- Invalid link / wrong input shape / missing edge 输出稳定 diagnostics，不坠落为笼统 `execution_failed`。
- 输出 subshape / element map 至少保护源 edge 到输出边的可追溯关系；focused tests 和 expected 断言 element_map/history，不靠 fixture 名、bbox、face 数量或输出端修剪猜测所有权。

Wire / wire 的 `BRepFill::Shell` 分支和非 edge/wire 输入自动提取 wire 的分支在 S2 已裁决为 deferred。当前没有 source-backed collector、fixture input schema 和 shell/topo provenance 验收闭环；S3 不得把 wire/wire 扩入 required，也不得发布 full RuledSurface support。

## S2 fixture / oracle 设计

- S3 required fixtures 固定为 `part-ruled-surface-line-line`、`part-ruled-surface-conic-line`、`part-ruled-surface-orientation-reversed`、`part-ruled-surface-invalid-input`。
- JSON schema 固定走 DocumentObject：`Objects[]` 或等价 consumer object 必须声明 `TypeId: "Part::RuledSurface"`，`Properties.Curve1` / `Properties.Curve2` 使用 `App::PropertyLinkSub` 的 `value` + `SubList` / `StableSubList` 指向源 edge，`Properties.Orientation` 使用 `App::PropertyEnumeration` 的 `Automatic` / `Forward` / `Reversed`。
- `part-ruled-surface-line-line`、`part-ruled-surface-orientation-reversed`、`part-ruled-surface-invalid-input` 的 expected collector 必须优先创建 native `Part::RuledSurface` object，覆盖真实 `RuledSurface::execute()` 的 link/subname/diagnostics 路径。
- `part-ruled-surface-conic-line` 可复用 PARTCONIC 的 request-local conic edge producer；若 native FreeCAD 不能把该 DTO edge materialize 成 `DocumentObject` link，collector 才允许用 `Part.makeRuledSurface(conic_shape, line_shape)` 采集 edge/edge geometry expected。该路径的等价边界是：FreeCAD `RuledSurface::execute()` 已解析出两个 edge shape 后调用 `makeElementRuledSurface()`；它不证明 `Curve1` / `Curve2` link schema、invalid link diagnostics 或 source edge provenance。
- `test_p8_features.py` 后续只应为这四个 fixture 添加 focused tests；`test_expected_fixtures.py` 继续通过 checked-in expected 做通用 parity，不用为 S2 新增 expected 文件。

## ProjectionOnSurface 分批原则

`Part::ProjectOnSurface` 已完成本主线内的 S4 source audit，但实现拆到后续独立主线，禁止混入 S3/S5 的 RuledSurface 发布口径。后续实现必须基于 S1/S4 的源码裁决做以下分批：

- 第一批仅允许 `Mode=Edges`、`Height=0`、`Offset=0`、单 edge/wire 投影到单 support face，直接对齐 `projectWire()` / `BRepProj_Projection`。
- 第一批前必须先补 collector native type、普通 `App::PropertyLinkSubList` setter、cad-core `Part::ProjectOnSurface` executor 注册、明确 unsupported diagnostics 和 projected edge topo provenance policy。
- `Mode=Faces` / `All`、face rebuild、holes、`ShapeFix_Wire`、`createSolidIfHeight()`、offset placement、多个 projection shape 的 compound 顺序，必须单独列入后续 fixture，不得在没有 oracle 的情况下随手实现。
- 本主线 S5 发布口径固定为 `Part::RuledSurface` 第一批 supported、`ProjectOnSurface` source-audited / planned。

## 非目标

- 不实现完整 Part surface family；`Loft`、`Sweep`、`Filling`、`PipeShell`、GeomPlate 等不在本主线第一批。
- 不实现 GUI / ViewProvider / TaskPanel / Workbench 命令行为。
- 不把 BREP 放入请求或响应作为长期状态；fixture expected 只能作为 oracle artifact。
- 不把 `ProjectOnSurface` 的所有 mode / height / offset / face rebuild / solid 分支一次性写成 supported。
- 不靠 bbox、fixture 名、face 顺序、输出端 pruning 或后处理补猜 topo ownership。
- 不把上一轮 `PartConicCurveDTO` 当作假的 FreeCAD `DocumentObject`；它只能作为 request-local producer 或 fixture helper。

## 工作包结构

- `矩阵/part_surface_source_candidates.tsv`
- `矩阵/part_surface_scope_review_matrix.tsv`
- `矩阵/part_surface_blocker_queue.tsv`
- `矩阵/part_surface_non_goal_registry.tsv`
- `矩阵/part_surface_fixture_oracle_matrix.tsv`
- `工作步骤细分/6-19-18-22-PARTSURF-S0-live基线与范围冻结.md`
- `工作步骤细分/6-19-18-23-PARTSURF-S1-FreeCAD源码与批次裁决.md`
- `工作步骤细分/6-19-18-24-【已实现】PARTSURF-S2-fixture与oracle矩阵设计.md`
- `工作步骤细分/6-19-18-25-【已实现】PARTSURF-S3-RuledSurface首批实现.md`
- `工作步骤细分/6-19-18-26-【已实现】PARTSURF-S4-ProjectionOnSurface裁决与分批.md`
- `工作步骤细分/6-19-18-27-【已实现】PARTSURF-S5-能力发布与提交闸门.md`

后续独立主线草案：

- `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/6-19-19-18-C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线草案.md`
- `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/矩阵/part_project_on_surface_plan_matrix.tsv`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线 docs/CADCore3.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
python3 -m unittest tests.test_adapters
```

重型收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_diagnostics tests.test_mvp tests.test_p5_sketch tests.test_p8_features tests.test_expected_fixtures
```
