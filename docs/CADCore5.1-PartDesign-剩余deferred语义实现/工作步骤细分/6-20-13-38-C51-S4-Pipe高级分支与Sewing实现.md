# C51-S4 Pipe 高级分支与 Sewing 实现

## 目标

实现 Pipe 的 AuxiliarySpine、Binormal、Fixed、Round corner、Transformation scaling laws、SpineTangent 和 full front/back MapperSewing history。

## 必读

- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

- 记录 `buildPipePath()`、`setupAlgorithm()`、`BRepOffsetAPI_MakePipeShell`、Transition / Mode / Transformation 分支。
- 按完整语义批次实现：orientation modes、transition modes、scaling laws、spine tangent expansion、front/back sewing history。
- 采集 native expected 覆盖每类 mode，不用一个 fixture 代表所有 PipeShell 行为。
- 同步 capability metadata，保留仅剩无法支持的 mode-specific exact blocker。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- C5 Pipe deferred 列表被 supported / exact blocker 替代。
- Full sewing history 进入 topo mapper owner，不在 `feature_pipe` 里拼 synthetic aliases。
