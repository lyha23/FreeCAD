# 【已实现】C6-M4-S0 live 基线与 blocker 边界复核

## 目标

复核当前 live 代码、capability contract、c5m10 expected known_gap 与 focused tests，冻结 C6-M4 的起点。S0 不做代码实现，不改 expected，不删除 capability gap。

## S0 live 结论

- live repo：`/home/user/Chili3DProject/FreeCAD`
- S0 live HEAD：`fab981dc85`
- S0 live last commit：`fab981dc85 docs: 新建 C6-M4 Sweep LocatedProfile 主线方案包`
- 起始工作区：`git -c core.quotepath=false status --short -uall` 无输出。
- capability：`part_workbench.sweep.status` 仍为 `supported_multi_profile_linearize_c5m13_wrapper_expected_backed_with_location_overload_blockers`。
- remaining gaps：`part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 与 `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker` 仍在 `cad-core/src/runtime/capability_contract.cpp` 和 c5m10 expected 中。
- focused 结果：`python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority` 通过，`Ran 2 tests`，`OK`。
- 关闭范围：只关闭 `C6M4-SCOPE-000`、`C6M4-CAT-000`、`C6M4-BLK-000`。`C6M4-CAT-101` 保持 `notCollected`；`C6M4-CAT-102` / `C6M4-CAT-201` 保持 `backendGap`；`C6M4-BLK-101/102/201` 继续指向 S2/S3/S4。

## 必读文件

- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c5m10/expected/part-sweep-located-profile-contract.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-advanced-combined-contract.freecad.json`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-C5-M13-S2-sweepLocationCombinedWrapper收窄记录.md`

## 允许声明

| 项 | 允许口径 |
| --- | --- |
| located profile | 当前是 `add(Profile, Location, WithContact, WithCorrection)` Location overload build-stage blocker。 |
| combined advanced | 当前依赖 Location overload；no-location auxiliary + tolerance control 可 build。 |
| cad-core status | 当前 executor 发布 request metadata 和 known_gap，不发布 shape / named_shapes。 |
| product contract | C6-M4 可以规划 CAD Core product contract，但不能在 S0 声明已支持。 |

## 禁止声明

- 已支持 located profile 或 combined advanced。
- FreeCAD parity 已成立。
- auxiliary / tolerance 单独路径仍是本 blocker。
- `Part::Sweep::execute()` 已有 native advanced direct properties。
- 可以从 cad-core 当前输出直接生成 FreeCAD expected。

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `notCollected` | FreeCADCmd wrapper 不能返回稳定 `shape_summary`，只能作为 oracle blocker 证据。 |
| `backendGap` | CAD Core 要发布 product contract，但当前实现、fixture 或 capability 还未闭环。 |
| `releaseGate` | 功能已落地但还需 focused/stage/heavy 验收和 capability/docs 收口。 |
| `nonGoal` | 本主线不处理，只记录 reopen condition。 |
| `closed` | 对应步骤的验收证据完整，且矩阵状态已同步。 |

## 验收标准

通过条件：

- `scope_review_matrix` 中 `C6M4-SCOPE-000` 记录 live HEAD、last commit、capability remaining gap 和 focused test 结果。
- `backend_gap_classification` 中两个 existing blockers 分别保留为 `notCollected` / `backendGap`，没有被提升为 supported。
- `blocker_queue` 中 `C6M4-BLK-000` 关闭，`C6M4-BLK-101/201` 仍指向后续步骤。
- S0 记录的 focused 命令通过或记录明确环境原因；普通 S0 不跑阶段回归。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git rev-parse --short HEAD
git log -1 --oneline
rg -n 'part_sweep_located_profile_freecadcmd_wrapper_build_blocker|part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py cad-core/fixtures/c5m10/expected
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority
git diff --check -- docs/CADCore6.0
```
