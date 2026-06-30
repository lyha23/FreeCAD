# C12-M6 S1 FreeCAD 源码与旧 PARTSURF 证据复核

## 目标

复核 FreeCAD `Part::RuledSurface` 的真实调用链，并把旧 C3M4 deferred evidence 与当前 cad-core landing 做一次对照，确认 S2-S4 应验证的最小闭环。

## 必读来源

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::RuledSurface`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`
- `cad-core/src/part/part_ruled_surface.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- 旧 C3M4 `part_surface_*` 矩阵。

## 操作

1. 用 `rg` 复核 `OrientationEnums`、`Curve1`、`Curve2`、`res.makeElementRuledSurface(shapes, Orientation.getValue())`。
2. 用 `rg` 复核 FreeCAD `makeElementRuledSurface()` 的 edge/wire normalization、`BRepFill::Face`、`BRepFill::Shell` 和 source edge recovery comment。
3. 对照 current `cad-core`：Part executor、TopoShapeExpansion helper、capability source、focused tests 和 checked-in fixture 是否都有 wire/wire landing。
4. 更新 source candidates 与 backend classification：区分 source-supported、current-supported-candidate、historical-deferred-doc-drift 和 true backend gap。

## 裁决规则

- 若 FreeCAD source 不支持 wire/wire，则 S2-S5 全部改为 no-code docs repair，不允许 implementation。
- 若 FreeCAD source 支持 wire/wire，但 cad-core current landing 缺一项，则保留到 S2/S3/S4 分类。
- 不能把旧 `deferred-S2` 本身当作 current backend gap；必须用当前 expected/current evidence 判断。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "RuledSurface::execute|makeElementRuledSurface|BRepFill::Shell|BRepFill::Face|Both BRepFill" src/Mod/Part/App/PartFeatures.cpp src/Mod/Part/App/TopoShapeExpansion.cpp
rg -n "Part::RuledSurface|wire_wire_brepfill_shell|part-ruled-surface-wire-wire" cad-core/src cad-core/tests cad-core/fixtures
git diff --check
```
