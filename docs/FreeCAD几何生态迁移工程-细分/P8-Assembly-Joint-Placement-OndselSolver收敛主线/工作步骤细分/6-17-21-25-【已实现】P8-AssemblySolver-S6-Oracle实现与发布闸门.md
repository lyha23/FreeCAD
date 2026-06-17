# 【已实现】P8 AssemblySolver S6 Oracle 实现与发布闸门

## 收口结论

S6 已把 S2-S5 留下的 `releaseGate` 发布成 build-mode aware supported 子集，并保留缺 FreeCAD native oracle 的路径为 `notCollected`：

- `CAD_CORE_HAS_ONDSEL_SOLVER=1`：C ABI `assembly.ondsel_solver_adapter` 发布 `available=true`、`status=covered_full`、`mode=request_local_runPreDrag`，focused C3M6 tests 验证 real Ondsel adapter、grounded validation 和 placement writeback。
- `CAD_CORE_HAS_ONDSEL_SOLVER=0`：C ABI `assembly.ondsel_solver_adapter` 发布 `available=false`、`status=not_linked`、`mode=representative_fallback_only`，focused tests 断言 representative fallback，不再用 real-solver 断言硬失败。
- `representative_solver_adapter`、`placement_writeback` 和 `unsupported_joint_matrix` 保留发布，但都带 build mode / solver mode 边界；`unsupported_joint_matrix` 公开 Cylindrical、Parallel、Perpendicular、RackPinion、Screw、Gears、Belt。
- `documentObjectUpdates.action=assembly_set_placement` 只发布 stateless `Placement` 更新建议；下一请求仍必须由前端 graph 消费更新，CAD Core 不保存跨请求 solver session 或后端 placement 状态。
- `P8ASM-SCOPE-006` 继续 `notCollected`：本机 FreeCADCmd 可运行，但 `--phase c3m6 --check --skip-unsupported` 因缺 checked-in expected 得到 `processed=0 skipped=12 failed=0`；单 fixture probe 仍只得到 FreeCAD `solve=not_migrated` 风格输出，不能作为 native solver placement golden。

## Blocker 结果

| blocker | S6 结果 | 证据 |
| --- | --- | --- |
| `P8ASM-BLOCK-001` | closed / supported | P8 正式文档和总览已改为 build-mode aware solver 子集，不再把已覆盖 runtime contract 写成全部 `solve=not_migrated` |
| `P8ASM-BLOCK-002` | closed / supported | C++ 相邻注释和 docs 保留 FreeCAD `AssemblyObject::solve()` / `setNewPlacements()` / JointType source authority |
| `P8ASM-BLOCK-003` | partial close：runtime supported，native oracle notCollected | linked / unlinked focused tests 均通过；native FreeCAD placement expected 未入库 |
| `P8ASM-BLOCK-004` | closed / supported | writeback next-request、multi-component order、invalid grounded rejection 均由 focused tests 覆盖 |
| `P8ASM-BLOCK-005` | closed / unsupported retained | diagnostic-only JointType 矩阵已在 C ABI / docs / tests 公开，未新增无 oracle JointType 实现 |
| `P8ASM-BLOCK-006` | closed / supported | 新增 `CAD_CORE_ENABLE_ONDSEL_SOLVER` 开关，linked 与 unlinked build 均可验证 capability 口径 |

## 当前状态矩阵

| scope | 状态 | 说明 |
| --- | --- | --- |
| `P8ASM-SCOPE-001` | supported | 文档、C ABI capabilities、focused tests 的发布口径一致 |
| `P8ASM-SCOPE-002` | supported | JointGroup / Joint DTO 作为 request-local solver input 发布，不声明完整 Joint lifecycle |
| `P8ASM-SCOPE-003` | supported | real Ondsel grounded solver path 在 linked build 验证；unlinked build 不声明 real adapter |
| `P8ASM-SCOPE-004` | supported | representative fallback 只作为 adapter / DTO transport 和 stateless writeback contract |
| `P8ASM-SCOPE-005` | supported | placement writeback lifecycle 为 stateless `Placement` update suggestion |
| `P8ASM-SCOPE-006` | notCollected | native FreeCAD solver placement oracle 未入库 |
| `P8ASM-SCOPE-007` | unsupported | 复杂 JointType / Distance geometry 无 oracle 前保持 diagnostic-only |
| `P8ASM-SCOPE-008` | supported | linked / unlinked capability publication 已可分别验证 |
| `P8ASM-SCOPE-009` | nonGoal | GUI、跨请求 session 和完整 Link 写回事务仍不属于本主线 |

## 验收证据

已验证：

```bash
cmake -S . -B build
cmake --build build --target cad-core cad_core_ffi
cmake -S . -B build-no-ondsel -DCAD_CORE_ENABLE_ONDSEL_SOLVER=OFF
cmake --build build-no-ondsel --target cad-core cad_core_ffi
rg -n "CAD_CORE_HAS_ONDSEL_SOLVER" build/CMakeFiles/cad-core-lib.dir/flags.make build-no-ondsel/CMakeFiles/cad-core-lib.dir/flags.make
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_p8_assembly_joint_group_reports_solver_inputs_and_placement_writeback tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
CAD_CORE_TEST_BUILD_DIR=build-no-ondsel python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_p8_assembly_joint_group_reports_solver_inputs_and_placement_writeback tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py --phase c3m6 --check --skip-unsupported
```

最终 gate：

```bash
git diff --check
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
cd cad-core && python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

## 非目标

- 不实现 GUI / Workbench / drag / postDrag 生命周期。
- 不引入跨请求 solver session。
- 不把完整 Link ledger、copy-on-change 或跨文档 hash 生命周期混入本主线。
- 不把 representative fallback 或 cad-core focused runtime 输出倒推成 FreeCAD native solver golden。
