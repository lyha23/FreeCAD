# P5/P6：ExternalGeometry 与 TopoNaming 主线

本主线是 CAD Core 2.0 的第一优先级。它负责把当前分散的 ExternalGeometry、InternalShape、ReferenceShadow、`NamedShape`、`ElementMap` 和 maker history 子集统一成一条 FreeCAD 风格引用恢复路径。

## 目标边界

- ExternalGeometry 支持 Defining / Frozen / Detached / Missing / Sync 状态，并按 FreeCAD `SketchObjectExternal` 调用链决定刷新、复用、跳过或诊断。
- MapperHistory 成为 `NamedShape` / `ElementMap` 的统一入口，覆盖 identity、preserved、generated、modified、split、merge、deleted、source-prefixed stable key 和 mapped postfix。
- FaceMaker / WireJoiner 不再只输出 summary，而要产出可消费的 history evidence。
- 旧引用恢复统一走 ElementMap、MapperHistory 和 ReferenceShadow，不在 executor、adapter 或输出层猜唯一目标。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::updateElementReference()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedName.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoiner::getOpenWires()`

## cad-core 落点

| 层 | 文件 / 模块 | 职责 |
| --- | --- | --- |
| `document` | `document/model.*` | 解析 ExternalGeometry flags、ReferenceShadow、stable subname 输入 |
| `topo` | `topo/named_shape.*`、`topo/element_map.*`、新增 MapperHistory 类型 | 统一 history event、ElementMap、stable key、mapped postfix、split / merge / deleted 终态 |
| `geometry` | `geometry/face_maker.*`、`geometry/wire_joiner.*`、`geometry/sketch_internal_builder.*` | 产出 FaceMaker / WireJoiner history evidence，不做输出端修剪 |
| `runtime` | `runtime/recompute.*`、`topo/reference_matcher.*` | reference resolver、ReferenceShadow 校验、`elementReferenceUpdates` |
| `features` | `features/sketch_object.*` | 表达 `SketchObjectExternal` 调用顺序和 ExternalGeometry 状态机 |
| `adapters` | CLI / C ABI | 暴露 diagnostics、capabilities 和引用更新结果 |

## C2-M1：MapperHistory core

交付内容：

- 新增或重构 cad-core 自己的 MapperHistory 数据结构。
- 统一表达 source object、source subname、target object、target subname、shape kind、relation、maker stage、evidence、recoverability。
- 将现有 prism、Body boolean、RefineModel、taper partial history、Link retag、transformed copy 的 history 子集接入同一入口。
- 明确一对多 split、deleted、ambiguous 不写入唯一 ElementMap target，只写 terminal history 和 diagnostics。

完成判定：

- `NamedShape` JSON 能稳定输出统一 `history` / `element_history_status`。
- `ElementMap` 只包含可唯一解析的 target。
- P3 / P5 / P6 / P7 / P8 现有 expected 不发生语义倒退。

## C2-M2：FaceMaker / WireJoiner history producer

交付内容：

- 将 FaceMaker pre-split / splitter summary 升级为 MapperHistory event。
- 将 WireJoiner EdgeInfo / WireInfo、ordered vertices、iteration / iteration2、splitWire / done lifecycle、primary / secondary owner slot、`noOriginal` 过滤转为可消费 history evidence。
- 用真实 history 表达 source edge 一对多 fragment、source edge deleted、open-wire carry-through、bounded owner slot。
- 固定 `findTightBoundSplitWire()`、`findTightBoundUpdateVertices()`、`exhaustTightBoundUpdateWire()` 的迁移边界。

完成判定：

- `InternalFaceN` 来源于 FaceMaker outer boundary history。
- `InternalEdgeN / InternalVertexN` 的 split / deleted 由 WireJoiner / FaceMaker history 解释。
- 当前几何匹配式 `internal_element_map` 不再承担复杂 split 判断。

## C2-M3：Reference resolver + ExternalGeometry 状态机

交付内容：

- 在 `runtime` 建立统一 reference resolver，处理 `SubList`、`StableSubList`、source-prefixed key、mapped postfix、ReferenceShadow fingerprint 和 BREP snapshot。
- 在 `document` 解析 ExternalGeometry Defining / Frozen / Detached / Missing / Sync flags。
- 在 `features/sketch_object.*` 实现 ExternalGeometry 状态机消费：Defining 参与 profile，Frozen 不刷新，Sync 允许一次刷新，Detached 不追随源对象，Missing 先尝试恢复。
- 成功恢复时输出 `elementReferenceUpdates` / `documentObjectUpdates` 建议；失败时输出 stable diagnostics。

完成判定：

- ExternalGeometry edge / face / vertex 的 indexed 和 source-prefixed stable key 都能走 resolver。
- Missing / Frozen / Detached / Sync 都有成功路径和失败 diagnostics。
- 一对多 split 不自动猜唯一目标。

## C2-M4：Sketch InternalShape 主路径切换

交付内容：

- `Sketch.InternalShape` 的 InternalFace / InternalEdge / InternalVertex 命名全部通过 MapperHistory / ElementMap 或 diagnostics 解释。
- 删除或隔离 summary-only、geometry-match、fixture-specific fallback。
- `features/sketch_object.*` 只表达 FreeCAD SketchObject 调用顺序，不承担 split history 合成。

完成判定：

- P5 fixture 覆盖 self-intersection、inter-edge intersection、open wire、bounded faces、source edge one-to-many、source edge deleted。
- `Face1` 不被伪装成稳定 `InternalFace1`。
- open wire 不产生假 `profile_ready=true`。
