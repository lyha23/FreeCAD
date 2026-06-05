# TopoNaming 与 History 完全体

## 目标

C3-M1 / C3-M2 的目标是把 CAD Core 2.0 中显式暴露的拓扑命名与引用恢复 gap 收敛为正式主路径。该主线是 3.0 的前置主线；在它完成前，不继续扩大高层 executor 的 fixture 特判。

## FreeCAD 依据

优先读取：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedName.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::updateElementReference()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.cpp`

## cad-core 落点

| 层 | 落点 | 职责 |
| --- | --- | --- |
| `app` | `app/document.*`、`app/document_object.*`、`app/property_links.*`、`app/element_map.*` | ReferenceShadow、StableSubList、FullSubList、ExternalGeometry native slots、document hash 和 ElementMap 输入模型 |
| `graph` | `graph/recompute_plan.*` | request-side dependency、missing external document 和 frozen / missing external geometry 分流 |
| `runtime` | `runtime/recompute.*` | reference resolver、写回建议、rename recovery、diagnostics |
| `part` | `part/topo_shape.*`、`part/topo_shape_mapper.*`、`part/topo_shape_expansion.*`、`part/topo_shape_reference.*`、`part/shape_fix.*`、`part/part_import.*`、`part/face_maker.*`、`part/wire_joiner.*` | full MapperHistory、TopoShape / NamedShape、stable subname 恢复、ShapeFix / import / FaceMaker / WireJoiner producer evidence |
| `sketcher` | `sketcher/sketch_object*.*`、`sketcher/sketch_internal_builder.*` | 只按 SketchObject 调用顺序消费 ExternalGeometry / InternalShape evidence，不合成拓扑语义 |
| `part_design` | `part_design/*` | 只传递 PartDesign maker evidence，不在 executor 输出端修 subname |
| `adapters` | CLI / C ABI / Worker / WASM | 暴露 capabilities / diagnostics，不做引用恢复 |

## C3-M1：full MapperHistory producer 生命周期

交付内容：

- `complete_mapper_history` 这类笼统 gap 已在 capabilities 中拆成可执行 producer / lifecycle 残项；当前仅剩 `hole_threaded_model_thread_profile_head_oracle_matrix`。ElementMap child-map source range preserve、nested child-map recursive source range、child-map postfix source range、hashed child-map key evidence、ElementMapPolicy::Propagate `makeElementWires` edge identity、ElementMapPolicy::Propagate `makeElementShell` source-to-shell map、Part::Compound makeElementCompound、Offset2D `Intersection=false` compound child recursion 与 Offset2D `Intersection=true` compound collective MakeOffsetFix 已落为 producer coverage。
- ShapeFix 主路径 history：已覆盖 deleted-small-edge 和 ShapeFix Root modified 证据；本地 FreeCAD `ShapeFix_Wire` generated history 已审计为空，不补 synthetic generated fixture。
- import shape ElementMap：导入 STEP / IGES / BREP 等 Part TopoShape 时建立稳定 source ownership；Mesh import 不伪装为 Part TopoShape ElementMap。
- Part Boolean、General Fuse、Section、Refine、DressUp、Transformed、Link retag 后继续传播 terminal split / deleted / merge。
- 一对多 split、deleted、ambiguous 不写唯一 target，只写 terminal history 和 diagnostics；C3-M1 已用 `mapper-history-ambiguous-split` 约束 `subname_split_requires_reselect` 状态。

完成判定：

- `topo_history.remaining_gaps` 不再保留笼统 `complete_mapper_history`；producer matrix 继续暴露已覆盖 producer，未完成项必须落到具体 lifecycle / producer gap。
- `topo_history.producer_matrix` 已进入 C ABI capabilities：已显式列出 prism、body_boolean、part_boolean、section、general_fuse、refine、shape_fix、import_shape、link_retag、sketch_internalshape、taper_thru_sections、dressup、transformed、Hole 的 covered / remaining 状态；body_boolean / part_boolean 已覆盖 flagged source 的 Cut tool slot compound expansion；Hole 已补 `hole-supported-model-thread-counterbore` native known-gap fixture，remaining 指向 FreeCAD `makeThread()` / `findHoles()` local-frame 与 head-cut residual topology 收敛。
- `shapefix_history` 已拆为 `shapefix_deleted_small_edge`、`shapefix_root_modified_history` 和 `generated_empty_review`；`import_shape_element_map` 已进入 `topo_history.maker_history`，当前覆盖 STEP / IGES / BREP owner-qualified alias；`sketch_internalshape` 已覆盖 FaceMaker / WireJoiner producer evidence 和 bounded-face/open-wire mixed oracle 第一切片。
- `tests/src/Mod/Part/App/WireJoiner.cpp::Generated` 明确记录 ShapeFix_Wire history 未调用 `AddGenerated()`，因此 ShapeFix generated 已从 implementation gap 降级为 native-empty 证据项，不新增 generated 假 fixture。
- ElementMapPolicy::Drop 和 ambiguous split 已有 C3-M1 probe；后续不把 split reselect 当唯一 target 恢复。
- P5 / P6 / P7 / P8 / C3-M4 / C3-M5 的稳定引用更新不因 ShapeFix、import、Part Section、DressUp、transformed/pattern 或 Link retag 中断；Part Section 已覆盖 source-qualified edge history、terminal deleted history、`Approximation` 与 auto-fuzzy 第一片；DressUp Face selection 已记录请求 subname 到实际 EdgeN 的展开证据，DressUp empty / invalid / unsupported selection 与 invalid parameter 已有结构化 diagnostics，链式 DressUp + Pattern 已覆盖 SupportTransform AddSubShape cache 与 terminal split/deleted 传播，`transformed_pattern_full_history` 已用 LinearPattern multi-original + App::Link retag 组合 fixture 收口。

## C3-M2：ExternalGeometry native 生命周期

交付内容：

- 为旧 `ExternalGeo` 几何持久复用建立无状态请求模型：前端持久化必要 graph 字段，后端只在请求内消费。
- Frozen / Detached 在无源对象或源对象变化时可以复用旧几何证据，不能伪装成新投影。
- Missing 修复路径输出稳定 diagnostics 和 `documentObjectUpdates` / `elementReferenceUpdates`。
- cross-document postfix、FullSubList、document hash、source object rename、label rename 进入同一 resolver。
- ReferenceShadow fingerprint / BREP snapshot 只作为旧单 subshape 证据，不作为建模输入。

当前基线：

- Source object rename 已有第一条主路径：当旧 LinkSub `value` 缺失、`ReferenceShadow.targetId` 唯一匹配当前文档对象时，`app/document.*`、`app/document_object.*`、`app/property*.*` 在 graph 前把 link 规范化到当前对象名，`runtime/recompute.*` 通过 `elementReferenceUpdates` 写回新 `value`、当前 `SubList`、`StableSubList`、`ShadowSub` 和刷新后的 `ReferenceShadow.target`。
- Label rename 已有第一批主路径：当 direct LinkSub / XLinkSub 的 persisted `SubList` 使用 stale leading `$OldLabel.`，且 link target 当前 `Label` 唯一可用时，`app/document.*`、`app/document_object.*`、`app/property*.*` 在 graph 前改写到 `$CurrentLabel.`；当 nested Link / Group path 中的 `$OldLabel.` 可通过 document-only prefix 解析到唯一当前对象时，也写回当前 `$Label.`。`runtime/recompute.*` 通过 `elementReferenceUpdates.labelReferenceRename` 写回新 `SubList`；若当前 target `Label` 重复，则输出 `label_reference_ambiguous`，不猜测改写。
- Cross-document nested label 已有第一条组合路径：显式 `FullSubList` 中的 `Doc#nested.$OldLabel.` 会先通过 request-side `Document` evidence 归一化 document prefix，再复用 nested Link / Group label resolver；`runtime/recompute.*` 在同一条 `elementReferenceUpdates` 中同时返回 `FullSubList`、`labelReferenceRename` 和 `documentReference`。
- XLink document restore 已有第一条主路径：`app/document.*`、`app/document_object.*`、`app/property*.*` 读取 request-side `Document {file,name,label,stamp,status,currentName,currentLabel,currentStamp,currentStatus,allowPartial}`，`runtime/recompute.*` 在 name / label 变化时输出 `elementReferenceUpdates.documentReference`，在 stamp/hash 变化时输出 warning `document_hash_mismatch`。`graph/recompute_plan.*` 在带 `Document` 证据但外部目标缺失时按 request-side status 分流：普通缺失输出 `missing_external_document`，pending / partial reload 输出 `external_document_pending_reload`，unloaded / deleted / detached 输出 `external_document_unloaded`；当第二次请求已带回外部目标对象时，仍从当前 `DocumentObject graph` 正常 recompute，不建立后端 document session。
- Missing 可解析修复已有第一条主路径：当 `ExternalGeometry.SubSet[].ExternalFlags` 带 `Missing` 且源对象 / subshape 当前可解析时，`sketcher/sketch_object*.*` 重建 ExternalGeometry，并通过 `documentObjectUpdates` 清掉 `Missing`。
- Frozen 旧几何复用已有第一条无状态快照路径：当 `ExternalGeometry.SubSet[].ExternalFlags` 带 `Frozen` 且没有 `Sync`，并提供单 subshape `ReferenceShadow.brep` 时，`graph/recompute_plan.*` 不把缺失源对象判为阻塞依赖，`runtime/recompute.*` 不再把该旧快照拿去校验当前源 subshape，`sketcher/sketch_object*.*` 在请求内解码并投影该旧 subshape。
- Missing 未解析旧几何已有第一条无状态快照路径：当 `ExternalGeometry.SubSet[].ExternalFlags` 带 `Missing`、源对象缺失且提供单 subshape `ReferenceShadow.brep` 时，`sketcher/sketch_object*.*` 在请求内投影旧 subshape，但不清掉 `Missing` flag，也不写 `documentObjectUpdates`。
- Frozen / Missing 缺失源对象且无 `ReferenceShadow.brep` 时已有 `missing_external_geometry_snapshot` graph diagnostic，不再落回普通 `missing_link_target`。
- Detached 已有第一条无状态 link-list 路径：当 `ExternalGeometry.SubSet[].ExternalFlags` 带 `Detached` 时，`sketcher/sketch_object*.*` 输出 `reason=external_geometry_detach` 的 `documentObjectUpdates`，删除对应 `ExternalGeometry.SubSet` entry。
- 原生旧 `ExternalGeo` 几何池已有第一条无状态请求模型：`SketchObject.Properties.ExternalGeo` 可携带 `Part::PropertyGeometryList` 风格 `Geometry` / `Values` / `Items`，条目复用 sketch geometry schema，并携带 `Ref`、`RefIndex`、`ExternalFlags` / `Flags`。Frozen / Missing 缺源时优先消费 matching `Ref` 的旧几何；Detached 会复用旧几何，同时在 `documentObjectUpdates` 中删除 `ExternalGeometry.SubSet` entry 并清理 matching `ExternalGeo` 条目的 `Ref` 与 `Detached` / `Missing` flag。

完成判定：

- ExternalGeometry 的 Defining / Frozen / Detached / Missing / Sync 已有 request-side 生命周期入口；native oracle 后续只用于阶段收口校验，不再作为 broad gap。
- Frozen / Detached 旧几何复用和 Missing 恢复不需要 executor 猜测。
- rename / document hash 失败时进入专用 diagnostics，不返回错误 shape。

## 不允许的实现路径

- 不在 `sketcher/sketch_object.cpp` 中按几何类型、顺序或 fixture 名称合成 split ownership。
- 不在 adapter 或 JSON 输出层修正 subname。
- 不把旧完整 BREP 作为长期前端状态。
- 不把 Link retag、ExternalGeometry、ReferenceShadow 各自做成独立旁路 resolver。

## 验收命令

本主线代码修改后优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_adapters
```

阶段收口时执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures
```
