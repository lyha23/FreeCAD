# C9-M3 S3 PointCurve 平面化 oracle 复审

## 目标

围绕 `PointCurve` 单独关闭 evidence：读取 checked-in native expected，比较 current cad-core 输出，判定它是 expected-backed current match、backend gap，还是仍需保留 diagnostic boundary。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：Vertex + 非 line edge 进入 `DistanceType::PointCurve`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：`PointCurve` 创建 `ASMTPointInPlaneJoint`，`offset = getJointDistance(joint)`。
- `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()`：当前已能给 `PointCurve` 设置 `ASMTPointInPlaneJoint` / `offset`。
- `cad-core/src/assembly/joint_solver.cpp::unsupportedReasonForOndselJoint()`：当前仍有显式 `point_curve_diagnostic_boundary` guard。

## 范围

| scope | fixture / expected | S3 判定 |
| --- | --- | --- |
| `C9M3-SCOPE-101` | `assembly-distance-point-curve-real-solver` | checked-in expected 和 current runtime 比较。 |
| `C9M3-SCOPE-302` | diagnostics guard | 若移除 PointCurve guard，unsupported JointType、missing marker、default boundary 仍需可见。 |

## 必须回写的矩阵行

- `C9M3-SCOPE-101`
- `C9M3-BLOCKER-301`
- `C9M3-BG-101`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
test -f cad-core/fixtures/c3m6/expected/assembly-distance-point-curve-real-solver.freecad.json
rg -n 'assembly-distance-point-curve-real-solver|PointCurve|point_curve_diagnostic_boundary|ASMTPointInPlaneJoint|offset' cad-core/fixtures/c3m6 cad-core/tests/test_p8_features.py cad-core/src/assembly/joint_solver.cpp docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 记录 expected 中 native solver class、offset、placement update / diagnostic route。
- 明确 current cad-core 是 diagnostic-only、current match，还是 expected-backed mismatch。
- 如果 mismatch 存在，只路由为 S6 `backend_gap_candidate`；S3 不直接改 C++。
- diagnostics guard 的后续责任写入 S5/S6。

## 非目标

- 不把全部 default branch 跟随 PointCurve 一起支持。
- 不用 ellipse / curve 几何形态猜测 marker placement。
- 不刷新 unrelated expected。
