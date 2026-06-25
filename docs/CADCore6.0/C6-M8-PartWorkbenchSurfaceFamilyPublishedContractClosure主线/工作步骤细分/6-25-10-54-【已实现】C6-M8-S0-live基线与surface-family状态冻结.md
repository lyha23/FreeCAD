# 【已实现】C6-M8 S0 live 基线与 surface-family 状态冻结

## 目标

冻结 C6-M8 live 起点，记录 C6-M1 到 C6-M7 队列状态、当前 surface family capability、adapter assertion 和 root README 入口。S0 是文档/矩阵步骤，不改 C++、fixtures 或 expected。

## live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`02798d9ca9`
- `git log -1 --oneline`：`02798d9ca9 docs: 新增 C6-M8 表面族合同收口方案`
- `git -c core.quotepath=false status --short -uall`：空输出，S0 开始时工作区干净。
- C6-M1 到 C6-M7 的 `工作步骤细分` 队列均为空；C6-M8 初始队列从 S0 开始，S0 文件标记后推进到 S1。

## surface-family capability baseline

- `part_workbench.project_on_surface.status=supported_expected_backed_published_slice`；`remaining_gaps=[gui_projection_task_panel, unverified_advanced_branches]`，且两项也在 `non_goals`，S2 必须给每项裁决单一路由。
- `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`；`remaining_gaps=[]`，`non_goals=[]`。
- `part_workbench.loft.status=supported_profile_linearize_complex_expected_backed_plus_c6m7_product_contract_non_parity`；`remaining_gaps=[]`，`narrowed_gaps` 保留 `part_loft_subelement_assignment_native_hidden`。
- `part_workbench.sweep.status=supported_multi_profile_linearize_c6m4_product_contract_non_parity`；`remaining_gaps=[]`，`narrowed_gaps` 保留 located profile 与 advanced combined FreeCADCmd wrapper build blocker。
- `part_workbench.filling.status=supported_expected_backed_plus_c6m5_product_contract_non_parity`；`remaining_gaps=[]`，native helper blocker 保留在 `narrowed_gaps` / historical evidence。
- `part_workbench.geomplate.status=supported_expected_backed_projected_initial_surface_plus_c6m6_product_contract_non_parity`；`remaining_gaps=[]`，G1 CurveOnSurface、ProjectedCurve2d no-initial-surface、criteria setter 和 PlateSurface wrapper 边界保留在 `narrowed_gaps` / `non_goals` / historical evidence。
- `cad-core/tests/test_adapters.py` 当前 assertion 覆盖上述 status、fixtures、`remaining_gaps`、`narrowed_gaps` 与 `non_goals` 口径；S0 不修改 C++ 或 assertion。

## 必读

- `docs/CADCore6.0/README.md`
- `docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/README.md`
- `docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure方案.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`

## 动作

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 分别运行 C6-M1 到 C6-M8 `工作步骤细分` 的 `step_goal_queue.py`。
3. grep 当前 `part_workbench.project_on_surface`、`ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 的 `status`、`remaining_gaps`、`narrowed_gaps` 和 `non_goals`。
4. 更新 C6-M8 README、总入口和矩阵中的 live baseline 行。
5. 确认 root `docs/CADCore6.0/README.md` 已链接 C6-M8。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```

## 通过条件

- live baseline 写入本步骤、README / 总入口和矩阵。
- ProjectOnSurface 的 active/non-goal overlap 被记录为 S2 必裁决项。
- S0 文件名和标题标记为 `【已实现】` 后，队列推进到 S1。
