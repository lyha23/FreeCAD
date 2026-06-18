# P8 DistanceTypeBasicGeometry S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、纳入范围、排除范围和状态字典，并用 live repo 证明当前 `Distance` JointType 仍是 scalar-only 基线。

## 声明口径

| 类别 | 允许声明 | 禁止声明 |
| --- | --- | --- |
| 本包目标 | 基础 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` DistanceType 的 request-local Ondsel 支持 | 完整 Distance geometry matrix 已支持 |
| cad-core 边界 | 从每次请求的 DocumentObject graph 构建 solver DTO，返回 solver evidence / placement updates | 持久保存 MBD session、shape、NamedShape、ElementMap 或完整 BREP |
| `swapJCS` | request-local DTO ordering，保持 graph 不变 | 直接修改前端 DocumentObject graph 或跨请求缓存 JCS |
| oracle | 每个基础 DistanceType 至少有 focused fixture 和 FreeCADCmd expected | 只靠当前 fixture 输出、bbox、shape 数量或手写 expected 宣称支持 |

## 纳入范围

| scope | DistanceType | FreeCAD 映射 |
| --- | --- | --- |
| point basic | `PointPoint` | 零距离 `ASMTSphericalJoint`；非零 `ASMTSphSphJoint.distanceIJ` |
| edge basic | `LineLine` | `ASMTRevCylJoint.distanceIJ` |
| point-edge basic | `PointLine` | `ASMTCylSphJoint.distanceIJ` |
| face basic | `PlanePlane` | `ASMTPlanarJoint.offset` |
| point-face basic | `PointPlane` | `ASMTPointInPlaneJoint.offset` |
| edge-face basic | `LinePlane` | `ASMTLineInPlaneJoint.offset` |

## 排除范围

| 排除项 | 原因 | 重新打开条件 |
| --- | --- | --- |
| `LineCircle` / `CircleCircle` | 依赖 `getEdgeRadius()` | 第二批半径类 DistanceType package |
| `PlaneCylinder` / `PlaneSphere` / `CylinderCylinder` / `CylinderSphere` / `PointCylinder` / `PointSphere` | 依赖 `getFaceRadius()` | 第二批半径类 DistanceType package |
| `PointCurve` / `CurvePlane` / default `Other` | FreeCAD 仍有 TODO 或 fallback 语义，需要单独判断 | 单独 curve/default package |
| GUI/session/full transaction | 超出 stateless cad-core request boundary | 单独 Assembly protocol package |

## 状态字典

| 状态 | 含义 | 使用条件 |
| --- | --- | --- |
| `supportedBaseline` | 已有能力，只作为输入基线 | 不由本包修改 |
| `backendGap` | FreeCAD 有明确语义且 cad-core 当前不等价 | 必须有源码依据和 current mismatch |
| `notCollected` | oracle 尚未采集 | 必须在 S6 变成 fixture / expected 任务或继续留作下一包 |
| `releaseGate` | 实现后发布前必须同步的测试 / capability / docs | 必须有验收命令 |
| `nonGoal` | 本包明确不做 | 必须写用户 / 协议行为和 reopen 条件 |
| `supported` | 已实现并通过验收 | 只能在代码和 expected 全部通过后使用 |

## 必须回写的矩阵行

- `DTC-SCOPE-001` 当前 scalar Distance baseline。
- `DTC-SCOPE-002..005` 基础 DistanceType backend gap。
- `DTC-SCOPE-006..007` oracle / capability release gate。
- `DTC-SCOPE-008..009` radius-bearing / curve / GUI 边界。

## 验收标准

```bash
git status --short
rg -n 'joint.jointType == "Distance"|ASMTSphSphJoint|distanceIJ = joint.distance' cad-core/src/assembly/joint_solver.cpp
rg -n 'DistanceType::PointPoint|DistanceType::LineLine|DistanceType::PlanePlane|DistanceType::PointPlane|DistanceType::LinePlane|DistanceType::PointLine' src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/App/AssemblyUtils.cpp
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

完成条件：

- 本文件保留 `待执行` 直到上述命令输出被记录到后续执行提交或方案验收中。
- scope / nonGoal / blocker / backend gap 矩阵必须存在，并且包含本文件列出的 rows。
- 不允许把本包写成完整 Assembly solver 或完整 Distance geometry 支持。

## 非目标

- 不编辑 cad-core 代码。
- 不采集 native expected。
- 不修改已收口的 Screw/RackPinion、Gears/Belt 或 Cylindrical 包。
