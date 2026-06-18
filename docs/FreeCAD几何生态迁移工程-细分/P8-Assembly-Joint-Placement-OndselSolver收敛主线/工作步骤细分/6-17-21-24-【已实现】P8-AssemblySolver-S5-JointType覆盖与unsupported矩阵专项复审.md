# P8 AssemblySolver S5 JointType 覆盖与 unsupported 矩阵专项复审

## 目标

裁决当前 CAD Core 支持的 JointType 子集、diagnostic-only JointType 和下一轮可实现顺序。S5 不允许把复杂 JointType 只按名称塞进 supported，必须有 FreeCAD source、DTO 字段、fixture / oracle 和 focused tests。

## 后续修正

2026-06-18 后续实现已删除 representative fallback 和 optional unlinked build 路径；下文关于 unlinked build 的发布约束只保留为当时裁决，不代表当前发布状态。native solver placement expected 已在 S6 之后入库，并已在 S7 修复到 supported；当前状态以 S7、矩阵、C++ 和 C ABI capabilities 的 real-only 口径为准。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有大量非 S5 改动和 P8 seed 未跟踪文件；本轮只编辑 S5 文档、P8 入口和 S5 相关矩阵行，不 reset、不 revert、不清理。 |

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py`：`JointTypes` 包含 Fixed、Revolute、Cylindrical、Slider、Ball、Distance、Parallel、Perpendicular、Angle、RackPinion、Screw、Gears、Belt。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()`：JointType 到 ASMT joint 的 mapping。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointDistance()`：Distance 需要按 reference geometry 再分类。

## FreeCAD JointType 映射

| JointType | `JointObject.py` 字段 | `makeMbdJointOfType()` / `makeMbdJointDistance()` 语义 | S5 裁决 |
| --- | --- | --- | --- |
| Fixed | `JointUsingOffset`、`JointUsingRotation`、`JointUsingPreSolve` | `ASMTFixedJoint`，`bundleFixed` 时可返回空 | 当前 cad-core 子集 `releaseGate` |
| Revolute | `JointUsingOffset`、`JointUsingReverse`、angle limit | `ASMTRevoluteJoint`，后续可加 rotation limit | 当前 cad-core 子集 `releaseGate` |
| Cylindrical | length / angle limit、preSolve | `ASMTCylindricalJoint`，后续还会加 translation / rotation limits | diagnostic-only；S6 可作为优先实现项 |
| Slider | `Distance`、length limit、rotation flag | `ASMTTranslationalJoint`，FreeCAD 后续加 translation limits | 当前 cad-core scalar DTO 子集 `releaseGate` |
| Ball | preSolve | `ASMTSphericalJoint` | 当前 cad-core 子集 `releaseGate` |
| Distance | `Distance` | 进入 `makeMbdJointDistance()`；PointPoint 可为 `ASMTSphericalJoint` 或 `ASMTSphSphJoint`，其它 geometry case 会按 Point / Line / Plane / Cylinder / Sphere / Torus / Curve 组合选择 `ASMTRevCylJoint`、`ASMTPlanarJoint`、`ASMTLineInPlaneJoint`、`ASMTPointInPlaneJoint`、`ASMTCylSphJoint` 等，并读取 face / edge radius | 仅 scalar/request-local PointPoint 风格子集 `releaseGate`；复杂 geometry case 保持 `notCollected` |
| Parallel | `JointUsingReverse` | `ASMTParallelAxesJoint` | diagnostic-only；S6 可作为优先实现项 |
| Perpendicular | `JointParallelForbidden` | `ASMTPerpendicularJoint` | diagnostic-only；S6 可作为优先实现项 |
| Angle | `JointUsingAngle`、`JointParallelForbidden` | 角度为 0 时使用 `ASMTParallelAxesJoint`，否则 `ASMTAngleJoint::theIzJz` | 当前 cad-core scalar DTO 子集 `releaseGate` |
| RackPinion | `Distance` 表示 pitch radius | `ASMTRackPinionJoint::pitchRadius`，并走 `getRackPinionMarkers()` 特殊 marker | diagnostic-only；无 oracle 前不实现 |
| Screw | `Distance` 表示 pitch | 依赖 `slidingPartIndex()`，必要时 `swapJCS()`，再用 `ASMTScrewJoint::pitch` | diagnostic-only；无 oracle 前不实现 |
| Gears | `Distance` / `Distance2` | `ASMTGearJoint::radiusI` / `radiusJ` | diagnostic-only；无 oracle 前不实现 |
| Belt | `Distance` / `Distance2` | 复用 `ASMTGearJoint`，`radiusJ = -Distance2` | diagnostic-only；无 oracle 前不实现 |

FreeCAD 的连接遍历还把 RackPinion / Screw / Gears / Belt 排除在 `isJointTypeConnecting()` 之外；这进一步说明它们不是简单把名称映射到 ASMT 类就能发布的 solver 子集。

## cad-core 当前矩阵

| 类别 | JointType | 当前证据 | 发布边界 |
| --- | --- | --- | --- |
| supported / releaseGate | Fixed、Revolute、Slider、Ball、Distance、Angle | `isSupportedRepresentativeJointType()` 只列这 6 种；linked real path 的 `makeOndselJointOfType()` 也只转换这 6 种；C3M6 fixtures / focused tests 覆盖 Ball、Revolute、Slider、Distance、Angle，P8 hidden reference fixture 覆盖 Fixed | 仍受 S3 build-mode 影响：`CAD_CORE_HAS_ONDSEL_SOLVER=0` 时不能宣称 real solver，只能发布 representative / DTO / diagnostic 子集 |
| diagnostic-only | Cylindrical、Parallel、Perpendicular、RackPinion、Screw、Gears、Belt | runtime 对不在 supported set 的 JointType 统一返回 `unsupported_assembly_solver`；当前 focused unsupported fixture 只覆盖 RackPinion；C ABI `unsupported_joint_matrix` 当前列 RackPinion / Screw / Gears / Belt / Cylindrical，尚未列 Parallel / Perpendicular | S6 需要补齐 capability wording 和 focused matrix；S5 不改 C++ |
| notCollected | complex Distance geometry cases | FreeCAD `makeMbdJointDistance()` 需要 `DistanceType`、Reference element kind、face / edge radius 和多种 ASMT joint；cad-core DTO 当前只读取 scalar `Distance` | 没有 FreeCAD oracle / checked-in expected 前不得宣称 supported 或 backendGap |

## Distance / Angle 边界

- Distance 当前只发布已有 DTO / focused 覆盖的 scalar request-local 子集：`JointConstraint.distance` 只来自 `Distance`，representative path 只做现有 transport 行为，linked real path只构造 `ASMTSphSphJoint::distanceIJ`。
- FreeCAD 的完整 Distance 不是单一 scalar：`LineLine`、`PlanePlane`、`CylinderSphere`、`PointCurve` 等都需要 reference geometry 分类和 radius 读取。S6 若实现，必须先有 FreeCAD oracle、fixture、DTO 字段和 focused tests。
- Angle 当前只发布 scalar `Angle` 子集：cad-core 将 degree 转 radian，0 度走 `ASMTParallelAxesJoint`，非 0 度写 `ASMTAngleJoint::theIzJz`。FreeCAD GUI 的 `preventParallel()`、交互侧禁止平行姿态等不属于 S5 发布闭环。

## S6 可执行顺序

1. 先做 publication / diagnostic matrix 对齐：C ABI `unsupported_joint_matrix`、正式 P8 文档和 focused tests 必须同时列出 Cylindrical、Parallel、Perpendicular、RackPinion、Screw、Gears、Belt，且 unlinked build 不声明 real Ondsel 支持。
2. 再做低风险 ASMT 映射：Cylindrical、Parallel、Perpendicular。它们已有 FreeCAD ASMT 类映射，但仍需要 DTO 字段、fixture / oracle 和 focused assertions，不能只按名称塞进 supported。
3. 再做 Distance geometry 子集：每次只选一个 `DistanceType`，补 reference geometry DTO / oracle / expected / test 后再实现。
4. 最后再考虑 RackPinion、Screw、Gears、Belt：这些依赖特殊 marker、滑动方向、`Distance2`、gear/belt radius 或 motion 语义；无 oracle 时继续留在 diagnostic-only / nonGoal。

## 必须回写的矩阵行

- `P8ASM-SCOPE-007`：已拆成 supported / releaseGate、diagnostic-only 和 notCollected Distance geometry。
- `P8ASM-BLOCK-005`：关闭 S5 裁决，保留 S6 的 capability / focused test / implementable queue。
- `P8ASM-BG-004`：复杂 JointType 仍需产品决策；Cylindrical / Parallel / Perpendicular 可作为 S6 evidence-backed 实现候选。
- `P8ASM-NG-005`：继续约束 RackPinion / Screw / Gears / Belt 和复杂 Distance，不把 fixture guess 当实现依据。

## 验收

- `cad-core/src/assembly/joint_solver.cpp::isSupportedRepresentativeJointType()` 和 `makeOndselJointOfType()` 的支持矩阵当前都只覆盖 Fixed / Revolute / Slider / Ball / Distance / Angle。
- unsupported JointType 当前有通用 `unsupported_assembly_solver` diagnostic；focused fixture 只证明 RackPinion，S6 需要补 matrix 覆盖。
- S5 未选择实现新增 JointType；所有新增实现路由到 S6，且必须列出 exact C++ fields、fixture、expected 和 focused tests。
- 验证命令：

```bash
rg -n "JointTypes|JointUsingDistance|JointUsingAngle|makeMbdJointOfType|makeMbdJointDistance|RackPinion|Screw|Gears|Belt|Cylindrical|Parallel|Perpendicular" src/Mod/Assembly/JointObject.py src/Mod/Assembly/App/AssemblyObject.cpp
rg -n "isSupportedRepresentativeJointType|makeOndselJointOfType|unsupported_assembly_solver|unsupported_joint_matrix|Fixed|Revolute|Slider|Ball|Distance|Angle|Cylindrical|Parallel|Perpendicular|RackPinion|Screw|Gears|Belt" cad-core/src/assembly/joint_solver.cpp cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_p8_features.py
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_unsupported_joint_stays_diagnostic
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不在没有 oracle 的情况下实现 RackPinion / Screw / Gears / Belt。
- 不把 Distance 全部 geometry cases 一次性宣称 supported。
- 不靠 joint 名称、fixture 名称或输出排序猜 solver 结果。
- 不运行 FreeCADCmd collector。
