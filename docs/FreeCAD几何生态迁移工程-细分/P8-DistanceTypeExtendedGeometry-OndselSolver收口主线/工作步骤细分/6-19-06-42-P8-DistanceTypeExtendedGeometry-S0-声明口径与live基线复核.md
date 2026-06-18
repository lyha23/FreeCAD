# P8 DistanceTypeExtendedGeometry S0 声明口径与 live 基线复核

## 目标

冻结本包的 supported claim、remaining boundary 和禁止声明。S0 只复核 live 文档、代码、capability、fixtures 和矩阵，不写 C++，不采 oracle。

## 必须读取

- `AGENTS.md`
- `docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/6-18-18-29-P8-DistanceTypeBasicGeometry-OndselSolver收口主线总入口.md`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement收口主线总入口.md`
- `src/Mod/Assembly/App/AssemblyUtils.h`
- `src/Mod/Assembly/App/AssemblyUtils.cpp`
- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 输出

- 更新本步骤文档的 live baseline。
- 确认 `basic_distance_type` 已收口，不在本包重做。
- 确认本包必须覆盖所有 remaining DistanceType 的矩阵分类，而不是单 case。
- 若发现已有代码已经覆盖某些 extended cases，仍需进入 S1/S2 复核后才能裁决 supported。

## 验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
```

## 非目标

- 不修改 `cad-core/fixtures/c3m6/expected`。
- 不把 current cad-core output 当 FreeCAD oracle。
- 不把 curve/default branch 直接发布成 supported。
