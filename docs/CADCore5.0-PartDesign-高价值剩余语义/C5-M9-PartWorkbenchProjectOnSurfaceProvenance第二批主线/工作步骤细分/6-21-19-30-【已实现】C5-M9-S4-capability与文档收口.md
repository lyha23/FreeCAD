# 【已实现】C5-M9-S4 capability 与文档收口

状态：`done_C5M9-S4_capability_docs_closed`

## live baseline

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`cc46064dba`
- `git log -1 --oneline`：`cc46064dba 实现ProjectOnSurface面与复合来源追踪`
- `git -c core.quotepath=false status --short -uall`：无输出，S4 起点工作区干净。

## 目标

把 C5-M9 的实现结果同步到 capabilities、gap 对照、oracle queue、root matrices 和本包文档。关闭 `projected_edge_provenance_mapper_history` broad gap，并证明本包队列为空后收口。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- 已复核 `cad-core/src/adapters/c_api/c_api.cpp` 与 `cad-core/tests/test_adapters.py`：`part_workbench.project_on_surface.remaining_gaps` 只保留 `gui_projection_task_panel` 与 `unverified_advanced_branches`；`native_project_on_surface_mapper_history_hidden_until_probe` 是 request-local boundary/source-backed native-hidden known_gap 删除条件，不再是 broad projected provenance gap。本步无需修改 adapter 代码或测试。
- 已更新 `docs/CADCore3.0/capabilities-gap对照表.md`：ProjectOnSurface 发布口径覆盖 C4M1 expected-backed geometry、C5-M9 edge/wire/face/all provenance、Projection item ledger、MapperHistory / ElementMap / reference recovery hook，以及 exact non-goal / remaining gap。
- 已更新 `docs/CADCore3.0/oracle-fixture队列.md`：ORC-M4-PARTSURF-006 记录 C5-M9 source-backed provenance fixtures、native-hidden mapper/history delete condition，以及 GUI / unverified advanced / full surface family 边界。
- 已更新 C5 root matrices：`C5-BLK-901` 与 `C5-SCOPE-901` 关闭为 `done_c5m9_capability_docs_closed`。
- 已更新本包入口、方案、local scope/blocker matrices：`C5M9-BLK-401` 与 `C5M9-SCOPE-401` 关闭为 `done_C5M9-S4_capability_docs_closed`。
- 本 step 文件已改名为 `6-21-19-30-【已实现】C5-M9-S4-capability与文档收口.md`。

## 非目标

- 不把 unsupported GUI / full family 写成 supported。
- 不补超出 S2/S3 证据的实现。
- 不用 broad “ProjectOnSurface 完整支持” 替代精确 capability wording。
- 不删除 source-backed native-hidden known_gap 删除条件。

## capability 边界

- supported：C4M1 `Mode=Edges/Faces/All`、face rebuild / hole wires、height solid、offset、多 Projection order、普通 indexed NamedShape；C5-M9 edge/wire projection item ledger、wire fragment ownership、invalid provenance diagnostics、face outer/inner wire evidence、face rebuild ownership、Mode=All compound/height solid provenance、ElementMap/reference recovery hook。
- source-backed native-hidden boundary：`native_project_on_surface_mapper_history_hidden_until_probe`，删除条件是 FreeCADCmd collector 或专用 probe 能导出 native ProjectOnSurface mapper history、child map 或 ElementMap reference recovery。
- remaining gaps / non-goals：`gui_projection_task_panel`、`unverified_advanced_branches`；完整 `ProjectOnSurface` family 与完整 Part surface family 不声明为 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

结果：

- `git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core`：通过，无输出。
- `python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`：`Ran 211 tests in 70.093s`，`OK (skipped=25)`。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：只输出表头，无 pending step，本包队列为空。
