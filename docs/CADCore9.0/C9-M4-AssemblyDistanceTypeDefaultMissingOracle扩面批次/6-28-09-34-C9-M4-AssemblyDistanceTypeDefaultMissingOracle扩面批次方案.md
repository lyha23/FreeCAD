# C9-M4 Assembly DistanceType default missing oracle 扩面批次方案

## 背景

C9-M3 已把 `PointCurve` 和四条 checked-in default expected（`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`）推进到 expected-backed supported。当前 Assembly capability 仍保留 13 个 `default_or_todo_boundaries`，但这些行没有 input fixture / native expected，不能继承 C9-M3 的 supported 结论，也不能直接写成 backend gap。

## 批次策略

1. S0 冻结 live capability 和 forbidden claim：C9-M4 只处理缺 oracle default rows。
2. S1 复核 FreeCAD source authority 与 cad-core current landing，确认这些 rows 都来自同一 `getDistanceType()` / `makeMbdJointDistance()` default branch。
3. S2 把 13 个 rows 分为三组，明确哪些只做 oracle，哪些可以在 S6 消费。
4. S3 处理 Face / Face cone family：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`。
5. S4 处理 Point / Line + Surface family：`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`。
6. S5 处理 Curve + Surface family：`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。
7. S6 只消费 native expected 已存在且 current mismatch 的 rows；无法采集的 rows 保留 `notCollected`。

## 最小完整语义批次

本包不是单个 fixture probe。它覆盖同一 FreeCAD 调用链和同一 cad-core DTO / capability 边界：

- FreeCAD 分类：`AssemblyUtils.cpp::getDistanceType()`。
- FreeCAD solver mapping：`AssemblyObject.cpp::makeMbdJointDistance()` default branch。
- cad-core landing：`joint_solver.cpp`、collector、expected、focused tests、capability。
- publication：accepted rows 进入 supported；missing rows 保留在 `default_or_todo_boundaries`。

## 拆分理由

三组 S3-S5 按 FreeCAD 分类入口拆分，而不是按 fixture 名单随机拆分：

- Face / Face cone family 只涉及 surface pair ordering。
- Point / Line + Surface family 涉及 Vertex / Edge 与 Face 的 swapJCS 和 marker resolver。
- Curve + Surface family 涉及 non-line Edge 的 curve-as-plane default classification，同时影响 `part_workbench.conic_curves.distance_type_publication` mirror。

## 非目标

- 不重开 C9-M3 已 supported rows。
- 不把缺 oracle rows 写成 supported。
- 不靠 fixture 名称、bbox、几何排序或 adapter 文案绕过 native expected。
- 不实现 GUI/session/persistent solver state。
