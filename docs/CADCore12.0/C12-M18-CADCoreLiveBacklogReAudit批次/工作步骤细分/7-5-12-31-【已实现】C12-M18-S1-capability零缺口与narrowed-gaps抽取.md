# 【已实现】C12-M18 S1 capability 零缺口与 narrowed gaps 抽取

从 current capability 中抽取所有可作为 backlog 输入的结构化事实。

## 必读

- `../README.md`
- `../矩阵/c12m18_live_backlog_source_candidates.tsv`
- `../矩阵/c12m18_live_backlog_backend_gap_classification.tsv`
- `../../README.md`
- `../../C12-M9-CADCoreImplementationCandidate再盘点批次/README.md`
- `../../C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/README.md`
- `../../../../cad-core/src/runtime/capability_contract.cpp`
- `../../../../cad-core/tests/test_adapters.py`

## 操作

1. 生成 `/tmp/c12m18-capabilities.json`。
2. 抽取非空 `remaining_gaps` 和 `known_gaps`；当前预期为空。
3. 抽取所有 `narrowed_gaps` path 和 keys，写入 source / backend 矩阵。
4. 记录 publication authority 与 focused adapter assertions。
5. 不做 current mismatch 判断；S1 只建立输入清单。
6. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 完成记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=09c7dad8ed`（`09c7dad8ed 文档：冻结 C12-M18 S0 live 基线`）。
- dirty boundary：`git -c core.quotepath=false status --short -uall` 输出为空。
- capability snapshot：`cad-core/build/cad-core capabilities > /tmp/c12m18-capabilities.json`。
- 非空 `remaining_gaps` jq 查询无输出；非空 `known_gaps` jq 查询无输出。
- `narrowed_gaps` 抽取结果为 6 个 path / 15 个 key：
  - `part_design.revolution_groove.narrowed_gaps`: `partdesign_groove_upto_brepfeat_cut_native_failure`
  - `part_workbench.filling.narrowed_gaps`: `filling_non_boundary_support_order_native_helper_blocker`, `filling_params_all_native_helper_blocker`, `filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker`, `filling_support_order_g1_native_helper_blocker`, `filling_support_order_g2_native_helper_blocker`, `filling_surface_native_helper_blocker`
  - `part_workbench.geomplate.narrowed_gaps`: `curve_constraint_criteria_setters_not_implemented`, `g1_curve_on_surface_native_hidden_diagnostic_only`, `platesurface_curves_wrapper_lifecycle`, `projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`
  - `part_workbench.loft.narrowed_gaps`: `part_loft_subelement_assignment_native_hidden`
  - `part_workbench.project_on_surface.narrowed_gaps`: `native_project_on_surface_mapper_history_oracle_unavailable`
  - `part_workbench.sweep.narrowed_gaps`: `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`, `part_sweep_located_profile_freecadcmd_wrapper_build_blocker`
- Publication authority：`cad-core/src/runtime/capability_contract.cpp::capabilityContractJson()` 与本地 capability helpers；S1 只读取并记录，不修改 capability source。
- Focused adapter assertions：`cad-core/tests/test_adapters.py::CadCoreAdapterTest.test_c_api_capabilities_publication_smoke` 与 `test_c_api_capabilities_exposes_web_contract_facts` 覆盖 schema/core entrypoint、family `remaining_gaps=[]`、top-level `known_gaps=[]` 和主要 narrowed-gaps keys/statuses。
- S1 只更新 C12-M18 source/backend 矩阵、C12-M18 README、root `docs/CADCore12.0/README.md` 与本步骤状态；未运行 FreeCADCmd，未修改 C++、fixtures、expected、tests 或 adapters。
- S1 不做 current mismatch 判断，不把 `narrowed_gaps` 判成实现项；后续 S2/S3 继续走 stable expected / approved product contract、request-local boundary、current mismatch 三闸门。

## 非目标

- 不把 `narrowed_gaps` 直接判成实现项。
- 不改 capability source。
- 不运行 FreeCADCmd。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities > /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "remaining_gaps") and ((getpath($p)|type)=="array") and ((getpath($p)|length)>0)) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "known_gaps") and (((getpath($p)|type)=="array" and (getpath($p)|length)>0) or ((getpath($p)|type)=="object" and (getpath($p)|length)>0))) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select($p[-1]? == "narrowed_gaps") | {path:($p|join(".")), keys:(getpath($p)|keys)}' /tmp/c12m18-capabilities.json
git diff --check
```
