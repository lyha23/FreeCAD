# C6-M4 PartWorkbench Sweep LocatedProfile Combined PipeShell ProductContract 方案

## 方案口径

C6-M4 只处理 `Part::Sweep` / request-local `Part.BRepOffsetAPI_MakePipeShell` wrapper 中的 located profile 与 combined auxiliary + transition + tolerance case。当前 FreeCADCmd wrapper 对 Location overload 无法采到 `shape_summary`，所以本主线的第一目标是把 blocker 边界、oracle 条件和 CAD Core product contract 分开：能采集的继续 expected-backed；不能采集的只在产品合同证据足够时作为 CAD Core product extension 实现，不伪装成 FreeCAD parity。

## 最小完整语义批次

本批次必须一起覆盖：

- located profile：`SectionOptions[].Location`、`WithContact`、`WithCorrection`、profile placement metadata、invalid location diagnostics。
- combined advanced：`AuxiliarySpine`、`AuxiliaryCurvilinear`、`Transition`、`Tolerance.tol3d/boundTol/tolAngular` 与 located section 同时出现。
- fixtures/tests：保留 c5m10 known_gap guard，新建 c6m4 product fixtures 与 expected；capability 删除 blocker 只能发生在 S5/S6。
- docs/capability：明确 `not FreeCAD parity`、request-local、无 persistent wrapper lifecycle、无 BREP 长期状态。

拆成单个 fixture 会导致 Location blocker 与 combined 依赖长期分离，因此 C6-M4 采用一个主线批次；S2 已复采集并保留 native wrapper `notCollected`，同时冻结 C6-M4 non-parity product contract 路线，S3 必须消费 `C6M4-SCOPE-102`，不能把 c5m10 known_gap 改成 supported 或做窄路径输出修正。

## 步骤框架

1. S0：冻结 live baseline、capability remaining gaps、c5m10 known_gap 与 focused tests。
2. S1：复核 FreeCAD source authority、wrapper overload、probe 脚本和 cad-core 现有落点。
3. S2：冻结 located profile input/output/diagnostics/product contract；复采集或复核 FreeCADCmd blocker delete condition。
4. S3：已实现 profile placement / Location overload 产品路径；通过显式 `ProfilePlacement=AnchorLocationToSpineStart` 进入非 parity CAD Core product contract，旧 c5m10 known_gap 保留。
5. S4：在 S3 的 located profile 结果上接入 auxiliary + transition + tolerance combined case。
6. S5：已落 fixtures、expected、focused tests、capability contract 和 docs；两个 exact FreeCADCmd wrapper blocker 已从 `part_workbench.sweep.remaining_gaps` 移除，但 c5m10 expected/tests 作为 historical guard 保留。
7. S6：跑阶段回归，复核 release gate 和状态一致性。

## 验收纪律

- S0-S5 只跑本轮短跑与 focused 命令；阶段回归和 heavy 只在 S6 或能力发布时成为必须项。
- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 的删除条件：Location overload 已可采到稳定 FreeCADCmd `shape_summary`，或 C6-M4 product contract 已完整发布并有 c6m4 fixtures/tests/capability/docs 证明，同时保留 non-parity provenance。
- `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 的删除条件：S3 Location path 已发布，且 combined auxiliary + located section + tolerance 的 product fixture 和 diagnostics 均通过。
- Filling、Loft、Groove 只记录在 non-goal，不参与本主线实现。
