# C9-M4 S5 CurveSurface 族 native oracle 复审

## 目标

围绕 non-line Edge / Face default branch 批量关闭 oracle evidence：处理 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`，并复核 `part_workbench.conic_curves.distance_type_publication` mirror。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：non-line Edge / Face 被作为 curve family，进入 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：这些未显式 case 进入 `default`，创建 `ASMTPlanarJoint`，`offset = getJointDistance(joint)`。

## 范围

| scope | DistanceType | S5 判定 |
| --- | --- | --- |
| `C9M4-SCOPE-301` | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` | native expected 与 current runtime 比较。 |
| `C9M4-SCOPE-501` | capability / conic publication mirror | accepted / missing rows 在 capability 和 conic mirror 中一致。 |
| `C9M4-SCOPE-601` | diagnostics guard | 未采 curve rows 继续可见。 |

## 必须回写的矩阵行

- `C9M4-SCOPE-301`
- `C9M4-BLOCKER-501`
- `C9M4-BG-301`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'CurveCylinder|CurveSphere|CurveCone|CurveTorus|conic_curves|distance_type_publication|ASMTPlanarJoint|default_or_todo_boundary|notCollected|backend_gap_candidate' cad-core/fixtures/c3m6 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/src/runtime/capability_contract.cpp cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 每个 curve row 有 input fixture / native expected / current comparison / final route。
- `part_workbench.conic_curves.distance_type_publication` 与 Assembly capability 保持一致，不隐藏 missing rows。
- 若 current mismatch 存在，只路由为 S6 `backend_gap_candidate`；S5 不直接改 C++。
- 采集失败或缺 fixture 时保持 `notCollected`，不继承 `CurvePlane` supported。

## 非目标

- 不实现完整 conic GUI 或 Sketcher full solver。
- 不靠 ellipse / curve 几何形态猜测 marker placement。
- 不刷新 unrelated expected。
