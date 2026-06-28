# C9-M4 S4 PointLineSurface 族 native oracle 复审

## 目标

围绕 Vertex / Face 与 line Edge / Face default branch 批量关闭 oracle evidence：处理 `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：Vertex / Face 形成 `PointCone`、`PointTorus`；line Edge / Face 形成 `LineSphere`、`LineCone`、`LineTorus`，并按 FreeCAD 顺序 `swapJCS(joint)`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：这些未显式 case 进入 `default`，创建 `ASMTPlanarJoint`，`offset = getJointDistance(joint)`。

## 范围

| scope | DistanceType | S4 判定 |
| --- | --- | --- |
| `C9M4-SCOPE-201` | `PointCone`、`PointTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-202` | `LineSphere`、`LineCone`、`LineTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-601` | diagnostics guard | missing marker / unsupported diagnostics 不能被隐藏。 |

## 必须回写的矩阵行

- `C9M4-SCOPE-201..202`
- `C9M4-BLOCKER-401`
- `C9M4-BG-201`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PointCone|PointTorus|LineSphere|LineCone|LineTorus|ASMTPlanarJoint|default_or_todo_boundary|notCollected|backend_gap_candidate' cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 每个 row 有 input fixture / native expected / current comparison / final route。
- `swapJCS` 和 solver reference ordering 必须写入 expected comparison。
- 若 current mismatch 存在，只路由为 S6 `backend_gap_candidate`；S4 不直接改 C++。
- 采集失败或缺 fixture 时保持 `notCollected`，不继承 C9-M3 supported。

## 非目标

- 不把 Face / Face cone 或 Curve / Surface families 混入 S4。
- 不靠 line / surface 类型名直接发布 supported。
- 不引入 persistent solver state。
