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

当前状态：

- 已新增 `cad-core/include/cad_core/topo/mapper_history.h` 与 `cad-core/src/topo/mapper_history.cpp`，作为 `topo` 层统一 history event core。
- `NamedShape` JSON 保持旧 `history` 兼容，并新增 `mapper_history`；旧 `ElementHistory`、唯一 `element_map` preserved alias、terminal split / deleted、merge、Link retag、transformed copy 和 Sketch InternalShape 的 FaceMaker / WireJoiner summary 状态会序列化或转换为统一 event。
- `mapper_history` event 当前字段固定为 source endpoint、target endpoint、shape kind、relation、maker stage、evidence、recoverability、diagnostic status。
- `ElementMap` 唯一性规则保持不变：只有可唯一解析的 target 写入 `element_map`；split / deleted 只进入 terminal history、`mapper_history` 和 diagnostics，不猜唯一目标。

剩余缺口：

- ExternalGeometry Defining / Frozen / Detached / Missing / Sync 基础状态机已在 C2-M3 落地；旧 `ExternalGeo` 几何持久复用和复杂 UI 修复流仍是后续 native 生命周期缺口。
- taper ThruSections、RefineModel 当前 P7 覆盖子路径与 DressUp SupportTransform AddSubShape slot 已在 C2-M5 接入统一 MapperHistory；transformed / pattern Add/Sub ownership 子路径已在 C2-M6 由 `transformed_pattern_addsub_ownership` 暴露，完整 pattern history 与 DressUp 复杂余量仍属于 C2-M5+。

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

当前状态：

- FaceMaker producer 已从 summary 升级为结构化 evidence：`edge_evidence` 表达 pre-split / splitter / deleted source edge，`bounded_face_evidence` 表达 bounded `InternalFaceN` 的 outer-boundary source。
- WireJoiner open export 已携带 producer identity、source edge lineage、target wire / edge 和 `noOriginal` purge 诊断；`topo/named_shape` 消费这些 evidence 写入 `mapper_history`、terminal split / deleted 和 diagnostics。
- `features/sketch_object.*` 只把 FaceMaker / WireJoiner evidence 传给 topo 并序列化 JSON，不再合成 split ownership。
- `internal_element_map` 保持唯一 alias 职责；source edge 一对多 fragment、source edge deleted、bounded owner slot 不写入唯一 target。

剩余缺口：

- C2-M3 已补 ExternalGeometry flags / resolver / 写回建议基础；完整 native `ExternalGeo` 持久复用仍未冻结。
- taper ThruSections、RefineModel 当前 P7 覆盖子路径与 DressUp SupportTransform AddSubShape slot 已在 C2-M5 接入统一 MapperHistory；transformed / pattern Add/Sub ownership 子路径已在 C2-M6 暴露；ShapeFix 主路径 / RefineModel 复杂 identity-change / transformed full history / DressUp 复杂余量的跨 feature history 收敛仍属于 C2-M5+。

验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

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

当前状态：

- `document/model.*` 支持 ExternalGeometry `ExternalFlags` 字符串数组、FreeCAD `Flags` bitset 和单 flag bool 输入，解析为 Defining / Frozen / Detached / Missing / Sync。
- `features/sketch_object.cpp` 消费状态机：Defining 投影几何进入 profile；Frozen 在没有 Sync 时不刷新；Detached 不追随源对象；Missing / Sync 成功后返回 `documentObjectUpdates` 清理一次性 flag。
- `runtime/recompute.cpp` 保留 ExternalFlags，并继续通过集中 ReferenceShadow resolver 产出 `elementReferenceUpdates`；split / deleted / ambiguous 只给 diagnostics 或 mapper diagnostic，不猜唯一 target。

剩余缺口：

- FreeCAD 旧 `ExternalGeo` 几何在 Frozen / Detached 下可持久复用；cad-core 当前无状态请求没有等价持久几何槽，只能跳过刷新并显式暴露状态。
- Missing 的复杂 UI 修复、跨文档 postfix 与 Link retag 后的完整生命周期仍依赖 C2-M7 的 Link / XLink 产品化。

验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_diagnostics tests.test_feature_flows tests.test_p6_topology tests.test_p5_sketch
```

## C2-M4：Sketch InternalShape 主路径切换

交付内容：

- `Sketch.InternalShape` 的 InternalFace / InternalEdge / InternalVertex 命名全部通过 MapperHistory / ElementMap 或 diagnostics 解释。
- 删除或隔离 summary-only、geometry-match、fixture-specific fallback。
- `features/sketch_object.*` 只表达 FreeCAD SketchObject 调用顺序，不承担 split history 合成。

完成判定：

- P5 fixture 覆盖 self-intersection、inter-edge intersection、open wire、bounded faces、source edge one-to-many、source edge deleted。
- `Face1` 不被伪装成稳定 `InternalFace1`。
- open wire 不产生假 `profile_ready=true`。

当前状态：

- `Sketch.InternalShape` 的复杂 InternalFace / InternalEdge / InternalVertex history 已切到 producer evidence 主路径：FaceMaker edge / bounded-face evidence 和 WireJoiner open-export identity 负责 generated / split / deleted / purge 解释。
- `consumeSketchInternalTerminalHistory()` 不再在缺少 edge evidence 时用 history stage、`internal_element_map` 缺口或 raw/internal 几何采样合成 split / deleted terminal history。
- `mapper_history` 中旧 FaceMaker / WireJoiner stage/count 汇总只作为 `summary_only:*` diagnostic event 保留，source / target 为空，recoverability 为 diagnostic，不参与引用恢复。
- `internal_element_map` 只保留 FreeCAD `SketchObject::getInternalElementMap()` 等价的 InternalEdge / InternalVertex 简单唯一 alias；source edge 一对多、deleted、bounded face ownership 均由 MapperHistory 或 diagnostics 解释。

剩余缺口：

- 旧 `ExternalGeo` 几何持久复用和跨文档 ExternalGeometry 生命周期仍属于 C2-M7 native 产品化边界。
- taper ThruSections、RefineModel 当前 P7 覆盖子路径与 DressUp SupportTransform AddSubShape slot 已在 C2-M5 接入统一 MapperHistory；transformed / pattern Add/Sub ownership 子路径已在 C2-M6 暴露；ShapeFix 主路径 / RefineModel 复杂 identity-change / transformed full history / DressUp 复杂余量 / PartDesign pattern 的完整 history 收敛进入 C2-M5 / C2-M6。

验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology
```
