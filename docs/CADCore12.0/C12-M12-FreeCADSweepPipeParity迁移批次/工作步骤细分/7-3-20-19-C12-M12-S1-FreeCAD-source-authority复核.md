# C12-M12 S1 FreeCAD source authority 复核

## 目标

把 FreeCAD Sweep / Pipe 源权威写清楚，后续实现只能从这些 source-backed 行为迁移。

## 必读文件

- `../README.md`
- `../矩阵/c12m12_sweep_source_matrix.tsv`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`

## 操作

1. 复核 `Pipe::execute()` 的 profile、spine、multi-section、solid/cap/sewing、Body add/cut flow。
2. 复核 `Pipe::setupAlgorithm()` 的 Fixed/Frenet/Auxiliary/Binormal mode、transition、auxiliary correction、binormal、law。
3. 复核 `TopoShape::makeElementPipeShell()` 的 `BRepOffsetAPI_MakePipeShell` 调用顺序与 history。
4. 复核 `Sweep::execute()` 的 Part Workbench wrapper 和 `makeElementPipeShell` 参数。
5. 复核 Python helper public API，确认 advanced DTO 需要支持的参数边界。
6. 更新 source matrix；关闭缺 source blocker；将本步骤重命名为 `【已实现】`。

## 关闭条件

- 每个 source row 都有 exact file、symbol、role、current landing、review status。
- 明确哪些行为属于 FreeCAD source-backed，哪些只是 chili3d / cad-core 当前实现。
- S2 drift audit 的 code landing 已足够具体。

## 非目标

- 不跑 current tests。
- 不采 native oracle。
- 不改 `cad-core`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/矩阵/*.tsv
```
