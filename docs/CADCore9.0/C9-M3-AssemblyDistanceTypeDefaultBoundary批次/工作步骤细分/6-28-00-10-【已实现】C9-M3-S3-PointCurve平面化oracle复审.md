# 【已实现】C9-M3 S3 PointCurve 平面化 oracle 复审

## 目标

围绕 `PointCurve` 单独关闭 evidence：读取 checked-in native expected，比较 current cad-core 输出，判定它是 expected-backed current match、backend gap，还是仍需保留 diagnostic boundary。

## S3 live baseline

- 执行目录：`/home/user/Chili3DProject/FreeCAD`。
- 起始 HEAD：`696046ca6c`（`696046ca6c docs: 关闭 C9-M3 S2 范围准入路由`）。
- 起始 `git -c core.quotepath=false status --short -uall` 无输出。
- S3 执行前队列首项为本文件；S3 关闭后只应剩余 S4-S6。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`：Vertex + 非 line edge 进入 `DistanceType::PointCurve`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()`：`PointCurve` 创建 `ASMTPointInPlaneJoint`，`offset = getJointDistance(joint)`。
- `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()`：当前已能给 `PointCurve` 设置 `ASMTPointInPlaneJoint` / `offset`。
- `cad-core/src/assembly/joint_solver.cpp::unsupportedReasonForOndselJoint()`：当前仍有显式 `point_curve_diagnostic_boundary` guard。

## S3 关闭证据

- checked-in expected：`cad-core/fixtures/c3m6/expected/assembly-distance-point-curve-real-solver.freecad.json` 来自 FreeCADCmd `1.2.0 revision 20260519`，`native_solver.return_code=0`，`solver_adapter.status=solved`，`unsupported_joints=[]`。
- native solver DTO：`DistanceJoint` 的 `distance_type=PointCurve`，`jcs_swapped_for_solver=true`，`solver_joint_class=ASMTPointInPlaneJoint`，`offset=1.5`，`distance_type_mapping_status=mapped_s4_extended`，`distance_type_boundary=extended_mapping_pending_s5_oracle`。
- native placement：expected 中 `solver_adapter.placement_updates[0]` 写回 `ComponentB`，`action=assembly_set_placement`，`reason=assembly_solver_placement_writeback`，Placement Base 为 `[6.0008939064949205, 1.7215251713424476e-14, -0.0034709294137236]`。
- checked-in expected metadata 仍保留 `known_gap=DTE-BLOCK-006/DTE-NG-003` 和 `nonGoal.ids=["DTE-NG-003"]`；delete condition 是后续 DistanceType scope 接受 `PointCurve` 产品行为、cad-core 落 focused parity、capability docs 显式发布 `PointCurve`。
- current cad-core：已运行 `cd cad-core && ./cad-core recompute fixtures/c3m6/assembly-distance-point-curve-real-solver.json --output /tmp/c9m3-s3-pointcurve.current.json`；输出为 `unsupported_assembly_solver`，message 为 `Ondsel solver adapter keeps PointCurve DistanceType diagnostic until product acceptance`，`documentObjectUpdates=[]`。
- focused tests/source guard：`test_p8_features.py` 仍断言 `point_curve` 的 DTO 是 `ASMTPointInPlaneJoint` / `offset=1.5` / `mapped_s4_extended`，但 unsupported reason 是 `point_curve_diagnostic_boundary`；`joint_solver.cpp::unsupportedReasonForOndselJoint()` 在任何 `DistanceType=PointCurve` 时仍先返回该 guard。

## S3 判定

`C9M3-SCOPE-101` 判定为 expected-backed mismatch，路由到 S6 `backend_gap_candidate`：native expected 已有 solved placement writeback，current runtime 仍被 `point_curve_diagnostic_boundary` 截住且无 `documentObjectUpdates`。S3 不改 C++、fixture、expected 或 tests，也不直接发布 supported；S6 必须消费这个 candidate，决定移除 PointCurve guard 并同步 tests/capability/expected metadata，或保留 retained diagnostic 并写明产品边界。

`C9M3-SCOPE-302` 的 diagnostics guard 责任保留给 S5/S6：unsupported JointType、missing marker、missing grounded、未接受 default boundary 和未消费的 PointCurve diagnostic 仍需在 focused diagnostics 中可见。

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
