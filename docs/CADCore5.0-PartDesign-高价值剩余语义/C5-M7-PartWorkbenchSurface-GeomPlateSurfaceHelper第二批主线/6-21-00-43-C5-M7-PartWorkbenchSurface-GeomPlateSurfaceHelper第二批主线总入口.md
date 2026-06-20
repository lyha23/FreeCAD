# C5-M7 Part Workbench Surface GeomPlateSurface Helper 第二批主线

本包承接 C5-M6 后的 Part Workbench Surface family 剩余项，但只打开 `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` 这一条 API 边界。

## 目标

把 GeomPlate helper 的第二批完整语义做成同一轮，而不是单 fixture 推进：

- 复核现有 `c4m1/part-geomplate-advanced-constraints` 与 `part-geomplate-advanced-deferred` 的 live 支持边界。
- 扩展同一 DTO 下的 initial surface reference、G1 curve-on-surface、2D curve-on-surface / projected 2D curve、2D point-on-surface 和 point custom criteria。
- 对 curve criteria setter 与 `Part.PlateSurface.Curves` wrapper 做 owner 判定：FreeCAD 未实现的 setter / wrapper lifecycle 只能产出 concrete diagnostic，不默默写成 supported。

## 入口文件

- 方案：`6-21-00-43-C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批方案.md`
- scope 矩阵：`矩阵/c5m7_geomplate_surface_helper_scope.tsv`
- fixture / oracle 矩阵：`矩阵/c5m7_geomplate_surface_helper_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m7_geomplate_surface_helper_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m7_geomplate_surface_helper_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m7_geomplate_surface_helper_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

C5-M7 的最小完整批次不是“一个高级约束”，而是同一 FreeCAD 调用链、同一 DTO / API 边界、同一 expected 类型下的代表场景集合：

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live guard | 现有 3D G0 curve + 3D point + explicit approximation params | S1 已把 `c4m1/part-geomplate-advanced-constraints` 固化为 expected-backed guard；只证明 `Approx*` 参数和 3D source evidence，不代表 InitialSurface / G1 / 2D / wrapper support |
| initial surface / G1 | `Surface` / `InitialSurface`、curve-on-surface G1 | S2 已产出 c5m7 fixtures：InitialSurface expected-backed；G1 source-backed 且 FreeCAD native oracle blocker 以 known_gap 固化 |
| 2D constraints | curve2d-on-surface、projected 2D curve、point2d-on-surface | S3 已产出 c5m7 fixtures：curve2d-on-surface、point2d-on-surface、G0+2D mixed expected-backed；projected 2D curve cad-core source-backed，FreeCAD native oracle 以 known_gap 固化 |
| criteria / wrapper | G0/G1/G2 criterion setters、`Part.PlateSurface.Curves` wrapper | S4 已支持 point criteria expected-backed；curve criteria setter 因 FreeCAD `NotImplementedError` 保留 `unsupported_curve_criteria`；`Part.PlateSurface.Curves` 因 wrapper lifecycle 保留 `unsupported_wrapper_lifecycle`，不留 broad gap |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现 GUI GeomPlate feature、TaskPanel 或 ViewProvider。
- 不伪造原生 FreeCAD `Part::GeomPlate` DocumentObject；本包仍是 source-backed geometry helper。
- 不把 Filling `BRepOffsetAPI_MakeFilling` 的 support/order/param family 混入本包。
- 不用 bbox、fixture 名、输出顺序或后处理修剪替代 GeomPlate constraint DTO 与 FreeCAD 调用链。
