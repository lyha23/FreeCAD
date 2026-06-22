# 【已实现】C5-M14-S3 edge-face Intersection 专项复审

状态：`s3_edge_face_intersection_reviewed_pending_S6`

## 目标

复核并准备实现 FreeCAD `getProximityPoint()` 的 edge-face 特殊路径：当两个 support 是 edge 和 face 时，不先走最近距离，而是先用曲线曲面相交求交点，存在交点时返回第一个。

## FreeCAD 依据

- `AttachEnginePoint::getProximityPoint()` 先归一化输入顺序：`FACE+EDGE` 与 `EDGE+FACE` 都落到本地变量 `face` / `edge`，交点本身不区分 `ProximityPoint1/2`。
- `BRepAdaptor_Curve` 必须通过 `GeomAdaptor::MakeCurve(crv)` 转成 `GeomAdaptor_Curve`，保留 edge 的 location / orientation / transformation；`Standard_DomainError` fallback 才复制底层 curve 并 `curve->Transform(crv.Trsf())`。
- `BRepIntCurveSurface_Inter::Init(face, typedcrv, Precision::Confusion())` 是正式 edge-face 相交路径，不能用 bbox、fixture 名称、输出顺序或距离近似替代。
- 多交点只写 warning：`proximity calculation gave %d solutions, ambiguous.`，仍返回 `points.front()`。
- `Standard_Failure` 被捕获并忽略，只表示继续进入 fallback，不产生 hard diagnostic。
- 只有 edge-face 无交点或 intersection 分支异常时，才进入 `BRepExtrema_DistShapeShape` distance fallback；no-hit fallback 的代表场景仍归 S4/S6，不在 S3 提前标 done。

## cad-core 设计

- helper 名称建议：`proximityPointEdgeFaceIntersection()`。
- 输入：两个 `SupportResolution`，内部归一化 edge / face 顺序，返回 `std::optional<gp_Pnt>`。
- 行为：支持 face-edge 与 edge-face 两种顺序；命中 intersection 时返回第一个交点，不分 `PointOnShape1` / `PointOnShape2`。
- 异常：`Standard_Failure` 只作为继续 fallback 的信号，不应产生 hard diagnostic；`Standard_DomainError` 只用于 MakeCurve 的等价 fallback。
- 落点：S6 才允许在 `cad-core/src/part_design/datum_attachment.h` 落 C++，并用 native oracle + focused test 关闭 `C5M14-BLK-201`。

## 必须回写的矩阵行

- `C5M14-SCOPE-201`
- `C5M14-BLK-201`
- `C5M14-ORC-203`
- `C5M14-VAL-201`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'BRepIntCurveSurface_Inter|GeomAdaptor::MakeCurve|PointOnShape1|PointOnShape2' src/Mod/Part/App/Attacher.cpp
rg -n 'C5M14-BLK-201|C5M14-ORC-203|edge-face|BRepIntCurveSurface_Inter|GeomAdaptor::MakeCurve' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/工作步骤细分 --format markdown
```

S6 实现后再运行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-point-proximity-modes.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_point_proximity_modes_match_expected
```

## 非目标

- 不在 S3 实现 distance fallback 的所有代表场景。
- 不处理 surface helper / Part Workbench edge-face 语义。
- 不修改 C++、fixtures、capability supported 或 exact blocker。
- 不声明 `ProximityPoint1/2` supported。
