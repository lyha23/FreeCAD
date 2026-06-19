# C4-M1 RuledSurface / Loft 补完方案

## 目标

补 C3.0 first batch 之外的 `RuledSurface` wire/wire 和 `Loft` `Linearize=true` / profile family。若 FreeCAD 调用链分叉，先拆矩阵再实现。

## 范围

- `TopoShape::makeElementRuledSurface()` 的 `BRepFill::Shell` 分支。
- `PartFeatures.cpp::Loft::execute()` 的 `Linearize`、face / vertex profile、复杂 profile family。
- cad-core 落点：`part_ruled_surface.cpp`、`part_loft.cpp`、`topo_shape_expansion.cpp`、expected fixtures、adapter capability。

## 当前状态

- 已支持：`RuledSurface` whole-wire / whole-wire `BRepFill::Shell`，fixture `c4m1/part-ruled-surface-wire-wire`。
- 已支持：`Loft` `Linearize=true` face-only post-processing，以及 face / vertex profile，fixtures `c4m1/part-loft-linearize-profile-face`、`c4m1/part-loft-linearize-profile-vertex`。
- Deferred：复杂 profile family 仍需后续按独立 FreeCAD 调用链拆分 oracle，不作为本包 supported。

## 非目标

- 不声明 full Part surface family。
- 不用 source edge 猜测替代 MapperHistory。
- 不在 adapter 中修正拓扑命名。
