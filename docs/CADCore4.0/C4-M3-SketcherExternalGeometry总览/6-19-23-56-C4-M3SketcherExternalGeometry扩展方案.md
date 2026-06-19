# C4-M3 Sketcher 与 ExternalGeometry 扩展方案

## 目标

C4-M3 扩展 Sketcher 在前端 CAD 运行时所需的 solver-facing 和 ExternalGeometry 语义。它不迁移完整 GCS solver，只补 request-local 几何更新、约束状态、diagnostics、ExternalGeometry 生命周期、InternalShape 和 topo naming 相关能力。

## 当前基线

C3.0 已覆盖：

- conflict / redundancy / malformed / partial redundancy diagnostics。
- request-local DoF、dependent parameter group metadata。
- 常用约束的 geometry update 第一批。
- ExternalGeometry Frozen / Detached / Missing 与 native `ExternalGeo` request-side pool 第一批。
- InternalShape bounded face / open wire / WireJoiner full ledger 第一批。

## 目标内缺口

| 方向 | 说明 |
| --- | --- |
| 更多 solver-facing 关系 | 只补前端实际会发出的约束关系、状态和几何更新，不做完整 solver session |
| ExternalGeometry projection / intersection | 扩展外部 edge / face / vertex、投影、交线和 missing / frozen / detached 组合 |
| InternalShape 压力 case | self-intersection、open wire、bounded face、split / deleted / ambiguous relation 同时出现 |
| ReferenceShadow 复用 | 继续只允许单 subshape snapshot，不引入完整 BREP state |
| diagnostics policy | unsupported relation 必须有稳定 code、object、property、subname、target |

## 实施批次

| 批次 | 内容 | 验收重点 |
| --- | --- | --- |
| C4-M3-S1 | 约束关系审计 | 列出前端会发出的 constraint DTO，映射 FreeCAD `Sketch.cpp` / `SketchObjectConstraints.cpp` |
| C4-M3-S2 | ExternalGeometry projection / intersection | native oracle、link-list update、ReferenceShadow fallback、missing diagnostics |
| C4-M3-S3 | InternalShape stress fixtures | mixed bounded faces / open wires / self-intersection / split history |
| C4-M3-S4 | Solver-facing diagnostics 扩展 | unsupported / malformed / redundant / partial redundant / no fake profile |

## 非目标

- 不实现完整 GCS solver。
- 不保存 solver session。
- 不把 GUI sketch edit 行为迁入 cad-core。
- 不用 synthetic geometry 假装 profile ready。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

## 可执行包入口

- Sketcher 约束关系审计：`docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/工作步骤细分/6-20-00-07-C4-S6-M3-Sketcher约束关系审计.md`
- ExternalGeometry / InternalShape 压力包：`docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/工作步骤细分/6-20-00-08-【已实现】C4-S7-M3-ExternalGeometry-InternalShape压力包.md`
- non-goal 矩阵：`docs/CADCore4.0/矩阵/cadcore4_non_goal_registry.tsv`
