# P8 GearsBeltJoint S4 NativeOracle 与半径符号专项复审

## 目标

关闭 `GBJ-BLOCK-003` 和 `GBJ-BLOCK-004`：为 Gears / Belt 增加 native FreeCADCmd expected，并证明 cad-core request-local solver output 与 expected 一致。

当前状态：已实现。S4 只关闭 Gears / Belt native expected、focused runtime assertion 和 Belt `radius_j` 负号证据；capability publication、unsupported matrix 发布和 S6 仍未关闭。

## FreeCAD 依据

- `AssemblyObject::solve()`：`mbdAssembly->runPreDrag()` 后 `setNewPlacements()`。
- `makeMbdJointOfType()`：Gears / Belt 直接创建 `ASMTGearJoint`。
- `makeMbdJoint()`：Gears / Belt 使用通用 Reference1 / Reference2 marker path。

## scope 表

| scope | 复审内容 |
| --- | --- |
| `GBJ-SCOPE-004` | 新增 `assembly-grounded-gears-joint-real-solver` 和 `assembly-grounded-belt-joint-real-solver` native expected |
| `GBJ-SCOPE-002` | Gears fixture 的 `solver_joints` / 半径字段 / placement updates 与 FreeCAD expected 对齐 |
| `GBJ-SCOPE-003` | Belt fixture 的 `radiusJ` 负号和 expected parity 对齐 |

## 必须回写的矩阵行

- `GBJ-CAND-010`
- `GBJ-CAND-011`
- `GBJ-BLOCK-003`
- `GBJ-BLOCK-004`

## 关闭结论

- `GBJ-BLOCK-003` 已由 S4 关闭：`assembly-grounded-gears-joint-real-solver.json` 与 FreeCADCmd expected 已入库，`solver_joints[0]` 输出 `distance=2.0`、`distance2=1.0`、`radius_i=2.0`、`radius_j=1.0`，`unsupported_joints=[]`。
- `GBJ-BLOCK-004` 已由 S4 关闭：`assembly-grounded-belt-joint-real-solver.json` 与 FreeCADCmd expected 已入库，`solver_joints[0]` 输出 `distance=2.0`、`distance2=1.0`、`radius_i=2.0`、`radius_j=-1.0`，`unsupported_joints=[]`。
- `GBJ-BLOCK-005` 和 `GBJ-BLOCK-006` 继续等待 S5/S6；本文件不声明 C ABI capability supported，也不关闭 RackPinion / Screw / complex Distance 边界。

## 验收记录

- `cmake --build build --target cad-core cad_core_ffi` 通过。
- `FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check` 通过。
- `FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check` 通过。
- `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver` 通过。
- 新增 Gears / Belt 两个 c3m6 expected fixture 的 `assert_result_matches_expected()` 通过。
- 阶段回归命令 `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 在当前环境仍报告非本包 `p7/hole-supported-model-thread-counterbore` volume delta：`434.05358560604475 != 434.05359569539525`，差值 `1.0089350496400584e-05`，略高于该 fixture 的 `1e-05` 容差；未修改该 expected，未把该 OCCT drift 当作 S4 evidence。

## 验收标准

- `cad-core/fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json` 和 expected 入库。
- `cad-core/fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json` 和 expected 入库。
- expected 文件包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver` 和对应 `joint_type`。
- focused test 断言 Gears / Belt 进入 `solver_joints`，`unsupported_joints=[]`，且 Belt `radiusJ` 为负值。
- 检查命令：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

## 非目标

- 不刷新无关 c3m6 expected。
- 不把 current machine OCCT drift 当 expected 修正依据。
- 不声明 GUI reverse / rotate lifecycle。
