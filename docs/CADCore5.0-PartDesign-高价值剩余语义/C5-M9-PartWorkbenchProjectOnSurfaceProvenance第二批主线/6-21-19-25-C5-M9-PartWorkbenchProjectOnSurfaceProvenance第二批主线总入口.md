# C5-M9 Part Workbench ProjectOnSurface Provenance 第二批主线

本包承接 C4M1 / C5 root 中已经发布的 `Part::ProjectOnSurface` expected-backed geometry slice，只打开 projected subshape provenance / mapper history 第二批。第一批的 `Mode=Edges/Faces/All`、face rebuild / hole wires、Height solid、Offset placement、多 `Projection` ordered metadata、普通 indexed `NamedShape` 和 deferred diagnostics 是 live guard，不在本包重新证明。

范围边界：C5-M9 的交付物只落在 `cad-core`、fixtures、tests、capabilities 和文档矩阵。FreeCAD `src/Mod/Part/App/FeatureProjectOnSurface.cpp/.h`、`TopoShapeMapper*`、`PropertyTopoShape*` 只作为语义依据读取。若 native oracle 无法直接暴露 mapper/history，需要记录 source-backed known_gap 与删除条件，不能用 bbox、输出顺序、fixture 名或 adapter 后处理伪造 provenance。

## 目标

- 冻结 C4M1 `part-project-on-surface-*` fixtures，确保已发布几何能力不回退。
- 复核 FreeCAD 调用链：`getSupportFace()` -> `getProjectionShapes()` -> `createProjectedWire()` -> `projectWire()` / `projectFace()` -> `filterShapes()` -> `createCompound()`，明确哪些输出 shape 需要保留源 object/subname、Projection LinkSubList item index、Mode 分支和 fragment ownership。
- 为 edge / wire 投影补 projected edge provenance 与 mapper/history 证据，覆盖一对一 edge、wire 拆分或重建 edge、invalid provenance diagnostics。
- 为 face rebuild / hole wire / Mode=All compound 或 height solid 结果补 source evidence、ElementMap / reference recovery hook 和 capability metadata。
- 收口 `projected_edge_provenance_mapper_history` remaining gap，只发布 expected-backed、source-backed known_gap 或 diagnostic-backed 的精确边界。

## 入口文件

- 方案：`6-21-19-25-C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批方案.md`
- scope 矩阵：`矩阵/c5m9_project_on_surface_provenance_scope.tsv`
- fixture / oracle 矩阵：`矩阵/c5m9_project_on_surface_provenance_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m9_project_on_surface_provenance_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m9_project_on_surface_provenance_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m9_project_on_surface_provenance_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live guard | `c4m1/part-project-on-surface-edge-plane`、face / hole / all / height / offset / multi projection / deferred boundaries | S0 冻结现有 expected-backed slice 和 root capability 状态 |
| source / oracle matrix | FreeCAD `FeatureProjectOnSurface` + `TopoShapeMapper` / `PropertyTopoShape` 审计 | S1 写清 projected result ownership、oracle 可采字段、known_gap 删除条件 |
| edge / wire provenance | `c5m9/part-project-on-surface-edge-provenance`、`wire-split-provenance`、invalid provenance diagnostics | S2 补 projected edge/wire source evidence、MapperHistory / ElementMap 传播和 focused tests |
| face / all provenance | `c5m9/part-project-on-surface-face-rebuild-provenance`、`all-compound-provenance` | S3 补 face rebuild、hole wire、Mode=All compound/solid ownership与 reference recovery evidence |
| capability closeout | docs、capability metadata、root matrices、remaining gaps | S4 发布精确 support / known_gap / diagnostic / non-goal 边界，并清空本包队列 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现 GUI projection task panel、ViewProvider 或 command UI。
- 不声明完整 `ProjectOnSurface` 或完整 Part surface family。
- 不重写已 expected-backed 的基础 projection geometry。
- 不用 bbox、输出顺序、fixture 名、投影结果几何相似性或 adapter 后处理修剪替代 mapper/history。
- 不把 Loft、Sweep、Filling、GeomPlate 或 RuledSurface 的后续 owner 混入本包。
