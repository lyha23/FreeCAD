# 【已实现】C5-M15-S4 TangentPlane SurfaceNormal 专项复审

状态：`s4_tangent_plane_surface_review_verified`

## 目标

复审 `TangentPlane` 的 face+vertex support order、surface projection、normal/tangent 方向和 diagnostics。该步骤是本包风险最高的边界，不能用 planar-only 简化替代。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1590`：`mmTangentPlane` 要求 one face and one vertex。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1598`：第一个 support 是 vertex 时 swap，`bThruVertex = true`。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1627`：`GeomAPI_ProjectPointOnSurf` 投影，失败抛 ValueError。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1642`：`Tools::getNormal(face, u, v, Precision::Confusion(), SketchNormal, done)`。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1651`：优先 tangent U；否则 tangent V crossed normal；`SketchXAxis = gp_Vec(dirX).Reversed()`。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1667`：vertex-first 时 base point = vertex，否则 base point = projected nearest point。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:728`：`getNormalBySLProp()` 先用 `prop.Normal()`，法线未定义时用 `CSLib::Normal(...)`，并按边界状态修正方向。
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp:783`：`Tools::getNormal(const TopoDS_Face&, ...)` 用 `BRepAdaptor_Surface` / `BRepLProp_SLProps`，并在 face reversed 时 reverse normal。

## 范围

| scope | 代表场景 | 风险 |
| --- | --- | --- |
| `C5M15-SCOPE-301` face then vertex | base point 是 projected point | surface projection / normal orientation |
| `C5M15-SCOPE-301` vertex then face | base point 是 original vertex | support order affects placement |
| `C5M15-SCOPE-301` reversed face | tangent / normal orientation | must match FreeCAD expected |
| `C5M15-SCOPE-301` invalid | missing face/vertex, projection or normal failure | stable diagnostic code |

## S4 复审结论

| 项 | FreeCAD 语义 | S6 实现要求 |
| --- | --- | --- |
| support order | `shapes[0]` 是 vertex 时 swap，并设置 `bThruVertex = true` | `datum_attachment.h` 必须读取两个 supports；支持 face+vertex 与 vertex+face，不能只解析第一个 support |
| base point | face+vertex 使用 `projector.NearestPoint()`；vertex+face 使用原始 vertex `p` | fixture 必须同时覆盖两种顺序，不能用同一 projected base 近似 |
| projection | `GeomAPI_ProjectPointOnSurf(p, hSurf)`，`NbPoints()==0` 抛 projection failure | S6 使用 projection 和 `LowerDistanceParameters(u, v)`；不能 bbox/平面化 |
| normal | `Tools::getNormal(face,u,v,Precision::Confusion(),SketchNormal,done)`，face reversed 会 reverse normal | 需 source-backed 复刻 `BRepLProp_SLProps` + `CSLib::Normal` fallback；若无法完成，必须拆 precise blocker |
| tangent | 优先 `TangentU(dirX)`，face reversed 时 reverse `dirX`；否则 `dirX = TangentV.Crossed(SketchNormal)` | `SketchXAxis = gp_Vec(dirX).Reversed()` 必须进 expected-backed fixture，不靠输出排序修正 |
| diagnostics | not enough supports、null face、null vertex、projection failed、normal failed | diagnostics fixture 要区分 missing/wrong support 与 projection/normal failure，不 fallback default placement |

## cad-core 落点

- `cad-core/src/part_design/datum_attachment.h` 当前已有 `BRepAdaptor_Surface`、`GeomAPI_ProjectPointOnSurf`、`BRepLProp_SLProps` 相关依赖基础，但 `selectedDatumPlacement()` 还没有 `TangentPlane` 分支，也没有对该 mode 读取两个 supports。
- S6 需要补 `tangentPlanePlacement()`，并同步 `requiresSubshape` / support count；实现必须位于 `datum_attachment.h` 或后续提取的 part_design/geometry helper，不放到 adapter。
- `Tools::getNormal` 等价逻辑可按 FreeCAD `Tools.cpp` 迁移为局部 helper；如果 OCCT/LibPack 差异导致边界 normal 不能 parity，应保留 `C5M15-BLK-301` 为 precise blocker，而不是把 `TangentPlane` 改成 planar-only supported。

## 必须回写的矩阵行

- `C5M15-BLK-301`：projection / normal / tangent。
- `C5M15-BLK-302`：support order and through-vertex base point。
- `C5M15-ORC-301`、`C5M15-ORC-302`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C5M15-BLK-301|C5M15-BLK-302|C5M15-ORC-301|C5M15-ORC-302' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵
rg -n 'case mmTangentPlane|Tools::getNormal|TangentU|TangentV|bThruVertex' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Tools.cpp src/Mod/Part/App/Tools.h
```

验收标准：

- S4 明确 face+vertex 与 vertex+face 的 base point 差异。
- S4 明确 tangent U/V 和 reversed face 对 orientation 的影响。
- 若 cad-core 无法 source-backed 复刻 `Tools::getNormal` 等价行为，S4 必须把 `TangentPlane` 细化成 precise blocker，不得静默用 planar face normal 替代。
- S4 `【已实现】` 只代表 source-backed 复审完成；`C5M15-ORC-301/302` 仍是 `pending_native_oracle`，`C5M15-BLK-301/302` 仍需 S6 落代码和 expected-backed tests。

## 非目标

- 不实现 curve frame / Frenet / curvature。
- 不更改 FreeCAD expected。
- 不改 C++、不新增 fixture、不中途移除 capability exact blocker。
