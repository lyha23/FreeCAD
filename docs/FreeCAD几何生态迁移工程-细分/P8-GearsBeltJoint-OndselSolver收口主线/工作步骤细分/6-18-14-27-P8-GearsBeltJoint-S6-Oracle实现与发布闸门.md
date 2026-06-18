# P8 GearsBeltJoint S6 Oracle 实现与发布闸门

## 目标

消费 S2-S5 留下的 `unsupportedImplementable`、`notCollected` 和 `releaseGate`，把 Gears / Belt 作为最小 request-local JointType 支持发布，或者明确阻断原因。

## 输入

- `GBJ-BLOCK-001` 到 `GBJ-BLOCK-006`
- FreeCAD source authority：`AssemblyObject::makeMbdJointOfType()`、`AssemblyObject::makeMbdJoint()`、`JointObject.py::JointUsingDistance2`
- c3m6 fixture / expected
- hard-linked real Ondsel build

## 实施顺序

1. 复核 S3 已关闭的 `GBJ-BLOCK-001`：`JointConstraint.distance2` 与 Gears / Belt 的 `Distance` / `Distance2` request builder 不回退。
2. 复核 S3 已关闭的 `GBJ-BLOCK-002`：`ASMTGearJoint` include、Gears / Belt factory mapping 和 supported predicate 不回退。
3. 复核 S4 已关闭的 `GBJ-BLOCK-003`：Gears c3m6 fixture、native expected、focused runtime assertion 不回退。
4. 复核 S4 已关闭的 `GBJ-BLOCK-004`：Belt c3m6 fixture、native expected、focused runtime assertion 与负 `radius_j` 不回退。
5. 关闭 `GBJ-BLOCK-005`：更新 `c_api.cpp` capabilities 和 `test_adapters.py`。
6. 关闭 `GBJ-BLOCK-006`：更新既有 P8 docs / TSV，只移除 Gears / Belt 的 unsupported 状态，保留 RackPinion / Screw。

## 下一轮代码落点

| blocker | C++ / 测试 / 文档落点 | 成功标准 |
| --- | --- | --- |
| `GBJ-BLOCK-001` | `cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp` | 已由 S3 关闭；S6 只复核 `JointConstraint.distance2` 与 request builder 不回退 |
| `GBJ-BLOCK-002` | `cad-core/src/assembly/joint_solver.cpp` | 已由 S3 关闭；S6 只复核 `Gears` / `Belt` 的 `ASMTGearJoint` 映射与 supported predicate 不回退 |
| `GBJ-BLOCK-003` | `cad-core/fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json`、expected、`cad-core/tests/test_p8_features.py` | 已由 S4 关闭；S6 只复核 real_ondsel_solver focused test 和 expected parity 不回退 |
| `GBJ-BLOCK-004` | `cad-core/fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json`、expected、`cad-core/tests/test_p8_features.py` | 已由 S4 关闭；S6 只复核 expected parity 与负 `radius_j` 不回退 |
| `GBJ-BLOCK-005` | `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py` | supported matrix 加入 Gears / Belt，unsupported matrix 移除它们 |
| `GBJ-BLOCK-006` | 既有 P8 AssemblySolver docs / matrices、本包 TSV | RackPinion / Screw 仍为 unsupported；complex Distance 仍为 notCollected；GUI/session 仍为 nonGoal |

## 禁止路径

- 禁止恢复 representative fallback 或 unlinked build support。
- 禁止靠 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 JointType。
- 禁止在 adapter 层补业务语义。
- 禁止把 RackPinion / Screw 顺手映射到 supported。
- 禁止把完整 Distance geometry、GUI drag、postDrag 或 persistent session 混入本包。

## 验收标准

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check -- ../docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线 src/assembly/joint_solver.cpp include/cad_core/assembly/joint_solver.h src/adapters/c_api/c_api.cpp tests/test_p8_features.py tests/test_adapters.py fixtures/c3m6
```

native oracle gate：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

发布前 TSV 检查：

```bash
for f in /home/user/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

关闭条件：

- `GBJ-SCOPE-002/003/004/005` 全部从 unsupported / notCollected / releaseGate 转为 `supported`，且证据记录在本文件或后续 `【已实现】` S6 文件中。
- `GBJ-SCOPE-006` 保持 `unsupported`，`GBJ-SCOPE-007` 保持 `notCollected`，`GBJ-SCOPE-008` 保持 `nonGoal`。
- `git status --short` 中只包含本轮相关文件，且无 build 产物 / `__pycache__` 混入。

## 非目标

- 不运行全量 FreeCAD 构建。
- 不实现 RackPinion / Screw。
- 不调整 unrelated P5/P6/P7 docs 或 matrices。
