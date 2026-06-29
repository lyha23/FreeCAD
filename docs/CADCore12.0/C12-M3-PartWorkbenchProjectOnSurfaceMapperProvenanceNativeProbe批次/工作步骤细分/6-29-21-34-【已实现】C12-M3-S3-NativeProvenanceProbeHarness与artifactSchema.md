# C12-M3 S3 NativeProvenanceProbe harness 与 artifact schema【已实现】

## 目标

在不改变 C++ 和 checked-in expected 的前提下，定义 C12-M3 native provenance probe 的 artifact schema、probe harness 复用方式和失败分类。

## 必读文件

- `docs/temp/6-29-20-12-c12m2-native-probe-schema.md`
- `docs/temp/6-29-20-12-c12m2-native-probe-harness.py`
- `docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json`
- `docs/temp/6-29-20-40-c12m2-s5-project-on-surface-native-probe-output.json`
- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_probe_matrix.tsv`
- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_blocker_queue.tsv`

## 操作

1. 复核 C12-M2 S3 schema 是否足以承载 provenance；若不足，新增 C12-M3 schema 文档到 `docs/temp/`，字段必须包括 source endpoint、target endpoint、history API name、history return summary、request-local judgement 和 classification。
2. 可新增或扩展 `docs/temp` 下的 probe harness，不得写入 `cad-core` 源码、tests 或 fixtures expected。
3. 冻结 C12-M3 classifications：`native_provenance_expected_ready`、`current_covered`、`backend_gap_candidate`、`native_hidden_retained`、`collector_bug`、`product_boundary_rejected`、`sandbox_runtime_limit`。
4. 更新 probe matrix 和 validation matrix，说明 S4 artifact 名称与通过标准。

## 非目标

- 不在 S3 采集 family expected。
- 不比较 current cad-core。
- 不把 C12-M2 `None` history 直接当作最终结论；S4 负责重新 probe。

## 完成记录

- 已复核 C12-M2 schema/harness、runtime baseline artifact 和 ProjectOnSurface C12-M2 S5 artifact；C12-M2 harness 可复用作 FreeCADCmd/runtime/process wrapper，但 C12-M2 schema 缺少 row-level provenance 字段。
- 已新增 `docs/temp/6-29-22-15-c12m3-native-provenance-probe-schema.md`，固定 source endpoint、target endpoint、history API name、history return summary、request-local judgement、classification、current comparison path、S4 artifact 命名和通过标准。
- 已冻结分类：`native_provenance_expected_ready`、`current_covered`、`backend_gap_candidate`、`native_hidden_retained`、`collector_bug`、`product_boundary_rejected`、`sandbox_runtime_limit`。
- 已更新 probe matrix、blocker queue 和 validation matrix；`C12M3-BLOCKER-002` 关闭。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
