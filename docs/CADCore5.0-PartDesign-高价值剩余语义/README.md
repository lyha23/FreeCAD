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
| C5-M10 Part Workbench Sweep Advanced PipeShell Contract | `C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/6-21-21-49-C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线总入口.md` | `C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/` |

C5-M6 已完成最终发布收口：`part_workbench.loft` 只发布 profile / `Linearize=true` expected-backed slice，剩余 `complex_profile_family` 路由到 `future_loft_complex_profile_family`；`part_workbench.sweep` 只发布 multi-profile / `Linearize=true` expected-backed slice，advanced contract 路由到 `future_sweep_advanced_contract`。

C5-M7 已完成 `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` 第二批发布：3D default / explicit approximation / InitialSurface / Curve2dOnSurface / Point2dOnSurface / mixed G0+2D / point criteria 为 expected-backed；G1 curve-on-surface 与 ProjectedCurve2d 为 source-backed known_gap；curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle 为 diagnostic-backed；GUI/native DocumentObject/Filling/full surface family 不进入支持声明。

C5-M8 已完成 `Part.makeFilledFace(...)` / source-backed Filling helper 第二批收口：S0-S4 已关闭 live guard、Surface/Supports/Orders source-backed known_gap、non-default constructor params metadata、non-boundary wire/edge/face/vertex constraints、compound optional expected-backed case，以及 direct wrapper / UV point-on-support `unsupported_wrapper_lifecycle` diagnostics；native helper geometry expected 只在 FreeCADCmd explicit kwargs / support-order oracle 稳定后替换 known_gap。native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native、cross-request mutable wrapper 和完整 Part surface family 仍为 non-goal。

C5-M9 打开 `Part::ProjectOnSurface` 第二批：第一批 `Mode=Edges/Faces/All`、face rebuild、height/offset、多 Projection ordered metadata 和普通 indexed `NamedShape` 继续作为 live guard；本包只推进 projected edge / wire / face / compound provenance、MapperHistory / ElementMap 账本和引用恢复证据，不重做 GUI task panel、完整 `ProjectOnSurface` 或完整 Part surface family。

C5-M10 打开 `part_workbench.sweep` 的 advanced PipeShell contract：S0 已冻结 C5-M6/C4M1 multi-profile / `Linearize=true` expected-backed 基础 slice 和 `part-sweep-advanced-deferred` locatable `unsupported_property` diagnostic baseline；S1 已冻结字段级 source / DTO / oracle 矩阵；S2 已实现 AuxiliarySpine / SupportMode / Binormal source-diagnostic-backed 合同；S3 已实现 `SectionOptions[].Location`、`WithContact`、`WithCorrection`、`Tolerance.tol3d/boundTol/tolAngular` 和 S2+S3 组合压力。上述 advanced 字段仍是 request-local DTO / API 合同，FreeCAD wrapper expected 采集未落地前保持 source-backed known_gap；native `Part::Sweep` 直接属性仍只声明基础六项，GUI、PartDesign Pipe 产品支持和 persistent Python wrapper lifecycle 不进入本包。S4 仍需做 capability/docs/root matrix 最终收口。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M0-范围审计与基线重建主线/工作步骤细分 --format markdown
```
