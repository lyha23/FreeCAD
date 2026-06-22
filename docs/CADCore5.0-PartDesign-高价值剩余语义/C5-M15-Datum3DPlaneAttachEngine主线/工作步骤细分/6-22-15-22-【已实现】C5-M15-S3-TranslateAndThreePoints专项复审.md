# 【已实现】C5-M15-S3 TranslateAndThreePoints 专项复审

状态：`s3_translate_threepoints_review_verified`

## 目标

复审 `Translate`、`ThreePointsPlane`、`ThreePointsNormal` 的 placement 数学、support 解析、diagnostics 和 fixture batch，为 S6 的 C++ 实现做精确落点。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1220`：`modeRefTypes[mmTranslate].push_back(cat(rtVertex))`，`Translate` 的正式 support 是一个 vertex。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1363-1385`：`case mmTranslate` 拒绝空 shape、null shape 和非 vertex；读取 `BRep_Tool::Pnt(TopoDS::Vertex(sh))`；`position = vertex + attachmentOffset.getPosition()`；`rotation = origPlacement.getRotation()`；在函数尾部 `placementFactory()` 前直接 `return plm`。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1284-1300`：`ThreePointsPlane` / `ThreePointsNormal` 接受三 vertex、line+vertex、vertex+line、line+line 的 ref type 组合。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1857-1945`：`ThreePoints*` 按 `readLinks()` 的 support 顺序遍历；vertex 取 `BRep_Tool::Pnt`；edge 取 `BRepAdaptor_Curve` 的 first / last endpoint，infinite parameter 改用 `0.0..1.0`；收满三点立即停止。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1890-1934`：少于三点、coincident、collinear 分别抛 `ValueError`；`ThreePointsPlane` 使用 `vec01 x vec02` 和三点 centroid；`ThreePointsNormal` 使用 `vec02` 去掉 `vec01` 分量后 reverse，并把 `p2` 投影到以 `p0` 为点、normal 为法向的 plane。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2294-2305`：非 `Translate` 分支继续由 `placementFactory()` 和 `attachmentOffset` 组合；S6 不能把三点分支的 base/normal 当作最终 placement 后再做输出修正。

## S3 复审结论

| mode | support 解析 | placement 数学 | S6 实现约束 |
| --- | --- | --- | --- |
| `Translate` | 只接受一个 vertex；空 support、null shape、非 vertex 都是 invalid | base position = vertex point + `attachmentOffset.getPosition()`；rotation = `origPlacement.getRotation()`；直接 return，不走 `placementFactory()`；不组合 `AttachmentOffset` rotation | 在 `cad-core/src/part_design/datum_attachment.h` 增加专用 helper；不能 fallback 到 object/default placement |
| `ThreePointsPlane` | 按 support 顺序读取 vertex 点或 edge first/last endpoints；收满三点即停；不按几何类型重新排序 | `vec01 = p0->p1`、`vec02 = p0->p2`；normal = `vec01 x vec02`；base = `(p0+p1+p2)/3`；随后走 `placementFactory()` | fixture 覆盖三 vertex 和 edge endpoints + vertex；不得靠 output sorting 或 bbox 对齐修正 |
| `ThreePointsNormal` | 与 `ThreePointsPlane` 共用点收集；同样只消费前三点 | normal = `vec02 - vec01 * dot(vec02, vec01)` 后 reverse；base = `p2` 投影到 `p0 + normal` plane；随后走 `placementFactory()` | fixture 必须覆盖 projected-base 行为；normal 方向以 FreeCAD expected 为准 |

## invalid diagnostics 分类

| 分类 | FreeCAD 条件 | S6 diagnostic 要求 |
| --- | --- | --- |
| less-than-three | 收集后 `points.size() < 3` | 结构化 diagnostic，不能返回 default placement |
| coincident | `vec01` 或 `vec02` 长度小于 `Precision::Confusion()` | 结构化 diagnostic，message / code 与 collinear 区分 |
| collinear | Plane 叉积或 Normal 垂直分量长度小于 `Precision::Confusion()` | 结构化 diagnostic，不能用 bbox、排序或扰动点绕过 |
| wrong support type | `Translate` 非 vertex；`ThreePoints*` 无法贡献 vertex / edge endpoint | S6 可与 less-than-three 区分；不应报 unsupported mode |

## 范围

| scope | 代表场景 | 需要 oracle |
| --- | --- | --- |
| `C5M15-SCOPE-101` | DatumPlane/DatumCS `Translate` 到 vertex，含 non-default AttachmentOffset | `partdesign-datum-3d-plane-modes` |
| `C5M15-SCOPE-201` | 三个 vertex 成面 | `partdesign-datum-3d-plane-modes` |
| `C5M15-SCOPE-201` | edge endpoints + vertex 成面 | `partdesign-datum-3d-plane-modes` |
| `C5M15-SCOPE-202` | `ThreePointsNormal` base point projection | `partdesign-datum-3d-plane-modes` |
| `C5M15-SCOPE-201/202` invalid | less-than-three、coincident、collinear | `partdesign-datum-3d-plane-diagnostics` |

## 必须回写的矩阵行

- `C5M15-BLK-101`：Translate helper 已细化为 one-vertex support、inline position offset、original rotation、return before placementFactory。
- `C5M15-BLK-201`：three-point collection and plane math 已细化为 ordered support consumption、vertex/edge endpoint point source、Plane centroid/cross normal、Normal projected base/reversed perpendicular normal。
- `C5M15-BLK-202`：three-point diagnostics 已细化为 less-than-three、coincident、collinear，不允许 default placement fallback。
- `C5M15-ORC-101`、`C5M15-ORC-201`、`C5M15-ORC-202` 已同步 expected fields、diagnostic route、禁止捷径和 S6 owner。

## 本轮回写结果

- `c5m15_datum_3d_plane_blocker_queue.tsv`：`C5M15-BLK-101/201/202` 已从 broad blocker 改成 S6 可直接生成 helper、fixtures 和 focused tests 的条目。
- `c5m15_datum_3d_plane_fixture_oracle_matrix.tsv`：`C5M15-ORC-101/201/202` 已写清 expected fields、source authority、fixture target 和 notes；状态仍为 `pending_native_oracle`。
- `scope_review_matrix.tsv`、`backend_gap_classification.tsv`：S3 覆盖项的 next step 已推进为 “S6 collect/implement/test”。
- `validation_matrix.tsv`：新增 S3 matrix/source anchor 验证行，并把 queue 备注更新为 S0-S3 不再出队、S4-S6 继续 pending。

## 禁止捷径

- 不按 fixture 名称分支。
- 不按输出顺序、几何类型排序或 bbox/volume 对齐倒推 FreeCAD expected。
- 不在 adapter、fixture writer 或输出层修正 placement。
- 不在 invalid case fallback 到 default/object placement。
- 不把 `ThreePoints*` 的 base/normal 直接当最终 placement；必须保留 FreeCAD 函数尾部 `placementFactory()` 组合语义。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C5M15-BLK-101|C5M15-BLK-201|C5M15-BLK-202|C5M15-ORC-101|C5M15-ORC-201|C5M15-ORC-202' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵
rg -n 'case mmTranslate|case mmThreePointsPlane|case mmThreePointsNormal' src/Mod/Part/App/Attacher.cpp
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/工作步骤细分 --format markdown
```

验收标准：

- S3 明确 `Translate` 与非 `Translate` 的 offset composition 差异。
- S3 明确 three-point 的点收集顺序和 invalid 分类。
- S3 不允许 fixture-name 分支、output sorting、bbox 对齐或默认 placement fallback。
- S3 文件重命名为 `6-22-15-22-【已实现】C5-M15-S3-TranslateAndThreePoints专项复审.md` 后，队列只剩 S4-S6。

## 非目标

- 不实现 `TangentPlane`。
- 不采集 native expected；S6 才执行 oracle。
- 不修改 C++、不新增 fixture/expected、不改 capability。
