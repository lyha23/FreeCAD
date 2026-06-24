# C6-M4-S5 fixtures tests capability docs 发布

## 目标

把 S3/S4 的实现结果固化为 fixtures、expected、focused tests、capability contract 和 docs。S5 是 capability 发布闸门；只有 S3/S4 的 product path 和 diagnostics 均有证据时，才能删除两个 remaining gaps。

## 发布清单

| 类别 | 必须产物 |
| --- | --- |
| fixtures | `cad-core/fixtures/c6m4/part-sweep-located-profile-product.json`、`part-sweep-advanced-combined-product.json`、diagnostics fixture。 |
| expected | `cad-core/fixtures/c6m4/expected/*.freecad.json`，若为 product contract 必须写明非 FreeCAD parity。 |
| tests | `cad-core/tests/test_p8_features.py` focused assertions；adapter capability assertions。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` 更新 `part_workbench.sweep.covered/fixtures/field_boundaries/narrowed_gaps/remaining_gaps`。 |
| docs | C6-M4 README、总入口、矩阵状态、必要的 CADCore 方案总览更新。 |

## 删除 blocker 条件

- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker`：S3 product path 或 stable FreeCADCmd expected 已有 fixture/test/capability 证据。
- `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`：S4 combined product fixture/test/capability 证据成立，且明确依赖 S3 closed state。
- 删除任一 blocker 时，必须保留旧 c5m10 known_gap fixture 作为 historical guard 或迁移说明，避免误改 expected provenance。

## 验收标准

通过条件：

- `C6M4-ORC-101/102/201/301` 均落到 fixture/test/expected 或明确保留为 gap。
- `C6M4-BLK-301` 关闭；capability contract 与 adapter assertions 同步。
- `README.md`、主线总入口和矩阵状态一致。
- `remaining_gaps` 删除只覆盖已实现的 exact blocker，不扩大到 full Part surface family。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0
rg -n 'c6m4|part_sweep_located_profile_freecadcmd_wrapper_build_blocker|part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker|cad_core_product_contract' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
```

Focused：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```
