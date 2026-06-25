# 【已实现】C6-M8 S4 fixtures tests capability docs 发布

## 目标

核对 S3 已发布的 capability、adapter assertion、C6-M8 docs 和 root README。S4 负责让用户和前端看到一致的 surface family 合同；若 S3 的 capability/test 已一致，S4 不重新打开 executor、fixtures 或 expected 批次。

## S4 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`d4a96dea91`
- `git log -1 --oneline`：`d4a96dea91 docs: 完成 C6-M8 S3 发布口径收口`
- `git -c core.quotepath=false status --short -uall`：空输出，S4 开始时工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：队列从本 S4 文件开始，后续为 S5。

## 动作

1. 复核 `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench` surface family 状态已经采用 S3 发布口径。
2. 复核 `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已锁定 ProjectOnSurface `remaining_gaps=[]`、GUI non-goal、native mapper hidden `narrowed_gaps`。
3. 更新 C6-M8 矩阵，把 S2/S3 route 写成 published / closed / non-goal，并删除仍暗示 S3 未发布的措辞。
4. 更新 `docs/CADCore6.0/README.md` 的 C6-M8 当前状态。
5. S3 未新增 fixtures 或 expected/product metadata；S4 只确认 fixture list、object_fields、diagnostics 和 capability evidence 没有被错误扩大。

## S4 收口结果

- capability 与 adapter assertion 已发布一致：`part_workbench.project_on_surface.remaining_gaps=[]`；`gui_projection_task_panel`、`gui_selection_camera_session` 只保留在 `non_goals`；`native_project_on_surface_mapper_history_hidden_until_probe` 保留为 `request_local_boundaries` 与 `narrowed_gaps` historical evidence。
- `ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 未倒退：active `remaining_gaps=[]` 继续由 adapter assertion、fixtures、`narrowed_gaps` 和 `non_goals` 证据保护。
- S3/S4 未新增 fixtures 或 expected，未修改 `cad-core/src/part/*` executor；fixture list、object_fields、diagnostics 只是被验证和发布引用，没有扩大支持声明。
- C6-M8 README、主线总入口、root `docs/CADCore6.0/README.md` 与矩阵已同步到 S4 published/closed 状态，下一步为 S5 release gate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'C6-M8|project_on_surface|ruled_surface|loft|sweep|filling|geomplate|remaining_gaps|non_goals|narrowed_gaps' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```

## 验收结果

- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`：`Ran 1 test in 0.134s`，`OK`。
- `rg -n 'C6-M8|project_on_surface|ruled_surface|loft|sweep|filling|geomplate|remaining_gaps|non_goals|narrowed_gaps' ...`：通过，publication facts 可定位。
- `awk -F '\t' ... 矩阵/*.tsv`：通过。
- `git diff --check -- cad-core docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md`：通过。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：队列推进到 S5。

## 通过条件

- capability、adapter assertion、README 和矩阵口径一致。
- ProjectOnSurface 不再同时用 active gap 和 non-goal 表达同一个边界，除非 S2 明确保留并写出 delete condition。
- S4 文件名和标题已标记为 `【已实现】` 后，队列推进到 S5。
