# C6-M4 Part Workbench Sweep LocatedProfile Combined PipeShell Product Contract 主线

本目录是 CADCore6.0 的第四条主线。它聚焦 `Part::Sweep` / `BRepOffsetAPI_MakePipeShell` located profile、profile placement，以及 auxiliary + transition + tolerance combined 高级 case。

## 当前状态

- S0-S5 已实现；S6 仍为 `待执行`。工作步骤总入口只是已创建索引，当前执行入口从 S6 开始。
- 当前 live 代码保留 c5m10 `part-sweep-located-profile-contract` 和 `part-sweep-advanced-combined-contract` 作为 known_gap/request_metadata_only historical guard；capability 已通过 c6m4 fixtures 发布非 FreeCAD parity 的 located profile 与 advanced combined CAD Core product path。
- 两个 focused guard 已在 S0 live 复核通过：located profile known_gap guard 与 combined known_gap/diagnostics priority guard。
- S1 已冻结 FreeCAD wrapper source authority、cad-core DTO/builder 落点、oracle/input/non-goal 矩阵；不声明 FreeCAD parity，不提升 capability，不删除 remaining gaps。
- S2 复跑本机 `FreeCADCmd 1.2.0 Revision 20260519` checked-in probe 后仍得到 `OCCError: NCollection_Array1::Value` build-stage blocker；c5m10 expected 保持 current known_gap，C6-M4 fixtures 路线保持 planned non-parity product/diagnostic contract。
- S3 已落地 `SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart` 显式 product-contract selector，valid located profile 输出 shape/metadata/NamedShape history；missing/invalid/non-vertex/multi-subname Location 和 malformed bool 先返回 diagnostics，不进入 no-location fallback；capability 发布由 S5 完成。
- S4 已落地 combined auxiliary + transition + tolerance + located section product fixture：`part-sweep-advanced-combined-product` 输出 `contract_provenance=cad_core_product_contract_non_parity`、advanced auxiliary/tolerance/section placement metadata、shape 和 NamedShape PipeShell history；c5m10 combined known_gap guard 保持不变。
- S5 已发布 `part_workbench.sweep` capability：status 为 `supported_multi_profile_linearize_c6m4_product_contract_non_parity`，covered/fixtures/field_boundaries/request_local_boundaries/narrowed_gaps 已列入 c6m4 product 与 diagnostics fixtures；`part_sweep_located_profile_freecadcmd_wrapper_build_blocker`、`part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 不再是 `remaining_gaps`，但仍保留为 narrowed historical wrapper evidence。

## 主线边界

- 本包可以把 located profile 和 combined case 推进为 CAD Core product contract，但不声明 FreeCAD parity。
- `Part::Sweep::execute()` 的 native DocumentObject 只作为标准 Sweep source authority；高级 wrapper 依据来自 `BRepOffsetAPI_MakePipeShellPyImp.cpp`。
- 不混入 Filling、Loft、Groove、GUI/TaskPanel、PartDesign Pipe/Hole 或 persistent wrapper lifecycle。

## 队列

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```
