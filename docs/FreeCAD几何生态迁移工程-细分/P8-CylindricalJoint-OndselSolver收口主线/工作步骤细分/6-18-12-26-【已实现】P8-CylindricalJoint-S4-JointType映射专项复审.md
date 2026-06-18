# P8 CylindricalJoint S4 JointType 映射专项复审

## 目标

关闭 `CYL-BLOCK-002`：确认 `Cylindrical` 在 FreeCAD 和 cad-core 中都是一对一 `ASMTCylindricalJoint` 映射，且 supported predicate、runtime solver 和 tests 一致。

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| enum 顺序 | `src/Mod/Assembly/App/AssemblyUtils.h::JointType` | `Fixed, Revolute, Cylindrical, Slider, ...` |
| Python string | `src/Mod/Assembly/JointObject.py::JointTypes` | `"Cylindrical"` 与 C++ enum 同序 |
| MBD 映射 | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `CREATE<ASMTCylindricalJoint>::With()` |
| limit 边界 | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | Cylindrical 的 length / angle limit 不进入本轮完整支持 |

## scope 表

| scope | 复审结果要求 |
| --- | --- |
| `CYL-SCOPE-002` | `makeOndselJointOfType()` 返回 `MbD::ASMTCylindricalJoint::With()` |
| `CYL-SCOPE-004` | supported matrix 包含 `Cylindrical`，unsupported matrix 不包含 `Cylindrical` |
| `CYL-SCOPE-005` | Parallel / Perpendicular / RackPinion / Screw / Gears / Belt 保持 unsupported |

## 必须回写的矩阵行

- `CYL-CAND-001`
- `CYL-CAND-002`
- `CYL-CAND-003`
- `CYL-CAND-005`
- `CYL-BLOCK-002`

## 验收标准

- `cad-core/src/assembly/joint_solver.cpp` 有 FreeCAD 依据注释，明确 `Cylindrical -> ASMTCylindricalJoint`。
- `isSupportedOndselJointType()` 包含 `Cylindrical`。
- `cad-core/tests/test_p8_features.py` 的 grounded joint matrix 包含 Cylindrical fixture。
- `cad-core/tests/test_adapters.py` 检查 supported / unsupported matrix。
- 检查命令：

```bash
rg -n "ASMTCylindricalJoint|Cylindrical" cad-core/src/assembly/joint_solver.cpp cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

## 非目标

- 不实现 Cylindrical limit 的完整写回或 diagnostics matrix。
- 不把 `Cylindrical` 之外的 JointType 顺手迁入 supported。
- 不在 fixture 名称、bbox、输出顺序中推断 solver 语义。
