# C12-M12 S3 oracle fixture 与红灯闭环

## 目标

把用户失败样例和代表性 Sweep / Pipe 行为变成 native/current 可比较证据。只有出现稳定 FreeCAD expected 与 current mismatch，才授权 S4/S5 实现。

## 关闭结论

- 当前对话没有提供具体用户失败 input/output，仓库内也未找到 `C12M12-ORACLE-001` 对应的已记录样例；该行已标为 `blocked_missing_user_input` / `waiting_user_repro`。解除条件是用户提供 failing request JSON、current result/preview payload，或明确的 fixture 路径。
- S3 复用 existing native expected 形成代表性红灯：`cad-core/fixtures/c51m4/expected/partdesign-pipe-fixed-round-body.freecad.json` 记录 FreeCADCmd `1.2.0 revision 20260519`，Body volume `0.7199999999999999`、edges `20`、vertices `12`。
- 当前 `cad-core` 对同一 fixture 稳定输出 Body volume `0.3360000000000001`、edges `28`、vertices `15`，diagnostics 为空，history 包含 `part_sweep:rebuilt_invalid_planar_faces`。这是 PartDesign Pipe fixed/round selected-spine cap/sewing 的 source-backed current mismatch。
- Standard/Frenet、Auxiliary/Binormal、Part Sweep basic wrapper、Part Sweep advanced helper DTO / product-contract controls 均通过 focused tests；没有 Part Sweep wrapper / response mismatch。

## Gate 裁决

- S4 已授权：只允许围绕 ORACLE-003 的 PartDesign Pipe fixed/round selected-spine cap/sewing mismatch 进入实现，优先复核 `FeaturePipe.cpp::Pipe::execute()`、`Pipe::setupAlgorithm()`、`TopoShape::makeElementPipeShell()`、Simulate/Sewing 与 invalid planar rebuild 路径。
- S5 未授权：ORACLE-005/006 通过，不能把 Part Sweep controls 或 mesh response 质量门当成 Part Sweep implementation gap。
- ORACLE-007 仍只是 mesh response quality gate；不得用截图、mesh normals 或前端 preview 失败替代 BRep/history parity。

## Exact Commands

```bash
pwd
git rev-parse --short HEAD
git log -1 --oneline
git -c core.quotepath=false status --short -uall
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c4m2_partdesign_pipe_additive_body_matches_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c5m3_partdesign_pipe_transition_and_frenet_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_auxiliary_binormal_modes_match_native_oracle
```

Result: `Ran 3 tests in 0.512s OK`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_fixed_round_selected_spine_matches_native_oracle
```

Result: expected red failure, `AssertionError: 0.3360000000000001 != 0.7199999999999999`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_right_corner_surface_uses_pipeshell_history \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_solid_builds_solid_not_surface_only \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_frenet_false_routes_set_mode_false \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_auxiliary_spine_contract_is_expected_backed \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_binormal_contract_is_expected_backed \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c5m12_part_sweep_spine_support_surface_normal_is_expected_backed \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c6m4_part_sweep_advanced_combined_product_contract_builds_shape
```

Result: `Ran 8 tests in 0.886s OK`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
CAD_CORE_TEST_LEGACY_OUTPUT=1 ./build/cad-core recompute fixtures/c51m4/partdesign-pipe-fixed-round-body.json --output out/c12m12/partdesign-pipe-fixed-round-body.legacy-current.json
```

Observed current evidence: Body volume `0.3360000000000001`, edges `28`, vertices `15`, diagnostics `[]`, history contains `part_sweep:rebuilt_invalid_planar_faces`.

## 已更新文件

- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `../矩阵/c12m12_sweep_drift_audit.tsv`
- `../矩阵/c12m12_sweep_blocker_queue.tsv`
- `../矩阵/c12m12_sweep_validation_matrix.tsv`
- `../README.md`
- `../7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次总入口.md`
- `../../README.md`
