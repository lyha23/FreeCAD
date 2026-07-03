# C12-M12 S4 PartDesign Pipe 主路径迁移

## 目标

在 S1-S3 均成立后，对 `PartDesign::AdditivePipe` / `SubtractivePipe` 做最小 FreeCAD parity 迁移。本步实际关闭为 no-source-delta：C++ 当前源码已经包含 S4 所需 FreeCAD `Pipe::execute()` Simulate/Sewing/cap/solidification 路径，S3 红灯来自本地 `cad-core/build` CMake cache 错绑到另一个仓库源码树。

## 关闭结论

- S4 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=969ab49f64`（`969ab49f64 文档：关闭 C12-M12 S3 oracle 红灯闭环`），起点 `git status --short -uall` 干净。
- 初始 focused case 先复现 S3 红灯：`CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_fixed_round_selected_spine_matches_native_oracle` 输出 Body volume `0.3360000000000001` vs expected `0.7199999999999999`。
- 随后发现 `cad-core/build/CMakeCache.txt` 指向 `/Users/li/Chili3DProject/cad-web-background/cad-core/build`；`cmake --build build` 实际编译了错误源码树，不是当前 `/Users/li/Chili3DProject/FreeCAD/cad-core`。
- 执行 `cmake --fresh -S . -B build` 后重新绑定当前源码树，再 `cmake --build build`。重跑同一 fixture 后 Body volume `0.7199999999999999`、edges `20`、vertices `12`，diagnostics `[]`，与 checked-in FreeCAD expected 一致。
- 本步未修改 `cad-core/src`、tests、fixtures 或 expected；没有 source-backed C++ 缺口需要补。S4 关闭为 `current_source_supported_after_build_refresh`。
- `C12M12-ORACLE-001` 仍为 `blocked_missing_user_input`，不能编造用户失败 fixture。
- S5 仍未授权；本步没有修改 shared PipeShell builder 或 Part Sweep wrapper，只需后续 S5/S6按队列做回归收口。

## FreeCAD / cad-core 依据

- FreeCAD `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()`：非闭合 shell 调用 `mkPS.Simulate(2, sim)`，生成 front/back face，随后 `BRepBuilderAPI_Sewing` 和 `makeElementSolid()`。
- FreeCAD `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()`：`Fixed` 调用 `SetMode(gp_Ax2(...))`，`Frenet` 调用 `SetMode(true)`，`Auxiliary`/`Binormal` 使用对应 overload。
- cad-core `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/topo_shape_expansion.cpp::makeElementPipeShellFromSources()` 已包含 `Simulate(2)`、cap face、`BRepBuilderAPI_Sewing` 与 solidification 路径。
- cad-core `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part_design/feature_pipe.cpp::executePipeFeature()` 已设置 `pipeOptions.sewCaps = true`，并把 PartDesign Pipe 结果写入 Body add/sub flow。

## Exact Commands

```bash
pwd
git rev-parse --short HEAD
git log -1 --oneline
git -c core.quotepath=false status --short -uall
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeaturesTest.test_c51m4_partdesign_pipe_fixed_round_selected_spine_matches_native_oracle
```

Result: current test class is `CadCoreP7FeatureTest`; the user-provided class name produced `AttributeError`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_fixed_round_selected_spine_matches_native_oracle
```

Initial result before build refresh: expected red failure, `AssertionError: 0.3360000000000001 != 0.7199999999999999`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --fresh -S . -B build
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_fixed_round_selected_spine_matches_native_oracle
```

Result after build refresh: `Ran 1 test in 2.081s OK`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
CAD_CORE_TEST_LEGACY_OUTPUT=1 build/cad-core recompute fixtures/c51m4/partdesign-pipe-fixed-round-body.json --output out/c12m12-after-build-refresh-partdesign-pipe-fixed-round-body.legacy.result.json
```

Observed after build refresh: Body volume `0.7199999999999999`, faces `10`, edges `20`, vertices `12`, diagnostics `[]`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c4m2_partdesign_pipe_additive_body_matches_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_auxiliary_binormal_modes_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c51m4_partdesign_pipe_selected_spine_multisection_matches_native_oracle
```

Two c51m4 tests passed; the c4m2 method name in the prompt is `...matches_expected`, but current test file names it `...matches_native_oracle`.

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c4m2_partdesign_pipe_additive_body_matches_native_oracle
```

Result: `Ran 1 test in 0.158s OK`.

## 已更新文件

- `../矩阵/c12m12_sweep_drift_audit.tsv`
- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `../矩阵/c12m12_sweep_blocker_queue.tsv`
- `../矩阵/c12m12_sweep_validation_matrix.tsv`
- `../README.md`
- `../7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次总入口.md`
- `../../README.md`
- `7-3-20-22-【已实现】C12-M12-S4-PartDesignPipe主路径迁移.md`

## 非目标

- 不改 Part Workbench Sweep wrapper；本步没有 shared builder source delta。
- 不新增前端字段。
- 不解决完整 Topological Naming。
- 不编造 `C12M12-ORACLE-001` 用户失败 fixture。
