# P6：Topo Naming 主路径

P6 的目标是把稳定引用从导出层补丁升级为 CAD Core 的正式账本。完成后 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap`、MapperHistory 传播稳定 subname，而不是在 executor 或 adapter 中补猜测规则。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| Shape 属性 | `src/Mod/Part/App/PropertyTopoShape.cpp` |
| TopoShape | `src/Mod/Part/App/TopoShape.cpp`、`TopoShapeExpansion.cpp` |
| Mapper | `src/Mod/Part/App/TopoShapeMapper.cpp` |
| PartFeature | `src/Mod/Part/App/PartFeature.cpp` |
| GeoFeature 引用更新 | `src/App/GeoFeature.cpp` |
| LinkSub 更新 | `src/App/PropertyLinks.cpp` |
| FaceMaker / WireJoiner history | `src/Mod/Part/App/FaceMaker*.cpp`、`WireJoiner.cpp` |

## 当前问题

- 当前 subshape map 能导出 `FaceN` / `EdgeN` / `VertexN`；`topo/named_shape` 已建立 object-local indexed `NamedShape` 账本、identity `ElementMap`、source-preserved `ElementMap` 和一对一 history-derived `ElementMap`，并通过 `named_shapes` 输出。普通单侧非 taper prism、非 taper Two sides / Symmetric 的单-prism 快路径，以及 Two sides 的 UpTo 多 prism `makeElementXor` 子流程，已通过 `topo::makeElementXorFromSources` 消费 maker `Generated/Modified` history 并传播上一步 `ElementMap` 稳定 key；taper 路径已开始输出 source-preserved `ElementMap` 子集，但完整 taper maker history 仍标记为 `known_gap:taper_history`；Body 简单 additive / subtractive 组合已通过 `topo::makeElementBooleanFromSources` 消费 `BRepAlgoAPI_Fuse/Cut` 的多源 maker history，并继续传播输入 feature 已有的嵌套 `ElementMap` alias；完整 MapperHistory 消费还不是主路径。
- `topo/element_map` 已承接 Sketch closed profile 的最小 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN` 映射，作为 ElementMap 落点起点；这还不是完整旧引用恢复或 history 传播。
- P3a 的 UpToFace 解析和 Sketch `ExternalGeometry` 的 `PropertyLinkSubList` 解析已优先通过当前 object 的 `StableSubList -> ElementMap -> current subname` 取 face / edge / vertex；stable indexed name、source-preserved key 和一对一 history key 已可覆盖旧 `SubList` 过期或旧 `SubList` 不再是 `FaceN` / `EdgeN` 格式的最小场景。opaque stable name 不在当前 `ElementMap` 时返回 `unsupported_stable_subname`；UpToFace 和 ExternalGeometry 的 stable name 命中 split / deleted history 时分别返回 `split_stable_subname` / `deleted_stable_subname`。split / one-to-many 来源会记录 `split` history，deleted 来源会记录 `deleted` history，二者都不写入可解析 `ElementMap`。
- Refine、taper、通用 boolean history、split 旧引用恢复、merge history、FaceMaker / WireJoiner 都需要正式 history。

## Step 40：NamedShape 核心模型

目标：

- 为每个 feature 输出建立 `NamedShape`。
- 记录 generated / modified / deleted / split / merge 来源。
- 支持 bare indexed subname、stable subname、mapped tail 的解析。

验收：

- `Face1` 可解析，但不等同于稳定 topo naming 完成。
- stable subname 缺失时有 diagnostics 或 known gap，不伪装为恢复成功。

当前 `fixtures/p6/named-shape-indexed-pad.json` 约束 `named_shapes.Pad` 输出 indexed `FaceN` / `EdgeN` / `VertexN` 元素账本、identity `element_map`、prism `generated` history，以及 `Sketch.Edge1 -> 当前 EdgeN` / `Sketch.Vertex1 -> 当前 VertexN` 这类 source-preserved `element_map`；现有 `fixtures/p3b/pad-two-sides-length.json`、`pad-symmetric-length.json`、`pocket-two-sides-length.json` 和 `pocket-symmetric-length.json` 同时约束非 taper Two sides / Symmetric 单-prism 快路径保留 profile source history；现有 `fixtures/p3b/pad-two-sides-up-to-face*.json` / `pad-two-sides-up-to-shape*.json` 约束多 prism 通过 `topo::makeElementXorFromSources` 按 `makeElementXor` union / common / cut 子流程组合后仍保留 profile source edge / vertex history，并出现 `Pad.XorUnion1.*` 中间 maker alias，`fixtures/p6/sketch-external-edge-stable-multi-prism.json` 约束该 ElementMap 可被 Sketch `ExternalGeometry` 旧引用恢复消费；现有 P3b taper fixture 约束 source-preserved edge / vertex key 已写入 `named_shapes`，`fixtures/p6/sketch-external-edge-stable-taper-preserved.json` 约束该 preserved 子集可被 Sketch `ExternalGeometry` 消费，同时 feature 输出继续标记 `known_gap:taper_history`；`fixtures/p6/body-additive-fuse-history.json` 约束 fuse 后 `named_shapes.Body` 保留 `BRepAlgoAPI_Fuse` 的 `history_partial` 并记录 `BaseFeature.*` / `Pad.*` 来源；`fixtures/p6/body-boolean-history.json` 约束 cut 后 `named_shapes.Body` 保留 `BRepAlgoAPI_Cut` 的 `history_partial`、记录 `Pad.*` / `Pocket.*` 来源，并把无歧义来源写入 `element_map`；`fixtures/p6/sketch-external-edge-stable-body-profile-source.json` 约束 Body boolean 继续传播输入 Pad 的嵌套 `SketchPad.EdgeN` alias，并可被 Sketch `ExternalGeometry` 消费；`fixtures/p6/body-split-history.json` 约束未变 source 子元素输出 `Pad.Face1 -> Face1`、`Pad.Edge1 -> Edge1`、`Pad.Vertex1 -> Vertex1`，一对多 fragment 来源输出 `split` history 且不把 `Pad.Face5` 写入可解析 `element_map`，无目标来源输出 `deleted` history 且不把 `Pocket.Face5` 写入可解析 `element_map`；`fixtures/p6/up-to-face-stable-indexed-reference.json`、`fixtures/p6/up-to-face-stable-indexed-opaque-sublist.json`、`fixtures/p6/up-to-face-stable-body-history.json`、`fixtures/p6/up-to-face-stable-body-preserved.json`、`fixtures/p6/sketch-external-edge-stable-indexed-opaque-sublist.json` 和 `fixtures/p6/sketch-external-edge-stable-body-preserved.json` 约束先用 stable name 更新引用，再解析 current subname；`fixtures/p6/up-to-face-stable-subname-known-gap.json` 约束 opaque stable subname 尚未恢复时返回 `unsupported_stable_subname`；`fixtures/p6/up-to-face-stable-body-split.json` / `fixtures/p6/up-to-face-stable-body-deleted.json` 和 `fixtures/p6/sketch-external-edge-stable-body-split.json` / `fixtures/p6/sketch-external-edge-stable-body-deleted.json` 分别约束 UpToFace 与 ExternalGeometry 在 split / deleted stable name 下返回 `split_stable_subname` / `deleted_stable_subname`。

## Step 41：ElementMap 和引用更新

目标：

- 建立 object-local `ElementMap`。
- 支持 `PropertyLinkSub` / `PropertyLinkSubList` 的旧引用更新。
- 支持 internal element 和 external reference。

fixtures：

```text
fixtures/p6/
  pad-length-change-fillet-edge.json
  sketch-external-edge-after-pad-change.json
  pocket-face-reference-after-up-to.json
```

验收：

- 修改 Pad 长度后，引用不无声丢失。
- 无法恢复时返回 diagnostics。

## Step 42：MapperHistory 消费

目标：

- boolean、extrude、taper、refine、pattern 都把 OCCT / geometry history 消费到 topo。
- source edge 一对多 fragment 映射有明确记录。
- split / merge 不能靠几何排序后补。

当前普通单侧非 taper Pad / Pocket、非 taper Two sides / Symmetric 的单-prism 快路径，以及 Two sides 的 UpTo 多 prism `makeElementXor` 子流程，已通过 `topo/named_shape` 消费 maker history、保留 source subelement，并把一对一来源写入 `ElementMap`；XOR 组合 helper 已下沉为 `topo::makeElementXorFromSources`，Body additive / subtractive 也已切到 `topo::makeElementBooleanFromSources`，不再由 executor 直接承载 union / common / cut / fuse / cut 的 maker 账本。taper 路径已先接入 source-preserved 子集，但还没有消费完整 loft / offset / cut maker history。一对多来源已可显式归类为 `split`，无目标来源已可显式归类为 `deleted`。split 的完整旧引用恢复、taper 完整 maker history、Refine、ShapeFix 和 merge history 仍未完整进入主路径。

排查矩阵：

```text
1. FaceMakerBuildFace 几何结果是否与 FreeCAD 一致
2. WireJoiner::getOpenWires 几何结果是否与 FreeCAD 一致
3. raw compound / child shape identity 是否在组合时被重建或复制
4. NamedShape / ElementMap 是否完整消费 MapperHistory
```

如果 1-3 一致而 stable subname 不一致，应归类为 history 到 ElementMap 的传播缺口，不应在 sketch executor 继续加规则。

## Step 43：Refine 和 ShapeFix history

目标：

- Refine 迁移 FreeCAD maker / object chain。
- ShapeFix 的 generated / modified history 进入 ElementMap。
- 删除临时 refine fallback 的输出修正路径。

fixtures：

```text
fixtures/p6/
  refine-pad-pocket.json
  refine-fillet-reference.json
  shapefix-wire-history.json
```

## 完成定义

P6 完成需要同时满足：

- `NamedShape` / `ElementMap` / MapperHistory 是 recompute 主路径。
- LinkSub 旧引用更新可被 fixture 约束。
- Face/Edge/Vertex 数量、几何内容、stable subname 丢失被当作失败。
- 仅命名顺序不同且几何等价时，归类为命名顺序差异。
- executor 和 adapter 中没有 fixture 名称或几何形态猜测的 topo naming 修补逻辑。
