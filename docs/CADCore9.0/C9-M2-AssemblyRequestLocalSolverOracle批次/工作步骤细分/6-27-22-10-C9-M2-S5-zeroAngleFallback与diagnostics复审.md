# C9-M2 S5 zeroAngleFallback 与 diagnostics 复审

## 目标

采集或确认 zero Angle fallback native oracle，并验证 C9-M2 扩面不会把 unsupported diagnostics 静默降级为 success。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()`
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getJointAngle()`
- `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`

## 必须复核

- FreeCAD `Angle=0` / `2pi` fallback 到 `ASMTParallelAxesJoint` 的 native expected。
- current cad-core exact zero Angle class fallback 是否匹配 native expected。
- `assembly-angle-zero-and-signed-current-real-solver` 是否需要 expected 采集或 fixture schema 更新。
- unsupported JointType、PointCurve boundary、缺 marker placement、缺 solver joint class 是否仍走 diagnostic。

## 必须回写的矩阵行

- `C9M2-SCOPE-301`
- `C9M2-SCOPE-302`
- `C9M2-BG-301`
- `C9M2-BG-302`
- `C9M2-BLOCKER-501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Angle|ASMTParallelAxesJoint|unsupported_assembly_solver|ondsel_solver_failed|assembly-angle-zero-and-signed-current-real-solver|PointCurve' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
git diff --check
```

S5 关闭时，zero Angle fallback 必须明确进入 expected-backed covered、backend_gap_candidate 或 known_gap_retained；unsupported diagnostics 必须保持可见。

## 非目标

- 不把 zero Angle current output 当作 native expected。
- 不把 unsupported diagnostic 静默转 success。
- 不修改 GUI/session 行为。
