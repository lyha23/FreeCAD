# 【已实现】C5-M14-S4 distance Fallback And Diagnostics 专项复审

状态：`s4_distance_fallback_diagnostics_reviewed_pending_S6`

## 目标

复核并准备实现普通 shape-shape proximity fallback：当 edge-face 没有交点，或 S3 intersection 分支捕获 `Standard_Failure` 后，和输入不是 edge-face 的场景一样进入同一个 `BRepExtrema_DistShapeShape` fallback；按 mode 返回第一个或第二个 shape 上的点。S4 只冻结 S6 可实现边界，不改 C++、fixtures、capability supported，也不运行 FreeCADCmd。

## FreeCAD 依据

- `BRepExtrema_DistShapeShape distancer(s1, s2)`。
- `!distancer.IsDone()` 抛 `AttachEnginePoint::calculateAttachedPlacement: proximity calculation failed.`。
- `distancer.NbSolution() > 1` 只 warning `proximity calculation gave %i solutions, ambiguous.`，不改变返回策略。
- `gp_Pnt p1 = distancer.PointOnShape1(1)`。
- `gp_Pnt p2 = distancer.PointOnShape2(1)`。
- `mm0ProximityPoint1` 返回 p1，`mm0ProximityPoint2` 返回 p2。
- touch / intersect 对 DatumPoint proximity 是有效几何状态，可以返回同一点或交点；不得映射成 `ProximityLine` 的 `no_intersection` 或 invalid。

## 代表 fixture

| fixture | 目的 |
| --- | --- |
| `DatumPointProximityVertexVertex1` / `DatumPointProximityVertexVertex2` | 验证 p1 / p2 分别落在不同 vertex |
| `DatumPointProximityEdgeEdge1` / `DatumPointProximityEdgeEdge2` | 验证普通 distance fallback |
| `DatumPointProximityEdgeFaceNoHit1` / `DatumPointProximityEdgeFaceNoHit2` | 验证 edge-face 无交点或 intersection 异常后进入同一 distance fallback |
| `DatumPointProximityMissingSecond` | 验证少 support diagnostic |
| `DatumPointProximityBadSubname` | 验证 request-local subname resolve diagnostic |

上述 fixture 只做计划，S4 不落文件；native oracle 仍为 `pending_native_oracle` / `pending_diagnostic`，S6 实现后再采集和运行 focused tests。

## diagnostics 口径

- missing second support：和 FreeCAD `Proximity mode requires two shapes` 对齐，cad-core 需给出独立稳定 diagnostic，不与 null shape 混淆。
- unresolved subname：沿用 request-local 解析失败口径，保持 `subname_resolve_failed`。
- null shape：沿用 support shape invalid 口径，保持 `attachment_support_invalid_shape`。
- distance helper failure：`BRepExtrema_DistShapeShape` 的 `!IsDone()` 映射为 execution failure，错误文本保留 `proximity calculation failed` 语义。
- touch / intersect 不是 invalid。FreeCAD proximity point 可以返回同一点或交点，这与 `ProximityLine` 的 touch/intersect 失败不同。

## 必须回写的矩阵行

- `C5M14-SCOPE-202`
- `C5M14-SCOPE-203`
- `C5M14-BLK-202`
- `C5M14-BLK-203`
- `C5M14-ORC-201`、`C5M14-ORC-202`、`C5M14-ORC-204`
- `C5M14-VAL-202`、`C5M14-VAL-203`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'BRepExtrema_DistShapeShape|PointOnShape1|PointOnShape2|proximity calculation failed' src/Mod/Part/App/Attacher.cpp
rg -n 'C5M14-BLK-202|C5M14-BLK-203|touch/intersect|PointOnShape1|PointOnShape2|pending_diagnostic' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/工作步骤细分 --format markdown
```

S6 实现后再运行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_point_proximity_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_point_proximity_invalid_diagnostics
```

## 非目标

- 不把 proximity point 的 touch/intersect 改成 line mode 的 `no_intersection`。
- 不修改 `ProximityLine` 已支持路径。
- 不把 edge-face no-hit fallback、diagnostics 或 capability supported 标 done；它们必须等 S6 C++、oracle 和 focused tests 通过后关闭。
