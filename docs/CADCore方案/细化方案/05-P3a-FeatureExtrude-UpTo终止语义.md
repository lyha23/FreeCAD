# P3a：FeatureExtrude UpTo 终止语义

P3a 把 shared `FeatureExtrude` 从固定长度扩展到 FreeCAD 常用终止模式，服务 Pad、Pocket 和后续 transformed family。

## 当前基线

- `Type=Length`、`ThroughAll`、`UpToFace`、单目标 `UpToShape` 已接入。
- `PropertyLinkSub` 可解析当前 recompute 内目标 shape 的 `FaceN`。
- stable subname 已可通过 P6 `ElementMap` 更新到 current subname。
- 缺失目标、非法 subshape、非 face 终止面、unsupported type 返回 diagnostics。

## 边界

- 多 face / shell `UpToShape` 仍未完整迁移。
- 非平面终止面和完整 `makeElementPrismUntil` 仍未完整迁移。
- UpTo 与 taper、复杂 placement、外部 Part ownership 的组合仍归后续 P3b/P6。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/feature_extrude.*` | 终止模式、方向、prism 构造 |
| `features/pad.*` / `pocket.*` | Pad/Pocket 属性边界 |
| `topo/named_shape.*` | prism history 子集 |
| `document/model.*` | `PropertyLinkSub` 输入 |

## FreeCAD 依据

- `src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `src/Mod/PartDesign/App/FeaturePad.cpp`
- `src/Mod/PartDesign/App/FeaturePocket.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`

## 验收

- `fixtures/p3a` 覆盖 ThroughAll、UpToFace、UpToShape 和错误 diagnostics。
- UpToFace 不用 bbox 近似替代真实 face。
- 失败 diagnostics 指明 object、property、target 和 subname。
