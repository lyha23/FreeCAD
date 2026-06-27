# C9-M3 S4 DefaultPlanarBranch 批量 oracle 复审

## 目标

批量处理 FreeCAD default planar branch。已有 expected 先复审；缺 expected 的同源 DistanceType 组合必须采集 native oracle 或保留 `notCollected`，不得用单个 `PlaneCone` / `Other` 推断整族支持。

## FreeCAD 依据

- `AssemblyUtils.cpp::getDistanceType()` 会先分类 `PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`、`CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other`。
- `AssemblyObject.cpp::makeMbdJointDistance()` 对未显式处理的 `DistanceType` 进入 `default`，创建 `ASMTPlanarJoint`，写入 `offset = getJointDistance(joint)`。

## 范围

| scope | default branch family | S4 输出 |
| --- | --- | --- |
| `C9M3-SCOPE-201` | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` | 复审 checked-in expected 与 current behavior。 |
| `C9M3-SCOPE-202` | cone / sphere / torus / point / line / curve 扩面 | 采集 native expected 或保留 `notCollected`。 |
| `C9M3-SCOPE-203` | default branch solver DTO | 判定是否由 S6 补 `ASMTPlanarJoint` + `offset`。 |

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
