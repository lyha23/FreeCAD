# ExternalGeometry / TopoNaming 下一阶段主线

本方案把当前分散在 P5 / P6 的缺口合并为下一阶段重点工作：完整 `ExternalGeometryExtension` 状态机、完整 MapperHistory、FaceMaker / WireJoiner history 消费和复杂旧引用恢复。目标不是扩大 fixture 特判，而是补齐 FreeCAD 依赖的内部账本，让外部几何引用、InternalShape 和 stable subname 都走同一条 history 主路径。

## 目标边界

- ExternalGeometry 能表达 Defining / Frozen / Detached / Missing / Sync 等状态，并能按 FreeCAD `SketchObjectExternal` 的 rebuild 路径决定复用、刷新、跳过或诊断。
- MapperHistory 成为 `NamedShape` / `ElementMap` 的完整生命周期入口，覆盖 generated、modified、deleted、split、merge、source-preserved alias 和 source-prefixed stable key。
- FaceMaker / WireJoiner 不只输出 summary 或后处理结果，而要产出可被 `topo` 消费的 history evidence，用于 InternalFace / InternalEdge / InternalVertex 的来源、split 和 deleted 传播。
- 旧引用恢复统一走 ElementMap、MapperHistory 和 `ReferenceShadow` evidence，不在 ExternalGeometry executor、adapter 或导出层按几何类型、fixture 名称或 fragment 顺序猜测。

## FreeCAD 调用链

- `src/Mod/Sketcher/App/SketchObjectExternal.cpp`：`SketchObject::rebuildExternalGeometry()` 读取外部链接、ExternalGeometry / ExternalTypes、Missing / Frozen / Sync 状态，并把恢复后的几何交给草图内部几何和约束侧使用。
- `src/Mod/Sketcher/App/ExternalGeometryExtension.*`：`ExternalGeometryExtension` 维护外部几何元数据和状态位，决定 defining geometry、detached geometry、missing reference 与同步行为。
- `src/App/GeoFeature.cpp` 与 `src/App/PropertyLinks.cpp`：`GeoFeature::updateElementReference()`、`PropertyLinkBase::_updateElementReference()` 和 `resolveElement()` 负责把旧 subname 通过 ElementMap / shadow evidence 更新到当前 subname。
- `src/App/ElementMap.*` 与 `src/App/MappedName.*`：维护旧元素名、新元素名、mapped postfix、source key 和 element reference 的解析规则。
- `src/Mod/Part/App/TopoShape*.cpp`、`TopoShapeMapper.cpp` 与 `PropertyTopoShape.cpp`：承接 `TopoShape` maker history、`NamedShape` / `ElementMap` 保存和引用更新入口。
- `src/Mod/Part/App/FaceMaker*.cpp`：`FaceMaker::postBuild()` 消费 pre-split / splitter history，把构面结果、outer wire 来源和 split / generated 关系传给命名层。
- `src/Mod/Part/App/WireJoiner.cpp`：`WireJoinerP::EdgeInfo`、`WireInfo`、`wireInfo` / `wireInfo2`、`iteration` / `iteration2`、`superEdge` 和 `getOpenWires(noOriginal)` 决定 open-wire fragment ownership 与 history 过滤。

## cad-core 分层落点

| 层 | 落点 | 职责 |
| --- | --- | --- |
| `document/` | 外部几何链接、ReferenceShadow、extension flags 解析 | 保存 FreeCAD 风格对象图字段；除 `ReferenceShadow.brep` 单 subshape snapshot 外，不保存完整 BREP 或跨请求 shape |
| `topo/` | `MappedName`、`ElementMap`、`NamedShape`、MapperHistory | 统一 generated / modified / deleted / split / merge / source alias / mapped postfix 生命周期，并输出可解释 diagnostics |
| `geometry/` | FaceMaker / WireJoiner / ShapeFix history producer | 负责 OCCT 结果和内部账本，不在输出层 pruning，也不把 ownership 交给 sketch executor 猜 |
| `runtime/` | reference resolver、`elementReferenceUpdates`、`documentObjectUpdates` | 统一旧 subname 到当前 subname 的解析、ReferenceShadow 校验和可写回建议 |
| `features/sketch_object.*` | ExternalGeometry rebuild 和状态机消费 | 表达 `SketchObjectExternal` 的调用顺序、Defining profile 使用、Frozen / Missing / Detached / Sync 语义 |
| `adapters/` | CLI / C ABI capability 暴露 | 只转协议和暴露 diagnostics / updates，不承载引用恢复业务逻辑 |

## 核心结构草案

`topo` 先定义统一 history event，而不是让各 executor 自己拼 ElementMap：

| 字段 | 含义 |
| --- | --- |
| `sourceObject` / `sourceSubname` | 旧引用来源，如 `Sketch.Edge1`、`Pad.Face5`、source-prefixed key 或 mapped postfix key |
| `sourceShapeKind` / `targetShapeKind` | Face / Edge / Vertex / Wire / Shape，用于防止跨类型误恢复 |
| `targetObject` / `targetSubname` | 当前请求内可解析目标；一对多 split 可有多个 target |
| `relation` | `identity`、`preserved`、`generated`、`modified`、`split`、`merge`、`deleted`、`ambiguous` |
| `makerStage` | `prism`、`body_boolean`、`face_maker_pre_split`、`face_maker_splitter`、`wire_joiner`、`shape_fix`、`refine`、`transform`、`dressup` 等 |
| `evidence` | OCCT maker history、`BRepTools_History`、`WireJoiner::EdgeInfo` / `WireInfo`、`ReferenceShadow` fingerprint、indexed subshape map |
| `recoverability` | `unique` 可写入 ElementMap；`ambiguous_split` 只给诊断；`deleted` 只给 deleted terminal history |

`runtime` 再基于 history event 产出引用解析结果：

| 结果 | 行为 |
| --- | --- |
| `resolved` | 返回当前 subname，并生成可写回 `elementReferenceUpdates` |
| `resolved_with_shadow` | ElementMap 不足时由 `ReferenceShadow` fingerprint 佐证，仍记录 shadow evidence |
| `ambiguous_split` | 旧元素分裂为多个同类 target，不能伪造唯一目标 |
| `deleted` | 旧元素被删除，保留 terminal deleted history 和稳定 diagnostic |
| `missing_object` / `missing_subshape` | 对象或 subshape 缺失，进入 ExternalGeometry Missing 路径 |
| `unsupported` | 当前几何或 history producer 未迁移，返回 known gap diagnostic |

`features/sketch_object.*` 只消费状态机结果，不负责 history 推断：

| 记录 | 行为 |
| --- | --- |
| `ExternalGeometryRecord.ref` | 对齐 FreeCAD `ExternalGeometryFacade::getRef()`，保存对象和 subname key |
| `ExternalGeometryRecord.flags` | Defining / Frozen / Detached / Missing / Sync |
| `resolvedReference` | 来自 `runtime` resolver 的当前对象、subname、diagnostic 和写回建议 |
| `geometrySnapshot` | 来自请求 graph 内 `ExternalGeo` / 单 subshape `ReferenceShadow`；后端不得把它变成跨请求缓存 |

## ExternalGeometry 状态机

| 状态 / 触发 | cad-core 行为 |
| --- | --- |
| 无特殊 flag | 通过 resolver 刷新外部 subname，重新投影 / 相交；成功时清理旧 Missing diagnostic |
| `Defining` | 外部几何可参与 profile / shape 构建；reference-only 外部几何仍只作为约束和投影输入 |
| `Frozen` 且无 `Sync` | 不根据源对象重投影；只能使用请求 graph 中已有 ExternalGeo geometry 或 shadow evidence，缺失时输出 frozen-geometry-missing diagnostic |
| `Frozen + Sync` | 本次允许重新解析和投影；完成后建议清除 Sync，是否继续 Frozen 由输入 flag 保留 |
| `Detached` | 不再追随源对象；保留当前请求内几何作为 detached external geometry，引用更新不再改写源 subname |
| `Missing` | 先用 ElementMap / MapperHistory / ReferenceShadow 尝试恢复；成功时建议清除 Missing 并写回当前 subname，失败时保持 Missing diagnostic |
| 源对象或 subshape 删除 | 进入 deleted / missing 诊断；只有 history 能证明唯一 replacement 时才自动恢复 |

## 实施顺序

1. 补 topo history core：抽象 cad-core 自己的 MapperHistory 结构，明确旧名、新名、source object、shape kind、split/merge/deleted 终态、mapped postfix 和诊断编码；让现有 prism、Body boolean、RefineModel、taper partial history 都能逐步接入同一入口。
2. 补 FaceMaker / WireJoiner history producer：把当前 FaceMaker pre-split / splitter summary、WireJoiner EdgeInfo / WireInfo summary 升级为可消费 history；固定 `noOriginal` 过滤、open-wire carry-through、bounded owner slot、self-intersection split、source edge 一对多 fragment 和 source edge deleted 的通用表达。
3. 补 reference resolver：在 `runtime` 统一处理旧 `SubList`、`StableSubList`、source-prefixed key、mapped postfix、`ReferenceShadow` fingerprint 和 split/deleted diagnostics，产出前端可应用的 `elementReferenceUpdates`。
4. 补 `ExternalGeometryExtension` 状态机：在 `document` 解析并在 `features/sketch_object.*` 消费 Defining / Frozen / Detached / Missing / Sync；Frozen 可复用冻结几何或返回明确 diagnostics，Missing 必须走 resolver 和 shadow evidence，Detached 不再追随源对象，Sync 控制是否刷新投影。
5. 切换 Sketch InternalShape / ExternalGeometry 主路径：InternalShape 的 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 和 ExternalGeometry projection 都通过 topo history 解析，不再依赖几何排序、source edge 猜测或输出端修剪。
6. 删除临时 fallback：每个历史上的 summary-only、geometry-match 或 fixture-specific fallback 都要有删除条件；切换后保留的 fallback 必须标明边界、FreeCAD 依据和 diagnostics。

## 验收矩阵

| 场景 | 必须验证 |
| --- | --- |
| ExternalGeometry indexed edge / face / vertex | 旧 subname 通过 ElementMap 更新到当前 subname，投影结果与 FreeCAD oracle 对齐 |
| source-prefixed stable key | Body / Pad / Pocket / Sketch source key 经 MapperHistory 解析，不从当前输出反推 |
| Defining external profile | defining external geometry 能参与 profile 语义，且与 construction / reference-only 外部几何区分 |
| Frozen / Sync | 源对象变化时 Frozen 不刷新或给出冻结能力诊断，Sync 控制投影刷新建议 |
| Detached | detached geometry 不再跟随源对象更新，但仍保留可显示 / 可约束几何 |
| Missing | 缺失对象、缺失 subshape、deleted target 输出结构化 diagnostics，并尽量用 ReferenceShadow / MapperHistory 给出恢复建议 |
| FaceMaker bounded / self-intersection | `InternalFaceN` 来源于 outer boundary，self-intersecting edge split 只记录 split history，不伪造一对一 ElementMap |
| WireJoiner open-wire `noOriginal` | 原始 open edge 被过滤，非原始 split fragment 可保留；source 到多 fragment 记录 split history |
| split / merge / deleted 跨特征传播 | history 经 Body boolean、RefineModel、transformed、DressUp、Link retag 后仍能追溯或诊断 |
| 复杂引用恢复失败 | 一对多无法唯一恢复时输出 split diagnostic，不按面积、长度、索引或 geometry type 猜唯一目标 |

## 非目标

- 不在本阶段实现完整 Sketcher constraint solver。
- 不把 BREP 作为前端或后端长期状态；只有 `ReferenceShadow.brep` 的旧单 subshape snapshot 例外。
- 不实现 GUI task panel、ViewProvider、Workbench 行为。
- 不把 Assembly solver、完整 Link 持久写回事务或 Worker / WASM adapter 作为本阶段前置条件。

## 完成判定

- P5 / P6 中 ExternalGeometry、InternalShape 和 stable subname 的主路径都能引用同一个 MapperHistory / ElementMap 账本。
- FaceMaker / WireJoiner 的关键 ownership 不再只作为 summary 旁路存在，而能被 `NamedShape` / `ElementMap` 或诊断消费。
- 旧引用恢复有统一 resolver，成功时返回当前 subname 和写回建议，失败时返回稳定诊断。
- 相关 fixture / oracle 覆盖成功恢复、split 无法唯一恢复、deleted target、Missing / Detached / Frozen / Sync 状态和 InternalShape split / deleted 传播。
