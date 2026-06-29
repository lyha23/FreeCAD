# C12-M3 S1 FreeCAD 源码与现有 provenance 证据矩阵

## 目标

把 ProjectOnSurface 原生执行、TopoShape history API、PropertyTopoShape ElementMap 和当前 cad-core provenance landing 全部落入 source candidate matrix，作为后续 S4 probe 的唯一语义依据。

## 必读文件

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.h`
- `src/Mod/Part/App/TopoShapePyImp.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-edge-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-wire-split-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-face-rebuild-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-all-compound-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-invalid-provenance-diagnostics.freecad.json`

## 操作

1. 从 FreeCAD source 摘出 exact file、class/function 和支撑短句或字段名，回填 source candidates。
2. 记录当前 cad-core 已有 ProjectOnSurface provenance ledger、fixtures expected、focused tests 和 capability wording 的位置，只作为 context。
3. 标记哪些 evidence 是 source-backed known gap、哪些是 C12-M2 native-hidden blocker、哪些只是 geometry coverage。
4. 更新 blocker queue：关闭 missing source authority blocker，或列出仍缺 source authority 的 row。

## 非目标

- 不运行 native probe。
- 不改 C++、expected、tests 或 capability wording。
- 不从当前 cad-core 实现倒推 FreeCAD 语义。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次 docs/CADCore12.0/README.md
git diff --check
```
