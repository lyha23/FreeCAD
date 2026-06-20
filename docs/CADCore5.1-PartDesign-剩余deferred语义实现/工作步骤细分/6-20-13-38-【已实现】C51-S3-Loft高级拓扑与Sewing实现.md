# 【已实现】C51-S3 Loft 高级拓扑与 Sewing 实现

## 目标

实现 Loft Closed、多 section、多 wire ordering、explicit subelement selection 和 full MapperThruSections / MapperSewing ElementMap 传播。

## 必读

- `src/Mod/PartDesign/App/FeatureLoft.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

- 先关闭矩阵 child blockers：`C51-BLK-311` section ledger、`C51-BLK-312` Closed/multi-section parity、`C51-BLK-313` MapperThruSections/MapperSewing；对应 oracle 为 `C51-ORC-311`..`313`，validation 为 `C51-VAL-311`..`313`。
- 把 C5 的 Loft known_gap native expected 转为 active parity target。
- 补 profile / section wire ledger，保证 wire 数量、ordering、inner wires、vertices 与 FreeCAD 一致。
- 补 `makeElementLoft()` / `MapperThruSections` / `MapperSewing` 的 generated/modified history 到 ElementMap。
- 切换 `feature_loft` 主路径并删除旧 diagnostic fallback。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- Closed/multi-wire Loft 不再只返回 unsupported diagnostic。
- Stable subname / ElementMap 差异有 topo mapper 依据，不靠 executor 输出修正。

## 实现记录

- FreeCAD 依据：`FeatureLoft.cpp::getSectionShape()` 对 Sketch 非 `Vertex*` 子选择回退整草图，否则逐个子元素解析；`Loft::execute()` 按 profile/section wire ledger 逐条调用 `makeElementLoft(... IsSolid::notSolid, Ruled, Closed)`，少于 3 个 profile/section 时忽略 `Closed`，随后用 `BRepBuilderAPI_Sewing` 缝合 front/back/shells 并调用 `MapperSewing(sewer)`。
- topo 依据：`TopoShapeExpansion.cpp::MapperThruSections::generated()` 消费 `GeneratedFace(s)`、`FirstShape()`、`LastShape()`；`MapperSewing::modified()` 先查 `maker.Modified(s)`，再查 `maker.ModifiedSubShape(s)`。cad-core 在 `part/topo_shape.cpp` 新增 `namedShapeForSewingHistory()`，由 topo 层写入 ElementMap/history，不在 `feature_loft` 或 adapter 中合成输出别名。
- cad-core 结果：移除 Closed/multi-wire 旧 `unsupported_property` fallback；`feature_loft.cpp` 支持 multi-section、multi-wire、Closed、Profile/Sections explicit subelement resolution；`shape_exporter.cpp` 对齐 FreeCAD C++ `TopoShape::getBoundBoxOptimal()` 的 `AddOptimal(..., false, false)`，使 active Loft expected 不需要 bbox 放宽。
- fixture/test：`cad-core/fixtures/c5m3/expected/partdesign-loft-{closed-multisection,multiwire-ordering}.freecad.json` 删除 `known_gap`；同组 fixture 镜像到 `cad-core/fixtures/c51m3`；`tests.test_p7_features` 覆盖 c5m3/c51m3 active parity 和 AllowCompound diagnostic；capability 中 Loft remaining gaps/deferred 置空。
