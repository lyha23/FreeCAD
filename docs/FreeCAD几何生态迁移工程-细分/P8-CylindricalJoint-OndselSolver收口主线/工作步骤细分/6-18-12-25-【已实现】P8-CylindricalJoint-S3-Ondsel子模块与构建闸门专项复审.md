# P8 CylindricalJoint S3 Ondsel 子模块与构建闸门专项复审

## 目标

关闭 `CYL-BLOCK-001`：确保 hard-linked OndselSolver 存在并能构建 `cad-core` / `cad_core_ffi`，否则 Cylindrical 不能发布为 supported。

## FreeCAD / cad-core 依据

| 来源 | 依据 |
| --- | --- |
| `cad-core/CMakeLists.txt` | `CAD_CORE_ONDSEL_SOLVER_DIR` 指向 `../src/3rdParty/OndselSolver`，缺 `CMakeLists.txt` 时 fatal error |
| `cad-core/src/adapters/c_api/c_api.cpp` | capabilities 发布 `CAD_CORE_HAS_ONDSEL_SOLVER=1` real-only mode |
| `cad-core/src/assembly/joint_solver.cpp` | `hasOndselSolverAdapter()` 返回 true，求解走 real Ondsel |

## 范围

- 检查 `git submodule status --recursive` 中 `src/3rdParty/OndselSolver` 是否为未初始化状态。
- 如果缺失，只允许用正常子模块初始化恢复来源，不允许改 CMake 降级或恢复 fallback。
- 构建目标只要求 `cad-core` 和 `cad_core_ffi`。

## 必须回写的矩阵行

- `CYL-SCOPE-001`
- `CYL-BLOCK-001`
- `CYL-BG-001`

## 验收标准

- `src/3rdParty/OndselSolver/CMakeLists.txt` 存在。
- `cmake --build build --target cad-core cad_core_ffi` 在 `cad-core/` 下通过。
- capability 单测不再因旧 `libcad_core_ffi.so` 或缺字段失败。
- 检查命令：

```bash
git submodule status --recursive | rg 'src/3rdParty/OndselSolver'
test -f /home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/CMakeLists.txt
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

## 非目标

- 不恢复 `CAD_CORE_ENABLE_ONDSEL_SOLVER=OFF`。
- 不引入 representative fallback。
- 不改 expected 数据来绕过构建失败。
