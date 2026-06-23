# Reference Resolution 深模块重构方案（已实现）

## 来源与结论

本方案来自 `/var/folders/5k/fms98vy54k18w9n0j5_53r400000gn/T/architecture-review-20260623-154500.html` 中的第三个候选项 `Extract Reference Resolution`。

当前排序判断：

- `Deepen ProfileBased Profile` 已实现并更名为已实现方案。
- `Collapse Sketch Internal Result` 已实现并更名为已实现方案。
- 因此下一步应处理 HTML 顺序中的第三项：`Extract Reference Resolution`，而不是跳到后面的 `Deepen Topology Response Export`。

当前结论：本轮已把 ReferenceShadow 验证、StableSubList / ShadowSub 更新、MapperHistory 诊断事件和 elementReferenceUpdates JSON 组装从 `runtime/recompute.cpp` 拆出，形成两个 runtime 深模块。`runtime/recompute.cpp` 只保留 recompute orchestration、executor dispatch、NamedShape 注册和对象级成功 / 失败控制。

## 已实现落点

- `cad-core/include/cad_core/runtime/reference_resolution.h`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/include/cad_core/runtime/element_reference_update.h`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/runtime/recompute.cpp` 调用 `validateObjectReferences()`，不再手写 ReferenceShadow 恢复策略或 update JSON projection。
- `cad-core/tests/test_adapters.py` 新增 focused assertions，约束 recovery metadata 和 `PropertyLinkSubList` 未更新项保留行为。

## 实现前基线

实现前 `cad-core/src/runtime/recompute.cpp` 中至少混合了这些职责：

- recompute 顺序、executor dispatch、全局 placement、NamedShape 注册。
- `currentSubshapeForReference()`：解析当前 subname、StableSubList、NamedShape ElementMap、InternalShape 映射。
- `recoverSubshapeForReference()`：根据 ReferenceShadow 在当前 Shape / InternalShape 中恢复唯一 subshape。
- `recordReferenceRecoveryMapperDiagnostic()`：把失败恢复转成 `MapperHistoryEvent`。
- `stableSubnamesForReferenceUpdate()`、`shadowSubsForReferenceUpdate()`、`referenceShadowUpdateJson()`：塑造写回字段。
- `appendElementReferenceUpdate()`、`appendElementReferenceSubListUpdate()`：组装 `elementReferenceUpdates`。
- `validateReferenceShadows()`：在同一个函数中完成遍历、恢复、诊断、更新收集、失败早退。
- 旁路 metadata 更新：label rename、document reference rename / stamp、external geometry flags 等也在同一文件中扩张。

已有较好的底层边界：

- `cad-core/include/cad_core/part/topo_shape_reference.h`
- `cad-core/src/part/topo_shape_reference.cpp`
- `cad-core/include/cad_core/app/property_links.h`
- `cad-core/src/app/property_links.cpp`

缺口不在底层 fingerprint / BREP matcher，而在 runtime-facing 引用解析和更新发布仍散在 recompute caller glue 中。

## FreeCAD 依据

引用语义的 FreeCAD 依据集中在 App 和 Part：

- `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`：先通过 GeoFeature / ElementMap 解析当前元素，再更新 shadow 和 persisted subname。
- `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub`：`ShadowSub` 保存 `newName` / `oldName` 的 ElementNamePair。
- `/Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.h::searchElementCache()` 与 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::onBeforeChange()`：旧 subshape 几何来自 ElementCache；cad-core 用 request-local `ReferenceShadow` 作为无状态替代证据。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::findSubShapesWithSharedVertex()`：旧 / 新 subshape 几何恢复应集中在 topo reference matcher，不应在 recompute 中靠 JSON 字段猜测。
- `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::updateLabelReference()` 与 `PropertyXLinkContainer` / `DocMap`：label/document reference metadata 是同一 Link lifecycle 的旁路更新，但应和 ReferenceShadow 几何恢复保持边界。

## 目标 module

已新增并深化两个 runtime module：

- `cad-core/include/cad_core/runtime/reference_resolution.h`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/include/cad_core/runtime/element_reference_update.h`
- `cad-core/src/runtime/element_reference_update.cpp`

### Reference Resolution module

承接：

- 根据 `app::Link`、subname index、`ReferenceShadow` 和当前 request-local shape state 解析目标 subshape。
- 统一处理普通 `FaceN/EdgeN/VertexN`、Sketch `InternalFace/InternalEdge/InternalVertex`、StableSubList、NamedShape ElementMap、ShadowSub fallback、ReferenceShadow BREP / fingerprint recovery。
- 返回 typed `ReferenceResolutionResult`：status、resolved subname、current shape、recovery method / reason、diagnostic code / reason、是否需要 MapperHistory 诊断事件。
- 只消费 `part::recoverReferenceShadowSubshape()` 等底层 topo matcher，不复制 BREP / fingerprint 逻辑。

### Element Reference Update module

承接：

- 把成功恢复结果投影成 `ReferenceShadow` 更新 JSON。
- 生成 `StableSubList`、`FullSubList`、`ShadowSub`、`ExternalFlags`、`labelReferenceRename`、`sourceObjectRename`。
- 分别处理 `App::PropertyLinkSub` 与 `App::PropertyLinkSubList` 的 update shape。
- 返回 `elementReferenceUpdates` 的 typed package，让 `runtime/recompute.cpp` 只负责 append。

重构后的 `runtime/recompute.cpp` 角色：

- 保留 recompute orchestration、executor dispatch、NamedShape 注册、对象级成功 / 失败控制。
- 调用 Reference Resolution module 验证引用。
- 调用 Element Reference Update module 生成 updates。
- 不再直接写 ReferenceShadow 恢复策略、StableSubList / ShadowSub 推导规则、MapperHistory 诊断事件细节。

## 分层落点

- `runtime/recompute.cpp`：保留调度，删除引用恢复和 update JSON 细节。
- `runtime/reference_resolution.*`：承接 request-local 引用解析、恢复、诊断语义。
- `runtime/element_reference_update.*`：承接 update JSON projection。
- `part/topo_shape_reference.*`：继续只做 fingerprint / BREP / subshape 几何匹配，不接收 recompute context 或 JSON update 职责。
- `app/property_links.*`：继续只负责 JSON property/link 解析，不倒灌 runtime 恢复策略。
- `part/topo_shape.*`：继续负责 `NamedShape` / `ElementMap` / `MapperHistoryEvent` 类型和消费。

## 实施步骤

### S0：冻结当前行为面

先列出必须保持稳定的行为：

- `elementReferenceUpdates` JSON contract 不改变。
- `documentObjectUpdates` 不因本轮重构改变。
- `ReferenceShadow.brep` 仍只允许单个旧 subshape snapshot，不成为建模输入。
- `StableSubList`、`FullSubList`、`ShadowSub`、`ReferenceShadow` 字段保留现有空值 / 缺省行为。
- Sketch InternalShape 引用仍支持 `InternalFaceN`、`InternalEdgeN`、`InternalVertexN`。
- Frozen / Missing external geometry snapshot 不被错误验证成当前源对象失败。
- 失败恢复仍写入 mapper history diagnostic，且 diagnostic code / target / subname 不漂移。

### S1：定义 ReferenceResolutionResult

新增 typed API，避免 caller 继续搬运散字段：

- requested object / property / subname / stable subname。
- resolved subname。
- resolved `TopoDS_Shape`。
- status：resolved、recovered、ambiguous、split、deleted、semantic_drift、missing。
- recovery method / reason。
- diagnostic code / reason。
- optional `MapperHistoryEvent` 或可构造 mapper diagnostic 的 typed evidence。

public 类型必须在相邻注释中标注 FreeCAD 依据，至少指向 `PropertyLinkBase::_updateElementReference()`、`GeoFeature::searchElementCache()` 和 `Feature::onBeforeChange()`。

### S2：迁移 current subshape resolution

把以下逻辑迁入 `runtime/reference_resolution.cpp`：

- `internalSubnameFromStableElementMap()`。
- `internalSubshapeForCurrentName()`。
- `currentSubshapeForReference()`。
- `stableNameCandidatesForReference()`。
- `internalSubshapeFromShadowSub()`。

迁移后要求：

- `recompute.cpp` 不再知道 InternalShape / StableSubList / ShadowSub 的组合恢复细节。
- Sketch InternalShape 的 request-local 映射仍来自 `context.objects[Sketch]["internal_element_map"]`，但读取动作由 module 封装。
- `part/topo_shape_reference.*` 不依赖 runtime context。

### S3：迁移 ReferenceShadow recovery 与 diagnostics

把以下逻辑迁入 `runtime/reference_resolution.cpp`：

- `recoverSubshapeForReference()`。
- `referenceRecoveryDiagnosticCode()`。
- `referenceRecoveryDiagnosticReason()`。
- `referenceMatchStatusName()`。
- `referenceSubnameShapeKind()`。
- `referenceRecoverability()`。
- `referenceRelation()`。
- `recordReferenceRecoveryMapperDiagnostic()`。

迁移后要求：

- `validateReferenceShadows()` 只处理“遍历对象属性 + 调 module + 收集结果”。
- MapperHistory diagnostic 仍写到同一个 `NamedShape` / `Sketch.InternalShape NamedShape`。
- 失败 diagnostic 的 code / message / property / target / subname 保持兼容。

### S4：迁移 element reference update projection

把以下逻辑迁入 `runtime/element_reference_update.cpp`：

- `indexedSubnameForReference()`。
- `shadowSubToJson()` / `shadowSubsToJson()`。
- `stableSubnamesForReferenceUpdate()`。
- `fullSubnamesForReferenceUpdate()`。
- `shadowSubsForReferenceUpdate()`。
- `brepSnapshotToJson()`。
- `brepTextSnapshotForCurrentSubshape()`。
- `referenceShadowUpdateJson()`。
- `appendElementReferenceUpdate()`。
- `linkSubListItemUpdateJson()`。
- `appendElementReferenceSubListUpdate()`。

迁移后要求：

- `recompute.cpp` 不再手写 `PropertyLinkSub` / `PropertyLinkSubList` update JSON。
- `ReferenceShadow` 更新仍刷新 current fingerprint；有 BREP 时仍优先刷新 current BREP，刷新失败才保留旧 BREP。
- `FullSubList` 和 `ExternalFlags` 保留现有兼容行为。

### S5：拆分 validateReferenceShadows

把当前大函数拆成：

- `reference_resolution::validateObjectReferences(...)` 或 `resolveObjectReferences(...)`。
- 返回 `ReferenceValidationResult`：valid、diagnostics、mapper diagnostics、element reference updates。
- `recompute.cpp` 根据 result append diagnostics / mapper events / updates。

短期允许 module 接收一个明确的 `ReferenceResolutionView`，包含 shapes、objects、namedShapes、documentObjects；不要直接把整个 `ComputeContext` 作为长期依赖。如果必须修改 `namedShapes` 里的 mapper history，也用明确的 mutable 参数暴露。

### S6：补 focused tests

优先补或整理这些 focused assertions：

- `PropertyLinkSub` ReferenceShadow 成功更新仍返回 `StableSubList`、`ShadowSub`、`ReferenceShadow`。
- `PropertyLinkSubList` 多项更新仍保留 `SubSet` 结构与未更新项。
- `FullSubList` 在更新后仍保留。
- Sketch `InternalEdgeN` / `InternalVertexN` 通过 StableSubList / ShadowSub 恢复。
- ReferenceShadow split / deleted / ambiguous 仍产生相同 diagnostic code，并写 mapper history diagnostic。
- Frozen / Missing external geometry snapshot 不触发当前源对象验证。

已有 `tests/test_adapters.py` 和 `tests/test_p5_sketch.py` 覆盖大量现象；本轮不要只依赖全量 golden output，至少新增 1-2 个直接约束 module 输出或 adapter update JSON 的 focused case。

## 非目标

- 不改变 `/cad/recompute`、C API、worker、wasm 的 response JSON contract。
- 不改变 `ReferenceShadow.brep` 的单 subshape snapshot 边界。
- 不实现持久 backend session、文档缓存或长期 ElementCache。
- 不重写 `part/topo_shape_reference` 的 BREP / fingerprint matcher。
- 不改变 ProfileBased resolver、Sketch Internal Result、Body replay 或 Topology Response Export。
- 不把 label rename / document reference lifecycle 扩大成新的外部文档加载能力。

## 风险与控制

- 风险：update JSON 字段名或空值行为漂移。控制：S4 只移动字段，不重命名；用 `test_adapters.py` 中 ReferenceShadow update cases 做回归。
- 风险：module 接收整个 `ComputeContext` 后形成新隐式依赖。控制：定义只读 view 和少量 mutable 输出参数。
- 风险：MapperHistory diagnostic 漏写或写错 `Sketch.InternalShape`。控制：针对 InternalShape split/deleted case 做 focused 断言。
- 风险：把 external geometry Frozen/Missing 当普通引用验证，导致旧 snapshot 失效。控制：S0/S6 明确保留 bypass cases。
- 风险：借重构顺手改变 ReferenceShadow recovery 语义。控制：本轮只做结构迁移；若发现恢复规则缺口，另开语义方案。

## 验收命令

本轮已通过“本轮短跑”命令；`clang-format` 不在当前 PATH，未执行自动格式化，已用构建和 `git diff --check` 兜底。

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_recompute_returns_reference_shadow_update
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_recompute_returns_recovered_reference_shadow_update
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_recompute_returns_reference_shadow_update_for_link_sub_list
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
git diff --check
```

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_adapters.py
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py tests/test_p7_features.py
```

### 重型收口

仅在 S5 完成并触碰 mapper history / response projection 公共路径时执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐顺序

优先执行 S0-S5。理由是 `part/topo_shape_reference` 已经承担了几何匹配底层能力，当前最大 friction 在 `runtime/recompute.cpp` 同时处理 recompute 调度、引用恢复、update JSON 和 mapper diagnostic。先把 Reference Resolution 和 Element Reference Update 收成 deep module，可以让后续 `Deepen Topology Response Export` 更干净：response export 只处理已稳定的拓扑投影，不再顺带承接引用恢复副作用。
