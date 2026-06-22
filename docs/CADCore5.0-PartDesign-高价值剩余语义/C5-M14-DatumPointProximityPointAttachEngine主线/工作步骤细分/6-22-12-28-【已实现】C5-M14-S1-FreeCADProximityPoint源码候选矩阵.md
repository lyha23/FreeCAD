# 【已实现】C5-M14-S1 FreeCAD ProximityPoint 源码候选矩阵

状态：`s1_source_audit_verified`

## 目标

从 FreeCAD source 中确认 `ProximityPoint1/2` 的输入、计算顺序、edge-face 特殊路径、distance fallback、错误边界和 placement 输出。S1 只建立 source authority，不做 cad-core 实现。

## S1 结论

- 已回写 `C5M14-SRC-001..006`，覆盖 mode registration、placement diagnostics、edge-face first-hit、distance fallback、request-local subname writeback 和 live capability blocker。
- `C5M14-SCOPE-101` 已更新为 `s1_source_audit_verified`；`C5M14-ORC-101` 已更新为 `s1_source_audit_done`。
- 本步骤未运行 FreeCADCmd，未改 fixture，未改 C++，未移除 capability blocker，也未把 `IntersectionPoint` 或 Focus/curvature/Frenet/three-point/folding 等非目标升级为 supported/backendGap。

## FreeCAD 依据

| 源码 | 必查符号 | 要摘录的关键短句 / 字段 |
| --- | --- | --- |
| `src/Mod/Part/App/Attacher.cpp` | `AttachEnginePoint::AttachEnginePoint()` | `mm0ProximityPoint1`、`mm0ProximityPoint2`、`cat(rtAnything, rtAnything)` |
| `src/Mod/Part/App/Attacher.cpp` | `AttachEnginePoint::_calculateAttachedPlacement()` | `Proximity mode requires two shapes`、`BasePoint = getProximityPoint(...)` |
| `src/Mod/Part/App/Attacher.cpp` | `AttachEnginePoint::getProximityPoint()` | `BRepIntCurveSurface_Inter`、`BRepExtrema_DistShapeShape`、`PointOnShape1(1)`、`PointOnShape2(1)` |
| `src/Mod/Part/App/AttachExtension.cpp` | `positionBySupport()` | `calculateAttachedPlacement(..., &subChanged)`、`AttachmentSupport.setValues(...)` |
| `src/App/PropertyLinks.cpp` | `PropertyLinkSub/List::getSubValues()` | request subname evidence |

## 扫描轴

- mode registration：是否真是 DatumPoint mode。
- input contract：两个 shape 是否都可以是 whole-object 或 subshape。
- edge-face intersection：face-edge 输入顺序是否需要归一化。
- fallback：distance 是否返回两个点，并按 mode 选择。
- diagnostics：少 support、null shape、distance failure 如何映射到 cad-core code。
- placement：是否需要特殊 X/Y/Z 轴，还是固定 point placement。

## 必须回写的矩阵行

- `C5M14-SRC-001` 到 `C5M14-SRC-006`
- `C5M14-SCOPE-101`
- `C5M14-ORC-101`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'mm0ProximityPoint1|mm0ProximityPoint2|getProximityPoint|BRepIntCurveSurface_Inter|BRepExtrema_DistShapeShape|PointOnShape1|PointOnShape2' src/Mod/Part/App/Attacher.cpp
awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵/c5m14_datum_point_proximity_source_candidates.tsv
```

通过条件：

- source candidates 至少包含 registration、placement branch、edge-face intersection、distance fallback、request-local writeback、capability blocker 六类。
- 每条 source candidate 都有 cad-core landing 或 non-goal routing。
- 没有把 `IntersectionPoint` 升级为 supported 或 backendGap。

## 非目标

- 不运行 FreeCADCmd。
- 不修改 fixture。
- 不实现 Focus / curvature / Frenet / three-point / folding family。
