# C3M4 Part Workbench Surface RuledSurface / ProjectionOnSurface 收口方案

## 当前基线

- 方案时间：2026-06-19。
- 当前仓库：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`a8080d9b30`（`a8080d9b30 feat: 收口PARTCONIC S5能力发布`）。
- 脏工作区边界：当前仍有既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp` 改动；本主线只新增 docs/矩阵/步骤方案，不暂存、不回退、不覆盖这两个 Sketcher 改动。
- S0 live 队列结论：`step_goal_queue.py` 对 PARTCONIC `工作步骤细分` 返回空队列；对 PARTSURF `工作步骤细分` 返回 6 个 pending，起点为 `6-19-18-22-PARTSURF-S0-live基线与范围冻结.md`。
- S0 冻结边界：本步骤只冻结 live 基线、队列起点和范围口径，不改代码、不采集 oracle、不实现 cad-core；full Part surface family 和 `Part::ProjectOnSurface` 仍不得发布为 supported。
- 上一轮 PARTCONIC 已收口：`Part.Hyperbola` / `Part.Parabola` geometry wrapper 已通过 `PartConicCurveDTO` 输出有限 edge，并已验证 Hyperbola / Parabola edge 可进入 `Part::Extrusion` consumer 输出 `occt_face`。
- 新主线定位：把 PARTCONIC 明确保留的 full Part surface family / RuledSurface / ProjectionOnSurface gap 拆成 source-backed Part Workbench surface 主线；第一实现批次优先 `Part::RuledSurface`，`Part::ProjectOnSurface` 先做源码裁决和 fixture 分批，不和 RuledSurface 直接混成一个实现任务。

## 为什么接这条主线

- PARTCONIC 的发布口径只覆盖 typed conic edge 与 `Part::Extrusion` edge-to-face consumer，没有覆盖 FreeCAD Part Workbench 的曲面对象。
- FreeCAD 当前源码里存在 source-backed `Part::RuledSurface` `DocumentObject`，属性和 execute 路径清晰，且核心几何落在 `TopoShape::makeElementRuledSurface()`；这是最适合作为第一批 Part surface executor 的入口。
- `Part::ProjectOnSurface` 也是 source-backed `DocumentObject`，但它同时包含 support face、projection list、mode filter、height/offset、project face/wire、wire repair、face rebuild 和 solid 分支。它适合作为同主线内的 S4 裁决对象，不适合在第一批和 RuledSurface 混写。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::RuledSurface`：声明 `Orientation`、`Curve1`、`Curve2`，并覆盖 `execute()` / `mustExecute()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()`：属性组为 `"Ruled Surface"`，`OrientationEnums` 为 `Automatic` / `Forward` / `Reversed`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`：依次读取 `Curve1` / `Curve2`，通过 `Part::Feature::getTopoShape(... ShapeOption::ResolveLink | ShapeOption::Transform)` 或 `NeedSubElement` 获取 shape，随后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())` 并写入 `Shape`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：要求两个输入；接受 edge / wire，非 edge/wire 时从输入提取 single wire / single edge 或 `makeElementWires()`；edge/wire 类型不一致时把 edge 转 wire。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：`Automatic` orientation 通过两条曲线端点附近采样和三角面法向点积判断是否反转第二条曲线；`Reversed` 直接反转第二条曲线。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：edge 分支调用 `BRepFill::Face(Edge, Edge)`，wire 分支调用 `BRepFill::Shell(Wire, Wire)`；由于 OCCT 会修改输入边，FreeCAD 用 `findSubShapesWithSharedVertex()` 找回输出边并 `resetElementMap()`，最后 `makeShapeWithElementMap(res.getShape(), Mapper(), edges, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface`：声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`，并有 `createProjectedWire()`、`projectFace()`、`projectWire()`、`fixWire()`、`createFaceFromParametricWire()`、`createSolidIfHeight()` 等内部步骤。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`：读取 support face 与 projection shapes，按 `Direction` 投影，经过 `filterShapes()` 后写 `Shape`，并恢复原 `Placement`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectWire()`：使用 `BRepProj_Projection(wire, supportFace, dir)`，取距离参考形状最近的 projected wire，再拆成 edge 输出。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectFace()`：对 face wires 投影、`fixWire()`，再可能通过 surface parametric wire rebuild face，并在 `Height > 0 && Mode == All` 时生成 prism solid。

## cad-core 当前落点

- `cad-core/src/runtime/feature_registry.cpp` 当前注册了 `Part::Extrusion`、`Part::Offset`、`Part::Thickness`、`Part::Section`、`Part::BooleanFragments` 等 Part executor，但没有 `Part::RuledSurface` 或 `Part::ProjectOnSurface`。
- `cad-core/include/cad_core/part/part_feature.h` 当前没有 `executePartRuledSurface()` / `executePartProjectOnSurface()` 声明；S3 第一批需要新增 source-backed executor，而不是通过 adapter 特例直接输出 shape。
- `cad-core/src/part/part_geometry_curve.cpp` 已能创建 request-local Hyperbola / Parabola edge，并有 `partGeometryCurveConsumers` 喂给 `Part::Extrusion` 的先例。RuledSurface 第一批可以复用这条 producer 证据，但不应把 surface 逻辑塞回 `part_geometry_curve.cpp`。
- 建议新增或扩展 `cad-core/src/part/part_ruled_surface.cpp` / `cad-core/include/cad_core/part/part_ruled_surface.h`，并在需要时补 `cad-core/src/part/topo_shape_expansion.*` 的 `makeElementRuledSurface` 等价 helper。`ProjectOnSurface` 若进入实现，应另起 `part_project_on_surface.*`，不和 RuledSurface 混在一个 executor。

## 最小完整语义批次

第一批不追求 full Part surface family，只实现 source-backed `Part::RuledSurface` 的 edge 分支闭环：

- `Part::RuledSurface` `DocumentObject` executor 注册、属性解析、`Curve1` / `Curve2` link-sub shape acquisition、`Orientation` enum 解析。
- Edge / edge 输入输出 `TopoDS_Face`，覆盖 `Automatic`、`Forward`、`Reversed` 三种 orientation 的可验证行为。
- 至少一个 fixture 使用上一轮 PARTCONIC 的 conic edge producer，证明 conic edge 不是只能进入 `Part::Extrusion`，也能进入 `Part::RuledSurface`。
- Invalid link / wrong input shape / missing edge 输出稳定 diagnostics，不坠落为笼统 `execution_failed`。
- 输出 subshape / element map 至少保护源 edge 到输出边的可追溯关系；不得靠 fixture 名、bbox、face 数量或输出端修剪猜测所有权。

Wire / wire 的 `BRepFill::Shell` 分支和非 edge/wire 输入自动提取 wire 的分支先在 S1/S2 矩阵中列为 candidate。若 oracle 和 cad-core shape acquisition 已具备清晰路径，S3 可一起纳入；否则必须在 S3 文档中说明为何拆到后续批次，避免假装 full RuledSurface 已支持。

## ProjectionOnSurface 分批原则

`Part::ProjectOnSurface` 保留在本主线内，但不作为 S3 第一实现批次。S4 必须基于 S1/S2 的源码与 fixture 结果做以下裁决：

- 若只需要小批次实现，第一批仅允许 `Mode=Edges`、`Height=0`、`Offset=0`、单 edge/wire 投影到单 support face，直接对齐 `projectWire()` / `BRepProj_Projection`。
- `Mode=Faces` / `All`、face rebuild、holes、`ShapeFix_Wire`、`createSolidIfHeight()`、offset placement、多个 projection shape 的 compound 顺序，必须单独列入后续 fixture，不得在没有 oracle 的情况下随手实现。
- 如果 S4 判断 ProjectOnSurface 超出本主线可控范围，应输出新的后续主线草案，并把本主线 S5 发布口径固定为 `Part::RuledSurface` 第一批 supported、`ProjectOnSurface` source-audited / planned。

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
- `工作步骤细分/6-19-18-24-PARTSURF-S2-fixture与oracle矩阵设计.md`
- `工作步骤细分/6-19-18-25-PARTSURF-S3-RuledSurface首批实现.md`
- `工作步骤细分/6-19-18-26-PARTSURF-S4-ProjectionOnSurface裁决与分批.md`
- `工作步骤细分/6-19-18-27-PARTSURF-S5-能力发布与提交闸门.md`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
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
