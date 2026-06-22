# C5-M16-S1 FreeCAD CurveFrame 源码候选矩阵

## 目标

把 C5-M16 的 FreeCAD source authority 固化到 package-local source candidates，确认哪些 mode 真正共享 `AttachEngine3D` curve-frame path，哪些只是相邻 UI 名称。

## 必读

- `src/Mod/Part/App/Attacher.h`
- `src/Mod/Part/App/Attacher.cpp`
- `src/Mod/Part/App/AttachExtension.cpp`
- `src/App/PropertyLinks.cpp`
- `cad-core/src/part_design/datum_attachment.h`

## 工作内容

1. 复核 `AttachEngine3D::AttachEngine3D()` 的 `modeRefTypes`：edge/curve/circle、optional vertex、vertex-first support。
2. 复核 `_calculateAttachedPlacement()` 中 `mmNormalToPath`、`mmFrenetNB/TN/TB`、`mmRevolutionSection`、`mmConcentric` 的共享分支。
3. 复核 `AttachEngineLine` aliases：`AxisOfCurvature`、`Normal`、`Binormal`。
4. 复核 `AttachEnginePoint` alias：`CenterOfCurvature`。
5. 把 conic landmarks、`IntersectionPoint`、`Folding`、`TangentU/V` 记录为 out-of-scope source candidates 或 non-goal。

## 产物

- `矩阵/c5m16_datum_curve_frame_source_candidates.tsv`
- `矩阵/c5m16_datum_curve_frame_scope_review_matrix.tsv`

## 完成条件

- 每个 source candidate 有 FreeCAD 文件、函数/分支、关键短句或字段、cad-core landing 和下一步。
- source candidate 只证明待实现语义，不代表 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'case mmNormalToPath|case mmFrenetNB|case mmRevolutionSection|mm1AxisCurv|mm0CenterOfCurvature' src/Mod/Part/App/Attacher.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线
```
