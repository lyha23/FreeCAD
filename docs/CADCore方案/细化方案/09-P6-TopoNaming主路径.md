# P6：TopoNaming 主路径

P6 把稳定引用从导出层补丁升级为 CAD Core 的正式账本。目标是让 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap` 和 MapperHistory 传播 stable subname。

## 当前基线

- `topo/named_shape` 建立 object-local indexed `FaceN` / `EdgeN` / `VertexN` 账本。
- 支持 identity、source-preserved、一对一 history-derived `ElementMap` 和多个旧 stable key 指向同一当前元素的 merge history。
- 普通 prism、非 taper Two sides / Symmetric 单-prism、Two sides UpTo 多 prism XOR 子流程已消费 maker history 子集。
- taper 已暴露 `BRepOffsetAPI_ThruSections` maker 与 loft section，记录 source edge / offset section 的 generated history，并通过 XOR / boolean 组合透传到一侧、多侧和内环 taper 结果；仍保留 `known_gap:taper_history`，因为完整 MapperHistory 生命周期尚未迁移。
- `BRepBuilderAPI_RefineModel` 已按 FreeCAD `modelRefine` / `FaceUniter` 路径迁入 `geometry/refine_model.*`，Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 可消费 `Modified()` / `IsDeleted()` partial history。
- `makeElementXorFromSources` 和 `makeElementBooleanFromSources` 已下沉到 topo。
- Body additive / subtractive 组合通过 `BRepAlgoAPI_Fuse/Cut` 的多源 maker history 传播 source alias。
- UpToFace 和 Sketch ExternalGeometry 可通过 `StableSubList -> ElementMap -> current subname` 更新引用。
- split / deleted / unsupported stable subname 有结构化 diagnostics；deleted / split terminal history 已能跨后续 Body boolean / maker 继续保留诊断语义。

## 已知缺口

- 完整 MapperHistory 生命周期尚未迁移。
- ShapeFix、DressUp、transformed copy 的完整 maker history 仍未覆盖；RefineModel 和 taper 当前只算 partial history。
- split 的完整旧引用恢复还不完整；deleted / split terminal history 只保证后续 maker 不丢诊断语义，尚未恢复一对多旧引用；merge 当前已记录 history，但仍需和完整 MapperHistory 生命周期收敛。
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
| `features/body.*` | Body boolean history |
| `features/transformed.*` | transformed copy source alias |

## FreeCAD 依据

- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `src/Mod/Part/App/TopoShape.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`

## 验收

- `fixtures/p6` 覆盖 indexed named shape、source-preserved key、Body boolean history、stable subname 恢复、split / deleted diagnostics，以及 deleted / split terminal history 跨后续 Body boolean 的传播。
- Body boolean fixture 约束多个旧 stable key 指向同一当前元素时记录 `merge` history。
- P3b taper fixture 约束一侧、多侧和内环 taper 的 ThruSections generated history；taper 仍按 partial history 和 known gap 验收。
- P7 Refine fixture 约束 Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 走 RefineModel maker，并导出 history_partial `NamedShape`。
- 一对多 fragment 只能记录 split history，不写入可解析 `ElementMap`。
- 无目标 source 只能记录 deleted history，不写入可解析 `ElementMap`。
- 不能靠输出端排序或 fixture 名称修正稳定引用。
