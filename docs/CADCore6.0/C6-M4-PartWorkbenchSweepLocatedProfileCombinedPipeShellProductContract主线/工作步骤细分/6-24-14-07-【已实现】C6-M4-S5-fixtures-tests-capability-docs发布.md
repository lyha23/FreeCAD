# 【已实现】C6-M4-S5 fixtures tests capability docs 发布

## 目标

把 S3/S4 的实现结果固化为 fixtures、expected、focused tests、capability contract 和 docs。S5 是 capability 发布闸门；只有 S3/S4 的 product path 和 diagnostics 均有证据时，才能删除两个 remaining gaps。

## 完成结果

| 类别 | 已发布产物 |
| --- | --- |
| fixtures | `cad-core/fixtures/c6m4/part-sweep-located-profile-product.json`、`part-sweep-located-profile-diagnostics.json`、`part-sweep-located-profile-bool-diagnostics.json`、`part-sweep-advanced-combined-product.json`。 |
| expected | `cad-core/fixtures/c6m4/expected/*.freecad.json` 均保持 `freecad_native_parity=false` 或 `not FreeCAD native parity` 口径；不改 c5m10 known_gap expected。 |
| tests | `cad-core/tests/test_p8_features.py` 保留 c5m10 historical guard 和 c6m4 product/diagnostics focused assertions；`cad-core/tests/test_adapters.py` 同步 capability publication assertion。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` 将 `part_workbench.sweep.status` 发布为 `supported_multi_profile_linearize_c6m4_product_contract_non_parity`；covered/fixtures/field_boundaries/request_local_boundaries/narrowed_gaps 均列入 c6m4 product contract evidence。 |
| docs | README、主线总入口、方案和矩阵状态已同步到 S5 published。 |

## capability 口径

- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 不再是 `remaining_gaps`；它在 `narrowed_gaps` 中保留为 `released_c6m4_product_contract_non_parity`，fixtures 同时列出 c5m10 historical guard 与 c6m4 product/diagnostics。
- `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 不再是 `remaining_gaps`；它在 `narrowed_gaps` 中保留为 `released_c6m4_product_contract_non_parity`，并依赖 S3 located profile product path。
- capability 只能声明 C6-M4 product paths supported：`SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart` located product、located diagnostics、bool diagnostics、advanced auxiliary+tolerance+transition+located section product。
- 必须保留 `freecadcmd_location_overload_status=notCollected` / `contract_provenance=cad_core_product_contract_non_parity`，不声明 FreeCAD parity。

## 非目标保留

- c5m10 known_gap fixtures/tests 保留为 historical guard 或 wrapper evidence，不改 expected。
- 不扩大 `full_part_surface_family`。
- 不把 upstream native `Part::Sweep` advanced direct properties、GUI、persistent wrapper lifecycle、Filling、Loft、Groove 写成 supported。
- 不跑或修改 S6 阶段回归结论。

## 验收

本轮验证结论：`cmake --build build` 通过；`python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts` 通过，`Ran 205 tests`，`OK`；`git diff --check -- cad-core docs/CADCore6.0`、TSV 字段一致性检查和队列检查均通过，队列下一项为 S6。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
[ -d cad-core/build ] || (cd cad-core && cmake -S . -B build)
cd cad-core && cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0
rg -n 'c6m4|part_sweep_located_profile_freecadcmd_wrapper_build_blocker|part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker|cad_core_product_contract' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```
