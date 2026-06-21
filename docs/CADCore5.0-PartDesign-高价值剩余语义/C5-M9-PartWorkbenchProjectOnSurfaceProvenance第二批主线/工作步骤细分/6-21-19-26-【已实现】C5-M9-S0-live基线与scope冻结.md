# 【已实现】C5-M9-S0 live 基线与 scope 冻结

状态：`done_C5M9-S0_live_guard`

## live baseline

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`f7072e43f7`
- `git log -1 --oneline`：`f7072e43f7 文档: 开启C5-M9 ProjectOnSurface来源追踪主线`
- `git -c core.quotepath=false status --short -uall`：无输出，S0 起点工作区干净。

## 目标

冻结 C5-M9 的 live baseline，证明本包只打开 `Part::ProjectOnSurface` projected subshape provenance / mapper history 第二批，不重做 C4M1 已 expected-backed 的投影几何。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/6-21-19-25-C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-ProjectOnSurface独立主线/`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/tests/test_p8_features.py`

## 产物

- 已记录当前 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git status --short -uall`。
- 已复核 12 个 `cad-core/fixtures/c4m1/part-project-on-surface-*` fixture 与 adapter capability 的当前状态。
- 已修正文档/矩阵措辞，让 root row、local matrix 和 README 对 C5-M9 范围一致。
- 已更新本 step 文件名为 `【已实现】...`，并在局部 blocker queue 中关闭 `C5M9-BLK-000`。

## 复核结论

- `cad-core/fixtures/c4m1` 当前有 12 个 `part-project-on-surface-*` 输入 fixture；`expected/` 下对应 11 个 FreeCAD expected。`part-project-on-surface-deferred-boundaries` 是 stable diagnostics guard，不是 expected-backed geometry fixture。
- `cad-core/tests/test_p8_features.py` 当前覆盖 12 个 C4M1 ProjectOnSurface case：11 个 geometry case 调用 `assert_object_matches_expected()`，`deferred-boundaries` 固定 missing target、invalid subshape、unsupported subshape kind、missing property 等 diagnostics。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.project_on_surface` capability 当前状态为 `supported_expected_backed_published_slice`，fixtures 列出 12 个 C4M1 guard；`covered` 同时包含 `expected_backed_fixture` 与 `deferred_branch_diagnostics`。
- adapter capability 当前 remaining gaps 仍为 `projected_edge_provenance_mapper_history`、`gui_projection_task_panel`、`unverified_advanced_branches`；non-goals 也保留 GUI、provenance gap 和未验证高级分支。S0 不关闭这些 gap。
- `cad-core/src/part/part_project_on_surface.cpp` 当前仍发布 `topo_naming_history=indexed_projected_edges_no_mapper_history`，测试也断言普通 indexed `NamedShape` 不含 `ProjectionLine.Edge1` / `ProjectionFace.Face1` 的 ElementMap；这正是 C5-M9 后续打开的 provenance / mapper history 范围。

## scope 冻结

C5-M9 本包只打开 `Part::ProjectOnSurface` projected subshape provenance / mapper history 第二批；不重做 C4M1 已 expected-backed 的投影几何，不采集新 expected，不声明完整 `ProjectOnSurface` 支持。S0 只冻结 live guard 与能力口径，不修改 `cad-core` 实现。

## 非目标

- 不写 cad-core 实现。
- 不采集新 expected。
- 不关闭 `projected_edge_provenance_mapper_history` gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
