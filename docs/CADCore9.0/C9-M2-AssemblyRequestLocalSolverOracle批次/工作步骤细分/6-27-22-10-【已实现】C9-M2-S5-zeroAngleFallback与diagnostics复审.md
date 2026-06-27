# 【已实现】C9-M2 S5 zeroAngleFallback 与 diagnostics 复审

## 目标

采集或确认 zero Angle fallback native oracle，并验证 C9-M2 扩面不会把 unsupported diagnostics 静默降级为 success。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()`
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getJointAngle()`
- `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`

## 必须复核

- FreeCAD `Angle=0` / `2pi` fallback 到 `ASMTParallelAxesJoint` 的 native expected。
- current cad-core exact zero Angle class fallback 是否匹配 native expected。
- `assembly-angle-zero-and-signed-current-real-solver` 是否需要 expected 采集或 fixture schema 更新。
- unsupported JointType、PointCurve boundary、缺 marker placement、缺 solver joint class 是否仍走 diagnostic。

## 必须回写的矩阵行

- `C9M2-SCOPE-301`
- `C9M2-SCOPE-302`
- `C9M2-BG-301`
- `C9M2-BG-302`
- `C9M2-BLOCKER-501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Angle|ASMTParallelAxesJoint|unsupported_assembly_solver|ondsel_solver_failed|assembly-angle-zero-and-signed-current-real-solver|PointCurve|missing_grounded_part|invalid_assembly_solver_result' src/Mod/Assembly/App cad-core/src/assembly cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6 docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次
test -f cad-core/fixtures/c3m6/expected/assembly-angle-zero-and-signed-current-real-solver.freecad.json
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_adapters.CadCoreAdapterTest
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
```

S5 关闭时，zero Angle fallback 必须明确进入 expected-backed covered、backend_gap_candidate 或 known_gap_retained；unsupported diagnostics 必须保持可见。

## S5 关闭结论

- S5 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=1e67ff8971`（`1e67ff8971 test(cad-core): 激活C9-M2 S4 custom placement测试`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- native expected 采集结果：直接运行 `cd cad-core && python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-angle-zero-and-signed-current-real-solver.json` 时，Python 环境无 `FreeCAD` 模块且默认 `/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd` 不存在；改用 `--freecadcmd /home/user/.local/bin/freecadcmd` 后采集成功，生成 `cad-core/fixtures/c3m6/expected/assembly-angle-zero-and-signed-current-real-solver.freecad.json`。
- expected 记录 FreeCADCmd 1.2.0 revision 20260519、native solver return 0、`AngleZeroJoint.angle=0.0`、native current XY angle 0 和 `ComponentB` native placement update `[4,0,4]`。结合 `AssemblyObject::makeMbdJointOfType()` 中 exact-zero Angle 返回 `ASMTParallelAxesJoint` 的 source authority，该 fixture 已从 known_gap_retained 进入 expected-backed route。
- current cad-core 对同 fixture 保留 `Angle=0` solver DTO 和 resolved subshape marker evidence，但 solver 返回 `ondsel_solver_failed` / `iterNo > iterMax`，`placement_updates=[]`；因此 `C9M2-SCOPE-301`、`C9M2-BG-301` 标为 `backend_gap_candidate`，只交 S6，不在 S5 修改 C++ solver。
- `cad-core/tests/test_p8_features.py::CadCoreP8FeatureTest.test_c3m6_assembly_zero_angle_s5_native_oracle_routes_current_gap` 断言 native expected 与 current mismatch；`test_c9m2_s5_assembly_solver_diagnostics_remain_visible` 断言 unsupported JointType、PointCurve/default boundary、missing grounded 均仍走 diagnostics，zero-angle fixture 同时证明 `ondsel_solver_failed` 仍可见。`cad-core/tests/test_adapters.py` 保持 `invalid_assembly_solver_result` capability publication guard。
- `C9M2-SCOPE-302`、`C9M2-BG-302`、`C9M2-BLOCKER-501` 和 `C9M2-VAL-501` 已回写；S6 状态保持待执行。

## 非目标

- 不把 zero Angle current output 当作 native expected。
- 不把 unsupported diagnostic 静默转 success。
- 不修改 GUI/session 行为。
