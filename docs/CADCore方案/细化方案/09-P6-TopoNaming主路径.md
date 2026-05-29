# P6：TopoNaming 主路径

P6 把稳定引用从导出层补丁升级为 CAD Core 的正式账本。目标是让 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap` 和 MapperHistory 传播 stable subname。

## 当前基线

- `topo/named_shape` 建立 object-local indexed `FaceN` / `EdgeN` / `VertexN` 账本。
- 支持 identity、source-preserved 和一对一 history-derived `ElementMap`。
- 普通 prism、非 taper Two sides / Symmetric 单-prism、Two sides UpTo 多 prism XOR 子流程已消费 maker history 子集。
- `makeElementXorFromSources` 和 `makeElementBooleanFromSources` 已下沉到 topo。
- Body additive / subtractive 组合通过 `BRepAlgoAPI_Fuse/Cut` 的多源 maker history 传播 source alias。
- UpToFace 和 Sketch ExternalGeometry 可通过 `StableSubList -> ElementMap -> current subname` 更新引用。
- split / deleted / unsupported stable subname 有结构化 diagnostics。

## 已知缺口

- 完整 MapperHistory 生命周期尚未迁移。
- taper、Refine、ShapeFix、DressUp、transformed copy 的完整 maker history 仍未覆盖。
- split / merge 的完整旧引用恢复还不完整。
- FaceMaker / WireJoiner 的 history 消费需与 P5 geometry 账本联动。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `topo/named_shape.*` | NamedShape、ElementMap、maker history helper |
| `topo/element_map.*` | sketch internal element map |
| `features/feature_extrude.*` | prism source history |
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

- `fixtures/p6` 覆盖 indexed named shape、source-preserved key、Body boolean history、stable subname 恢复、split / deleted diagnostics。
- 一对多 fragment 只能记录 split history，不写入可解析 `ElementMap`。
- 无目标 source 只能记录 deleted history，不写入可解析 `ElementMap`。
- 不能靠输出端排序或 fixture 名称修正稳定引用。
