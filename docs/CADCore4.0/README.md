# CADCore4.0 专题包入口

本目录把 CADCore4.0 从一个大方案拆成多个专题包。根目录只做总览和全局矩阵索引；执行时进入具体专题包，读取该包根入口、方案、`矩阵/` 和 `工作步骤细分/`。

## 总览与全局矩阵

| 路径 | 用途 |
| --- | --- |
| `6-19-23-52-CADCore4.0总览方案.md` | 4.0 目标、边界、阶段和验收分层 |
| `矩阵/cadcore4_scope_review_matrix.tsv` | 4.0 阶段范围与准入矩阵 |
| `矩阵/cadcore4_blocker_queue.tsv` | 全局 blocker / step 索引 |
| `矩阵/cadcore4_source_candidates.tsv` | FreeCAD 源码依据和 cad-core 落点候选 |
| `矩阵/cadcore4_fixture_oracle_matrix.tsv` | oracle / fixture / expected 设计矩阵 |
| `矩阵/cadcore4_non_goal_registry.tsv` | 4.0 非目标与 deferred 边界 |
| `矩阵/cadcore4_validation_matrix.tsv` | 本轮、阶段、重型验收命令矩阵 |

## 专题包

| 专题包 | 根入口 | 队列 |
| --- | --- | --- |
| C4-M0 目标范围审计与矩阵重建 | `C4-M0-目标范围审计与矩阵重建主线/6-20-00-30-C4-M0目标范围审计与矩阵重建主线总入口.md` | `C4-M0-目标范围审计与矩阵重建主线/工作步骤细分/` |
| C4-M1 Part Workbench Surface Family 总览 | `C4-M1-PartWorkbenchSurfaceFamily总览/6-20-00-31-C4-M1PartWorkbenchSurfaceFamily总览入口.md` | 子包执行 |
| C4-M1 ProjectOnSurface 独立主线 | `C4-M1-PartWorkbenchSurface-ProjectOnSurface独立主线/6-20-00-20-C4-M1-ProjectOnSurface主线总入口.md` | `C4-M1-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分/` |
| C4-M1 RuledSurface / Loft 补完主线 | `C4-M1-PartWorkbenchSurface-RuledSurface-Loft补完主线/6-20-00-21-C4-M1-RuledSurface-Loft主线总入口.md` | `C4-M1-PartWorkbenchSurface-RuledSurface-Loft补完主线/工作步骤细分/` |
| C4-M1 Sweep / Filling / GeomPlate 补完主线 | `C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/6-20-00-22-C4-M1-SweepFillingGeomPlate主线总入口.md` | `C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/工作步骤细分/` |
| C4-M2 PartDesign Feature Family 总览 | `C4-M2-PartDesignFeatureFamily总览/6-20-00-32-C4-M2PartDesignFeatureFamily总览入口.md` | 子包执行 |
| C4-M2 Revolution / Groove 审计主线 | `C4-M2-PartDesign-RevolutionGroove审计主线/6-20-00-23-C4-M2-RevolutionGroove主线总入口.md` | `C4-M2-PartDesign-RevolutionGroove审计主线/工作步骤细分/` |
| C4-M2 Loft / Pipe / Boolean / Datum 主线 | `C4-M2-PartDesign-LoftPipeBooleanDatum主线/6-20-00-24-C4-M2-LoftPipeBooleanDatum主线总入口.md` | `C4-M2-PartDesign-LoftPipeBooleanDatum主线/工作步骤细分/` |
| C4-M3 Sketcher / ExternalGeometry 总览 | `C4-M3-SketcherExternalGeometry总览/6-20-00-33-C4-M3SketcherExternalGeometry总览入口.md` | 子包执行 |
| C4-M3 Sketcher Constraint-facing 扩展主线 | `C4-M3-SketcherConstraintFacing扩展主线/6-20-00-25-C4-M3-SketcherConstraintFacing扩展主线总入口.md` | `C4-M3-SketcherConstraintFacing扩展主线/工作步骤细分/` |
| C4-M3 ExternalGeometry / InternalShape 压力主线 | `C4-M3-ExternalGeometryInternalShape压力主线/6-20-00-26-C4-M3-ExternalGeometryInternalShape压力主线总入口.md` | `C4-M3-ExternalGeometryInternalShape压力主线/工作步骤细分/` |
| C4-M4 ReferenceRecovery / TopoNamingPressure 主线 | `C4-M4-ReferenceRecovery-TopoNamingPressure主线/6-20-00-27-C4-M4-ReferenceRecovery-TopoNamingPressure主线总入口.md` | `C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/` |
| C4-M5 Assembly / Runtime / Adapter 产品化主线 | `C4-M5-AssemblyRuntimeAdapter产品化主线/6-20-00-28-C4-M5-AssemblyRuntimeAdapter主线总入口.md` | `C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分/` |
| C4-M6 Freeze 收口主线 | `C4-M6-Freeze收口主线/6-20-00-29-C4-M6-Freeze收口主线总入口.md` | `C4-M6-Freeze收口主线/工作步骤细分/` |

## 执行规则

- 每个步骤先读对应专题包根入口、方案和矩阵行，再读步骤文件中列出的 FreeCAD / cad-core 代码。
- 实现步骤必须先记录 FreeCAD 调用链和 cad-core 分层映射，再改代码。
- Oracle / expected 只能来自 FreeCAD 源码语义和 native collector，不得从 cad-core 当前输出倒推。
- 若步骤发现范围过宽，先更新本专题包矩阵和后续步骤，不要在实现中临时扩大。
- 完成步骤后按仓库规则重命名为 `【已实现】...`，刷新该专题包队列再继续下一步。
