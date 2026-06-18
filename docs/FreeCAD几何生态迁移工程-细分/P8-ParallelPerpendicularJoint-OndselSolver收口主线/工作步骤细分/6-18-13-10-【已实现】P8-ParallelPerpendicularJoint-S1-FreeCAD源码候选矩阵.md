# P8 ParallelPerpendicularJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明 Parallel / Perpendicular 是可以单独实现的低风险 JointType 子集。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `Parallel`、`Perpendicular` 在 `enum class JointType` 中 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointTypes` 包含 Parallel / Perpendicular；`JointParallelForbidden` 包含 Perpendicular，但只作为 `preventParallel()` 交互 guard |
| MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `ASMTParallelAxesJoint` / `ASMTPerpendicularJoint` |
| marker 路径 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 非 RackPinion 走通用 `handleOneSideOfJoint` |
| solver 顺序 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::solve()` | `runPreDrag()` 后 `setNewPlacements()` |

## live 复核结论

- `PPJ-CAND-001..010` 的 `source_file` 均存在，`source_evidence` 已按当前源码和矩阵重写为可追溯措辞。
- `AssemblyUtils.h::JointType` 与 `JointObject.py::JointTypes` 顺序一致：`Parallel` / `Perpendicular` 位于 `Distance` 与 `Angle` 之间。
- FreeCAD `AssemblyObject::makeMbdJointOfType()` 对 direct `JointType::Parallel` / `JointType::Perpendicular` 分别返回 `CREATE<ASMTParallelAxesJoint>::With()` / `CREATE<ASMTPerpendicularJoint>::With()`。
- FreeCAD `Angle=0` 也会返回 `ASMTParallelAxesJoint`，但这只是 `Angle` 分支的特殊路径；不能用它证明 cad-core direct `Parallel` 已支持。
- 当前 cad-core `joint_solver.cpp` 只 include `ASMTParallelAxesJoint`，未 include `ASMTPerpendicularJoint`；`makeOndselJointOfType()` 不处理 direct `Parallel` / `Perpendicular`，`isSupportedOndselJointType()` 也不包含两者。
- `JointObject.py::JointParallelForbidden` 只服务 `preventParallel()` 交互 guard，不能作为 cad-core request-local support blocker。
- C ABI capability 当前仍把 `Parallel` / `Perpendicular` 放在 `unsupported_joint_matrix`，focused c3m6 tests 和 expected fixtures 也没有这两个 JointType。
- 上游 P8ASM-SCOPE-007 仍把 `Parallel` / `Perpendicular` 与其它 remaining JointTypes 保持 diagnostic-only，后续只能在 S3-S5 证据闭合后更新。

## 逐行复核

| 候选 | live 结论 | 后续路由 |
| --- | --- | --- |
| `PPJ-CAND-001` | 枚举顺序与 Python `JointTypes` 一致。 | S2 保持 `unsupportedImplementable`。 |
| `PPJ-CAND-002` | `JointUsingReverse` 包含 `Parallel`；`JointParallelForbidden` 包含 `Angle` / `Perpendicular`，属于 GUI 交互 guard。 | S2/S3 不把 GUI guard 写成 solver blocker。 |
| `PPJ-CAND-003` | FreeCAD direct `Parallel` 映射存在。 | S3 才能补 direct cad-core mapping；不能用 `Angle=0` 替代。 |
| `PPJ-CAND-004` | FreeCAD direct `Perpendicular` 映射存在。 | S3 需要新增 cad-core include / conversion / predicate。 |
| `PPJ-CAND-005` | 非 RackPinion 走通用 `handleOneSideOfJoint()` marker 绑定。 | S3/S4 复用 `Reference1/2` 与 `Placement1/2` DTO 路径。 |
| `PPJ-CAND-006` | cad-core 当前只在 `Angle angleRadians == 0.0` 时使用 `ASMTParallelAxesJoint`；direct `Parallel` / `Perpendicular` 未支持。 | S3 实现时同步 direct mapping 与 supported predicate。 |
| `PPJ-CAND-007` | C ABI supported/unsupported publication 与 S0 口径一致。 | S5 在实现和测试通过后再发布。 |
| `PPJ-CAND-008` | focused tests 缺 Parallel/Perpendicular case。 | S4 增加 fixture/assertion。 |
| `PPJ-CAND-009` | c3m6 fixture/expected 缺 Parallel/Perpendicular 文件。 | S4 采集或复核 FreeCADCmd expected。 |
| `PPJ-CAND-010` | upstream P8ASM-SCOPE-007 仍为 `unsupported_remaining`。 | S5 收口后再回写 upstream 矩阵。 |

## 扫描轴

- Parallel / Perpendicular 是否只需要 JointType string、Reference1 / Reference2 和 Placement1 / Placement2。
- Perpendicular 是否需要新增 Ondsel include。
- FreeCAD `JointParallelForbidden` 是否属于 GUI / interaction guard，而非 cad-core request-local solver 发布前置。
- Existing c3m6 fixture shape 是否可复用 grounded two-component pattern。

## 必须回写的矩阵行

- `PPJ-CAND-001` 到 `PPJ-CAND-010` 已完成 evidence 复核。
- `PPJ-SCOPE-001` 到 `PPJ-SCOPE-007` 必须由 S2 路由。

## 验收标准

- `p8_parallel_perpendicular_joint_source_candidates.tsv` 每行都有 source file、symbol、evidence、cad-core landing。
- 至少包含 `AssemblyObject::makeMbdJointOfType`、`AssemblyUtils.h::JointType`、`JointObject.py::JointTypes`、`cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`、c3m6 fixture / expected route。
- 检查命令：

```bash
awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线/矩阵/p8_parallel_perpendicular_joint_source_candidates.tsv
rg -n "ASMTParallelAxesJoint|ASMTPerpendicularJoint|JointType::Parallel|JointType::Perpendicular|Parallel|Perpendicular" src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/App/AssemblyUtils.h src/Mod/Assembly/JointObject.py cad-core/src/assembly/joint_solver.cpp
```

## 非目标

- S1 不推广 RackPinion / Screw / Gears / Belt。
- S1 不把 FreeCAD GUI forbidden rules 当 cad-core solver support blocker。
- S1 不从 fixture bbox 倒推 solver 语义。
