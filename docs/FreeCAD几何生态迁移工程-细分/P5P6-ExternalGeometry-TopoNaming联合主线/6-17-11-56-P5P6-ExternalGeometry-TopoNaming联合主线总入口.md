# P5/P6 ExternalGeometry / TopoNaming 联合主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的第一条实施主线：把 P5 Sketcher 外部几何 / 内部元素与 P6 TopoNaming / 引用恢复合并推进。

对应上游方案入口是 `docs/CADCore方案/细化方案/13-ExternalGeometry-TopoNaming下一阶段主线.md` 的实施顺序：补完整 MapperHistory / ElementMap 生命周期、FaceMaker / WireJoiner history producer、统一 reference resolver，以及 `ExternalGeometryExtension` 的 Defining / Frozen / Detached / Missing / Sync 状态机。

## 主线目标

- `MapperHistory` 成为 `NamedShape` / `ElementMap` / stable subname 的统一生命周期入口，覆盖 identity、preserved、generated、modified、split、merge、deleted、ambiguous 和 source-prefixed key。
- `FaceMaker` / `WireJoiner` 不再只输出 summary 或几何匹配结果，而要产出可被 topo 命名层消费的 history evidence。
- 旧引用恢复统一走 `ElementMap`、`MapperHistory`、mapped postfix、source key 和 `ReferenceShadow` 单 subshape evidence，不在 sketch executor、adapter 或导出层猜测。
- `ExternalGeometryExtension` 能表达并消费 Defining / Frozen / Detached / Missing / Sync，且这些状态只影响本次 request-local recompute，不把 BREP 或 shape 缓存变成长期状态。
- `Sketch.InternalShape`、ExternalGeometry 投影、UpToFace / LinkSub 等引用更新都通过同一套 resolver 和诊断输出。

## 当前基线

当前 P5/P6 已具备主路径骨架，S6 已消费 live blocker queue 的可实现部分，但仍不是发布完成状态：

- P5 已有 Sketch profile、基础约束、ExternalGeometry 子集、InternalShape bounded split 子集、FaceMakerBuildFace pre-split / splitter summary、WireJoiner EdgeInfo / WireInfo 部分账本和 terminal split / deleted history。
- P6 已有 `NamedShape` / `ElementMap` 基础、prism / Body boolean / RefineModel / transformed copy / Link retag 的 maker history 子集、ReferenceShadow 恢复和 split / deleted diagnostics。
- S2 已确认 MapperHistory event schema、ElementMap child-map / mapped postfix、resolver、ReferenceShadow、ExternalGeometry 基础投影和 PartDesign 下游消费者有 checked-in evidence；S3 已确认 projection/intersection、flags parse/capability 和 source-prefixed Missing recovery 保持 supported，同时保留 Defining 与 Frozen/Sync/Detached/Missing 的未采 oracle 队列；S4 已确认 MapperHistory / ElementMap / ReferenceShadow 的 request-local 生命周期边界，保持 `P5P6-SCOPE-001/002/003` supported，`P5P6-SCOPE-012` unsupported；S5 已确认 FaceMaker / WireJoiner producer 主路径和 fallback 删除边界；S6 已关闭 `P5P6-BLOCK-003/004/006`，把 `P5P6-SCOPE-009/010/011` 升级为 supported，把 `P5P6-SCOPE-013` 作为发布 fallback audit 关闭。`P5P6-BLOCK-001/002` 因 native `Sketch.addExternal()` probe 崩溃继续 `notCollected`，`P5P6-BLOCK-005` 继续 unsupported diagnostic，因此主线不能标为发布完成。

## 证明链条

本主线不按“先写哪个文件”推进，而按证据链推进：

```text
live 基线复核
  -> FreeCAD 源码候选矩阵
  -> scope review / nonGoal / blocker queue
  -> FreeCAD oracle 与 focused fixture
  -> topo / part / runtime / document / sketcher 实现
  -> executor 主路径切换与 fallback 删除
  -> 发布闸门与台账回写
```

关键约束：

- `notCollected` 行只能先采 FreeCAD oracle 或补 focused semantic test，不能直接进入 C++ 实现。
- `backendGap` 行必须有 FreeCAD 源码依据和当前 cad-core 不匹配证据，不能凭 fixture 输出倒推。
- 同类一对多 split 不能伪造成唯一 `ElementMap` target；只能在 history 足够唯一时恢复，否则输出稳定 split diagnostic。
- `ReferenceShadow.brep` 只能作为旧单 subshape snapshot 的恢复证据，不作为建模输入、完整对象 BREP 或跨请求 shape 缓存。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| ExternalGeometry rebuild | `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()` | 读取 `ExternalGeometry` / `ExternalTypes` / `ExternalGeo`，处理 `Missing`、`Frozen`、`Sync`、`Defining`，调用 `GeoFeature::resolveElement()` 恢复旧引用 |
| ExternalGeometry flags | `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.*` | 保存 `Defining` / `Frozen` / `Detached` / `Missing` / `Sync` 等状态位、ref 和 name |
| 引用更新 | `~/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::resolveElement()`、`GeoFeature::updateElementReference()` | 解析旧元素名、维护 `_ElementMapVersion`，触发 link property 更新 |
| LinkSub 更新 | `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` | 通过 `resolveElement()`、shadow 和 subname 更新 LinkSub |
| ElementMap 账本 | `~/Chili3DProject/FreeCAD/src/App/ElementMap.cpp`、`MappedName.cpp` | `addName()`、`find()`、`findAll()`、`getElementHistory()`、`traceElement()`、child map、postfix 和 hash/dehash |
| FaceMaker history | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()`、`FaceMakerBuildFace.cpp` | 消费 `myPreSplitHistory` 与 `mySplitter`，用 `MapperHistory` / `MapperMaker` 生成 element map |
| WireJoiner history | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoiner::getOpenWires()` | `EdgeInfo`、`WireInfo`、`wireInfo2`、`iteration2`、`superEdge`、`openWireCompound` 和 `BRepTools_History aHistory` 决定 open wire ownership |
| MapperHistory | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperMaker` / `MapperHistory` | 提供 `modified()` / `generated()`，支撑 `makeShapeWithElementMap()` |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| topo / app | `cad-core/include/cad_core/app/element_map.h`、`cad-core/src/app/element_map.cpp`、后续可扩展 `cad-core/src/part/topo_shape_mapper.cpp` | 定义 MapperHistory event、ElementMap lifecycle、mapped postfix、source-prefixed key、split / merge / deleted 终态 |
| part | `cad-core/include/cad_core/part/face_maker.h`、`cad-core/src/part/face_maker.cpp`、`cad-core/include/cad_core/part/wire_joiner.h`、`cad-core/src/part/wire_joiner.cpp` | 迁移 FaceMaker / WireJoiner history producer 和内部账本，不把 ownership 推给 sketch executor 猜 |
| runtime | `cad-core/include/cad_core/runtime/compute_context.h`、`cad-core/src/runtime/recompute.cpp`、`cad-core/src/runtime/io.cpp` | 统一 reference resolver、`elementReferenceUpdates`、diagnostics 和 writeback 建议 |
| document / app | `cad-core/include/cad_core/app/document.h`、`cad-core/src/app/document.cpp`、`cad-core/include/cad_core/app/property_links.h`、`cad-core/src/app/property_links.cpp` | 解析 FreeCAD 风格对象图、LinkSub、ReferenceShadow、ExternalGeometry 状态字段 |
| sketcher | `cad-core/include/cad_core/sketcher/sketch_object.h`、`cad-core/src/sketcher/sketch_object.cpp`、`cad-core/src/sketcher/sketch_object_external.cpp` | 消费 ExternalGeometryExtension 状态机和 resolver 结果；只表达 SketchObject 调用顺序，不承担 topo 推断 |
| adapters | `cad-core/src/adapters/cli/cli.cpp`、`cad-core/src/adapters/c_api/c_api.cpp` | 暴露 capabilities、diagnostics、mesh/subshape/update 响应；不承载业务语义 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-17-11-56-P5P6工作步骤与矩阵拆分逻辑总览.md` | 和 L2 工作步骤总入口一致，索引 S0-S6、执行顺序、当前闸门和状态纪律 |
| S0 声明口径 | `工作步骤细分/6-17-11-57-P5P6-S0-声明口径与live基线复核.md` | 已冻结本主线纳入 / 排除清单、禁用话术、状态词典和 S1-S6 准入边界 |
| S1 源码候选 | `工作步骤细分/6-17-11-58-P5P6-S1-FreeCAD源码候选矩阵.md` | 已从 FreeCAD 源码生成候选清单，不判定 supported |
| S2 范围准入 | `工作步骤细分/6-17-11-59-P5P6-S2-范围准入与blocker矩阵.md` | 已把候选转成 scope review、nonGoal、blocker 和 backendGap 分类 |
| S3 ExternalGeometry | `工作步骤细分/6-17-12-00-P5P6-S3-ExternalGeometry状态机专项复审.md` | 已裁决 Defining / Frozen / Detached / Missing / Sync 与 solver/live editing 边界；`P5P6-BLOCK-001/002` 继续等待 FreeCAD oracle |
| S4 MapperHistory / ElementMap | `工作步骤细分/6-17-12-01-P5P6-S4-MapperHistory与ElementMap生命周期专项复审.md` | 已裁决 request-local history / ReferenceShadow evidence lifecycle 与跨请求持久状态边界 |
| S5 FaceMaker / WireJoiner | `工作步骤细分/6-17-12-02-P5P6-S5-FaceMaker-WireJoiner-history-producer专项复审.md` | 裁决 history producer、InternalShape 主路径和 fallback 删除边界 |
| S6 发布闸门 | `工作步骤细分/6-17-12-03-P5P6-S6-Oracle实现与发布闸门.md` | 已消费可实现 blocker 并回写台账；因 `P5P6-BLOCK-001/002` native oracle 未采集，S6 不标 `【已实现】` |
| source candidates | `矩阵/p5p6_source_candidates.tsv` | FreeCAD 源码候选清单，S1 已扩充为 47 条；候选不等于实现任务 |
| scope review | `矩阵/p5p6_scope_review_matrix.tsv` | S2 已分类 15 条 scope，记录状态、依据、cad-core evidence、落点和下一步 |
| blocker queue | `矩阵/p5p6_blocker_queue.tsv` | S6 已关闭 `P5P6-BLOCK-003/004/006`；`P5P6-BLOCK-001/002` 继续 notCollected，`P5P6-BLOCK-005` 继续 unsupported |
| nonGoal registry | `矩阵/p5p6_non_goal_registry.tsv` | S2 已整理 6 条公开排除项、用户表现和重新打开条件 |
| backendGap 分类 | `矩阵/p5p6_backend_gap_classification.tsv` | S6 已把 FaceMaker concrete producer 与 WireJoiner recoverable parity 两条 P0 backendGap 标为 closed；当前无未关闭 P0/P1 backendGap |

当前 S0 已实现，已冻结声明口径与 live 基线复核；S1 已实现，已把本地 FreeCAD 源码入口扩充为候选矩阵；S2 已实现，已完成范围准入与 blocker 矩阵；S3 已实现，已完成 ExternalGeometry 状态机专项复审；S4 已实现，已完成 MapperHistory / ElementMap 生命周期专项复审；S5 已实现，已完成 FaceMaker / WireJoiner history producer 专项复审；S6 已执行 producer / fallback 发布闸门的可验证部分，但因 `P5P6-BLOCK-001/002` native oracle 未采集而不标 `【已实现】`。候选矩阵、S2 分类、S3/S4/S5 裁决和 S6 部分收口都不是整体 P5/P6 发布完成结论。

## 实施边界

本主线包含：

- 完整 MapperHistory event model 和 ElementMap lifecycle。
- FaceMakerBuildFace pre-split / splitter history producer。
- WireJoiner EdgeInfo / WireInfo / openWireCompound / noOriginal / tight-bound ownership history producer。
- 统一旧引用 resolver 与 ReferenceShadow evidence 消费。
- ExternalGeometryExtension Defining / Frozen / Detached / Missing / Sync 状态机。
- Sketch InternalShape / ExternalGeometry 主路径切换和旧 fallback 删除。

本主线不包含：

- 完整 Sketcher constraint solver。
- GUI、TaskPanel、ViewProvider、Workbench command。
- 持久后端文档、跨请求 shape cache、完整对象 BREP 状态。
- Assembly solver、完整 Link 持久写回事务、Worker / WASM adapter。
- 对一对多 split 的自动唯一恢复；除非 MapperHistory 能证明同类唯一 replacement。

## 完成判定

- `p5p6_blocker_queue.tsv` 中没有未关闭的 `notCollected` 或 `backendGap` 行。
- P5/P6 的 InternalShape、ExternalGeometry、UpToFace / LinkSub 旧引用恢复使用同一套 MapperHistory / ElementMap / resolver。
- FaceMaker / WireJoiner 的关键 ownership 不再只是 summary 或几何匹配 fallback，而能被 `NamedShape` / `ElementMap` 或稳定 diagnostics 消费。
- ExternalGeometryExtension 五类状态均有 FreeCAD oracle / fixture / focused semantic test 覆盖，或被明确保留为 `notCollected` / `unsupported` / `nonGoal` 路由且 blocker 未关闭。
- 旧 fallback 和 fixture-specific 输出修正已删除；保留的 fallback 必须有 FreeCAD 依据、适用边界、删除条件和 diagnostic。
- 文档台账回写到 `docs/CADCore方案/细化方案/08-P5-Sketcher核心与内部元素.md`、`09-P6-TopoNaming主路径.md`、`13-ExternalGeometry-TopoNaming下一阶段主线.md`。
