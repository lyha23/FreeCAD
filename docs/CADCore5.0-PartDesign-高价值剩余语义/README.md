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
| C5-M12 Part Workbench Surface Native Oracle Recovery | `C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/6-22-02-02-C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线总入口.md` | `C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分/` |

C5-M6 已完成最终发布收口：`part_workbench.loft` 只发布 profile / `Linearize=true` expected-backed slice，剩余 `complex_profile_family` 路由到 `future_loft_complex_profile_family`；`part_workbench.sweep` 的 C5-M6 发布口径只包含 multi-profile / `Linearize=true` expected-backed 基础 slice，advanced PipeShell 字段级合同由 C5-M10 收口。

C5-M7 已完成 `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` 第二批发布：3D default / explicit approximation / InitialSurface / Curve2dOnSurface / Point2dOnSurface / mixed G0+2D / point criteria 为 expected-backed；G1 curve-on-surface 与 ProjectedCurve2d 为 source-backed known_gap；curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle 为 diagnostic-backed；GUI/native DocumentObject/Filling/full surface family 不进入支持声明。

C5-M8 已完成 `Part.makeFilledFace(...)` / source-backed Filling helper 第二批收口：S0-S4 已关闭 live guard、Surface/Supports/Orders source-backed known_gap、non-default constructor params metadata、non-boundary wire/edge/face/vertex constraints、compound optional expected-backed case，以及 direct wrapper / UV point-on-support `unsupported_wrapper_lifecycle` diagnostics；native helper geometry expected 只在 FreeCADCmd explicit kwargs / support-order oracle 稳定后替换 known_gap。native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native、cross-request mutable wrapper 和完整 Part surface family 仍为 non-goal。

C5-M9 打开 `Part::ProjectOnSurface` 第二批：第一批 `Mode=Edges/Faces/All`、face rebuild、height/offset、多 Projection ordered metadata 和普通 indexed `NamedShape` 继续作为 live guard；本包只推进 projected edge / wire / face / compound provenance、MapperHistory / ElementMap 账本和引用恢复证据，不重做 GUI task panel、完整 `ProjectOnSurface` 或完整 Part surface family。

C5-M10 已完成 `part_workbench.sweep` advanced PipeShell contract 收口：S0 冻结 C5-M6/C4M1 multi-profile / `Linearize=true` expected-backed 基础 slice 和 `part-sweep-advanced-deferred` locatable diagnostic baseline；S1 冻结字段级 source / DTO / oracle 矩阵；S2 发布 AuxiliarySpine / SupportMode / Binormal source-diagnostic-backed 合同；S3 发布 `SectionOptions[].Location`、`WithContact`、`WithCorrection`、`Tolerance.tol3d/boundTol/tolAngular` 和组合压力；S4 同步 capability/docs/root matrix，把原 broad advanced bucket 收口为字段级 `part_sweep_wrapper_expected_collector` source-backed known_gap。native `Part::Sweep` 直接属性仍只声明基础六项，GUI、PartDesign Pipe/Hole 产品支持、persistent Python wrapper lifecycle 和输出端 fixup 均为 non-goal。

C5-M11 已完成 `Part.BRepOffsetAPI_MakePipeShell` wrapper expected-backed 批量闭环：它不新增 C5-M10 字段，只为同一 wrapper API 增加 FreeCADCmd request-local collector，并把 collectable 的 auxiliary spine、binormal、tolerance 三个代表场景替换为 `shape_summary` + `object_fields.advanced` expected-backed；support 保留为 diagnostic-only narrowed blocker，因为当前 fixture 没有 valid `SpineSupport` representative；located profile 与 combined 保留为 FreeCADCmd `OCCError: NCollection_Array1::Value` narrowed blockers。S3/S4 已同步 focused tests、capability metadata、C3 gap 文档和 root/package matrices，broad `part_sweep_wrapper_expected_collector` 不再作为 remaining gap。

C5-M12 打开 Part Workbench surface native-oracle recovery 批次：不重开完整 surface family，只批量处理 Sweep wrapper narrowed blockers、Loft `complex_profile_family`、Filling native helper expected blockers、GeomPlate native oracle blockers；S0-S5 以 oracle/probe-first、expected-backed 或 narrowed blocker、focused tests、capability/docs/root matrix 闭环为收口标准。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M0-范围审计与基线重建主线/工作步骤细分 --format markdown
```
