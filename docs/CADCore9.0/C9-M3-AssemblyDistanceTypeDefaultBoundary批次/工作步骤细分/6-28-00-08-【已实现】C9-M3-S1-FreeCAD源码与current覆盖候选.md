# 【已实现】C9-M3 S1 FreeCAD 源码与 current 覆盖候选

## 目标

复核 `DistanceType` 的 FreeCAD source authority、current cad-core 落点、checked-in expected inventory 和 diagnostics guard。S1 只形成 source candidates，不把候选直接写成 supported 或 backendGap。

## S1 live baseline

- 执行目录：`/home/user/Chili3DProject/FreeCAD`。
- 起始 HEAD：`8f209aab54`（`8f209aab54 docs: 关闭 C9-M3 S0 基线冻结`）。
- 起始 `git -c core.quotepath=false status --short -uall` 无输出。
- S1 执行前队列首项为本文件；S1 关闭后只应剩余 S2-S6。

## FreeCAD 依据

| 语义 | 源码 | S1 要确认 |
| --- | --- | --- |
| DistanceType 分类 | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | Vertex/Edge/Face 与 primitive type 如何进入 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 等。 |
| PointCurve solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointCurve` 是否明确创建 `ASMTPointInPlaneJoint` 并写 `offset`。 |
| default solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | default 是否创建 `ASMTPlanarJoint` 并写 `offset`。 |
| current DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`resolveDistanceJointMapping()`、`unsupportedReasonForOndselJoint()` 当前如何处理 diagnostic/default。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | checked-in expected 仍保留 native oracle 字段还是只保留 diagnostic metadata。 |

## S1 source authority 结论

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 是 `DistanceType` 分类 source authority。它读取 `Reference1` / `Reference2` 的 Vertex / Edge / Face 类型，必要时调用 `swapJCS(joint)` 把 line、circle、plane、cylinder、cone、torus、sphere 或 edge / face 放到 FreeCAD solver 期望侧，再返回分类。
- Vertex/Vertex 直接进入 `PointPoint`。Edge/Edge 中 line 优先，得到 `LineLine` 或 `LineCircle`；circle 对 circle 得到 `CircleCircle`；其他 Edge/Edge 走 `Other`。Face/Face 中 plane 优先得到 `PlanePlane`、`PlaneCylinder`、`PlaneSphere`、`PlaneCone`、`PlaneTorus`；cylinder 优先得到 `CylinderCylinder`、`CylinderSphere`、`CylinderCone`、`CylinderTorus`；cone 优先得到 `ConeCone`、`ConeTorus`、`ConeSphere`；torus 优先得到 `TorusTorus`、`TorusSphere`；sphere/sphere 得到 `SphereSphere`；未覆盖组合走 `Other`。
- Vertex/Face 会把 Face 放到第一引用并得到 `PointPlane`、`PointCylinder`、`PointSphere`、`PointCone`、`PointTorus`。Edge/Face 会把 Face 放到第一引用；line edge 进入 `LinePlane`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`，非 line edge 按 FreeCAD 注释“other curves we consider them as planes for now”进入 `CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。Vertex/Edge 会把 Edge 放到第一引用；line edge 得到 `PointLine`，非 line edge 按 FreeCAD 注释“point in plane-of-the-curve”得到 `PointCurve`。未命中组合返回 `Other`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 是 solver mapping source authority。`PointCurve` case 创建 `ASMTPointInPlaneJoint` 并写 `offset = getJointDistance(joint)`；`default` case 创建 `ASMTPlanarJoint` 并写 `offset = getJointDistance(joint)`。
- cad-core 当前只固化 request-local evidence 与 guard：`cad-core/src/assembly/joint_solver.cpp::classifyDistanceType()` 镜像 FreeCAD 分类和 swap 顺序；`resolveDistanceJointMapping()` 已给 `PointCurve` 写 `ASMTPointInPlaneJoint` / `offset`，但 `unsupportedReasonForOndselJoint()` 仍返回 `point_curve_diagnostic_boundary`；`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 等 default branch 缺 `solverJointClass` 时仍以 `default_boundary_not_mapped` 暴露。
- `cad-core/tools/collect_freecad_expected.py` 仍把 `PointCurve` 和 default/TODO branch expected 标为 diagnostic review metadata；checked-in expected 中 `assembly-distance-point-curve-real-solver`、`assembly-distance-plane-cone-default-boundary`、`assembly-distance-line-cylinder-default-boundary`、`assembly-distance-curve-plane-default-boundary`、`assembly-distance-other-default-boundary` 均保留 `DTE-NG-003` / known-gap 或 nonGoal metadata。S1 不改 expected。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 继续发布并校验 `deferred_diagnostic_cases=["PointCurve"]` 和 `default_or_todo_boundaries`；`cad-core/tests/test_p8_features.py` 继续把 `PointCurve` 锁在 `point_curve_diagnostic_boundary`，把 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 锁在 `default_boundary_not_mapped`。

## 必须回写的矩阵行

- `C9M3-SRC-101..404`
- `C9M3-SCOPE-101..404` 的 source linkage
- `C9M3-BLOCKER-101`

## S1 回写结果

- `矩阵/c9m3_distance_type_default_boundary_source_candidates.tsv` 已把 `C9M3-SRC-101..404` 固化为 FreeCAD classification、FreeCAD solver mapping、cad-core DTO / guard、collector metadata、capability/tests/expected inventory 的候选证据。
- `矩阵/c9m3_distance_type_default_boundary_scope_review_matrix.tsv` 已把 `C9M3-SCOPE-101..404` 重新链接到对应 source candidates；S1 不改变 S3/S4/S5/S6 owner step。
- `矩阵/c9m3_distance_type_default_boundary_blocker_queue.tsv` 已关闭 `C9M3-BLOCKER-101`，下一项仍是 S2 `C9M3-BLOCKER-201`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'DistanceType|getDistanceType|makeMbdJointDistance|PointCurve|PlaneCone|LineCylinder|CurvePlane|ASMTPointInPlaneJoint|ASMTPlanarJoint|default_or_todo_boundary' src/Mod/Assembly/App cad-core/src/assembly cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6/expected docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- source candidates 覆盖 FreeCAD 分类、FreeCAD solver mapping、cad-core DTO / guard、collector / fixture、capability tests。
- 每条 source candidate 都有 source evidence、cad-core landing 和 owner step。
- S1 不采集 oracle、不改 expected、不改 C++，也不把 candidate 标成 backendGap。
- S1 关闭时只改文档 / 矩阵 / step 文件名；S2-S6 保持 pending。

## 非目标

- 不设计新的 DistanceType 枚举。
- 不引入非 FreeCAD 的几何猜测分类。
- 不处理 non-AssemblyLink primitive frame DTO。
- 不采集 oracle、不改 `cad-core` C++、fixtures、expected 或 tests。
