# C4-S2 M1 RuledSurface wire/wire 与 Loft profile 补完

## 目标

补 C3.0 surface first batch 之外的 `RuledSurface` wire/wire 和 `Loft` `Linearize=true` / face / vertex profile / 复杂 profile family。若 FreeCAD 调用链分叉，拆成后续步骤而不是混成一个实现。

## 完成状态

- `RuledSurface` whole-wire / whole-wire 已按 FreeCAD `TopoShape::makeElementRuledSurface()` 的 `BRepFill::Shell` 分支进入 cad-core helper，新增 `c4m1/part-ruled-surface-wire-wire` native expected。
- `Loft` `Linearize=true` 已从 executor deferred 改为 `makeElementLoft()` 后的 face-only post-processing；face profile 与 vertex profile 新增 `c4m1/part-loft-linearize-profile-face`、`c4m1/part-loft-linearize-profile-vertex` native expected。
- capability metadata 已同步 covered fixtures、remaining gaps 和 non-goals；不声明 full Part surface family。
- 复杂 profile family 仍作为明确 deferred boundary，交给后续 owner 按独立 FreeCAD 调用链和 oracle 拆分。

## 必读文件

- `docs/CADCore4.0/C4-M1-PartWorkbenchSurfaceFamily总览/6-19-23-54-C4-M1PartWorkbenchSurfaceFamily补完方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_blocker_queue.tsv`
- `docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part/part_ruled_surface.cpp`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`

## 产物

- Native expected 草案：`part-ruled-surface-wire-wire*`、`part-loft-linearize-profile*`。
- cad-core executor / helper / diagnostics 更新。
- MapperHistory / ElementMap 只走正式 `topo_shape` 路径。
- capability metadata 同步 covered fixtures、payload keys 和 remaining gaps。

## 非目标

- 不实现所有 surface family。
- 不为了通过单一 fixture 做 source edge 猜测。
- 不把复杂 profile family 缺口伪装成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

wire/wire 和 Loft 相关矩阵行已更新为 expected-backed supported；复杂 profile family 已保留明确 deferred 行；`cad_core_capabilities_json()` 未 overclaim full surface family。
