# C5-M6-S0 live 基线与 scope 冻结

## 目标

确认 C5-M6 的 live 事实：Loft / Sweep profile-postprocess 第二批是否已经由当前代码、fixtures、expected 和 adapter capability 覆盖；冻结后续 S1-S4 的执行边界。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_fixture_oracle_matrix.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c4m1`

## 工作内容

1. 记录 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 复核 `part_workbench.loft` 和 `part_workbench.sweep` capability status、fixtures、remaining gaps。
3. 复核 c4m1 Loft/Sweep fixtures 与 expected 是否存在。
4. 更新本包矩阵，只把 live 证据明确的内容标成 supported / expected-backed。
5. 不改 C++，不新增 fixture。

## S0 live 记录

基线命令输出：

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
7217840df4

git log -1 --oneline
7217840df4 feat: 发布ProjectOnSurface能力收口

git -c core.quotepath=false status --short -uall
 M docs/CADCore5.0-PartDesign-高价值剩余语义/README.md
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_blocker_queue.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_fixture_oracle_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_non_goal_registry.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_scope_review_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_source_candidates.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_validation_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线总入口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批方案.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/6-20-22-04-C5-M6-S0-live基线与scope冻结.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/6-20-22-05-C5-M6-S1-LoftProfilePostProcess复核收口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/6-20-22-06-C5-M6-S2-SweepMultiProfilePostProcess复核收口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/6-20-22-07-C5-M6-S3-剩余复杂Profile与高级契约分流.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分/6-20-22-08-C5-M6-S4-capability与文档收口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_blocker_queue.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_fixture_oracle_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_non_goal_registry.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_scope.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/矩阵/c5m6_surface_profile_postprocess_validation_matrix.tsv
```

复核结论：

- `part_workbench.loft`：`c_api.cpp` 发布 `supported_profile_linearize_expected_backed`；`test_p8_features.py` 覆盖 `part-loft-linearize-profile-face` / `part-loft-linearize-profile-vertex` 并调用 `assert_object_matches_expected`；`cad-core/fixtures/c4m1/expected/` 有对应 FreeCAD expected。S1 只复核同步，不从旧计划重复实现。
- `part_workbench.sweep`：`c_api.cpp` 发布 `supported_multi_profile_linearize_expected_backed`；`test_p8_features.py` 覆盖 `part-sweep-multi-profile-linearize` 并调用 `assert_object_matches_expected`；`cad-core/fixtures/c4m1/expected/` 有对应 FreeCAD expected。S2 只复核同步。
- `part-sweep-advanced-deferred`：focused test 只断言 `AdvancedSweep` 的 `AuxiliarySpine` / `Tolerance` 输出 locatable `unsupported_property` diagnostics；该 fixture 不属于 expected-backed support。
- S3/S4 边界冻结：`complex_profile_family` 与 advanced PipeShell wrapper contract 继续作为 future owner / non-goal；S4 只负责 capability、CADCore3.0 文档和队列口径收口。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线
```

完成后重命名为 `6-20-22-04-【已实现】C5-M6-S0-live基线与scope冻结.md`。
