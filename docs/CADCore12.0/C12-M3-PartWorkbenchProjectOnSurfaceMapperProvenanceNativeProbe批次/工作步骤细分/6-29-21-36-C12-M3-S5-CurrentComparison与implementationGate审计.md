# C12-M3 S5 Current comparison 与 implementation gate 审计

## 目标

只对 S4 标记为 `native_provenance_expected_ready` 的 ProjectOnSurface row 进行 current cad-core comparison，并判断是否满足 implementation gate。

## 必读文件

- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_probe_matrix.tsv`
- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_backend_gap_classification.tsv`
- S4 产生的 `docs/temp/*c12m3*s4*project-on-surface*native*provenance*.json`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-edge-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-wire-split-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-face-rebuild-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-all-compound-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-invalid-provenance-diagnostics.freecad.json`

## 操作

1. 若 S4 没有 expected-ready row，关闭 S5 为 no comparison，不运行 current mismatch 结论。
2. 若 S4 有 expected-ready row，运行最小 focused comparison：只比较该 row 的 native provenance artifact、current fixture/result 和 checked-in expected context。
3. 区分 `current_covered`、`backend_gap_candidate`、`collector_bug` 和 `product_boundary_rejected`。
4. 若发现 mismatch，只记录后续 implementation 包建议和 C++ 落点，不在 S5 改代码。

## 非目标

- 不对 native-hidden row 做 comparison。
- 不把 C5-M9 source-backed known-gap expected 当成 native expected 直接通过。
- 不跑全量 FreeCAD build 或全量 CI。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
