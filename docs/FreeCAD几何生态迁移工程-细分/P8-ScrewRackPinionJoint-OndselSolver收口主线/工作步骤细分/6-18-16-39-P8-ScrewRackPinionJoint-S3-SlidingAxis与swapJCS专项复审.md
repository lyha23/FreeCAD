# P8 ScrewRackPinionJoint S3 SlidingAxis 与 swapJCS 专项复审

## 目标

关闭 `SRJ-BLOCK-001`：确认 cad-core 能按 FreeCAD `slidingPartIndex()` / `swapJCS()` 语义，从同一 request-local Assembly graph 中判断 Screw / RackPinion 需要的 sliding side，并在必要时交换 JCS 输入。

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| sliding side | `src/Mod/Assembly/App/AssemblyObject.cpp::slidingPartIndex()` | 扫描 `getJoints()` 中的 Slider joint，匹配 moving parts，比较 Slider JCS 与目标 joint JCS 的 pitch / roll |
| invalid precondition | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | Screw / RackPinion 在 `slidingPartIndex()==0` 时不能创建 MBD joint |
| swap JCS | `src/Mod/Assembly/App/AssemblyObject.cpp::swapJCS()` | sliding side 不是第一侧时交换 Reference / Placement 输入，确保 sliding/rack 在 I 侧 |

## scope 表

| scope | 复审结果要求 |
| --- | --- |
| `SRJ-SCOPE-002` | request builder 或 helper 能在单次请求中计算 sliding side，且不持久改 DocumentObject graph |
| `SRJ-SCOPE-003` | Screw 可消费 shared sliding evidence，不满足 precondition 时输出 diagnostic |
| `SRJ-SCOPE-004` | RackPinion 可消费 shared sliding evidence，为 S4 marker rewrite 提供 I/J 顺序 |

## 必须回写的矩阵行

- `SRJ-CAND-005`
- `SRJ-CAND-006`
- `SRJ-CAND-009`
- `SRJ-BLOCK-001`

## 验收标准

- `cad-core/src/assembly/joint_solver.cpp` 有 request-local sliding side helper，FreeCAD 依据注释明确指向 `slidingPartIndex()`。
- 不满足 Slider precondition 的 Screw / RackPinion 不进入 fake solve，应保留结构化 diagnostic。
- `swapJCS()` 等价语义只交换本次 solver DTO / marker input，不修改前端 DocumentObject graph。
- 检查命令：

```bash
rg -n "sliding|swapJCS|Screw|RackPinion|Slider|pitch|roll" cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp cad-core/tests/test_p8_features.py
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_unsupported_joint_stays_diagnostic
```

## 非目标

- S3 不发布 Screw / RackPinion capability。
- S3 不实现 RackPinion marker rotation；只准备 shared sliding / JCS ordering 前置。
- S3 不从 component 名称、fixture 名称、bbox、shape 类型或输出顺序猜测 sliding side。
