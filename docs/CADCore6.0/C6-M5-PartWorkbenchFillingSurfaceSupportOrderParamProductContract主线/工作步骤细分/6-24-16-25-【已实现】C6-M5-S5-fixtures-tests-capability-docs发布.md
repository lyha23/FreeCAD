# 【已实现】C6-M5-S5 fixtures / tests / capability / docs 发布

## 目标

把 S3/S4 已实现的 Filling product contract 发布到 fixture、focused tests、capability contract 和文档矩阵。S5 是发布整理步骤，不新增大块语义实现。

## 输入

- S3/S4 已通过的 C++ 改动。
- 新增或更新的 C6-M5 Filling fixtures。
- `cad-core/tests/test_p8_features.py` focused tests。
- `cad-core/src/runtime/capability_contract.cpp` capability 更新。
- 本目录矩阵与 README。

## 发布规则

- 只有已有 fixture / focused test 覆盖的 row 才能从 `remaining_gaps` 删除。
- 被 native helper blocker 证明但未实现的行必须留在 `narrowed_gaps` 或 blocker queue。
- capability `covered`、`request_local_boundaries`、`field_boundaries`、`fixtures`、`remaining_gaps` 必须一致。
- docs 只能记录最终证据，不写流水账。

## S5 发布结果

- `cad-core/src/runtime/capability_contract.cpp` 将 `part_workbench.filling.status` 发布为 `supported_expected_backed_plus_c6m5_product_contract_non_parity`。
- `part_workbench.filling.remaining_gaps=[]`；六个 S0 blocker 均由 S3/S4 fixture + focused test 覆盖后从 active remaining gaps 删除。
- `filling_surface_native_helper_blocker`、`filling_support_order_g1_native_helper_blocker`、`filling_support_order_g2_native_helper_blocker`、`filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker`、`filling_params_all_native_helper_blocker`、`filling_non_boundary_support_order_native_helper_blocker` 仍保留在 `narrowed_gaps`，状态为 `released_c6m5_product_contract_non_parity`，并保留 crash / timeout / notCollected native helper evidence。
- `covered`、`fixtures`、`request_local_boundaries`、`field_boundaries`、`narrowed_gaps`、`remaining_gaps` 已与 7 个 `cad-core/fixtures/c6m5` product / diagnostics fixture 对齐。
- 矩阵已同步所有 `SCOPE-*`、`BLK-*`、`GAP-*`、`ORC-*` 的 S5 发布状态，并写入 `VAL-301`、`VAL-302`、`VAL-303` 验收证据。

## 验收结果

- `cmake --build build` 通过。
- `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest` 通过：`Ran 211 tests in 22.826s`，`OK`。
- `python3 -m unittest tests.test_adapters` 通过：`Ran 29 tests in 1.012s`，`OK`。
- 单独 capability adapter smoke 曾通过：`CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`，`OK`。
- S5 不声明 FreeCAD parity，不实现 native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native feature、cross-request wrapper 或 full Part surface family。

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part_workbench\\.filling|remaining_gaps|filling_surface_native_helper_blocker|filling_params_all_native_helper_blocker' cad-core/src/runtime/capability_contract.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
git diff --check -- cad-core docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/矩阵/*.tsv
```

当前队列下一项应为 `6-24-16-26-C6-M5-S6-阶段回归与release-gate.md`；不要把 S6 标为已实现。

## 非目标

- 不扩大测试到全量 FreeCAD CI。
- 不在 S5 临时补核心语义。
- 不把尚未覆盖的 row 写成发布完成。
