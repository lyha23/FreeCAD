# C51-S3 Loft 高级拓扑与 Sewing 实现

## 目标

实现 Loft Closed、多 section、多 wire ordering、explicit subelement selection 和 full MapperThruSections / MapperSewing ElementMap 传播。

## 必读

- `src/Mod/PartDesign/App/FeatureLoft.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

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
