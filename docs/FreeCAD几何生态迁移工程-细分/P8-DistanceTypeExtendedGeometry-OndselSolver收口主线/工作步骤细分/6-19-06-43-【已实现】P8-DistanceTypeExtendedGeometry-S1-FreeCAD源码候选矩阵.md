# P8 DistanceTypeExtendedGeometry S1 FreeCAD 源码候选矩阵【已实现】

## 目标

复核并补全 source candidates：DistanceType enum、classification、radius helpers、ASMT switch、cad-core DTO / resolver / collector / tests / capability。S1 只建立 authority，不裁决 supported。

## live 基线

- 复核仓库：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=23e4f5c379`，最新提交为 `23e4f5c379 docs: 完成P8扩展DistanceType S0基线复核`。
- 复核开始时工作区仅见 unrelated `AGENTS.md` dirty；本步骤未编辑或暂存 `AGENTS.md`。
- 本步骤只更新 P8 ExtendedGeometry 包内文档和矩阵，不写 C++，不采 oracle，不修改 fixture / expected。

## 必须完成

- 对 `p8_distance_type_extended_geometry_source_candidates.tsv` 逐行复核。
- 确认 FreeCAD enum 中所有剩余 cases 都被 scope matrix 覆盖。
- 把 `/Users/li/...` 和 `/home/user/...` 路径差异统一理解为同一源码树的路径前缀差异；后续相对路径、类/函数和关键短句才是依据。
- 对 cad-core 当前只支持 basic classifier 的事实做 live 记录。

## live 结论

- `src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` 当前列出 37 个 enum 值；`PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` 属于已发布 basic baseline，其余 31 个值全部进入本包矩阵审计。
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 先读 `Reference1` / `Reference2` 的 element kind、element name 和 linked object，再用 `swapJCS(joint)` 把 solver 侧顺序调整为 line / circle、plane / cylinder / cone / torus / sphere、face before vertex/edge、edge before vertex；非 line edge + face 进入 `Curve*`，非 line edge + vertex 进入 `PointCurve`，末尾返回 `Other`。
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getEdgeRadius()` 只对 `GeomAbs_Circle` 返回 `sf.Circle().Radius()`；`getFaceRadius()` 只对 `GeomAbs_Cylinder` 和 `GeomAbs_Sphere` 返回半径，plane / cone / torus / other face 返回 0。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 显式 switch 覆盖 edge-circle、plane-cylinder/sphere/torus、cylinder/sphere/torus、point-cylinder/sphere、PointCurve 等 cases；未显式覆盖的 cone、line-surface、curve-face 和 `Other` 统一进入 default `ASMTPlanarJoint.offset=getJointDistance(joint)`，不能在 S1 被写成 supported。
- `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 当前有 `distanceType`、`solverJointClass`、`distanceIJ`、`offset`、`jcsSwappedForSolver`，但没有 edge/face radius、scalar source 或 default-route evidence。
- `cad-core/src/assembly/joint_solver.cpp` 当前 `edgePrimitiveName()` / `facePrimitiveName()` 能标出 circle / cylinder / sphere / cone / torus primitive，`classifyDistanceType()` 仍只产出 basic `PointPoint`、`LineLine`、`PlanePlane`、`PointPlane`、`LinePlane`、`PointLine`；`resolveDistanceJointMapping()` 和 `cad-core/tools/collect_freecad_expected.py::resolve_fixture_distance_type()` 也只映射 basic cases。
- `cad-core/src/assembly/joint_solver.cpp::makeOndselDistanceJoint()` 已有 extended 需要的 ASMT constructor 分支，但还没有 source-backed extended scalar mapping；`cad-core/src/adapters/c_api/c_api.cpp::ondselSolverCapabilityJson()` 仍只发布 `distance_type_basic_geometry`，且 `remaining_radius_gaps` 只有 8 个 radius cases，不等于完整 extended matrix。

## remaining enum 覆盖

| 覆盖路径 | enum cases | S1 结论 |
| --- | --- | --- |
| `DTE-SCOPE-004` | `LineCircle`、`CircleCircle` | 已有 FreeCAD edge radius + `ASMTRevCylJoint.distanceIJ` authority；支持结论留给 S4/S5/S6 |
| `DTE-SCOPE-005` | `PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` | 已有 cylinder/sphere face radius authority；支持结论留给 S4/S5/S6 |
| `DTE-SCOPE-006` | `PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere` | 已有显式 switch authority；torus 半径 helper 返回 0 的行为需 native expected 裁决 |
| `DTE-SCOPE-007` | `PointCurve` | 已有显式 `ASMTPointInPlaneJoint.offset` authority；TODO-like 注释要求先采 oracle |
| `DTE-SCOPE-008` | `PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus` | 已覆盖为 default / TODO boundary，不裁决支持 |
| `DTE-SCOPE-009` | `CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other` | 已覆盖为 curve/default boundary，不裁决支持 |

## 输出

- 已更新 `矩阵/p8_distance_type_extended_geometry_source_candidates.tsv`，把 enum、`getDistanceType()` ordering、radius helpers、ASMT switch/default、cad-core DTO/classifier/mapping/oracle/capability landing 改为 live authority。
- 已更新 `矩阵/p8_distance_type_extended_geometry_scope_review_matrix.tsv`，明确 S1 只确认全 enum 覆盖路径，不改变任何 supported / notCollected / oracleFirst / defaultBoundary 结论。

## 验收

```bash
rg -n 'DistanceType|getDistanceType|getEdgeRadius|getFaceRadius|makeMbdJointDistance' src/Mod/Assembly/App/AssemblyUtils.h src/Mod/Assembly/App/AssemblyUtils.cpp src/Mod/Assembly/App/AssemblyObject.cpp
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
```

## 非目标

- 不写 C++。
- 不采 oracle。
- 不把 source candidate 直接改成 supported。
