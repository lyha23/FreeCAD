# P6：TopoNaming 主路径

P6 把稳定引用从导出层补丁升级为 CAD Core 的正式账本。目标是让 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap` 和 MapperHistory 传播 stable subname。

## 当前基线

- `topo/named_shape` 建立 object-local indexed `FaceN` / `EdgeN` / `VertexN` 账本。
- 支持 identity、source-preserved、一对一 history-derived `ElementMap` 和多个旧 stable key 指向同一当前元素的 merge history。
- 普通 prism、非 taper Two sides / Symmetric 单-prism、Two sides UpTo 多 prism XOR 子流程已消费 maker history 子集。
- taper 已暴露 `BRepOffsetAPI_ThruSections` maker 与 loft section，并按 FreeCAD `MapperThruSections` 补 `GeneratedFace()` / first-section 映射，记录一侧 / 内环 taper 的 source face first-section history，以及各 taper source edge 和 offset section 的 generated history；该 ThruSections history 现在下沉为 `topo::namedShapeForTaperedExtrusionHistory()`，Pad / Pocket 和 Part::Extrusion 复用同一账本。source edge / offset section history 可通过 XOR / boolean 组合透传到一侧、多侧和内环 taper 结果；对象级结果暴露 `topo_naming_history=history_partial:taper`，但仍保留 `known_gap:taper_history`，因为完整 MapperHistory 生命周期尚未迁移。
- `BRepBuilderAPI_RefineModel` 已按 FreeCAD `modelRefine` / `FaceUniter` 路径迁入 `geometry/refine_model.*`，Refine history 已按 FreeCAD `MyRefineMaker::populate()` + `GenericShapeMapper::init()` 收敛：先消费 `Modified()` 账本，再对 result face 补共享边 / 同面 generated 映射；Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 均走该路径。
- `AddSubShape` cache 已保存 slot 级 `NamedShape`：Pad/Pocket/Hole 的 add/sub tool、DressUp delta cache 和 `SupportTransform` cache 不再被迫复用对象最终 Shape 的 ElementMap；Body boolean 和 transformed family 消费 add/sub slot 时使用对应 slot history。
- transformed family 按 BaseFeature / Body 前缀恢复 support 时，会消费前序 AddSub feature 的 final-result `Refine=true` Shape 和 RefineModel history，而不是只使用未 refine 的 add/sub tool cache。
- `makeElementXorFromSources` 和 `makeElementBooleanFromSources` 已下沉到 topo。
- Body additive / subtractive 组合通过 `BRepAlgoAPI_Fuse/Cut` 的多源 maker history 传播 source alias。
- transformed copy 已下沉到 `topo::namedShapeForTransformedCopy()`，按 FreeCAD `makeElementTransform()` / `copyElementMap(tmp, op)` 保留 source-prefixed alias、旧 stable key、merge history 和 split / deleted terminal history。
- Link retag 后续 maker 现在会继续保留上游 merge history 的精确 source ledger；P8 `app-link-body-merge-history-retag` 约束 `Body` boolean merge 经 `App::Link` 后仍能追溯到原 `Pocket.Edge3` / `SketchPocket.Edge1` 来源。
- UpToFace 和 Sketch ExternalGeometry 可通过 `StableSubList -> ElementMap -> current subname` 更新引用；带 `ReferenceShadow` 的普通 Shape LinkSub 校验也会先消费 `NamedShape.elementMap`，再刷新 `SubList` / `ShadowSub` / ReferenceShadow fingerprint。
- UpToFace 的 indexed stable subname、Body preserved source 和 Body history source 已有 FreeCADCmd native geometry expected；oracle collector 已支持 `PartDesign::Body` / `Pad` / `Pocket`，并在原生 FreeCAD 采集时把 `StableSubList` 作为 post-resolution `PropertyLinkSub` subname 输入。
- Sketch ExternalGeometry 的直接 indexed stable subname 已有 FreeCADCmd native geometry expected；collector 已通过 `SketchObject::addExternal()` 原生入口采集投影对照。
- Sketch ExternalGeometry 的 source-prefixed stable key 子集已有 FreeCADCmd native expected；collector 按 FreeCAD `rebuildExternalGeometry()` 的 missing reference 路径，用原 target object 加 `resolveElement().oldName` 生成投影对照；cad-core 已补 collapsed line -> construction point 和 Body profile-source oldName 投影语义。
- split / deleted / unsupported stable subname 有结构化 diagnostics；MapperHistory 同时给出同类 modified target 和低阶 generated target 时，已按同类唯一 target 自动恢复旧 stable 引用；Sketch ExternalGeometry split source edge 已按 FreeCAD collapsed line projection 恢复成 construction point；deleted / split terminal history 已能跨后续 Body boolean / maker、Link retag 和 transformed copy 继续保留诊断语义。
- Sketch `InternalShape` 已对 `InternalFaceN` 记录 generated-from-outer-boundary-`EdgeN` history，hole / inner wire 不混入同一个 face 来源，且不生成 raw `FaceN` alias；raw sketch edge -> 多个 `InternalEdgeN` 的 one-source-to-many 关系记录 terminal split history，自交单边 pre-split 也记录为 raw `EdgeN` 到多个 `InternalEdgeN` 的 terminal split history，raw sketch `EdgeN/VertexN` 被过滤且没有 Internal* target 的 one-source-to-zero 关系记录 terminal deleted history；这些非一对一关系不写入可解析 `ElementMap`，只为后续 stable reference diagnostics 和 history 传播保留证据。

## 已知缺口

- 完整 MapperHistory 生命周期尚未迁移。
- ShapeFix、DressUp、transformed copy 的完整 maker history 仍未覆盖；taper 当前仍按 partial history 和 `known_gap:taper_history` 验收。
- split 的完整自动旧引用恢复还不完整；当前只恢复 MapperHistory 能证明同类唯一 target 或 ExternalGeometry collapsed point 的旧 stable 引用；merge 已能记录并跨 Link retag 传播，但 ShapeFix / transformed / DressUp 等完整 MapperHistory 生命周期仍待收敛。
- FaceMaker / WireJoiner 的完整 history 消费需与 P5 geometry 账本联动；当前只固定了 `InternalFaceN` 的 outer-boundary generated history、self-intersecting edge pre-split terminal split history、raw sketch edge 到多个 `InternalEdgeN` 的 terminal split history，以及 raw sketch `EdgeN/VertexN` 被过滤后的 terminal deleted history 子集。
- 更复杂的 source-prefixed stable key 仍要服从完整 MapperHistory；同类一对多 split 和 deleted 只输出结构化诊断，不伪造可解析投影。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `topo/named_shape.*` | NamedShape、ElementMap、maker history helper |
| `topo/element_map.*` | sketch internal element map 与 InternalShape generated / split / deleted history 子集 |
| `features/feature_extrude.*` | prism source history |
| `features/part.*` | Part::Extrusion prism / taper maker history 发布 |
| `geometry/extrusion_helper.*` | taper `BRepOffsetAPI_ThruSections` maker 与 section 来源 |
| `geometry/refine_model.*` | FreeCAD `BRepBuilderAPI_RefineModel` / `FaceUniter` maker |
| `geometry/face_maker.*` | P5 closed-wire face-with-holes / island 构面，后续接入 FaceMaker history |
| `features/body.*` | Body boolean history 和 AddSubShape slot 级 NamedShape 消费 |
| `features/transformed.*` | transformed copy source alias 和 AddSubShape slot 级 NamedShape 消费 |
| `runtime/recompute.*` | ReferenceShadow 校验和 elementReferenceUpdates；普通 Shape LinkSub 与 Sketch internal LinkSub 都必须走当前 ElementMap / shadow evidence |
| `tools/collect_freecad_expected.py` | FreeCADCmd 原生几何 expected；P6 UpToFace stable-subname 和 Sketch ExternalGeometry source-prefixed stable key 生成 post-resolution 对照 |

## FreeCAD 依据

- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `src/Mod/Part/App/TopoShape.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`
- `src/App/PropertyLinks.cpp`
- `src/App/GeoFeature.cpp`

## 验收

- `fixtures/p6` 覆盖 indexed named shape、source-preserved key、Body boolean history、stable subname 恢复、同类唯一 split target 自动恢复、deleted diagnostics，以及 deleted / split terminal history 跨后续 Body boolean 的传播；P7 / P8 fixture 继续约束 transformed copy、Link retag 后的 terminal history diagnostics，以及 Link retag 后的 merge history 保留。
- P6 UpToFace stable-subname indexed / Body preserved / Body history 成功几何 expected 来自本机 FreeCADCmd；不再按 `stable_subname_oracle_pending` 跳过；ReferenceShadow 回归约束旧 `SubList` + stable Body face 引用会经 ElementMap 写回当前 `SubList` 和 `ShadowSub`。
- P6 Sketch ExternalGeometry direct indexed 与 source-prefixed stable-subname 子集成功几何 expected 来自本机 FreeCADCmd；source-prefixed 走 FreeCAD `resolveElement().oldName` 恢复路径，不从 cad-core 当前输出回填；嵌套 Body profile-source 已固定 native projection oldName oracle。
- P6 split fixture 约束 `Pad.Face5 -> Face4`、`Pocket.Edge1 -> Edge22` 这类同类唯一 target 写入 ElementMap；Sketch ExternalGeometry split edge 进入 FreeCAD native collapsed point 投影，不再退化为 `split_stable_subname` 或 `unsupported_geometry`。
- Body boolean fixture 约束多个旧 stable key 指向同一当前元素时记录 `merge` history。
- P3b taper fixture 约束一侧 / 内环 taper 的 ThruSections source face first-section history，以及一侧、多侧和内环 taper 的 source edge / offset section generated history；Pad / Pocket / Part::Extrusion taper 对象必须同时暴露 `topo_naming_history=history_partial:taper` 和 `topo_naming=known_gap:taper_history`。
- P7 Refine fixture 约束 Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 走 RefineModel + GenericShapeMapper history，并导出包含 modified / generated / deleted / merge 关键来源的 history_partial `NamedShape`。
- P7 DressUp / transformed fixture 约束 `SupportTransform` cache、refined prefix support 的 transformed copy 和 Body 后续 history 传播保留原 feature、transformed copy 与 support source。
- 一对多 fragment 只能记录 split history，不写入可解析 `ElementMap`；P5 through-open-cutter 已约束 raw open cutter split 为多个 `InternalEdgeN` 时只记录 split history。
- 自交单边 pre-split 只能记录 terminal split history，不写入可解析 `ElementMap`；P5 cubic figure-8 BSpline 已约束 raw `Edge1` split 为多个 `InternalEdgeN`，并约束 generated `InternalFaceN` 仍追溯到 `Edge1`。
- 无目标 source 只能记录 deleted history，不写入可解析 `ElementMap`；P5 dangling-line 已约束 filtered raw open edge 及其 free endpoint vertex 只记录 deleted history。
- `InternalFaceN` 只能记录 outer-boundary generated history，不写入 raw `FaceN` alias；P5 closed internal face 已约束 `InternalFace1` 来源于 `Edge1..Edge4` 且 `Face1` 不可解析，closed-wire hole 已约束 hole edge 不混入外框 face 来源。
- 不能靠输出端排序或 fixture 名称修正稳定引用。
