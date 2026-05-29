# P4：Document、Property、Placement 完整化

P4 的目标是让 executor 面对统一的 FreeCAD 属性模型，而不是各自解析临时 JSON。

## 当前基线

- typed getter 覆盖 Bool、Number / Length / Angle、String / Enumeration、Vector、Placement。
- `PropertyLink`、`PropertyLinkList`、`PropertyLinkSub`、`PropertyLinkSubList` 已归一为 dependency links。
- `StableSubList` / `FullSubList` 已进入 document link 结构。
- graph 只消费 document 层 dependency links。
- App::Part / PartDesign::Body 的 group placement、Sketch placement、FeatureBase placement 已进入 global placement。
- DatumPlane support、DatumLine ReferenceAxis、DatumPoint parent Part placement 已覆盖基础场景。

## 已知缺口

- Sketch 专有 Geometry 输入还不是完整 FreeCAD 属性模型。
- object-local inverse placement 和复杂 GeoFeatureGroup 边界仍需加强。
- Origin / AttachEngine 的完整 attachment 坐标传递仍未迁移。
- property editor / expression / status flag 不是当前 CAD Core 范围。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `document/model.*` | typed property、link、placement 解析 |
| `graph/dependency_graph.*` | dependency edge 和 recompute plan |
| `runtime/recompute.*` | global placement 和 context 初始化 |
| `features/datum_*.*` | Datum / Origin 基础 executor |

## FreeCAD 依据

- `src/App/Property*.cpp`
- `src/App/PropertyLinks.cpp`
- `src/App/GeoFeature.cpp`
- `src/App/GeoFeatureGroupExtension.cpp`
- `src/Mod/PartDesign/App/Datum*.cpp`

## 验收

- `fixtures/p4` 覆盖 LinkList、LinkSubList、missing target、cycle、invalid link value、invalid typed property、invalid placement 和 placement 组合。
- executor 不再通过临时字段猜依赖对象。
- diagnostics 必须包含 object / property / stage / target / subname 中可用的定位信息。
