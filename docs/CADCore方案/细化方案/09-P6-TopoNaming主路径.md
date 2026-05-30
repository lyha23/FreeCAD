# P6：TopoNaming 主路径

P6 把稳定引用从导出层补丁升级为 CAD Core 的正式账本。目标是让 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap` 和 MapperHistory 传播 stable subname。

## 当前基线

- `topo/named_shape` 建立 object-local indexed `FaceN` / `EdgeN` / `VertexN` 账本。
- 支持 identity、source-preserved、一对一 history-derived `ElementMap` 和多个旧 stable key 指向同一当前元素的 merge history。
- 普通 prism、非 taper Two sides / Symmetric 单-prism、Two sides UpTo 多 prism XOR 子流程已消费 maker history 子集。
- taper 已暴露 `BRepOffsetAPI_ThruSections` maker 与 loft section，并按 FreeCAD `MapperThruSections` 补 `GeneratedFace()` / first-section 映射，记录一侧 / 内环 taper 的 source face first-section history，以及各 taper source edge 和 offset section 的 generated history；source edge / offset section history 可通过 XOR / boolean 组合透传到一侧、多侧和内环 taper 结果，但仍保留 `known_gap:taper_history`，因为完整 MapperHistory 生命周期尚未迁移。
- `BRepBuilderAPI_RefineModel` 已按 FreeCAD `modelRefine` / `FaceUniter` 路径迁入 `geometry/refine_model.*`，Refine history 已按 FreeCAD `MyRefineMaker::populate()` + `GenericShapeMapper::init()` 收敛：先消费 `Modified()` 账本，再对 result face 补共享边 / 同面 generated 映射；Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 均走该路径。
- `AddSubShape` cache 已保存 slot 级 `NamedShape`：Pad/Pocket/Hole 的 add/sub tool、DressUp delta cache 和 `SupportTransform` cache 不再被迫复用对象最终 Shape 的 ElementMap；Body boolean 和 transformed family 消费 add/sub slot 时使用对应 slot history。
- transformed family 按 BaseFeature / Body 前缀恢复 support 时，会消费前序 AddSub feature 的 final-result `Refine=true` Shape 和 RefineModel history，而不是只使用未 refine 的 add/sub tool cache。
- `makeElementXorFromSources` 和 `makeElementBooleanFromSources` 已下沉到 topo。
- Body additive / subtractive 组合通过 `BRepAlgoAPI_Fuse/Cut` 的多源 maker history 传播 source alias。
- transformed copy 已下沉到 `topo::namedShapeForTransformedCopy()`，按 FreeCAD `makeElementTransform()` / `copyElementMap(tmp, op)` 保留 source-prefixed alias、旧 stable key、merge history 和 split / deleted terminal history。
- UpToFace 和 Sketch ExternalGeometry 可通过 `StableSubList -> ElementMap -> current subname` 更新引用。
- split / deleted / unsupported stable subname 有结构化 diagnostics；MapperHistory 同时给出同类 modified target 和低阶 generated target 时，已按同类唯一 target 自动恢复旧 stable 引用，只有同类一对多仍保留 split 诊断；deleted / split terminal history 已能跨后续 Body boolean / maker、Link retag 和 transformed copy 继续保留诊断语义。

## 已知缺口

- 完整 MapperHistory 生命周期尚未迁移。
- ShapeFix、DressUp、transformed copy 的完整 maker history 仍未覆盖；taper 当前仍按 partial history 和 `known_gap:taper_history` 验收。
- split 的完整自动旧引用恢复还不完整；当前只恢复 MapperHistory 能证明同类唯一 target 的旧 stable 引用，同类一对多仍保留 split 诊断；merge 当前已记录 history，但仍需和完整 MapperHistory 生命周期收敛。
- FaceMaker / WireJoiner 的 history 消费需与 P5 geometry 账本联动。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `topo/named_shape.*` | NamedShape、ElementMap、maker history helper |
| `topo/element_map.*` | sketch internal element map |
| `features/feature_extrude.*` | prism source history |
| `geometry/extrusion_helper.*` | taper `BRepOffsetAPI_ThruSections` maker 与 section 来源 |
| `geometry/refine_model.*` | FreeCAD `BRepBuilderAPI_RefineModel` / `FaceUniter` maker |
| `geometry/face_maker.*` | P5 closed-wire face-with-holes / island 构面，后续接入 FaceMaker history |
| `features/body.*` | Body boolean history 和 AddSubShape slot 级 NamedShape 消费 |
| `features/transformed.*` | transformed copy source alias 和 AddSubShape slot 级 NamedShape 消费 |

## FreeCAD 依据

- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `src/Mod/Part/App/TopoShape.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`

## 验收

- `fixtures/p6` 覆盖 indexed named shape、source-preserved key、Body boolean history、stable subname 恢复、同类唯一 split target 自动恢复、deleted diagnostics，以及 deleted / split terminal history 跨后续 Body boolean 的传播；P7 / P8 fixture 继续约束 transformed copy 和 Link retag 后的 terminal history diagnostics。
- P6 split fixture 约束 `Pad.Face5 -> Face4`、`Pocket.Edge1 -> Edge22` 这类同类唯一 target 写入 ElementMap；下游 fixture 不再退化为 `split_stable_subname`，而是进入真实几何能力诊断。
- Body boolean fixture 约束多个旧 stable key 指向同一当前元素时记录 `merge` history。
- P3b taper fixture 约束一侧 / 内环 taper 的 ThruSections source face first-section history，以及一侧、多侧和内环 taper 的 source edge / offset section generated history；taper 仍按 partial history 和 known gap 验收。
- P7 Refine fixture 约束 Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 走 RefineModel + GenericShapeMapper history，并导出包含 modified / generated / deleted / merge 关键来源的 history_partial `NamedShape`。
- P7 DressUp / transformed fixture 约束 `SupportTransform` cache、refined prefix support 的 transformed copy 和 Body 后续 history 传播保留原 feature、transformed copy 与 support source。
- 一对多 fragment 只能记录 split history，不写入可解析 `ElementMap`。
- 无目标 source 只能记录 deleted history，不写入可解析 `ElementMap`。
- 不能靠输出端排序或 fixture 名称修正稳定引用。
