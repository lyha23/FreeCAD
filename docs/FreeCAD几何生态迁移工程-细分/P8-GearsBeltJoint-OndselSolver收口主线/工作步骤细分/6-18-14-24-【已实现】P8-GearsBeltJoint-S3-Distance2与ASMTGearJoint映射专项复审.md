# P8 GearsBeltJoint S3 Distance2 与 ASMTGearJoint 映射专项复审

## 目标

关闭 `GBJ-BLOCK-001` 和 `GBJ-BLOCK-002`：确认 cad-core real Ondsel adapter 按 FreeCAD 映射 Gears / Belt，并补齐 `Distance2` DTO。

当前状态：已实现。S3 只关闭 `Distance2` DTO / request parser 和 Gears / Belt `ASMTGearJoint` runtime mapping；S4 native expected、S5 capability / unsupported matrix 发布和 S6 oracle 收口已由后续步骤关闭。

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| Gears mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Gears` 创建 `ASMTGearJoint`，`radiusI=Distance`、`radiusJ=Distance2` |
| Belt mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Belt` 创建 `ASMTGearJoint`，`radiusI=Distance`、`radiusJ=-Distance2` |
| Distance2 property | `src/Mod/Assembly/JointObject.py::JointUsingDistance2` | 只包含 `Gears` / `Belt` |
| marker path | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | Gears / Belt 不走 RackPinion 特殊 marker |

## scope 表

| scope | 复审结果要求 |
| --- | --- |
| `GBJ-SCOPE-002` | 已实现：`JointConstraint` 有 `distance2`，Gears conversion 设置 `radiusI=Distance` / `radiusJ=Distance2` |
| `GBJ-SCOPE-003` | 已实现：Belt conversion 设置 `radiusI=Distance` / `radiusJ=-Distance2` |
| `GBJ-SCOPE-006` | 已保持：RackPinion / Screw 不新增 supported |

## 代码落点

- `cad-core/include/cad_core/assembly/joint_solver.h`：新增 `JointConstraint.distance2`，相邻注释记录 `JointUsingDistance2` 和 `makeMbdJointOfType()` FreeCAD 依据。
- `cad-core/src/assembly/joint_solver.cpp`：新增 `ASMTGearJoint` include；`buildAssemblySolveRequest()` 为 Gears / Belt 读取 `Distance` / `Distance2`；`makeOndselJointOfType()` 按 FreeCAD 半径符号生成 `ASMTGearJoint`；`isSupportedOndselJointType()` 只新增 Gears / Belt。
- `cad-core/src/assembly/assembly_utils.cpp`：`solver_joints[]` 在 `distance2` 存在时输出该字段。

## 关闭结论

- `GBJ-BLOCK-001` 已由 S3 代码关闭：DTO、request parser 和 `solver_joints[].distance2` 输出已落地。
- `GBJ-BLOCK-002` 已由 S3 代码关闭：Gears / Belt runtime mapping 和 supported predicate 已落地。
- `GBJ-BLOCK-003` / `GBJ-BLOCK-004` 继续等待 S4 native expected；`GBJ-BLOCK-005` 继续等待 S5 capability / unsupported matrix publication；`GBJ-BLOCK-006` 继续保护 RackPinion / Screw / complex Distance 边界。

## 必须回写的矩阵行

- `GBJ-CAND-003`
- `GBJ-CAND-004`
- `GBJ-CAND-006`
- `GBJ-CAND-007`
- `GBJ-CAND-008`
- `GBJ-BLOCK-001`
- `GBJ-BLOCK-002`

## 验收标准

- `cad-core/include/cad_core/assembly/joint_solver.h` 新增 `distance2` 字段并有 FreeCAD 依据注释。
- `cad-core/src/assembly/joint_solver.cpp` 新增或确认 `ASMTGearJoint` include。
- `buildAssemblySolveRequest()` 为 `Gears` / `Belt` 读取 `Distance` / `Distance2`。
- `makeOndselJointOfType()` 按 FreeCAD 依据设置 `radiusI` / `radiusJ`。
- `isSupportedOndselJointType()` 包含 Gears / Belt，仍不包含 RackPinion / Screw。
- 检查命令：

```bash
rg -n "ASMTGearJoint|distance2|Distance2|radiusI|radiusJ|Gears|Belt|RackPinion|Screw" cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp cad-core/src/assembly/assembly_utils.cpp
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_unsupported_joint_stays_diagnostic
```

## 非目标

- 不实现 RackPinion / Screw。
- 不改 assembly DTO 字段，除 `distance2` 以外不新增复杂 Distance geometry 字段。
- 不在 expected 或 adapter 中补业务语义。
