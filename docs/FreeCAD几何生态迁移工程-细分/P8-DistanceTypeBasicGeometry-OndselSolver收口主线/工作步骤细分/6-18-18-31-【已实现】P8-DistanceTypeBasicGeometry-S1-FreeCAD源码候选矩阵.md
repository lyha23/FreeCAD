# 【已实现】P8 DistanceTypeBasicGeometry S1 FreeCAD 源码候选矩阵

## 目标

锁定本包的 FreeCAD source authority、cad-core mismatch evidence、oracle route 和 publication route，并把它们写入 source candidates TSV。S1 只建立候选，不把任何候选提升为 `supported`。

## 源码候选复核状态

2026-06-18 live 复核通过，S1 只关闭 FreeCAD / cad-core source candidates 的 evidence 与 route；S2-S6、oracle、cad-core 实现、capability 发布和 blocker 仍保持待执行。

baseline：

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`93c7e867b9`
- `git log -1 --oneline`：`93c7e867b9 docs: 完成P8 DistanceType S0基线复核`
- `git -c core.quotepath=false status --short -uall`：开始复核时无输出，工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：开始复核时队列首项为 `6-18-18-31-P8-DistanceTypeBasicGeometry-S1-FreeCAD源码候选矩阵.md`，S2-S6 仍为 pending。

关键结论：

- `DTC-CAND-001..016` 已全部复核并保留为 evidence / route 行，不把任何 scope 改为 `supported`。
- `DTC-CAND-003..008` 覆盖六个基础映射：`PointPoint`、`LineLine`、`PlanePlane`、`PointPlane`、`LinePlane`、`PointLine`。
- `DTC-CAND-009..010` 保留 radius-bearing 与 curve/default 边界：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`CylinderSphere`、`PointCurve`、`Other` 只作为后续包证据。
- `DTC-CAND-011..013` 记录当前 cad-core scalar-only gap：DTO、`makeOndselJointOfType()` 和 solver JSON 都没有 DistanceType 专属 evidence。
- `DTC-CAND-014..016` 记录 collector、focused tests 和 C ABI capability route；这些是后续 S5-S6 闸门，不在 S1 关闭。

关键 `rg` 证据：

```text
src/Mod/Assembly/App/AssemblyUtils.h:83-128 DistanceType lists PointPoint LineLine radius face variants PointPlane LinePlane PointLine PointCurve Other.
src/Mod/Assembly/App/AssemblyUtils.cpp:160-371 getDistanceType reads Reference1/Reference2 element type and returns PointPoint LineLine PlanePlane PointPlane LinePlane PointLine plus boundary types; swapJCS appears on ordering branches at 173, 203, 293, 315, 357.
src/Mod/Assembly/App/AssemblyObject.cpp:1190-1191 JointType::Distance calls makeMbdJointDistance(joint).
src/Mod/Assembly/App/AssemblyObject.cpp:1259-1386 makeMbdJointDistance maps PointPoint, LineLine, PlanePlane, PointPlane, LinePlane and PointLine to distinct Ondsel joint classes and distanceIJ/offset fields.
src/Mod/Assembly/App/AssemblyObject.cpp:1277-1353 radius-bearing mappings add getEdgeRadius/getFaceRadius; 1389-1403 keep PointCurve/default fallback separate.
cad-core/include/cad_core/assembly/joint_solver.h:45-77 JointConstraint lacks distanceType, primitive, solver class, distanceIJ and offset fields.
cad-core/src/assembly/joint_solver.cpp:615-618 Distance currently always creates ASMTSphSphJoint and writes distanceIJ from joint.distance.
cad-core/src/assembly/assembly_utils.cpp:88-147 solverJointJson exports scalar distance evidence but not distance_type, solver_joint_class, distance_ij or offset.
cad-core/tools/collect_freecad_expected.py:1403-1415 collector solver_joints currently record references and scalar distance only.
cad-core/src/adapters/c_api/c_api.cpp:150-174 and 681-683 publish JointType-level Distance capability without a basic DistanceType split.
```

## FreeCAD 依据

| 轴 | 源码 | 必看语义 |
| --- | --- | --- |
| 枚举全集 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` | 区分基础 point / line / plane、radius-bearing、curve/default |
| 分类规则 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `Vertex` / `Edge` / `Face` 组合，line / face 的 `swapJCS()` 顺序 |
| solver 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | 每个 `DistanceType` 映射到不同 Ondsel joint class 和 `distanceIJ` / `offset` |
| JointType 分发 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Distance` 应进入 `makeMbdJointDistance()` |
| radius 延后 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getEdgeRadius()`、`getFaceRadius()` | 第二批才处理 radius-bearing 类型 |

## 扫描轴

- `Reference1` / `Reference2` 的 subname element kind：`Vertex`、`Edge`、`Face`。
- edge primitive：本包只接受 `GeomAbs_Line`。
- face primitive：本包只接受 `GeomAbs_Plane`。
- `swapJCS(joint)` 是否需要在 CAD Core 中变成 request-local DTO ordering。
- `makeMbdJointDistance()` 的 resolved Ondsel class 与 scalar field：`distanceIJ` 或 `offset`。
- 当前 cad-core 是否仍固定把 `Distance` 映射为 `ASMTSphSphJoint`。

## candidate TSV 字段

```text
candidate_id	source_file	freecad_symbol	semantic_axis	source_evidence	cad_core_landing	scope_hint	next_step
```

## 必须回写的矩阵行

- `DTC-CAND-001..016` 必须存在。
- `DTC-CAND-003..008` 必须分别覆盖 6 个基础 DistanceType 映射。
- `DTC-CAND-009..010` 必须保留半径类 / curve-default 作为边界证据，不得删除。

## 验收标准

```bash
rg -n 'DTC-CAND-00[1-9]|DTC-CAND-01[0-6]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
rg -n 'PointPoint|LineLine|PlanePlane|PointPlane|LinePlane|PointLine' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
rg -n 'LineCircle|CircleCircle|PlaneCylinder|CylinderSphere|PointCurve|Other' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
```

完成条件：

- 每个候选都有 FreeCAD source file、symbol、source evidence、cad-core landing、scope hint 和 next step。
- candidate TSV 只表达 evidence 和 route，不把 `DTC-SCOPE-*` 改成 `supported`。
- 已复核：FreeCAD 源码与 S0 口径一致，S1 只关闭 source candidates；S2 仍为下一队列首项。

## 非目标

- 不实现代码。
- 不采集 native expected。
- 不补 radius extraction。
