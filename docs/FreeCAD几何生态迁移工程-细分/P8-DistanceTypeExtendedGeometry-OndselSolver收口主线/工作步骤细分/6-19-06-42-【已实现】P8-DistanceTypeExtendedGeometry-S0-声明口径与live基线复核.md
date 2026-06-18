# P8 DistanceTypeExtendedGeometry S0 声明口径与 live 基线复核【已实现】

## 目标

冻结本包的 supported claim、remaining boundary 和禁止声明。S0 只复核 live 文档、代码、capability、fixtures 和矩阵，不写 C++，不采 oracle。

## live 基线

- 当前仓库基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5f279b20c8`，最新提交为 `5f279b20c8 docs: 关闭P8扩展DistanceType索引入口`；复核开始时工作区仅见 unrelated `AGENTS.md` dirty，本步骤未编辑或暂存它。
- `P8-DistanceTypeBasicGeometry-OndselSolver收口主线` 仍是已发布 baseline：`PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` 已作为 `distance_type_basic_geometry` supported 子集收口；本包不重做 basic expected 或实现。
- `P8-Assembly-Reference-JCS-MarkerPlacement收口主线` 仍是独立的 representative subshape marker placement 子集：`active_expected_count=15`，radius-bearing DistanceType、curve/default DistanceType、GUI/session、persistent solver state、connector-only shortcut 和非 identity `offsetPlc` 不属于本包已支持声明。
- 当前 cad-core extended 状态：`JointConstraint` 只有 `distanceType`、`solverJointClass`、`distanceIJ`、`offset` 等 basic 映射字段，尚无 edge/face radius evidence；`classifyDistanceType()` 只识别点 / 线 / 平面 basic cases；`resolveDistanceJointMapping()` 只映射 basic cases；C ABI capability 只发布 `distance_type_basic_geometry`，`remaining_radius_gaps` 仍只覆盖 8 个 radius cases，尚未表达完整 extended matrix。
- 本包后续实施前必须先把 FreeCAD `DistanceType` 剩余枚举全部分类：显式 radius / torus / sphere / `PointCurve` cases 进入 oracle / DTO / mapping 复核，cone、line-surface、curve-face 和 `Other` 进入 default/TODO boundary 或 nonGoal；不得用单 fixture 或当前 cad-core 输出替代 FreeCAD oracle。

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
