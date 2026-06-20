# 【已实现】C5-S3 M3 Loft / Pipe 高级分支

## 目标

补齐 PartDesign Loft / Pipe advanced branches，按 FreeCAD 源码把 Loft 和 Pipe 分开审计、采集 oracle、实现或稳定 deferred diagnostic。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/矩阵/loft_pipe_advanced_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M3-LoftPipe高级分支主线/矩阵/loft_pipe_advanced_blocker_queue.tsv`
- `src/Mod/PartDesign/App/FeatureLoft.cpp`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`

## 工作内容

- 拆 Loft rows：Closed multi-section、multi-wire ordering、sewing history、AllowCompound diagnostics。
- 拆 Pipe rows：Sections、AuxiliarySpine、Binormal、Transformation、Transition、SpineTangent、setupAlgorithm。
- 为 supported row 采集 native expected；风险过大的 branch 写稳定 diagnostic，并记录 next owner。
- 更新 cad-core implementation、fixtures、tests 和 capability metadata。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- Loft / Pipe advanced rows 不再是 broad deferred。
- PartDesign Loft / Pipe capability 不借用 Part Workbench support，不声明 full completion。

## 实施结果

- FreeCAD 调用链已记录到 `../6-20-10-48-【已实现】C5-M3-LoftPipe高级分支方案.md`：Loft 走 `getSectionShape()`、`makeElementLoft()`、`BRepBuilderAPI_Sewing` / `MapperSewing`、solidification、`AddSubShape`；Pipe 走 `buildPipePath()`、`setupAlgorithm()`、PipeShell、front/back sewing、solidification、`AddSubShape`。
- supported：Pipe `Transformation=Multisection` + `Sections`、`Mode=Frenet` + `Transition=Right corner`，均有 native expected；C4 Loft/Pipe first slice 继续通过。
- diagnostic-backed：Loft `Closed=true` multi-section、multi-wire ordering、`AllowCompound=false` multi-wire pressure；Pipe `Mode=Auxiliary`、`AuxiliarySpine`、`AuxiliaryCurvilinear`、`Mode=Binormal`、`Binormal`、scaling law `Transformation`、`SpineTangent`。
- deferred：Loft explicit subelement selection beyond current Sketch whole-shape rule、full Closed/multi-wire MapperThruSections/Sewing parity、Pipe `Transition=Round corner`、`Mode=Fixed`、full front/back `MapperSewing` propagation。
- 新增 `cad-core/fixtures/c5m3/` fixtures/expected、focused tests 和 capability metadata；M3/global scope/oracle/blocker/validation 矩阵已同步，`C5-BLK-301` 与 `C5M3-*` blockers 已关闭为 supported/diagnostic split。
