# P8 DistanceTypeBasicGeometry S3 ReferenceElement 分类与 JCS 顺序专项复审

## 目标

复审并实现基础 DistanceType 所需的 request-local reference classification：元素类型、基础几何 primitive、`DistanceType` 结果和 `swapJCS` 等价的 DTO ordering。S3 不发布 solver support，只为 S4 映射提供稳定输入。

## FreeCAD 依据

| 语义 | FreeCAD 入口 | 关键行为 |
| --- | --- | --- |
| 元素类型读取 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 读取 `Reference1` / `Reference2` 的 `Vertex`、`Edge`、`Face` |
| line 优先 | 同上 | edge-edge 中 line 不在第一侧时 `swapJCS(joint)` |
| face 优先 | 同上 | vertex-face / edge-face 中 face 不在第一侧时 `swapJCS(joint)` |
| plane 基础面 | 同上 | 本包只接受 `GeomAbs_Plane` face |
| graph 边界 | `docs/CADCore方案/00-CAD-Core抽取方案.md` | CAD Core 只做 request-local recompute，不持久修改 graph |

## 范围

| scope | 本步处理 | 不处理 |
| --- | --- | --- |
| `DTC-SCOPE-002` | `JointConstraint` 承载 `distanceType`、reference element kind、primitive、`jcsSwappedForSolver` | radius、curve、persistent graph mutation |
| `DTC-SCOPE-003` | `PointPoint` 分类证据 | S4 才创建 Ondsel joint |
| `DTC-SCOPE-004` | `LineLine` / `PointLine` 分类证据 | circle edge radius |
| `DTC-SCOPE-005` | `PlanePlane` / `PointPlane` / `LinePlane` 分类证据 | cylinder / sphere face radius |

## 必须回写的矩阵行

- `DTC-BLOCK-001`：从 P1 改为 S3 关闭，前提是 DTO / JSON evidence 已存在。
- `DTC-SCOPE-002`：只有在 focused tests 能看到 classification / swap evidence 后才能改为 `supported` 或 `supportedFoundation`。
- `DTC-BG-001`：关闭或指向剩余 code gap。

## 验收标准

本步执行时至少需要补以下代码和检查：

```bash
rg -n 'distanceType|distance_type|jcsSwappedForSolver|jcs_swapped_for_solver|reference.*element|primitive' cad-core/include/cad_core/assembly cad-core/src/assembly
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线
```

文档 / 矩阵验收：

```bash
rg -n 'DTC-BLOCK-001.*Closed|DTC-SCOPE-002.*supported|DTC-SCOPE-002.*supportedFoundation' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

关闭条件：

- `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` 的 classification 证据在 solver JSON 中可见。
- `swapJCS` 只表现为 request-local DTO ordering 和 `jcs_swapped_for_solver` 证据，不修改 graph。
- radius-bearing 和 curve/default 类型仍没有 capability publication。

## 非目标

- 不创建 Ondsel joint。
- 不采集 FreeCAD expected。
- 不实现 edge / face 半径提取。
