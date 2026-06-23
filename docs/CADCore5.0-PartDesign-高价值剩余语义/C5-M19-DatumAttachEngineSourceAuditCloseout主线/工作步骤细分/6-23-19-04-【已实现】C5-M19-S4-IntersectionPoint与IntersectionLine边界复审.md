# 【已实现】C5-M19-S4 IntersectionPoint 与 IntersectionLine 边界复审

状态：`done_source_audited_non_goal`

## 结论

`IntersectionPoint` 没有 FreeCAD `AttachEnginePoint` 可执行 route。`IntersectionLine` 则是独立的 DatumLine `AttachEngineLine::mm1Intersection` route，已在 cad-core 支持，不能把它误当成 `IntersectionPoint` 的实现证据。

## 源码证据

| 证据 | 路径 | 说明 |
| --- | --- | --- |
| Point enum/name | `Attacher.h:95`、`Attacher.cpp:122` | `mm0Intersection` / `IntersectionPoint` 名称存在 |
| Point 注册缺失 | `Attacher.cpp:2827-2857` | `AttachEnginePoint` 注册 Origin、CenterOfCurvature、OnEdge、Vertex、Focus、ProximityPoint、CenterOfMass；没有 `mm0Intersection` |
| Point 执行缺失 | `Attacher.cpp:2866-3018` | switch 只有 Vertex、Focus、ProximityPoint、CenterOfMass；没有 `mm0Intersection` |
| Line 注册存在 | `Attacher.cpp:2432` | `modeRefTypes[mm1Intersection].push_back(cat(rtFace, rtFace))` |
| Line 执行存在 | `Attacher.cpp:2703-2752` | `GeomAPI_IntSS` face-face intersection，要求 exactly one straight line |
| cad-core 支持存在 | `cad-core/src/part_design/datum_attachment.h:849-923,2218-2228` | `IntersectionLine` helper 和 dispatch 已存在 |

## 必须回写的矩阵行

- `C5M19-SCOPE-201`
- `C5M19-SCOPE-301`
- `C5M19-NG-002`
- `C5M19-CAT-002`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'mm0Intersection|IntersectionPoint|modeRefTypes\\[mm0Intersection\\]|case mm0Intersection|modeRefTypes\\[mm1Intersection\\]|case mm1Intersection|IntersectionLine' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h cad-core/src/part_design/datum_attachment.h
rg -n 'DatumLine IntersectionLine selected MapMode|datum_attach_engine_remaining_modes' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
```

## 非目标

- 不从 face-face line intersection 合成 point placement。
- 不新增 `IntersectionPoint` DTO、fixture 或 expected。
