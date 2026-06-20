# C5-M6-S4 capability 与文档收口【已实现】

## 目标

同步 C5-M6、本包矩阵、CADCore3.0 capability 文档和 `cad_core_capabilities_json()` 的发布口径，关闭队列。

## 必读

- 本目录 S0-S3 已实现文件。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 工作内容

1. 确认 `part_workbench.loft` 发布为 profile / linearize expected-backed slice。
2. 确认 `part_workbench.sweep` 发布为 multi-profile / linearize expected-backed slice。
3. 确认 remaining gaps / non-goals 没有 broad `full_part_surface_family` 或旧 `linearize_post_processing`。
4. 更新 README、矩阵和 CADCore3.0 docs。
5. 运行队列脚本；队列为空后收口。

## S4 live 记录

基线命令输出：

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
623c79d6d2

git log -1 --oneline
623c79d6d2 docs: 收口 C5-M6 S3 剩余分流

git -c core.quotepath=false status --short -uall
<clean>
```

收口结论：

- `cad-core/src/adapters/c_api/c_api.cpp` 中 `part_workbench.loft.status=supported_profile_linearize_expected_backed`；Loft `remaining_gaps` / `non_goals` 均只保留 `complex_profile_family`，后续 owner 是 `future_loft_complex_profile_family`。
- `cad-core/src/adapters/c_api/c_api.cpp` 中 `part_workbench.sweep.status=supported_multi_profile_linearize_expected_backed`；Sweep `remaining_gaps` 只保留 `part_sweep_auxiliary_spine_contract`、`part_sweep_support_mode_contract`、`part_sweep_binormal_contract`、`part_sweep_location_mode_contract`、`part_sweep_tolerance_contract`。
- `cad-core/tests/test_adapters.py` 明确断言 Loft / Sweep 不把 `full_part_surface_family` 写入 covered 或 remaining gaps，并断言 Sweep remaining gaps 不含 `linearize_post_processing`、`multi_profile_sections_expected`、`advanced_pipeshell_wrapper`、`hole_model_thread_internal_pipeshell`。
- CADCore5 根 README / blocker / scope / validation 矩阵与本包总入口 / 方案 / 矩阵均已同步为 final done；`C5M6-BLK-005` 关闭。
- 本步未修改 C++ 或 fixture；不需要 `cmake --build build`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 docs/CADCore3.0 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

完成后按仓库规则提交本轮相关改动。
