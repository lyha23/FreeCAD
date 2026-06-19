# C4-M3 Sketcher / ExternalGeometry 总览入口

## 目标

C4-M3 只补 CAD Core 需要的 Sketcher solver-facing 输入、约束状态、diagnostics、几何更新入口、ExternalGeometry lifecycle 和 InternalShape / topo naming 相关语义，不复刻完整约束求解器。

## 子包

| 专题包 | 入口 | 队列 |
| --- | --- | --- |
| Sketcher Constraint-facing 扩展主线 | `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/6-20-00-25-C4-M3-SketcherConstraintFacing扩展主线总入口.md` | `docs/CADCore4.0/C4-M3-SketcherConstraintFacing扩展主线/工作步骤细分/` |
| ExternalGeometry / InternalShape 压力主线 | `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/6-20-00-26-C4-M3-ExternalGeometryInternalShape压力主线总入口.md` | `docs/CADCore4.0/C4-M3-ExternalGeometryInternalShape压力主线/工作步骤细分/` |

## 验收口径

总览包不把完整 GCS solver 列为缺口；子包只处理 solver-facing DTO、diagnostics、ExternalGeometry 和 topo naming 相关闭环。
