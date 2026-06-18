# P8 CylindricalJoint S5 NativeOracle 与 capability 发布专项复审

## 目标

关闭 `CYL-BLOCK-003` 和 `CYL-BLOCK-004`：确认 native expected、focused tests、C ABI capabilities、既有 P8 文档 / TSV 全部以同一口径发布 Cylindrical 支持。

## FreeCAD 依据

- `AssemblyObject::solve()` 返回 code 0 后 `setNewPlacements()`。
- `makeMbdJointOfType()` 对 Cylindrical 使用 `ASMTCylindricalJoint`。
- `JointObject.py` 暴露 `Cylindrical`，但 GUI / limit 语义不进入本包。

## scope 表

| scope | 复审内容 |
| --- | --- |
| `CYL-SCOPE-003` | `assembly-grounded-cylindrical-joint-real-solver.freecad.json` 是 FreeCADCmd native expected，不是手写 guess |
| `CYL-SCOPE-004` | capabilities、test assertions、P8 existing matrix 对 supported / unsupported 列表一致 |
| `CYL-SCOPE-005` | remaining unsupported 不因 Cylindrical 收口而消失 |

## 必须回写的矩阵行

- `CYL-CAND-006`
- `CYL-CAND-007`
- `CYL-CAND-008`
- `CYL-BLOCK-003`
- `CYL-BLOCK-004`
- `CYL-BLOCK-005`

## 验收标准

- native expected 文件包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver`、`solver_joints[0].joint_type=Cylindrical`。
- `CadCoreExpectedFixtureTest` 能消费 Cylindrical expected，且不因 known_gap skip。
- C ABI capabilities 有 `supported_joint_matrix` 和 `unsupported_joint_matrix`，二者不重叠。
- 如果更新既有 P8 主线，只更新 Cylindrical 相关行，不重写 unrelated step 或矩阵。
- 检查命令：

```bash
rg -n '"joint_type": "Cylindrical"|real_ondsel_solver|native_solver' cad-core/fixtures/c3m6/expected/assembly-grounded-cylindrical-joint-real-solver.freecad.json
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check -- cad-core/src/assembly/joint_solver.cpp cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6
```

## 非目标

- 不刷新无关 expected。
- 不把 current local FreeCAD / OCCT 偶然差异当成 expected 修正依据。
- 不把 supported matrix 写成完整 Assembly solver 复刻。
