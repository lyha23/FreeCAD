# InternalFace 内部面与 history 细化方案

## 范围

本文只细化 Sketch `InternalFace` / `InternalEdge` / `InternalVertex` 的内部面生成与 history 传播。它服务于 `docs/草图实现/5-31-09-28-草图到拉伸补齐方案.md` 的阶段二、阶段三。

不在本文范围内：

- 前端选择器和 Pad 参数写回。
- 通用 Extrude 命令设计。
- Sketcher 约束求解器迁移。
- Pad / Pocket 的 Length、Two sides、UpToFace、taper 等已有 FeatureExtrude 语义扩展。

## 目标

补齐一条 FreeCAD-like 的后端内部面管线：

1. 根据 sketch 几何生成带 source id 的 OCC edges。
2. 按 FreeCAD `FaceMakerBuildFace` / `WireJoiner` 语义完成交点分割、wire 归并、open wire 保留和 bounded face 生成。
3. 为 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 生成稳定的 request-local subshape 输出。
4. 把 split / generated / modified / deleted history 传播到 sketch `NamedShape` / `ElementMap`，支持后续 `StableSubList` 解析。
5. 对无法支持的复杂 region 返回明确 diagnostic，不做 fixture 特判、不生成假 region。

## FreeCAD 调用链依据

后续实现按以下 FreeCAD 源文件建立语义映射：

- `src/Mod/Sketcher/App/SketchObject.h::buildInternals(const Part::TopoShape& edges)`：Sketch 内部面构造入口。
- `src/Mod/Sketcher/App/SketchObject.h::getElementTypes(bool all)`：Sketch 暴露 `InternalFace` / `InternalEdge` / `InternalVertex`。
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::splitSelfIntersecting()`：自相交边分割。
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::splitAtIntersections()`：边与边交点分割。
- `src/Mod/Part/App/FaceMakerBuildFace.cpp::Build_Essence()`：平面判定与 `BOPAlgo_BuilderFace` 面域生成。
- `src/Mod/Part/App/FaceMaker.cpp::postBuild()`：FaceMaker 构造后的 history 消费与 shape 返回。
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP`：`EdgeInfo`、`WireInfo`、`superEdge`、closed/open wire、split/merge history 的账本。
- `src/Mod/Part/App/WireJoiner.cpp::getOpenWires()` / `getResultWires()`：open wire 与 result wire 输出。
- `src/Mod/Part/App/WireJoiner.cpp::Generated()` / `Modified()` / `IsDeleted()`：用于后续 history 映射的 BRepBuilderAPI 风格接口。

## 当前 cad-core 基线

当前实现已有：

- `cad-core/src/features/sketch_object.cpp::buildOptionalProfileFace()`：能从闭合 wire 生成 profile face。
- `cad-core/src/geometry/face_maker.cpp::makeFacesFromClosedWiresAndSplitEdges()`：能处理简单 closed wire + open splitter 形成多个 `InternalFaceN`。
- `cad-core/src/features/sketch_object.cpp::executeSketchObject()`：把 `internalShape` 写入 `context.shapes`、`context.subshapes` 和 `context.mesh`。
- `cad-core/src/features/feature_extrude.cpp::resolveSketchInternalFaceProfile()`：Pad / Pocket 能按 `Profile.SubList=["InternalFaceN"]` 选择内部面。

当前不足：

- 没有 FreeCAD 等价的 `WireJoinerP::EdgeInfo` / `WireInfo` 账本。
- 没有记录 source edge 到 split fragment 的 generated / modified history。
- 没有 sketch `internalShape` 的完整 `NamedShape` / `ElementMap`。
- 多面域依赖简化 splitter，不覆盖完整交叉、自交、样条、复杂 open wire。
- `InternalFaceN` 仍主要是本次 recompute 内的编号，不具备完整跨重建稳定恢复能力。

## 从 opencascade-rs 草图支持补齐的实现要点

### 1. FaceMakerBuildFace 不能只返回 face list

后续 `geometry/face_maker.*` 或新的 `geometry/sketch_internal_builder.*`
不应只把 `FaceMakerBuildFace` 当作“输入 wire -> 输出 faces”的几何 helper。FreeCAD 里
`TopoShape::makeElementFace()` 后面还要消费 `FaceMaker::Build()` 和 `FaceMaker::postBuild()`
的运行态。

需要保留或等价表达：

- `shape` / `shapes_to_return`：业务层以最终 shape 为准，faces 只作为诊断和测试辅助。
- `myPreSplitHistory` / `myPreSplitCompound`：自相交边预分割后，能把 original edge 映射到 fragments。
- `mySplitter` / splitter shape：edge-edge intersection 分割完成后，能用 `MapperMaker(mySplitter)` 继续传播 history。
- `FaceMaker::postBuild()`：先 `mapSubElement(mySourceShapes)`，再串接 pre-split history、splitter history，最后根据 face outer wire 的 edge history 生成 face element naming。

`Build_Essence()` 的几何细节也要按 FreeCAD 迁移，而不是在 executor 层补面：

- `findPlane()` 没有 supplied plane 时，先 copy edges 到 compound，再用 `BRepLib_FindSurface(comp, -1, Standard_True)` 找平面。
- `splitSelfIntersecting()` 对非 line/conic 自交 edge 切 fragment，并记录 `AddModified(original, fragment)`。
- `splitAtIntersections()` 使用 non-destructive splitter；splitter 失败时继续使用原 edges，不返回空结果。
- base face 用足够大的外包平面，且必须 `TopAbs_FORWARD`。
- 每条 edge 以 forward / reversed 两个方向加入 `BOPAlgo_BuilderFace`。
- 调用 `BRepLib::BuildPCurveForEdgesOnPlane()`。
- `SetAvoidInternalShapes(Standard_True)`，避免 dangling edges 变成会在 extrusion 中制造退化几何的 internal wires。
- 过滤外部无限大区域和零面积区域。

### 2. WireJoiner 要迁移 final EdgeInfo ownership

`WireJoiner` 的核心不是“最后删掉一些 open fragment”，而是每条 edge 从 split 开始就进入同一本账：

```text
source edge
  -> split fragments
  -> EdgeInfo / WireInfo working ledger
  -> findClosedWires / findTightBound / exhaustTightBound 改写 ownership
  -> final EdgeInfo ownership
  -> result wires + openWireCompound
  -> MapperHistory / NamedShape / ElementMap
```

需要补的等价结构：

- `FinalEdgeArena`：用稳定 `EdgeId` 代替临时 Vec 下标，保存 edge shape、superEdge、source/split key、`iteration`、`iteration2`、primary / secondary owner、removed 状态和 history identity。
- `FinalWireInfoArena`：保存 `WireInfo` 等价记录，edge vertex 只引用 `EdgeId`，`wireInfo` / `wireInfo2` 只指向 final wire id。
- `FinalOwnershipSnapshot`：`build_closed_wire()` 之后生成，只读驱动 `getResultWires()`、`getOpenWires()`、history 和 `ElementMap`。

最终 open wire 导出只能按 FreeCAD 条件：

```text
iteration == -3 || (!wireInfo && iteration >= 0)
```

这里的 `wireInfo` 必须是 final live ownership，不是中间 graph/search 状态。`wireInfo2`
只表达 FreeCAD 允许的一条 edge 被两个 tight-bound wires 共享，不能用 source/split 几何规则补出来。

### 3. InternalShape 组合与 open-wire product identity

`SketchObject::buildInternals()` 的业务顺序固定为：

```text
FaceMakerBuildFace result
  -> WireJoiner::getOpenWires(openWires, "SKF")
  -> result.makeElementCompound({result, openWires})
```

约束：

- `FaceMakerBuildFace` 失败并抛出异常时，FreeCAD 外层 catch 返回空 `InternalShape`，不会再继续执行 `WireJoiner::getOpenWires()`。
- `WireJoiner.getOpenWires()` 和 `WireJoiner.getResultWires()` 是不同 API；不要把 `getResultWires()` 接进 `buildInternals()` 当补 face 路径。
- open-wire compound 应保留 WireJoiner 产物 identity，再进入最终 `{face_result, openWires}` 组合；不要为了输出整齐而重建/copy open-wire product，避免破坏与 face product 共享的 vertex / edge identity。
- 如果 face 数已经对齐但 edge / vertex under-export，优先看 WireJoiner openWires child、open-wire product identity 和 history / ElementMap 消费，而不是回 sketch executor 补 edge。

### 4. 失败定位顺序

内部面偏差按四层定位，不跳层：

1. `FaceMakerBuildFace` 的 face / edge / vertex 几何结果是否与 FreeCAD 一致。
2. `WireJoiner::getOpenWires()` 的 open wire 几何结果是否与 FreeCAD 一致。
3. raw compound / child shape identity 是否在组合时被重建或复制。
4. `NamedShape` / `ElementMap` 是否完整消费 `MapperHistory(aHistory)`。

如果前 1-3 层一致，只是 stable subname、internal element 或 source trace 不一致，应归类为 history 到 `ElementMap` 的传播缺口。

## 分层落点

### `features/sketch_object.cpp`

只负责 FreeCAD SketchObject 的业务调用顺序：

- 解析 `Geometry` / `Constraints` / `ExternalGeometry` / `SketchPlaneFrame` / `Placement`。
- 构造 raw sketch shape。
- 调用 geometry 层生成 `SketchInternalBuildResult`。
- 把结果写入 `context.shapes`、`context.subshapes`、`context.mesh`、`context.namedShapes`。

不得在这里做：

- 按 fixture 名称分支。
- 根据几何类型猜 region ownership。
- 手工按输出顺序修剪 `InternalFaceN`。
- 合成 split history。

### `geometry/`

承接 OCC 构造和 FreeCAD-like 内部面运行态：

- `geometry/sketch_internal_builder.*`：新的内部面主入口。
- `geometry/face_maker.*`：FaceMakerBuildFace 等价能力。
- `geometry/wire_joiner.*`：WireJoiner 等价账本与输出。

该层可以使用 OCC shape、edge、wire、face 和 BRepBuilderAPI 风格 history，但不依赖 document graph。

### `topo/`

承接命名和 history 消费：

- `topo/sketch_internal_history.*`：把 geometry 层账本转成 `NamedShape` / `ElementMap`。
- 处理 `InternalFace` / `InternalEdge` / `InternalVertex` 的稳定名、split、deleted、modified、generated。
- 为 `elementReferenceUpdates` 提供可消费的 old -> new 映射。

### `features/feature_extrude.cpp`

只消费结果：

- `Profile.SubList` 指向 `InternalFaceN` 时，从 sketch `internalShape` 取对应 face。
- `StableSubList` 解析由 `topo` / `NamedShape` 负责。
- 多面域未选择继续 diagnostic，不默认选第一个。

## 核心数据结构建议

### `SketchSourceEdge`

保存 sketch 原始几何到 OCC edge 的来源：

- `sourceIndex`：Geometry 数组中的索引。
- `sourceKind`：Line、ArcOfCircle、ArcOfEllipse、BSpline 等。
- `edge`：原始 OCC edge。
- `parameterRange`：原始曲线参数范围。
- `localName`：初始 `EdgeN` 或后续内部命名候选。

用途：后续 split fragment 和 stable subname 都必须能追溯到 source edge。

### `SketchSplitFragment`

保存 split 后的边片段：

- `fragmentId`：本次构建内唯一 id。
- `sourceEdgeId`：来自哪个 `SketchSourceEdge`。
- `edge`：split 后的 OCC edge。
- `sourceParameterRange`：在原始 edge 上的范围。
- `generatedFrom` / `modifiedFrom`：history 输入。

用途：支撑 `InternalEdgeN` 和 face boundary history。

### `SketchWireRecord`

保存 WireJoiner 结果：

- `wireId`。
- `closed`。
- `edges`：片段 id 列表和方向。
- `role`：outer、inner、open、unknown。
- `superEdge` / merged source：对齐 WireJoiner 中 super edge 概念。

用途：区分 bounded face、hole、open profile，不把 open wire 强行混进 face。

### `SketchInternalFaceRecord`

保存生成的内部面：

- `faceId`。
- `face`：OCC face。
- `outerWire`。
- `innerWires`。
- `boundaryFragments`。
- `sourceEdges`。
- `historySources`：参与面域生成的 source / fragment。

用途：生成 `InternalFaceN`、mesh face id、ElementMap 入口。

### `SketchInternalBuildResult`

geometry 层输出给 feature 层的聚合结果：

- `rawShape`：原始 sketch shape。
- `internalShape`：face / open wire compound。
- `faces`：`SketchInternalFaceRecord[]`。
- `internalEdges` / `internalVertices`。
- `openWires`。
- `history`：generated / modified / deleted / split。
- `diagnostics`。
- `requiresSubshapeSelection`。

## 算法流程

### 1. 生成 source edges

从 sketch `Geometry` 生成 OCC edges，并给每条 edge 标 source id。失败的 edge 不静默跳过，必须进入 diagnostic。

验收点：

- Line、ArcOfCircle、ArcOfEllipse、BSpline 都能生成 source edge 或给出明确 unsupported diagnostic。
- source id 与输入 `Geometry` 索引稳定对应。

### 2. 平面和 placement

使用 sketch 已计算好的 transform，把所有 source edges 放到 sketch 工作平面。`SketchPlaneFrame` 与 `Support` / `AttachmentSupport` 的冲突继续在 feature 层诊断。

验收点：

- 同一 sketch 在 `Placement` 和 `SketchPlaneFrame` 下生成同样拓扑结构，只是坐标变换不同。

### 3. 交点分割

迁移 `FaceMakerBuildFace` 的交点处理：

- 自相交边分割。
- edge-edge intersection 分割。
- split 失败时记录 diagnostic 和 fallback 条件。
- 记录 source edge -> fragment 的 generated / modified history。

验收点：

- inter-edge intersection 和 self-intersection 都产生 fragment history。
- 不允许在 executor 中按 fixture 形态补 split fragment。

### 4. WireJoiner 归并

建立 cad-core 等价 `WireJoiner` 账本：

- 初始化 `EdgeInfo` / `WireInfo` 等价结构。
- 归并可闭合 wire。
- 保留 open wire。
- 记录 merge、split、superEdge 和 wire ownership。

验收点：

- bounded faces 与 open wires 同时存在时，二者都能输出。
- open wire 不生成假 face。
- `getOpenWires()` 与 `getResultWires()` 的语义分开。

### 5. FaceMaker 生成 bounded faces

用分割后的 closed wires 建 face：

- 识别 outer / inner wire。
- 支持 holes。
- 支持多 face compound。
- 生成 `InternalFaceN` 时保持稳定的本次构建顺序。

验收点：

- 单矩形 -> 1 个 `InternalFace`。
- 矩形 + 内圆 -> 1 个带孔 `InternalFace`。
- 矩形 + 贯穿分割线 -> 2 个 `InternalFace`。
- 多独立闭合轮廓 -> 多个 `InternalFace`。

### 6. 构建 InternalShape 与 subshape 输出

把 faces、internal edges、internal vertices、open wires 合成 `internalShape`，同时输出 subshape 表：

- `InternalFaceN`。
- `InternalEdgeN`。
- `InternalVertexN`。
- raw `EdgeN` / `VertexN` 仍保留为 sketch raw shape 的拾取对象。

验收点：

- `mesh.faceIds` 使用 `InternalFaceN`。
- `subshapes[].id` 与 `mesh.faceIds` 一一可对应。

### 7. 生成 NamedShape / ElementMap

把 geometry history 转成 topo history：

- source edge generated fragment。
- fragment generated / modified face boundary。
- face split / deleted / preserved。
- old stable subname 到 current `InternalFaceN` 的映射。

验收点：

- `StableSubList=["旧InternalFace稳定名"]` 能解析到当前 `InternalFaceN`。
- split 时返回 split diagnostic，不默认选第一个。
- deleted 时返回 deleted diagnostic。

## StableSubList 语义

正式输入：

```json
"Profile": {
  "PropertyType": "App::PropertyLinkSub",
  "value": "Sketch",
  "SubList": ["InternalFace1"],
  "StableSubList": ["InternalFace1"]
}
```

规则：

1. `SubList` 是当前或上次可见 subname。
2. `StableSubList` 是同一选择项对应的稳定身份。
3. `StableSubList` 可省略；省略时后端按 `SubList` 同值处理。
4. `StableSubList` 存在时长度必须和 `SubList` 一致。
5. `FullSubList` 不属于正式接口，出现必须 `invalid_link_value`。

后续 history 完整后，`StableSubList` 应优先通过 sketch `ElementMap` 解析；解析失败才考虑当前 `SubList` 是否仍可直接命中。不得把二者合并成单字段。

## Diagnostics 设计

建议稳定以下诊断：

- `unsupported_profile_region`：FaceMaker / WireJoiner 暂不支持的区域组合。
- `invalid_subshape`：`SubList` 指向不存在或非 `InternalFaceN` 的 subshape。
- `split_stable_subname`：稳定名被拆成多个候选，不能自动选。
- `deleted_stable_subname`：稳定名对应的内部面已删除。
- `unsupported_stable_subname`：稳定名不在当前 `ElementMap` / history 中。
- `invalid_link_value`：`StableSubList` 长度不一致或出现 `FullSubList`。

## Fixture 计划

### Oracle 采集纪律

复杂内部面 fixture 应优先用本地 FreeCAD oracle 采集，而不是从当前 `cad-core` 输出回填 expected。
可复用 opencascade-rs 的 fixture 经验：

- 每个 case 用独立 JSON 描述 sketch 几何，expected 由本地 `FreeCADCmd` 创建内存文档、启用 `Sketch.MakeInternals`、执行 `doc.recompute()` 后采集。
- expected 至少记录 `InternalShape` 的 face / edge / vertex 数量、面积/质心/bbox、wire / hole 数量、普通 `Shape` 摘要和 Python 暴露的 internal element map。
- `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 纯命名顺序差异不作为硬失败；face/edge/vertex 数量、几何内容、稳定 subname 或引用语义不一致仍是失败。
- 新增或修复 case 时，仍按 `FaceMakerBuildFace`、`WireJoiner`、raw compound/child identity、`NamedShape` / `ElementMap` 四层定位。

### P5 扩展

- `sketch-internal-face-hole`：外矩形 + 内圆孔。
- `sketch-internal-face-two-closed-profiles`：两个独立闭合轮廓。
- `sketch-internal-face-intersection-split`：两条边相交产生 split fragments。
- `sketch-internal-face-self-intersection`：自交路径。
- `sketch-internal-face-open-and-bounded`：bounded face 与 open wire 同时存在。
- `pad-internal-face-stable-after-insert-edge`：插入不影响目标面的边后稳定恢复。

### Known gap fixtures

对短期无法完成的复杂 BSpline / 椭圆相交 case，先加入 known gap 或 diagnostic fixture，不允许用输出补丁伪造通过。

### 建议补充的复杂覆盖桶

以下 case 类型来自 opencascade-rs 已用过的 FreeCAD internal-face oracle corpus，可按 cad-core P5/P6
能力逐步迁入：

- `rectangle_with_center_cutter`：矩形 + 单条 open cutter。
- `two_open_cutters_four_regions`：矩形 + 两条 open cutter，覆盖 repeated splitWire。
- `shared_edge_adjacent_rectangles`：共享边闭合矩形，覆盖 `wireInfo2` 竞争。
- `three_level_nested_rectangles`：outer / hole / island 三层嵌套。
- `intersecting_holes_outline_merge`：相交 hole，覆盖 outline 合并。
- `non_xy_outer_hole_island`：非 XY 平面嵌套 profiles。
- `circle_circle_lens_network`：多圆交点和 lens fragment。
- `partial_overlap_with_cutter`：部分重叠线段 + cutter。
- `cubic_bspline_self_intersection` / `bezier_self_intersection`：样条自交。
- `arc_line_mixed_profiles`：圆弧 + 直线混合 profile。
- `hyperbola_parabola_arc_profiles`：双曲线弧 / 抛物线弧 + chord 闭合线。
- `intersecting_open_edges_no_profile`：只有相交 open edges，无闭合 profile。
- `tangent_circles_no_micro_face`：近切圆不应生成微小假面。
- `closed_profiles_with_multiple_open_cutters`：bounded faces 与 open cutters 同时存在。
- `dense_conic_spline_intersection_network`：conic / spline 密集交点网络。
- `overlap_duplicate_cutter_chain`：duplicate / reversed / partial overlap 线段。
- `shared_edge_with_cross_cutters`：shared source edge 上的 tight-bound split 顺序。
- `tangent_arc_spline_cutter_competition`：near-tangent 与 conic/spline open-wire 竞争。
- `multi_dangling_open_star_no_profile`：FaceMakerBuildFace 失败后应得到空 `InternalShape`。
- `nested_intersecting_holes_with_open_cutters`：split-hole 与 open diagnostics 同时出现。
- `periodic_bspline_profile_with_cross_cutters`：periodic spline closure 与 open cutters。
- `touching_hole_vertex_with_cutter`：hole 顶点接触外轮廓并叠加 cutters。
- `rotated_ellipse_hole_spline_cutters`：旋转椭圆、重叠 conic holes 和 spline cutters。

## 实施顺序

1. 抽出 `SketchInternalBuildResult`，让当前简化实现先走新结构，行为不变。
2. 给 source edges 和 split fragments 建账本，先覆盖 line segment。
3. 迁移 inter-edge intersection split，记录 fragment history。
4. 迁移 self-intersection split。
5. 建立 `WireJoiner` 等价 `WireInfo` / `EdgeInfo`，区分 closed / open wires。
6. 接入 holes 与多 closed profiles。
7. 生成 sketch internal `NamedShape` / `ElementMap`。
8. 让 `Profile.StableSubList` 通过 `ElementMap` 解析到当前 `InternalFaceN`。
9. 补 `elementReferenceUpdates`。
10. 扩展 Arc / Ellipse / BSpline，逐步替换 known gaps。

## 验收命令

后端实现阶段优先运行：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_diagnostics
```

涉及 Pad / Pocket profile 消费时补跑：

```bash
python3 -m unittest tests.test_feature_flows tests.test_p7_features tests.test_p8_features
```

## 风险与边界

- 最大风险是把 FaceMaker / WireJoiner 的内部账本简化成输出后处理。出现这种迹象应暂停继续加规则，回到 FreeCAD 账本映射。
- `InternalFaceN` 的编号顺序只要求本仓库稳定；与 FreeCAD 顺序不一致但几何等价时，作为命名顺序差异处理。
- 真正影响 Pad / Extrude 稳定性的不是编号顺序，而是 `StableSubList -> ElementMap -> 当前 InternalFaceN` 的解析能力。
- 前端不要依赖 `InternalFaceN` 跨重建稳定；跨重建稳定只认 `StableSubList`。
