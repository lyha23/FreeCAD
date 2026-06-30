# C12-M6 S4 shell/topo provenance 准入验证

## 目标

验证 current wire/wire shell 输出是否具备足够的 topo provenance，能支撑 `supported_wire_wire_expected_backed`，而不是只匹配 topology counts 或 bbox。

## 必读来源

- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/fixtures/c4m1/expected/part-ruled-surface-wire-wire.freecad.json`
- `cad-core/tests/test_p8_features.py::assert_ruled_surface_source_edge`
- `cad-core/tests/test_p8_features.py::test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance`

## 操作

1. 复核 current implementation 是否进入 `BRepFill::Shell(Wire, Wire)` 分支，并在 named shape / history event 中记录 `part_ruled_surface:wire_wire_brepfill_shell`。
2. 核对 expected 中 `shape=occt_shell`、faces/edges/vertices、bbox、volume 和 `element_history_status_contains`。
3. 核对 source edge provenance 至少覆盖两个 source wires 的代表 edge，并明确是否只覆盖 Edge1 smoke 还是完整 wire edge family。
4. 若 provenance 只够 smoke，不足以关闭旧 blocker，S4 应输出 `retained_validation_blocker` 或 `publication_repair_required`，不得继续 overclaim full surface family。

## 裁决规则

- 几何等价但 source provenance 缺失，不算准入通过。
- provenance 不能在 adapter 输出端补猜；必须来自 Part executor / TopoShapeExpansion / named shape history。
- `InternalFaceN` / `EdgeN` 顺序差异可单列为非失败项，但 source edge 丢失或 shell/face 类型错误是失败。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```
