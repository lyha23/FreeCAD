# P8 CylindricalJoint S6 Oracle 实现与发布闸门

## 目标

消费 S2-S5 留下的可实现项，把 Cylindrical 作为一个最小 request-local JointType 支持发布，并记录验收证据。

## 输入

- `CYL-BLOCK-001` 到 `CYL-BLOCK-005`
- c3m6 Cylindrical fixture / expected
- 本轮 C++ / tests / docs 证据
- hard-linked OndselSolver build 环境

## 实施顺序

1. 先关闭 `CYL-BLOCK-001`：恢复 `src/3rdParty/OndselSolver/CMakeLists.txt`，构建 `cad-core` 和 `cad_core_ffi`。
2. 再关闭 `CYL-BLOCK-002`：确认 `joint_solver.cpp` 中 include、`makeOndselJointOfType()`、`isSupportedOndselJointType()` 都包含 `Cylindrical`。
3. 关闭 `CYL-BLOCK-003`：确认 c3m6 fixture / expected 被 expected runner 覆盖。
4. 关闭 `CYL-BLOCK-004`：同步 `c_api.cpp` capabilities、`test_adapters.py`、`test_p8_features.py` 和既有 P8 docs/matrices。
5. 关闭 `CYL-BLOCK-005`：保留 remaining unsupported matrix，并用 test 锁定不重叠。

## 代码落点

| blocker | C++ / 测试 / 文档落点 | 成功标准 |
| --- | --- | --- |
| `CYL-BLOCK-001` | `src/3rdParty/OndselSolver`、`cad-core/CMakeLists.txt` | `cmake --build build --target cad-core cad_core_ffi` 通过，不恢复 fallback |
| `CYL-BLOCK-002` | `cad-core/src/assembly/joint_solver.cpp` | `Cylindrical` 返回 `MbD::ASMTCylindricalJoint::With()`，supported predicate 包含 `Cylindrical` |
| `CYL-BLOCK-003` | `cad-core/fixtures/c3m6/assembly-grounded-cylindrical-joint-real-solver.json`、`cad-core/fixtures/c3m6/expected/assembly-grounded-cylindrical-joint-real-solver.freecad.json` | expected runner 不 skip，native solver mode 为 `real_ondsel_solver` |
| `CYL-BLOCK-004` | `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py`、`cad-core/tests/test_p8_features.py` | capabilities supported matrix 包含 Cylindrical，unsupported matrix 不包含 Cylindrical，focused tests 通过 |
| `CYL-BLOCK-005` | 既有 P8 AssemblySolver docs / matrices 的 Cylindrical 行 | 只更新 Cylindrical 相关状态，Parallel / Perpendicular / RackPinion / Screw / Gears / Belt 仍为 unsupported |

## 禁止路径

- 禁止恢复 `representative_ondsel_solver` 或 unlinked fallback。
- 禁止靠 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 JointType。
- 禁止在 adapter 层补业务语义。
- 禁止把 `ReferenceShadow.brep` 之外的 BREP 或 solver state 当成跨请求状态。
- 禁止把 Cylindrical limit、GUI drag、postDrag 或完整 transaction lifecycle 混入本包。

## 验收标准

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check -- ../docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线 src/assembly/joint_solver.cpp src/adapters/c_api/c_api.cpp tests/test_p8_features.py tests/test_adapters.py fixtures/c3m6
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

发布前 TSV 检查：

```bash
for f in /home/user/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

关闭条件：

- `CYL-SCOPE-001/002/003/004` 全部为 `supported`，且证据记录在本文件或对应矩阵中。
- `CYL-SCOPE-005` 保持 `unsupported`，`CYL-SCOPE-006` 保持 `nonGoal`。
- `git status --short` 中只包含本轮相关文件，且无 build 产物 / `__pycache__` 混入。

## 非目标

- 不运行全量 FreeCAD 构建。
- 不采集或实现复杂 Distance / RackPinion / Screw / Gears / Belt。
- 不调整 unrelated P5/P6/P7 docs 或 matrices。
