# P3b：FeatureExtrude 双侧、方向与 Placement

P3b 固定 Pad / Pocket shared `FeatureExtrude` 的常用方向矩阵和坐标传递。

## 当前基线

- `SideType=Two sides`、`Symmetric`、`Length2`、第一侧 / 第二侧 `UpToFace` / `UpToShape` 已有基础路径。
- `UpToFirst` / `UpToLast` 可基于 previous-body 候选面选择终止面。
- Pad / Pocket taper 可生成几何结果；一侧 / 内环 taper 已记录 `BRepOffsetAPI_ThruSections` 的 source face first-section history，一侧、多侧和内环 taper 已记录 source edge / generated section history 子集，但对象结果仍显式标记 `known_gap:taper_history`，等待 P6 完整 MapperHistory。
- custom direction 支持 sketch axis / EdgeN / DatumLine 子集。
- Sketch、Body、FeatureBase placement 已进入运行态变换。

## 已知缺口

- taper 已有 section-level generated history 透传；完整 `Generated/Modified` MapperHistory 生命周期尚未迁移。
- `Symmetric + UpTo*` 的完整 mirror 语义仍需补齐。
- sketch axis ownership、外部 Part feature axis、attachment/support/subname 恢复仍不完整。
- 复杂 placement 下的 stable subname 恢复依赖 P6 继续补强。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/feature_extrude.*` | 双侧、对称、方向、终止候选、taper 组合和 partial history 接入 |
| `geometry/extrusion_helper.*` | OCCT prism / taper helper、ThruSections maker 与 section 来源 |
| `geometry/placement.*` | placement 转换 |
| `topo/named_shape.*` | prism 和 XOR history 子集 |

## FreeCAD 依据

- `src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `src/Mod/PartDesign/App/FeaturePad.cpp`
- `src/Mod/PartDesign/App/FeaturePocket.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`

## 验收

- `fixtures/p3b` 覆盖双侧、对称、UpTo、UpToFirst/Last、taper、inner-wire taper、custom direction 和 placement。
- 未支持组合必须 diagnostics，不回退成 One side。
- taper partial history 可验证一侧 / 内环 source face、source edge 和 generated section key，但完整 MapperHistory 缺口必须保留显式标记。
