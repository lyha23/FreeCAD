# 【已实现】C9-M3 S4 DefaultPlanarBranch 批量 oracle 复审

## 目标

批量处理 FreeCAD default planar branch。已有 expected 先复审；缺 expected 的同源 DistanceType 组合必须采集 native oracle 或保留 `notCollected`，不得用单个 `PlaneCone` / `Other` 推断整族支持。

## FreeCAD 依据

- `AssemblyUtils.cpp::getDistanceType()` 会先分类 `PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`、`CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other`。
- `AssemblyObject.cpp::makeMbdJointDistance()` 对未显式处理的 `DistanceType` 进入 `default`，创建 `ASMTPlanarJoint`，写入 `offset = getJointDistance(joint)`。

## S4 live baseline

- 执行目录：`/home/user/Chili3DProject/FreeCAD`。
- 起始 HEAD：`213583d369`（`213583d369 docs: 关闭 C9-M3 S3 PointCurve 复审`）。
- 起始 `git -c core.quotepath=false status --short -uall` 无输出。
- S4 执行前队列首项为本文件；S4 关闭后只应剩余 S5-S6。

## 范围

| scope | default branch family | S4 输出 |
| --- | --- | --- |
| `C9M3-SCOPE-201` | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` | 复审 checked-in expected 与 current behavior。 |
| `C9M3-SCOPE-202` | cone / sphere / torus / point / line / curve 扩面 | 采集 native expected 或保留 `notCollected`。 |
| `C9M3-SCOPE-203` | default branch solver DTO | 判定是否由 S6 补 `ASMTPlanarJoint` + `offset`。 |

## S4 关闭证据

### existing expected

四条 checked-in expected 均存在，且 native FreeCAD oracle 均为 solved：`native_solver.return_code=0`、`solver_adapter.status=solved`、`unsupported_joints=[]`。FreeCAD source authority 是 `AssemblyObject.cpp::makeMbdJointDistance()` 的 `default` 分支：创建 `ASMTPlanarJoint` 并写 `offset = getJointDistance(joint)`；四个 fixture 的 `Distance` 均为 `1.5`，因此 S6 的候选 DTO 是 `ASMTPlanarJoint` + `offset=1.5`。

| case | native expected / source evidence | native placement evidence | current cad-core |
| --- | --- | --- | --- |
| `PlaneCone` | expected `distance_type=PlaneCone`、`distance=1.5`、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`；source default solver class 为 `ASMTPlanarJoint` | `ComponentB` `assembly_set_placement` Base `[7.0, 0.0, 1.5]` | `/tmp/c9m3-s4-plane-cone.json` 为 `unsupported_assembly_solver`，message `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`；tests/source guard 仍断言无 `solver_joint_class` 且 reason `default_boundary_not_mapped` |
| `LineCylinder` | expected `distance_type=LineCylinder`、`distance=1.5`、`jcs_swapped_for_solver=true`、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`；source default solver class 为 `ASMTPlanarJoint` | `ComponentB` `assembly_set_placement` Base `[6.0, 0.0, -1.5]` | `/tmp/c9m3-s4-line-cylinder.json` 为 `unsupported_assembly_solver`，message `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`；tests/source guard 仍断言 `default_boundary_not_mapped` |
| `CurvePlane` | expected `distance_type=CurvePlane`、`distance=1.5`、`jcs_swapped_for_solver=true`、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`；source default solver class 为 `ASMTPlanarJoint` | `ComponentB` `assembly_set_placement` Base `[6.0, 0.0, -1.5]` | `/tmp/c9m3-s4-curve-plane.json` 为 `unsupported_assembly_solver`，message `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`；tests/source guard 仍断言 `default_boundary_not_mapped` |
| `Other` | expected `distance_type=Other`、`distance=1.5`、`jcs_swapped_for_solver=true`、`distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`；source default solver class 为 `ASMTPlanarJoint` | `ComponentB` `assembly_set_placement` Base `[6.0, 0.0, 1.5]` | `/tmp/c9m3-s4-other.json` 为 `unsupported_assembly_solver`，message `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`；tests/source guard 仍断言 `default_boundary_not_mapped` |

四条 expected 仍带 `known_gap=DTE-BLOCK-006/DTE-NG-003` 与 `nonGoal.ids=["DTE-NG-003"]`，delete condition 是后续 DistanceType scope 接受 FreeCAD default/TODO 行为、cad-core 实现并用 focused tests/capability docs 显式发布。S4 不改 expected metadata；S5/S6 消费该发布/删除条件。

### missing oracle

`cad-core/fixtures/c3m6` 下未发现 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 对应 input 或 expected。S4 未采集、未刷新 expected，也未把这些缺 oracle 行写为 backendGap；统一路由为 `notCollected` / `native_oracle_required`，后续必须先补 native oracle 才能进入实现或发布判断。

## S4 判定

- `C9M3-SCOPE-201`：expected-backed mismatch。`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` native expected 已 solved 并有 placement writeback；current cad-core 单 fixture recompute 仍为 `unsupported_assembly_solver`，tests/source guard 仍保留 `default_boundary_not_mapped`。只路由到 S6 `backend_gap_candidate`。
- `C9M3-SCOPE-202`：缺 input/expected 的扩面族只关闭为 `notCollected` / `native_oracle_required`，不写 backendGap。
- `C9M3-SCOPE-203`：default solver DTO 由 FreeCAD source 指向 `ASMTPlanarJoint` + `offset`，但只对四条 expected-backed mismatch 形成 S6 candidate；缺 oracle 族保持 `notCollected`。

## 必须回写的矩阵行

- `C9M3-SCOPE-201..203`
- `C9M3-BLOCKER-401`
- `C9M3-BG-201..203`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
test -f cad-core/fixtures/c3m6/expected/assembly-distance-plane-cone-default-boundary.freecad.json
test -f cad-core/fixtures/c3m6/expected/assembly-distance-line-cylinder-default-boundary.freecad.json
test -f cad-core/fixtures/c3m6/expected/assembly-distance-curve-plane-default-boundary.freecad.json
test -f cad-core/fixtures/c3m6/expected/assembly-distance-other-default-boundary.freecad.json
rg -n 'PlaneCone|CylinderCone|ConeCone|ConeTorus|ConeSphere|PointCone|PointTorus|LineCylinder|LineSphere|LineCone|LineTorus|CurvePlane|CurveCylinder|CurveSphere|CurveCone|CurveTorus|Other|default_or_todo_boundary|ASMTPlanarJoint' src/Mod/Assembly/App cad-core/fixtures/c3m6 cad-core/tests/test_p8_features.py cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- existing expected 和 missing expected 分开记录。
- 对每个 accepted default branch case，写明 native solver class / scalar / marker / placement evidence。
- 对每个未采 case，写明 `notCollected` 和后续采集条件，不写 backendGap。
- 如果 current cad-core 缺 `ASMTPlanarJoint` mapping，只把 expected-backed rows 路由 S6。

## 非目标

- 不把 default branch 全量产品化为一句 capability 文案。
- 不修改 expected 来匹配 current unsupported output。
- 不跨入 GUI/session 或 persistent solver state。
