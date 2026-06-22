# 【已实现】C5-M15-S1 FreeCAD 3D plane 源码候选矩阵

状态：`s1_source_evidence_verified`

## 目标

把 C5-M15 的 FreeCAD source authority 写入 package-local source candidates，确保 S2/S6 的每个实现项都能追溯到 `Attacher.cpp` 的具体分支。

## FreeCAD 依据

| candidate | FreeCAD 路径 | 关键证据 |
| --- | --- | --- |
| `C5M15-SRC-101` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1220,1363-1385,2294-2305` | `modeRefTypes[mmTranslate].push_back(cat(rtVertex))`；`case mmTranslate` 拒绝非 vertex，读取 `BRep_Tool::Pnt`，`setPosition(vertex)` 后加 `attachmentOffset.getPosition()`，rotation 取 `origPlacement.getRotation()`，并在 `placementFactory()` 前直接 `return` |
| `C5M15-SRC-201` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1284-1300,1857-1945,2294-2305` | ThreePoints ref types 接受三 vertex、line+vertex、vertex+line、line+line；`case mmThreePointsPlane` / `mmThreePointsNormal` 从 vertex 或 edge endpoints 收集前三点；分别处理 less-than-three、coincident、collinear；Plane 用叉积和 centroid，Normal 用垂直分量反向和 projected base |
| `C5M15-SRC-301` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1239-1240,1590-1672,2294-2305` | `case mmTangentPlane` 接受 face+vertex / vertex+face，vertex-first 时 swap 并走 through-vertex base；投影到 surface 后调用 `Tools::getNormal()`，优先 tangent U，否则 tangent V crossed normal，`SketchXAxis` 取反；非 Translate 继续走 `placementFactory()` 与 `attachmentOffset` |
| `C5M15-SRC-900` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.h:61-70,76-95`；`~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1242-1282,1313,1674-1855,1947-2056,2413-2416,2613-2694,2842,2937-2984` | curve frame / curvature 依赖 edge/curve projection、`attachParameter`、D1/D2、Frenet axes 和 curvature center；`Folding` 是四 rtLine、共享顶点 signs、`calculateFoldAngle` 状态机；conic landmarks 属 1D/0D engine；`IntersectionPoint` 本轮只证明 enum/name-table route，未证明 C5-M15 3D plane branch |

## 扫描轴

- 输入 shape 数量和 shape type：vertex、edge、face。
- base point 语义：vertex point、centroid、projected point、through vertex。
- normal / X axis 语义：叉积、projected normal、surface normal、tangent U/V。
- offset / reverse 语义：`Translate` inline offset 与非 `Translate` placementFactory。
- failure 语义：missing support、wrong shape type、projection failure、coincident/collinear。

## 必须回写的矩阵行

- `c5m15_datum_3d_plane_source_candidates.tsv`：`C5M15-SRC-101`、`201`、`301`、`900`。
- `c5m15_datum_3d_plane_fixture_oracle_matrix.tsv`：`C5M15-ORC-101`、`201`、`202`、`301`、`302`。
- `c5m15_datum_3d_plane_non_goal_registry.tsv`：`C5M15-NG-001..004`。

## 本轮回写结果

- `c5m15_datum_3d_plane_source_candidates.tsv` 已把 `C5M15-SRC-101/201/301/900` 补到 FreeCAD 文件、函数/分支、语义轴、关键短句或字段、cad-core landing、scope hint 和下一步；全部保持 `pending_backendGap` 或 `nonGoal`，不声明 supported。
- `c5m15_datum_3d_plane_fixture_oracle_matrix.tsv` 已把 `C5M15-ORC-101/201/202/301/302` 的 source authority 更新为可追溯源码片段；状态仍是 `pending_native_oracle`，不采集 expected。
- `c5m15_datum_3d_plane_non_goal_registry.tsv` 已为 `C5M15-NG-001..004` 增加 `source_entry`，并写清 reopen 条件；excluded family 仍留在 exact blocker / nonGoal，不进入 C5-M15 support 包。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'case mmTranslate|case mmTangentPlane|case mmThreePointsPlane|case mmFolding|case mmFrenetNB' src/Mod/Part/App/Attacher.cpp
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
rg -n 'C5M15-SRC-101|C5M15-SRC-201|C5M15-SRC-301|C5M15-SRC-900' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵/c5m15_datum_3d_plane_source_candidates.tsv
```

验收标准：

- 每个 source candidate 有 FreeCAD 文件、函数、语义轴、关键证据、cad-core landing 和下一步。
- source candidate 不直接晋级 supported。
- excluded family 有源码入口和 reopen 条件。

## 非目标

- 不写实现。
- 不新增 fixture。
- 不把 FreeCAD 相邻分支都纳入同一包。
