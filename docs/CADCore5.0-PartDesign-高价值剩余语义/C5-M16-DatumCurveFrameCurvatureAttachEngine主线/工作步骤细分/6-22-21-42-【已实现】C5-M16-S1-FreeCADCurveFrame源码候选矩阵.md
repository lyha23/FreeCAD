# 【已实现】C5-M16-S1 FreeCAD CurveFrame 源码候选矩阵

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

## S1 冻结结论

- `AttachEngine3D::AttachEngine3D()` 的 edge-driven ref table 已复核：本包目标共享 `rtEdge` / `rtCurve` / `rtCircle`，并支持 optional `rtVertex` 与 vertex-first swap；`NormalToPath` 只进入 curve projection 合同，不作为本包新增 release mode。
- `_calculateAttachedPlacement()` 的 `mmNormalToPath`、`mmFrenetNB/TN/TB`、`mmRevolutionSection`、`mmConcentric` 共用 path edge、`attachParameter` / vertex projection、D1 zero derivative、D2、T/N/B 与 curvature center 分支。
- `AttachEngineLine` 的 `AxisOfCurvature -> mmRevolutionSection`、`Binormal -> mmFrenetTN`、`Normal -> mmFrenetTB` 和 `AttachEnginePoint` 的 `CenterOfCurvature -> mmRevolutionSection` 已作为 aliases 写入 source candidates。
- `AttachExtension::updateSinglePropertyStatus()` 的 `modeIsPointOnCurve` / `MapPathParameter` 可见性与 `PropertyLinks.cpp` 的 request-local subname/shadow link 证据已冻结为 release 边界证据；它们不代表 backend session 或 long-lived BREP。
- `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 只保留为 out-of-scope / non-goal source candidates；S1 不采 oracle、不改 code、不声明 supported。
- 下一步进入 S2：把已冻结 source candidates 路由到 scope、backendGap、fixture/oracle、non-goal 和 release gate。

## 完成条件

- 每个 source candidate 有 FreeCAD 文件、函数/分支、关键短句或字段、cad-core landing 和下一步。
- source candidate 只证明待实现语义，不代表 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'case mmNormalToPath|case mmFrenetNB|case mmRevolutionSection|mm1AxisCurv|mm0CenterOfCurvature' src/Mod/Part/App/Attacher.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线
```
