# P8 DistanceTypeBasicGeometry S1 FreeCAD 源码候选矩阵

## 目标

锁定本包的 FreeCAD source authority、cad-core mismatch evidence、oracle route 和 publication route，并把它们写入 source candidates TSV。S1 只建立候选，不把任何候选提升为 `supported`。

## FreeCAD 依据

| 轴 | 源码 | 必看语义 |
| --- | --- | --- |
| 枚举全集 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` | 区分基础 point / line / plane、radius-bearing、curve/default |
| 分类规则 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `Vertex` / `Edge` / `Face` 组合，line / face 的 `swapJCS()` 顺序 |
| solver 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | 每个 `DistanceType` 映射到不同 Ondsel joint class 和 `distanceIJ` / `offset` |
| JointType 分发 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Distance` 应进入 `makeMbdJointDistance()` |
| radius 延后 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getEdgeRadius()`、`getFaceRadius()` | 第二批才处理 radius-bearing 类型 |

## 扫描轴

- `Reference1` / `Reference2` 的 subname element kind：`Vertex`、`Edge`、`Face`。
- edge primitive：本包只接受 `GeomAbs_Line`。
- face primitive：本包只接受 `GeomAbs_Plane`。
- `swapJCS(joint)` 是否需要在 CAD Core 中变成 request-local DTO ordering。
- `makeMbdJointDistance()` 的 resolved Ondsel class 与 scalar field：`distanceIJ` 或 `offset`。
- 当前 cad-core 是否仍固定把 `Distance` 映射为 `ASMTSphSphJoint`。

## candidate TSV 字段

```text
candidate_id	source_file	freecad_symbol	semantic_axis	source_evidence	cad_core_landing	scope_hint	next_step
```

## 必须回写的矩阵行

- `DTC-CAND-001..016` 必须存在。
- `DTC-CAND-003..008` 必须分别覆盖 6 个基础 DistanceType 映射。
- `DTC-CAND-009..010` 必须保留半径类 / curve-default 作为边界证据，不得删除。

## 验收标准

```bash
rg -n 'DTC-CAND-00[1-9]|DTC-CAND-01[0-6]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
rg -n 'PointPoint|LineLine|PlanePlane|PointPlane|LinePlane|PointLine' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
rg -n 'LineCircle|CircleCircle|PlaneCylinder|CylinderSphere|PointCurve|Other' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_source_candidates.tsv
```

完成条件：

- 每个候选都有 FreeCAD source file、symbol、source evidence、cad-core landing、scope hint 和 next step。
- candidate TSV 只表达 evidence 和 route，不把 `DTC-SCOPE-*` 改成 `supported`。
- 若发现 FreeCAD 源码与本文口径不一致，先修正矩阵和 S0 声明，不进入 S2。

## 非目标

- 不实现代码。
- 不采集 native expected。
- 不补 radius extraction。
