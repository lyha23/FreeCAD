# CADCore5.0 PartDesign 高价值剩余语义

本目录承接 CADCore4.0 freeze 后留下的 PartDesign 具体边界，只推进前端 CAD 运行时高价值、可 oracle、可 cad-core 分层落地的剩余语义。

## 入口

- 总览方案：`6-20-10-47-【已实现】CADCore5.0-PartDesign高价值剩余语义总览方案.md`
- 全局 source 候选：`矩阵/cadcore5_source_candidates.tsv`
- 全局 scope 矩阵：`矩阵/cadcore5_scope_review_matrix.tsv`
- 全局 blocker 队列：`矩阵/cadcore5_blocker_queue.tsv`
- fixture / oracle 矩阵：`矩阵/cadcore5_fixture_oracle_matrix.tsv`
- non-goal registry：`矩阵/cadcore5_non_goal_registry.tsv`
- 验收矩阵：`矩阵/cadcore5_validation_matrix.tsv`
- C5-M5 freeze summary：`C5-M5-Freeze收口主线/6-20-12-45-C5-M5-freeze收口总结.md`

## 专题包

| 专题包 | 入口 | 队列 |
| --- | --- | --- |
| C5-M0 范围审计与基线重建 | `C5-M0-范围审计与基线重建主线/6-20-10-48-C5-M0范围审计与基线重建主线总入口.md` | `C5-M0-范围审计与基线重建主线/工作步骤细分/` |
| C5-M1 Revolution / Groove 参数补完 | `C5-M1-RevolutionGroove参数补完主线/6-20-10-48-C5-M1-RevolutionGroove参数补完主线总入口.md` | `C5-M1-RevolutionGroove参数补完主线/工作步骤细分/` |
| C5-M2 Boolean / Body ownership | `C5-M2-Boolean-BodyOwnership主线/6-20-10-48-C5-M2-Boolean-BodyOwnership主线总入口.md` | `C5-M2-Boolean-BodyOwnership主线/工作步骤细分/` |
| C5-M3 Loft / Pipe 高级分支 | `C5-M3-LoftPipe高级分支主线/6-20-10-48-C5-M3-LoftPipe高级分支主线总入口.md` | `C5-M3-LoftPipe高级分支主线/工作步骤细分/` |
| C5-M4 Datum Attachment 引用稳定 | `C5-M4-DatumAttachment-引用稳定主线/6-20-10-48-C5-M4-DatumAttachment引用稳定主线总入口.md` | `C5-M4-DatumAttachment-引用稳定主线/工作步骤细分/` |
| C5-M5 Freeze 收口 | `C5-M5-Freeze收口主线/6-20-10-48-C5-M5-Freeze收口主线总入口.md` | `C5-M5-Freeze收口主线/工作步骤细分/` |
| C5-M6 Part Workbench Surface Profile / PostProcess 第二批（已收口） | `C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线总入口.md` | `C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/` |
| C5-M7 Part Workbench GeomPlateSurface Helper 第二批（已收口） | `C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/6-21-00-43-C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线总入口.md` | `C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/工作步骤细分/` |
| C5-M8 Part Workbench Filling Support / Order / Param 第二批（已收口） | `C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/6-21-10-01-C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线总入口.md` | `C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分/` |
| C5-M9 Part Workbench ProjectOnSurface Provenance 第二批 | `C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/6-21-19-25-C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线总入口.md` | `C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分/` |
| C5-M10 Part Workbench Sweep Advanced PipeShell Contract（已收口） | `C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/6-21-21-49-C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线总入口.md` | `C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/` |
| C5-M11 Part Workbench Sweep Wrapper Expected Parity（已收口） | `C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/6-22-00-23-C5-M11-PartWorkbenchSweepWrapperExpectedParity主线总入口.md` | `C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分/` |
| C5-M12 Part Workbench Surface Native Oracle Recovery（已收口） | `C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/6-22-02-02-C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线总入口.md` | `C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分/` |
| C5-M13 Part Workbench Surface Narrowed Blocker Recovery（已收口） | `C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/6-22-04-02-【已实现】C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线总入口.md` | `C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分/` |
| C5-M14 DatumPoint ProximityPoint AttachEngine（已收口） | `C5-M14-DatumPointProximityPointAttachEngine主线/6-22-12-25-C5-M14-DatumPointProximityPointAttachEngine主线总入口.md` | `C5-M14-DatumPointProximityPointAttachEngine主线/工作步骤细分/` |
| C5-M15 Datum3DPlane AttachEngine（已收口） | `C5-M15-Datum3DPlaneAttachEngine主线/6-22-15-17-【已实现】C5-M15-Datum3DPlaneAttachEngine主线总入口.md` | `C5-M15-Datum3DPlaneAttachEngine主线/工作步骤细分/` |
| C5-M16 Datum CurveFrame / Curvature AttachEngine（已收口） | `C5-M16-DatumCurveFrameCurvatureAttachEngine主线/6-22-21-39-【已实现】C5-M16-DatumCurveFrameCurvatureAttachEngine主线总入口.md` | `C5-M16-DatumCurveFrameCurvatureAttachEngine主线/工作步骤细分/` |
| C5-M17 Datum Remaining AttachEngine（已收口） | `C5-M17-DatumRemainingAttachEngine主线/6-23-03-26-【已实现】C5-M17-DatumRemainingAttachEngine主线总入口.md` | `C5-M17-DatumRemainingAttachEngine主线/工作步骤细分/` |
| C5-M18 Datum Folding AttachEngine（已收口） | `C5-M18-DatumFoldingAttachEngine主线/6-23-18-16-【已实现】C5-M18-DatumFoldingAttachEngine主线总入口.md` | `C5-M18-DatumFoldingAttachEngine主线/工作步骤细分/` |
| C5-M19 Datum AttachEngine Source Audit Closeout（已收口） | `C5-M19-DatumAttachEngineSourceAuditCloseout主线/6-23-18-58-【已实现】C5-M19-DatumAttachEngineSourceAuditCloseout主线总入口.md` | `C5-M19-DatumAttachEngineSourceAuditCloseout主线/工作步骤细分/` |
| C5-M20 Part Workbench Filling Precise Blocker Recovery（已收口） | `C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/6-23-19-17-【已实现】C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线总入口.md` | `C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/工作步骤细分/` |

C5-M6 已完成最终发布收口：`part_workbench.loft` 的 profile / `Linearize=true` expected-backed slice 保持为基础发布面，原 `complex_profile_family` broad gap 已由 C5-M12 代表 profile expected-backed 关闭；`part_workbench.sweep` 的 C5-M6 发布口径只包含 multi-profile / `Linearize=true` expected-backed 基础 slice，advanced PipeShell 字段级合同由 C5-M10/C5-M11/C5-M12 收口。

C5-M7 已完成 `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` 第二批发布：3D default / explicit approximation / InitialSurface / Curve2dOnSurface / Point2dOnSurface / mixed G0+2D / point criteria 为 expected-backed；G1 curve-on-surface 与 ProjectedCurve2d 为 source-backed known_gap；curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle 为 diagnostic-backed；GUI/native DocumentObject/Filling/full surface family 不进入支持声明。

C5-M8 已完成 `Part.makeFilledFace(...)` / source-backed Filling helper 第二批收口：S0-S4 已关闭 live guard、Surface/Supports/Orders source-backed known_gap、non-default constructor params metadata、non-boundary wire/edge/face/vertex constraints、compound optional expected-backed case，以及 direct wrapper / UV point-on-support `unsupported_wrapper_lifecycle` diagnostics；native helper geometry expected 只在 FreeCADCmd explicit kwargs / support-order oracle 稳定后替换 known_gap。native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native、cross-request mutable wrapper 和完整 Part surface family 仍为 non-goal。

C5-M9 打开 `Part::ProjectOnSurface` 第二批：第一批 `Mode=Edges/Faces/All`、face rebuild、height/offset、多 Projection ordered metadata 和普通 indexed `NamedShape` 继续作为 live guard；本包只推进 projected edge / wire / face / compound provenance、MapperHistory / ElementMap 账本和引用恢复证据，不重做 GUI task panel、完整 `ProjectOnSurface` 或完整 Part surface family。

C5-M10 已完成 `part_workbench.sweep` advanced PipeShell contract 收口：S0 冻结 C5-M6/C4M1 multi-profile / `Linearize=true` expected-backed 基础 slice 和 `part-sweep-advanced-deferred` locatable diagnostic baseline；S1 冻结字段级 source / DTO / oracle 矩阵；S2 发布 AuxiliarySpine / SupportMode / Binormal source-diagnostic-backed 合同；S3 发布 `SectionOptions[].Location`、`WithContact`、`WithCorrection`、`Tolerance.tol3d/boundTol/tolAngular` 和组合压力；S4 同步 capability/docs/root matrix，把原 broad advanced bucket 收口为字段级 `part_sweep_wrapper_expected_collector` source-backed known_gap。native `Part::Sweep` 直接属性仍只声明基础六项，GUI、PartDesign Pipe/Hole 产品支持、persistent Python wrapper lifecycle 和输出端 fixup 均为 non-goal。

C5-M11 已完成 `Part.BRepOffsetAPI_MakePipeShell` wrapper expected-backed 批量闭环：它不新增 C5-M10 字段，只为同一 wrapper API 增加 FreeCADCmd request-local collector，并把 collectable 的 auxiliary spine、binormal、tolerance 三个代表场景替换为 `shape_summary` + `object_fields.advanced` expected-backed；C5-M12 又补上 valid `SpineSupport` / `SupportMode` expected-backed representative；located profile 与 combined 保留为 FreeCADCmd `OCCError: NCollection_Array1::Value` narrowed blockers。broad `part_sweep_wrapper_expected_collector` 不再作为 remaining gap。

C5-M12 已完成 Part Workbench surface native-oracle recovery 收口：Sweep valid `SpineSupport` / `SupportMode` 已 expected-backed，located/combined 仍是 `NCollection_Array1::Value` blocker；Loft broad `complex_profile_family` 已由 wire/face 与 whole-sketch-object/vertex representatives expected-backed 关闭，只保留 sketch subelement native-hidden diagnostic；Filling non-boundary edge no-support/order 已 expected-backed，surface/support/order/params/non-boundary support-order 仍为 FreeCADCmd blocker；GeomPlate G1 为 native-hidden diagnostic-only，ProjectedCurve2d 为 RuntimeError blocker。完整 Part surface family、GUI/native DocumentObject 与 persistent helper/wrapper lifecycle 仍不进入支持声明。

C5-M13 已完成 C5-M12 后的 narrowed blocker recovery 收口：Sweep located/combined 保留为 `add(Profile, Location, WithContact, WithCorrection)` Location overload build-stage `NCollection_Array1::Value` blocker，combined 只依赖该 overload，no-location controls 已可 build；Filling `Degree`、`NumIter`、`Tol2d+Tol3d`、`MaxDegree` 单字段 representatives 已 expected-backed，其余 Surface、support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params、non-boundary support/order 保留 precise blocker；GeomPlate `ProjectedCurve2d + InitialSurface` 已 expected-backed，无 `InitialSurface` 的 ProjectedCurve2d 仍是 `Geom_RectangularTrimmedSurface::V1==V2` blocker，G1、criteria setter、`Part.PlateSurface.Curves` 保留 native-hidden/NotImplemented/non-goal 边界。Loft broad `complex_profile_family`、完整 Part surface family、GUI/native DocumentObject、persistent wrapper lifecycle 和 cad-core-output-derived expected 均未重开。

C5-M14 已完成 DatumPoint `ProximityPoint1/2` AttachEngine 主线：它承接 C51X `datum_attach_engine_remaining_modes` 的下一批可实现子项，只处理 `AttachEnginePoint::getProximityPoint()` 的 edge-face intersection 优先路径、`BRepExtrema_DistShapeShape` fallback、双 support request-local 解析、diagnostics、fixtures/expected/tests 与 capability blocker 移除。`IntersectionPoint`、Focus/curvature、Frenet/Concentric、三点/折叠、Directrix/Asymptote、GUI attachment editor、cross-request backend session 和 C5-M13 Part Workbench surface blocker 都不进入本包。

C5-M15 已完成 Datum3DPlane AttachEngine 主线：它不再做单 case thin plan，而是把同一 `AttachEngine3D::_calculateAttachedPlacement()` 调用链、同一 `datum_attachment.h` selected placement 边界、同一 `c51m5` expected 家族下的 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal` 纳入一轮。S6 已完成 FreeCADCmd expected、cad-core C++、fixtures、focused tests、capability/docs 与验收记录闭环，并只从 `datum_attach_engine_remaining_modes` 移除本包四个 proven modes。`Folding`、curve frame/curvature、conic landmarks、`IntersectionPoint` 和 GUI/session 仍为后续分包。

C5-M16 已完成 Datum curve frame / curvature AttachEngine 主线：S6 落地 FreeCADCmd expected、shared curve-frame/curvature helper、diagnostics、focused tests、adapter capability 和 docs/root matrix closeout。范围只发布共享 `AttachEngine3D::_calculateAttachedPlacement()` 曲线帧调用链下的 `FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`，以及复用该 3D branch 的 `AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature` aliases；`NormalToPath` 仍只是 shared projection/helper 合同，不作为本包 release mode。成功 fixture 覆盖 edge parameter、vertex projection、Frenet T/N/B orientation、curvature center 和 alias source mode；diagnostics fixture 覆盖 missing/invalid support、D1 zero derivative、undefined Frenet normal、infinite curvature radius。`projection_failed` 已有 source/code-path-backed diagnostic，但没有稳定 primitive fixture。capability exact blocker 只移除 expected-backed proven modes；`Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V`、GUI/session 与跨请求状态仍为后续分包或 non-goal。request-local helper 输入、response suggestions、无 backend session 和 `ReferenceShadow.brep` 单 subshape snapshot 例外保持不变。

C5-M17 已完成 Datum remaining AttachEngine conic landmark 首批发布：DatumLine `Directrix1/2`、`Asymptote1/2` 和 DatumPoint `Focus1/2` 已用 FreeCADCmd expected、`BRepAdaptor_Curve` + `gp_Elips/gp_Hypr/gp_Parab` C++ helper、`c51m5` fixtures、focused tests 和 adapter capability test 闭环，并从 exact blocker 删除本批 proven modes。`Folding` 仍是四线 fold-angle 状态机，`IntersectionPoint` 仍需要 face/face DTO 与 oracle，`TangentU/V` 仍属 surface tangent adjacent family，继续留在 exact blocker 或 later-package guard。

C5-M18 已完成 Datum Folding AttachEngine 主线：DatumPlane / Datum CoordinateSystem 的 `Folding` selected MapMode 已按 FreeCAD `mmFolding` branch 迁移到 `cad-core`，覆盖四条有序 straight line supports、shared vertex、direction sign normalization、`calculateFoldAngle`、DatumPlane normal/axis evidence、FreeCADCmd success expected、invalid diagnostics、focused tests 和 adapter capability closeout。C5-M18 发布后仅剩 `TangentU`、`TangentV`、`IntersectionPoint` 进入后续审计；GUI/session/full BREP 仍不进入 stateless CAD Core。

C5-M19 已完成 Datum AttachEngine 剩余模式源码审计与 blocker 收口：FreeCAD `Attacher.h` / `Attacher.cpp` 证明 `TangentU`、`TangentV`、`IntersectionPoint` 只有 enum/name 暴露，没有非空 `modeRefTypes` 和 `_calculateAttachedPlacement()` 执行分支；`TangentU/V` 只是 `mmTangentPlane` 内部 surface tangent 方向，`IntersectionPoint` 也不能从已支持的 DatumLine `IntersectionLine` 推导。capability exact blocker 已删除 `datum_attach_engine_remaining_modes`，三项转为 source-audited non-goal；Datum AttachEngine 线正式收口，后续优先转向 Filling / GeomPlate / Sweep 等 Part surface precise blockers。

C5-M20 已完成 Filling precise blocker 复核收口：它只承接 C5-M13 后保留的 `Part.makeFilledFace(...)` / `BRepOffsetAPI_MakeFilling` 同一 helper owner 下的 Surface、support/order G1/G2、`PtsOnCurve`、`Anisotropy`、`TolG1+TolG2`、`MaxSegments`、all-params 和 non-boundary support/order。FreeCADCmd 逐 case probe 证明当前 `Part.makeFilledFace(...)` 代表场景仍不能稳定采集 expected：Surface 为 SIGSEGV，support/order 为 CADKernelError / OCCError / timeout，剩余 params 为 SIGSEGV / timeout / OCCT build crash，non-boundary support/order 为 OCCError / timeout。Direct `Part.BRepOffsetAPI.MakeFilling` wrapper controls 可 build，但只作为低层 owner evidence，不替代 request-local helper expected，也不引入 persistent wrapper lifecycle。C5-M20 不新增 fixture、expected、collector supported path 或 C++ fallback，只更新 precise blocker evidence 和 delete condition。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M0-范围审计与基线重建主线/工作步骤细分 --format markdown
```
