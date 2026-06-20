# 【已实现】C51-S4 Pipe 高级分支与 Sewing 实现

## 目标

实现 Pipe 的 AuxiliarySpine、Binormal、Fixed、Round corner、Transformation scaling laws、SpineTangent 和 full front/back MapperSewing history。

## 必读

- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

- 先关闭矩阵 child blockers：`C51-BLK-411` path/SpineTangent、`C51-BLK-412` orientation modes、`C51-BLK-413` transition/scaling、`C51-BLK-414` front/back sewing；对应 oracle 为 `C51-ORC-411`..`414`，validation 为 `C51-VAL-411`..`414`。
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

## 实施结果

- `cad-core/src/part_design/feature_pipe.cpp` 解析 `Mode=Fixed/Auxiliary/Binormal`、`Transition=Round corner`、selected `Spine` subelements 和 `Sections`，并把 `Transformation=Linear/S-shape/Interpolation`、`SpineTangent`、`AuxiliarySpineTangent` 改为 FreeCAD 源码注释支撑的 exact `unsupported_property` blocker。
- `cad-core/src/part/topo_shape_expansion.cpp` 新增 `PipeShellOptions`，把 PipeShell `SetMode`、front/back cap `BRepBuilderAPI_Sewing`、`MapperSewing` 和 solidification 放到 topo-owned history 路径。
- `cad-core/fixtures/c51m4` 新增 native expected 覆盖 Fixed/Round、Auxiliary/Binormal、selected spine + Multisection，以及 source-backed blocker fixture。
- capability 状态为 `supported_c51s4_pipe_advanced_with_exact_source_blockers`，`deferred=[]`、`remaining_gaps=[]`，exact blockers 只剩 `partdesign_pipe_transformation_laws_source_commented` 与 `partdesign_pipe_spine_tangent_source_commented`。
