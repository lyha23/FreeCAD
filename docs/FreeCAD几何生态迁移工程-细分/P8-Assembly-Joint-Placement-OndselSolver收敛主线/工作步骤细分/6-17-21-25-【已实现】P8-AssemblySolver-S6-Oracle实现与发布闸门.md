# 【已实现】P8 AssemblySolver S6 Oracle 实现与发布闸门

## 收口结论

S6 已把 S2-S5 留下的 `releaseGate` 发布成 real-only supported 子集，并把 FreeCAD native solver placement oracle 从 `notCollected` 推进为 checked-in expected，后续由 S7 完成 parity 收口：

- `CAD_CORE_HAS_ONDSEL_SOLVER=1`：C ABI `assembly.ondsel_solver_adapter` 发布 `available=true`、`status=covered_full`、`mode=request_local_runPreDrag`，focused C3M6 tests 验证 real Ondsel adapter、grounded validation 和 placement writeback。
- CMake 缺 `src/3rdParty/OndselSolver/CMakeLists.txt` 时直接配置失败；不再支持 `CAD_CORE_ENABLE_ONDSEL_SOLVER=OFF` 或 unlinked fallback。
- `representative_solver_adapter` 已从 C ABI capabilities 删除；`placement_writeback` 只列出 `real_ondsel_solver`，`unsupported_joint_matrix` 公开 Cylindrical、Parallel、Perpendicular、RackPinion、Screw、Gears、Belt。
- `documentObjectUpdates.action=assembly_set_placement` 只发布 stateless `Placement` 更新建议；下一请求仍必须由前端 graph 消费更新，CAD Core 不保存跨请求 solver session 或后端 placement 状态。
- 有 Joint 但没有 GroundedJoint 时按 FreeCAD `AssemblyObject::solve()` 的 grounded gate 返回 `missing_grounded_part` / `no_grounded_part`，不再用代表路径写回 placement。
- `P8ASM-SCOPE-006` 已采集：`cad-core/fixtures/c3m6/expected` 保存 9 个 FreeCADCmd native solver placement expected，`--phase c3m6 --check --skip-unsupported` 得到 `processed=9 skipped=4 failed=0`。后续 S7 已移除 `known_gap`，现在 9 个 expected 都通过 cad-core parity。

2026-06-18 后续状态：Cylindrical 已在 `P8ASM-SCOPE-010` 中以 checked-in FreeCADCmd native expected 和 real `ASMTCylindricalJoint` adapter 转为 supported；当前 unsupported matrix 只保留 Parallel、Perpendicular、RackPinion、Screw、Gears、Belt，c3m6 native expected 为 10 个。上方列表保留为 S6 当时发布闸门记录。

## Blocker 结果

| blocker | S6 结果 | 证据 |
| --- | --- | --- |
| `P8ASM-BLOCK-001` | closed / supported | P8 正式文档和总览已改为 real-only solver 子集，不再把已覆盖 runtime contract 写成全部 `solve=not_migrated` |
| `P8ASM-BLOCK-002` | closed / supported | C++ 相邻注释和 docs 保留 FreeCAD `AssemblyObject::solve()` / `setNewPlacements()` / JointType source authority |
| `P8ASM-BLOCK-003` | closed by S7 | real Ondsel focused tests 通过；9 个 native FreeCAD placement expected 已入库并通过 parity |
| `P8ASM-BLOCK-004` | closed / supported | writeback next-request、multi-component order、invalid grounded rejection 均由 focused tests 覆盖 |
| `P8ASM-BLOCK-005` | closed / unsupported retained | diagnostic-only JointType 矩阵已在 C ABI / docs / tests 公开，未新增无 oracle JointType 实现 |
| `P8ASM-BLOCK-006` | closed / supported | 删除 `CAD_CORE_ENABLE_ONDSEL_SOLVER` 开关，缺 bundled OndselSolver 时 CMake 配置失败，capability 只发布 real mode |

## 当前状态矩阵

| scope | 状态 | 说明 |
| --- | --- | --- |
| `P8ASM-SCOPE-001` | supported | 文档、C ABI capabilities、focused tests 的发布口径一致 |
| `P8ASM-SCOPE-002` | supported | JointGroup / Joint DTO 作为 request-local solver input 发布，不声明完整 Joint lifecycle |
| `P8ASM-SCOPE-003` | supported | real Ondsel grounded solver path 验证；无 GroundedJoint 的求解返回错误 |
| `P8ASM-SCOPE-004` | removed | representative fallback 已删除，不再作为 adapter / DTO transport 或 writeback contract |
| `P8ASM-SCOPE-005` | supported | placement writeback lifecycle 为 stateless `Placement` update suggestion |
| `P8ASM-SCOPE-006` | supported | native FreeCAD solver placement oracle 已入库并通过 parity；无 known_gap |
| `P8ASM-SCOPE-007` | unsupported | 复杂 JointType / Distance geometry 无 oracle 前保持 diagnostic-only |
| `P8ASM-SCOPE-008` | supported | capability publication 只发布硬依赖 real Ondsel mode |
| `P8ASM-SCOPE-009` | nonGoal | GUI、跨请求 session 和完整 Link 写回事务仍不属于本主线 |

## 验收证据

已验证：

```bash
cmake -S . -B build
cmake --build build --target cad-core cad_core_ffi
rg -n "CAD_CORE_HAS_ONDSEL_SOLVER" build/CMakeFiles/cad-core-lib.dir/flags.make
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_p8_assembly_joint_group_reports_solver_inputs_and_placement_writeback tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_ungrounded_joint_errors_without_fallback
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
- 不把 cad-core focused runtime 输出倒推成 FreeCAD native solver golden。
