# C12-M3 S4 ProjectOnSurface 原生 provenance 可观测性 probe

## 目标

运行或阻断 ProjectOnSurface 原生 provenance probe，判断 FreeCAD 是否能通过 request-local native API 暴露 source subelement 到 projected Edge/Wire/Face 的稳定 mapper/history。

## 必读文件

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/TopoShapePyImp.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `docs/temp/6-29-20-40-c12m2-s5-project-on-surface-native-probe-output.json`
- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_probe_matrix.tsv`
- `docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/c12m3_project_on_surface_mapper_native_probe_scope_review_matrix.tsv`

## 操作

1. 按 S3 schema 对 edge/wire split、face rebuild、all-compound/height/offset 和 invalid diagnostic 场景执行 native provenance probe。
2. 对 object result shape、必要的 intermediate projected wire/face，以及 `getElementHistory`、`mapShapes`、`mapSubElement`、ElementMap save/load 等 API 分别记录结果。
3. artifact 必须写入 `docs/temp/`，并记录 FreeCAD / OCCT baseline、command、stdout/stderr、classification、source endpoint、target endpoint 和 request-local judgement。
4. 若 native history 仍是 `None` 或只能通过输出顺序/bbox/EdgeN 推断，分类为 `native_hidden_retained`，不得进入 S5 current mismatch。
5. 若 artifact 稳定暴露 source-backed provenance，分类为 `native_provenance_expected_ready` 并指定 S5 comparison input。

## 非目标

- 不修改 `cad-core`。
- 不刷新 checked-in expected。
- 不把 geometry build 成功等同 provenance 成功。
- 不把 collector bug、sandbox limitation 或 TypeError 写成 backend gap。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M3-PartWorkbenchProjectOnSurfaceMapperProvenanceNativeProbe批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
