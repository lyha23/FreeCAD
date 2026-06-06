# 真实 Ondsel Solver 迁移方案

## 1. 目标

把 CADCore3 Assembly 从当前 representative solver / placement writeback contract 推进到真实 Ondsel solver 语义。

完成后，Assembly 的 capability 口径应从：

- `representative_solver_adapter.status=covered_representative`
- `ondsel_solver_adapter.status=not_implemented` 或无真实求解路径
- `placement_writeback.status=covered_contract`

推进到：

- `ondsel_solver_adapter.status=covered_full`
- representative solver 只作为 mock、diagnostic 或 unsupported fallback
- writeback update 来自真实 solver result 和 validation，而不是静态代表性结果

## 2. 非目标

- 不实现 GCS / Sketch solver。
- 不保存跨请求 backend solver session。CAD Core 每次仍然从请求里的 `DocumentObject graph` 重建 solver 输入。
- 不迁移 Assembly GUI、Workbench、TaskPanel、拖拽交互 UI。
- 不用固定 fixture 的 placement 结果倒推 solver 输出。

## 3. FreeCAD 依据入口

开工前必须先复核这些本地源码入口，并把关键字段和调用链补到实现注释或方案增量中：

| FreeCAD 源码 | 需要提取的语义 |
| --- | --- |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | Assembly solve 主流程、solver 状态、错误返回、placement 更新时机 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::fixGroundedParts()` | Grounded part 如何固定，以及固定体如何进入 solver |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()` | Joint 类型到 Ondsel/MBD constraint 的映射 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp` 中 `mbdAssembly->runPreDrag()` 调用点 | 求解运行入口、预拖拽语义、失败诊断 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::setNewPlacements()` | solver result 写回 DocumentObject placement 的规则 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::validateNewPlacements()` | 新 placement 的合法性检查、拒绝条件和诊断 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/JointGroup.cpp` | Joint 收集、组织和 Assembly 对象关系 |

## 4. 目标调用链

目标 CAD Core 链路：

1. `DocumentObject graph` 解析出 Assembly 对象、part 对象、joint 对象和 grounded 状态。
2. Assembly executor 生成 `AssemblySolveRequest`，包含 part placement DTO、joint constraint DTO、fixed body 列表和 solve options。
3. Ondsel solver adapter 把 DTO 转为真实 MBD/Ondsel 输入。
4. solver 返回 `AssemblySolveResult`，包含每个 part 的候选 placement、solver 状态和 diagnostics。
5. `validateNewPlacements()` 等价逻辑过滤非法 placement。
6. CAD Core 返回 `documentObjectUpdates`，只把 validated placement 写回给前端。
7. 下一次请求仍由前端保存后的 graph 重新计算，不依赖 backend 内存状态。

## 4.1 本轮源码复核与 blocker

已复核 FreeCAD 调用链：

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()`：顺序是 `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`getJoints()`、`removeUnconnectedJoints()`、`jointParts()`、`mbdAssembly->runPreDrag()`、`setNewPlacements()`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::fixGroundedParts()` / `fixGroundedPart()`：把 grounded part 的当前 `Placement` 转成 `ASMTPart`，并通过 `ASMTFixedJoint` 固定到 `/OndselAssembly` marker。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()`：`Fixed`、`Revolute`、`Cylindrical`、`Slider`、`Ball`、`Distance`、`Parallel`、`Perpendicular`、`Angle` 等映射到对应 `ASMT*Joint`，`Angle` 写 `mbdJoint->theIzJz = angle`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::setNewPlacements()`：从 `getMbdPlacement(mbdPart)` 取 solver 后 placement，叠加 `offsetPlc` 后写 `propPlacement->setValue(newPlacement)`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::validateNewPlacements()`：校验 grounded object 未移动；若移动则输出 `Ignoring bad solve, a grounded object (...) moved.` 并拒绝。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py`：`JointType`、`Reference1/2`、`Placement1/2`、`Offset1/2`、`Detach1/2`、`Distance`、`Angle`、limit 字段由 Python JointObject 建立；`JointGroup.cpp::JointGroup::getJoints()` 只收集未 suppressed 且 proxy 有 `setJointConnectors` 的 joint。
- 构建来源：`src/Mod/Assembly/App/CMakeLists.txt` 链接 `OndselSolver`；`src/Mod/Assembly/CMakeLists.txt` 在非 external 模式下只 include `${CMAKE_SOURCE_DIR}/src/3rdParty/OndselSolver`；`.gitmodules` 声明 `src/3rdParty/OndselSolver` 子模块。

当前进展与剩余 blocker：

- `src/3rdParty/OndselSolver` 已初始化到 `30e9b64e8bf881d438d4b88834f9ba3674865418`；`git submodule status -- src/3rdParty/OndselSolver` 当前无前缀 `-`，说明子模块源码可用。
- `cad-core/CMakeLists.txt` 已在存在 `../src/3rdParty/OndselSolver/CMakeLists.txt` 时 `add_subdirectory()` 并链接 `OndselSolver`，同时给 `cad-core` / `cad_core_ffi` 增加 Ondsel build rpath。
- `cad-core/include/cad_core/assembly/joint_solver.h` 与 `cad-core/src/assembly/joint_solver.cpp` 已建立 `AssemblySolveRequest`、`AssemblySolveResult`、`JointConstraint`、placement validation diagnostic 边界；`assembly_utils.cpp` 只组装请求、调用 solver adapter、消费结果。
- 当前真实求解 joint matrix：有 grounded part 的 Fixed joint fixture 会构造 request-local `ASMTAssembly`，按 FreeCAD `fixGroundedPart()` 建 assembly marker / part marker / `ASMTFixedJoint`，再执行 `ASMTAssembly::runPreDrag()`；Ball / Revolute / Slider / Distance / Angle fixture 按 FreeCAD `makeMbdJointOfType()` 的 `ASMTSphericalJoint`、`ASMTRevoluteJoint`、`ASMTTranslationalJoint`、`ASMTSphSphJoint`、`ASMTAngleJoint` 映射进入同一真实 solver 路径，且 Angle=0 分支已按 FreeCAD 映射到 `ASMTParallelAxesJoint`。solver result 通过 `validateNewPlacementsEquivalent()` 后进入 `documentObjectUpdates`。`tests.test_p8_features` 中 `assembly-joint-group-diagnostics` 与 `assembly-grounded-*-joint-real-solver` 已锁 `real_ondsel_solver`。
- validation matrix：`assembly-invalid-grounded-distance-real-solver` 用双 grounded part 加 `Distance=2` 的矛盾约束触发真实 `ASMTAssembly::runPreDrag()`，随后 `validateNewPlacementsEquivalent()` 按 FreeCAD `validateNewPlacements()` 的 grounded object moved 规则输出 `invalid_assembly_solver_result`，并拒绝 `documentObjectUpdates` writeback。
- 当前可以把 `ondsel_solver_adapter.status` 提升为 `covered_full`；无 grounded part 的旧 representative fixture 仍只能作为 fallback / diagnostic 口径。

## 5. 候选落点

| CAD Core 落点 | 职责 |
| --- | --- |
| `cad-core/include/cad_core/assembly/assembly_object.h` | Assembly solve public DTO、solver result、validation result |
| `cad-core/src/assembly/assembly_object.cpp` | Assembly object FreeCAD 调用顺序、grounded part 和 placement validation |
| `cad-core/include/cad_core/assembly/joint_solver.h` | solver adapter 接口、joint DTO、unsupported matrix |
| `cad-core/src/assembly/joint_solver.cpp` | Ondsel adapter 转换、mock solver、diagnostics |
| `cad-core/src/features` 或现有 Assembly executor 文件 | 只负责从 document graph 调用 Assembly solve API，不承载 solver 内部语义 |
| `cad-core/src/adapters` | 只做协议转换和 update 输出，不写 Assembly 业务规则 |

如果现有 CAD Core 已经有 Assembly 文件，应优先复用现有模块名和 include 层级，避免为了本方案新增平行概念。

## 6. 实施切片

### A. FreeCAD 调用链和字段清单

- 补齐 `solve()` 中从 Assembly 对象到 solver 的完整调用顺序。
- 列出 joint 类型、part placement、grounded part、solve options、diagnostic 状态。
- 明确哪些语义必须进入 CAD Core public DTO，哪些只是 adapter 内部细节。

验收：方案或相邻代码注释中能追溯到 FreeCAD 源文件、函数和关键字段。

### B. Assembly DTO 和 diagnostics

- 新增或整理 `AssemblySolveRequest`、`AssemblyPartRef`、`JointConstraint`、`AssemblySolveResult`。
- 对 unsupported joint type、missing linked object、invalid placement、over-constrained solve 输出结构化 diagnostics。
- representative solver 暂时保留，但只通过同一个 adapter interface 返回结果。

验收：无真实 Ondsel 依赖时，mock / representative path 仍可覆盖 transport 和 diagnostics，不声明 `covered_full`。

### C. Ondsel solver adapter

- 按 FreeCAD `makeMbdJointOfType()` 建立 joint type 到 solver constraint 的映射。
- 支持最小矩阵：Fixed、Revolute、Slider、Ball、Distance、Angle。
- 对未支持 joint 类型返回 explicit unsupported diagnostic，不生成静态假 placement。

验收：Fixed / Revolute / Slider / Ball / Distance / Angle 已有成功 fixture；未支持 joint 有失败 fixture 和 diagnostics。

### D. Grounded 和 validation

- 迁移 `fixGroundedParts()` 的固定体规则。
- 迁移 `validateNewPlacements()` 的 placement 合法性检查。
- 只有 validated placements 才能进入 `documentObjectUpdates`。

验收：grounded part 不被 solver 移动；invalid placement 不写回。

### E. Writeback 和 capability flip

- `documentObjectUpdates` 从 solver result 生成，不再由 representative contract 直接构造。
- 更新 `docs/CADCore3.0/capabilities-gap对照表.md` 和 `docs/CADCore3.0/FreeCAD语义矩阵.md`。
- 删除或降级 representative solver 的 covered 口径。

验收：`ondsel_solver_adapter.status=covered_full` 只在真实 solver fixture 全通过后出现。

## 7. 验收矩阵

必须新增或补齐的 case：

| Case | 期望 |
| --- | --- |
| Fixed joint + grounded part | 已覆盖：grounded part placement 不变，另一 part 按 solver 结果更新 |
| Revolute joint | 已覆盖：真实 `ASMTRevoluteJoint` / `runPreDrag()` 路径生效 |
| Slider joint | 已覆盖：真实 `ASMTTranslationalJoint` / `runPreDrag()` 路径生效 |
| Ball joint | 已覆盖：真实 `ASMTSphericalJoint` / `runPreDrag()` 路径生效 |
| Distance joint | 已覆盖：distance constraint 写回 2mm 间距 |
| Angle joint | 已覆盖：非零 angle 进入 `ASMTAngleJoint`，Angle=0 映射 `ASMTParallelAxesJoint` |
| Missing linked object | 返回 diagnostic，不写假 placement |
| Unsupported joint type | 返回 unsupported，不声明 covered |
| Invalid solver result | 已覆盖：双 grounded + Distance 矛盾 fixture 输出 `invalid_assembly_solver_result`，不写回 |

阶段回归命令：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_p8_features
```

代码改动后的轻量检查：

```bash
git diff --check -- cad-core docs/CADCore3.0 docs/偏移处理
```

## 8. 完成条件

- Assembly solve 主路径使用真实 Ondsel adapter。
- `setNewPlacements()` / `validateNewPlacements()` 等价语义在 CAD Core 中有明确落点。
- representative solver 不再作为 full coverage 依据。
- capability gap、语义矩阵、oracle fixture 队列同步更新。
- 所有 supported joint fixture 通过；unsupported 已有稳定 diagnostics；invalid placement case 已覆盖，`ondsel_solver_adapter.status=covered_full`。
