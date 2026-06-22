# C5-M17-S1 FreeCAD Remaining AttachEngine 源码候选矩阵

## 目标

把 C5-M17 的 remaining modes source authority 固化到 source candidates，确认 conic landmarks、Folding、IntersectionPoint、TangentU/V 是否共享实现 owner。

## 必读源码

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2413-2419`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2613-2702`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2842-2845`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2937-2990`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1947-2056`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2703+`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1652-1661`

## 必做

1. 复核 `AttachEngineLine` conic modes：`Asymptote1/2`、`Directrix1/2`。
2. 复核 `AttachEnginePoint` conic modes：`Focus1/2`。
3. 复核 `Folding` 是否仍应是独立 four-line fold-angle package。
4. 复核 `IntersectionPoint` 的 face/face branch 和 DTO 风险。
5. 复核 `TangentU/V` 是否属于 surface tangent package，而非 conic landmark。
6. 更新 source candidates 与 scope review 矩阵。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'mm1Directrix|mm1Asymptote|mm0Focus|mmFolding|mm1Intersection|TangentU|TangentV' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线
```

## 非目标

- 不把 source candidate 直接等同 supported。
- 不采集 oracle。
- 不改 capability exact blocker。
