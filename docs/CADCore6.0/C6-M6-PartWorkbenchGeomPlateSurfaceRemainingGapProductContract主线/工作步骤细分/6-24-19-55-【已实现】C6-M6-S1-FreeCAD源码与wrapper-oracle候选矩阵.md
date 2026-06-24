# 【已实现】C6-M6-S1 FreeCAD 源码与 wrapper oracle 候选矩阵

## 目标

读取 FreeCAD 和 cad-core 源码，建立 GeomPlate remaining gap 的候选矩阵。S1 只记录 source evidence、wrapper 行为和 cad-core 落点，不提前把候选升级为 supported 或 backendGap。

## S1 收口结论

- `c6m6_geomplate_remaining_gap_source_candidates.tsv` 已按 G1 curve-on-surface、ProjectedCurve2d without InitialSurface、curve criteria setter、PlateSurface.Curves wrapper lifecycle 和 capability publication 五类补齐 source / wrapper / oracle 候选。
- S1 只建立候选矩阵和落点；4 个 active `remaining_gaps` 仍保留，不把 `Part.PlateSurface.Curves` 或 remaining gap 写成 supported。
- 本步未运行 native oracle、未改 C++ executor、未新增 fixture。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp`
- `cad-core/include/cad_core/part/part_geomplate.h`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c5m7`、`cad-core/fixtures/c5m13`

## 产物

- 更新 `矩阵/c6m6_geomplate_remaining_gap_source_candidates.tsv`。
- 每条 candidate 写清 `source_file`、`freecad_symbol`、`semantic_axis`、`source_evidence` 和 `cad_core_landing`。
- 将候选分为 G1 curve-on-surface、ProjectedCurve2d without InitialSurface、curve criteria setter、PlateSurface.Curves wrapper lifecycle、capability publication。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'makeSurface|BuildPlateSurface|setCurve2dOnSurf|setProjectedCurve|setG0Criterion|setG1Criterion|setG2Criterion|Curves|Save|Restore' src/Mod/Part/App/Tools.cpp src/Mod/Part/App/GeomPlate src/Mod/Part/App/PlateSurfacePyImp.cpp src/Mod/Part/App/Geometry.cpp cad-core/src/part/part_geomplate.cpp cad-core/include/cad_core/part/part_geomplate.h
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
```

验收通过后，本文已重命名为 `6-24-19-55-【已实现】C6-M6-S1-FreeCAD源码与wrapper-oracle候选矩阵.md`。

## 非目标

- 不运行 native oracle 采集。
- 不改 C++ executor。
- 不把 `Part.PlateSurface.Curves` 写成 supported。
