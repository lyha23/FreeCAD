# P3b：FeatureExtrude 双侧、方向与 Placement

P3b 固定 Pad / Pocket shared `FeatureExtrude` 的常用方向矩阵和坐标传递。

## 当前基线

- `SideType=Two sides`、`Symmetric`、`Length2`、第一侧 / 第二侧 `UpToFace` / `UpToShape` 已有基础路径。
- `UpToFirst` / `UpToLast` 可基于 previous-body 候选面选择终止面。
- Pad / Pocket taper 已从 partial history 收口到 P6 MapperHistory 路径；一侧 / 内环 taper 已记录 `BRepOffsetAPI_ThruSections` 的 source face first-section history，一侧、多侧和内环 taper 已记录 source edge / generated section history，当前对象结果暴露 `topo_naming_history=maker_history:taper_thru_sections`，不再输出对象级 `topo_naming=known_gap:taper_history`。
- custom direction 支持 sketch axis / EdgeN / DatumLine 子集。
- Sketch、Body、FeatureBase placement 已进入运行态变换。

## 已知缺口

- UpToShape face-list + taper 这类组合仍按 unsupported / diagnostic 边界处理；Length / Two sides / Symmetric / inner-wire taper 的 ThruSections history 已进入 P6 正式账本。
- `Symmetric + UpTo*` 的完整 mirror 语义仍需补齐。
- sketch axis ownership、外部 Part feature axis、attachment/support/subname 恢复仍不完整。
- 复杂 placement 下的 stable subname 恢复依赖 P6 继续补强。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `part_design/feature_extrude.*` | 双侧、对称、方向、终止候选、taper 组合和 ThruSections history 接入 |
| `part/extrusion_helper.*` | OCCT prism / taper helper、ThruSections maker 与 section 来源 |
| `geometry/placement.*` | placement 转换 |
| `part/topo_shape.*` | prism、XOR 和 taper ThruSections history |

## FreeCAD 依据

- `src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `src/Mod/PartDesign/App/FeaturePad.cpp`
- `src/Mod/PartDesign/App/FeaturePocket.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`

## 验收

- `fixtures/p3b` 覆盖双侧、对称、UpTo、UpToFirst/Last、taper、inner-wire taper、custom direction 和 placement。
- 未支持组合必须 diagnostics，不回退成 One side。
- taper ThruSections history 可通过对象级 `topo_naming_history=maker_history:taper_thru_sections`、`NamedShape.element_map_status=history_partial`、maker-history generated events 和 C ABI `taper_history=covered_full` 验证；旧 `known_gap:taper_history` 已关闭。
