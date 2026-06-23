# 【已实现】C5-M19-S3 TangentUV 源码复审

状态：`done_source_audited_non_goal`

## 结论

`TangentU` 和 `TangentV` 不是 FreeCAD 可选 selected MapMode route。它们只作为 enum/name 表项存在；实际源码中出现的 `prop.TangentU(dirX)` / `prop.TangentV(dirY)` 是 `AttachEngine3D::_calculateAttachedPlacement()` 的 `mmTangentPlane` 分支内部选取 surface tangent 方向，不是 `mm1TangentU` / `mm1TangentV` 的独立执行分支。

## 源码证据

| 证据 | 路径 | 说明 |
| --- | --- | --- |
| enum/name | `Attacher.h:83-84`、`Attacher.cpp:110-111` | `mm1TangentU/V` 与显示名存在 |
| 可选性闸门 | `Attacher.cpp:555-561` | empty `modeRefTypes` 不启用 |
| 注册缺失 | `Attacher.cpp:2394-2434` | `AttachEngineLine` 注册了 reused 3D modes、two points、proximity、intersection 等，但没有 `mm1TangentU/V` |
| 执行缺失 | `Attacher.cpp:2444-2798` | `AttachEngineLine::_calculateAttachedPlacement()` switch 没有 `mm1TangentU/V` |
| 内部 tangent | `Attacher.cpp:1651-1663` | `mmTangentPlane` 内部调用 `TangentU/TangentV` 只为求 `SketchXAxis` |

## 必须回写的矩阵行

- `C5M19-SCOPE-101`
- `C5M19-SCOPE-102`
- `C5M19-NG-001`
- `C5M19-CAT-001`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'mm1TangentU|mm1TangentV|TangentU\\(|TangentV\\(|case mm1TangentU|case mm1TangentV|modeRefTypes\\[mm1TangentU\\]|modeRefTypes\\[mm1TangentV\\]' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
rg -n 'C5M19-SCOPE-101|C5M19-SCOPE-102|TangentU_TangentV' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M19-DatumAttachEngineSourceAuditCloseout主线
```

## 非目标

- 不把 `mmTangentPlane` 的内部 tangent direction 拆成新 cad-core API。
- 不新增 surface tangent fixture。
