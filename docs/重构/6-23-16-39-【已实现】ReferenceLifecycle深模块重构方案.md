# Reference Lifecycle 深模块重构方案

## 来源与结论

本方案来自 `/tmp/architecture-review-20260623-162528.html` 中的候选项 `Deepen the reference lifecycle`。

HTML 的核心判断是：`graph` 和 `runtime` 现在都在分类 `Frozen`、`Missing`、`Detached`、native `ExternalGeo`、`ReferenceShadow.brep` 和 linked document 状态；这些判断应该集中到一个更深的 reference lifecycle module，再分别供 graph planning、runtime validation、update publication 消费。

当前仓库已经有 `docs/重构/6-23-11-37-【已实现】ReferenceResolution深模块重构方案.md`，并已把 `ReferenceShadow` recovery、`StableSubList` / `ShadowSub` 写回、MapperHistory diagnostic 和 `elementReferenceUpdates` projection 从 `runtime/recompute.cpp` 拆出。本方案不重复这件事；本轮要解决的是更上游的“引用状态生命周期判定”仍散在多个 caller 里。

结论：新增一个 in-process 的 `runtime/reference_lifecycle.*` deep module。它不做 OCCT 几何匹配，也不生成 adapter response；它只把某个 `app::Link` 在当前 request 中的生命周期状态分类成 typed decision，让 `graph/recompute_plan.cpp`、`runtime/reference_resolution.cpp`、`runtime/element_reference_update.cpp` 和 `sketcher/sketch_object_external.cpp` 共用同一套策略。

## 当前基线

当前泄漏点主要有四类：

- `cad-core/src/graph/recompute_plan.cpp` 自己维护 `linkedDocumentName()`、`documentRefPendingReload()`、`documentRefUnloaded()`、`isFrozenExternalGeometryReference()`、`isMissingExternalGeometryReference()`、`isDetachedExternalGeometryReference()`、`hasReferenceShadowBrepSnapshot()` 和 native `ExternalGeo` evidence 判断，并直接决定 dependency / blocked object / graph diagnostic。
- `cad-core/src/runtime/reference_resolution.cpp` 再次维护 `isFrozenExternalGeometryReference()` 和 `isMissingOldExternalGeometrySnapshotReference()`，用于决定是否跳过 `ReferenceShadow` validation。
- `cad-core/src/runtime/recompute.cpp` 仍保留 standalone label reference rename、document reference rename、document stamp warning 和 metadata update JSON 的部分生命周期判断。
- `cad-core/src/sketcher/sketch_object_external.cpp` 维护 `ExternalGeometry` 的 `Frozen` / `Missing` / `Detached` / `Sync` flag 读取、规范化和 request graph update，同时还要和 graph/runtime 保持一致。

这些代码各自看起来合理，但 Interface 太浅：caller 必须知道 FreeCAD Link 生命周期细节、ExternalGeometry flag 组合、linked document status 组合、native old-geometry evidence 和 `ReferenceShadow.brep` 例外。新增状态时容易出现 graph 放行、runtime 阻断、sketch update 又另写一套的漂移。

## FreeCAD 依据

本方案的语义依据集中在 FreeCAD App Link 和 Sketcher ExternalGeometry：

- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`：先通过 `GeoFeature::resolveElement(...)` 解析当前元素，再维护 `ShadowSub` 和 element reference。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub`：`ShadowSub` 是 `ElementNamePair`，保存 `newName` / `oldName`。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::updateLabelReference()`：label 改名不是字符串替换；它先确认 found subobject 仍是目标 object。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::DocInfo::init()`：外部文档缺失或 partial 时调用 `addPendingDocument(...)`。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::DocInfo::attach()`：外部文档到达后调用 `restoreLink(obj)`。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::DocInfo::slotDeleteDocument()` 与 `PropertyXLink::detach()`：外部文档删除或卸载时 detach link，并重新触发 element reference update。
- `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::onExternalGeoChanged()`：`Detached` external geometry 清空 ref，并清掉 `Detached` / `Missing` flag。
- `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()`：`Missing` 旧 external geometry 有“linked external geometry will continue to work”的行为；`Frozen && !Sync` 通过 `refSet` 保留旧几何；重建后清 `Sync`，并按 `refSet` 切换 `Missing`。

cad-core 的无状态边界仍按仓库规则执行：`DocumentObject graph` 是唯一真实数据；`ReferenceShadow.brep` 只能作为单个旧 subshape 的 request-local 恢复证据，不能变成建模输入、session cache 或长期 backend 状态。

## 目标 Interface

新增文件：

- `cad-core/include/cad_core/runtime/reference_lifecycle.h`
- `cad-core/src/runtime/reference_lifecycle.cpp`

建议对外只暴露一个主分类入口，外加少量 projection helper：

```cpp
namespace cad_core::runtime {

enum class ReferenceLifecycleState {
    CurrentTarget,
    MissingTarget,
    FrozenOldExternalGeometry,
    MissingOldExternalGeometry,
    DetachedExternalGeometry,
    ExternalDocumentMissing,
    ExternalDocumentPendingReload,
    ExternalDocumentUnloaded,
    MetadataOnlyUpdate,
};

enum class ReferenceLifecycleAction {
    FollowDependency,
    IgnoreDependencyUseOldEvidence,
    DetachReference,
    BlockRecompute,
    PublishMetadataOnlyUpdate,
};

struct ReferenceLifecycleDecision {
    ReferenceLifecycleState state;
    ReferenceLifecycleAction action;
    bool requiresGraphDependency = true;
    bool shouldValidateReferenceShadow = true;
    bool canUseNativeExternalGeoEvidence = false;
    bool canUseReferenceShadowBrepEvidence = false;
    bool shouldPublishElementReferenceUpdate = false;
    std::optional<Diagnostic> diagnostic;
};

struct ReferenceLifecycleView {
    const app::Document& document;
    const std::map<std::string, const app::DocumentObject*>& documentObjects;
};

ReferenceLifecycleDecision classifyReferenceLifecycle(
    const app::DocumentObject& owner,
    const app::PropertyValue& propertyValue,
    const app::Link& link,
    const ReferenceLifecycleView& view);

}
```

实际字段可按实现收敛，但 Interface 必须保持两个约束：

- caller 不再手写 `Frozen/Missing/Detached/Sync`、linked document status、native `ExternalGeo` evidence、`ReferenceShadow.brep` evidence 的组合规则。
- module 不接收整个 `ComputeContext`。需要的事实通过只读 view 或明确的输入结构传入；输出是 typed decision，不是直接 mutate graph/runtime/sketch 状态。

## 模块职责

`reference_lifecycle` 承接：

- 规范化 `ExternalGeometry` flags：`Frozen`、`Missing`、`Detached`、`Sync`。
- 判断 old external geometry evidence：native `ExternalGeo` pool、`ReferenceShadow.brep` 单 subshape snapshot。
- 判断 graph 是否应该 follow dependency、忽略 missing source、detach reference，或 block recompute。
- 统一 graph diagnostic code / message / target / subname：`missing_external_geometry_snapshot`、`missing_external_document`、`external_document_pending_reload`、`external_document_unloaded`、`missing_link_target`。
- 统一 runtime 是否应对 `ReferenceShadow` 做当前 subshape validation。
- 统一 label/document metadata-only update 是否应进入 `elementReferenceUpdates`。

它不承接：

- 不做 `ReferenceShadow` fingerprint / BREP 几何匹配；继续由 `part/topo_shape_reference.*` 和 `runtime/reference_resolution.*` 承担。
- 不做 Sketch external geometry 投影、intersection、native `Part::Geometry` 构造；继续留在 `sketcher/sketch_object_external.cpp`。
- 不改变 `/cad/recompute`、C ABI、Worker、WASM response JSON。
- 不保存跨请求 state，不引入 backend session，不扩大 `ReferenceShadow.brep` 边界。

## 分层落点

- `runtime/reference_lifecycle.*`：新增 typed policy module，集中生命周期状态分类和 diagnostic spec。
- `graph/recompute_plan.cpp`：删除本地 lifecycle helper；只调用 `classifyReferenceLifecycle()`，根据 decision 添加 dependency、blocked object 或 diagnostic。
- `runtime/reference_resolution.cpp`：删除本地 ExternalGeometry bypass helper；用 decision 的 `shouldValidateReferenceShadow` 决定是否跳过 old snapshot validation。
- `runtime/element_reference_update.*`：承接 metadata-only update projection；`runtime/recompute.cpp` 不再自己判断 label/document reference rename。
- `sketcher/sketch_object_external.cpp`：保留几何构建，但用 lifecycle decision 读取 `Frozen/Missing/Detached/Sync` 状态和 old evidence 可用性，避免和 graph/runtime 重复规则。
- `cad-core/CMakeLists.txt`：加入新 `.cpp`。

## 实施步骤

### S0：冻结状态矩阵

先写一张 request-local lifecycle matrix，作为代码迁移时的保护线。至少覆盖：

| 场景 | Graph decision | Runtime validation | Update publication |
| --- | --- | --- | --- |
| 普通 target 存在 | follow dependency | validate ReferenceShadow | 保持现有 update |
| 普通 target 缺失 | block `missing_link_target` | 不执行 | 无 update |
| `Detached` ExternalGeometry | ignore missing dependency / detach | 不验证旧 source | 发布 detach update |
| `Frozen && !Sync`，有 native `ExternalGeo` | ignore missing dependency | 不验证旧 source | 保留旧 geometry |
| `Frozen && !Sync`，有 `ReferenceShadow.brep` | ignore missing dependency | 不验证旧 source | 保留旧 geometry |
| `Frozen && !Sync`，无旧证据 | block `missing_external_geometry_snapshot` | 不执行 | 无 update |
| `Missing && !Sync`，source 存在 | follow dependency | 正常验证 / 重建 | 成功时清 `Missing` |
| `Missing && !Sync`，source 缺失但有旧证据 | ignore missing dependency | 不验证旧 source | 保留 `Missing` |
| XLink pending reload | block `external_document_pending_reload` | 不执行 | 无 update |
| XLink unloaded / deleted | block `external_document_unloaded` | 不执行 | 无 update |
| label / document rename metadata-only | follow dependency | 不做几何恢复 | 发布 metadata update |

S0 只新增或整理测试断言，不改变行为。

### S1：新增 `ReferenceLifecycleDecision`

新增 `runtime/reference_lifecycle.*`，先把 graph 里已有的纯判断函数搬入 module，但不切调用点。

要求：

- public 类型旁边写 FreeCAD 依据注释，至少指向 `PropertyLinks.cpp::_updateElementReference()`、`PropertyLinks.cpp::DocInfo::init()/attach()/slotDeleteDocument()`、`SketchObjectExternal.cpp::rebuildExternalGeometry()`。
- module 内部可以有 private helper，但不要把 `DocumentObject graph` 之外的 session 状态塞进 Interface。
- `ReferenceLifecycleDecision` 能表达 graph、runtime、update 三类 caller 需要的事实；不要为每个 caller 暴露一组重复函数。

### S2：切换 graph planning

把 `graph/recompute_plan.cpp` 的本地 lifecycle helper 全部替换为 `classifyReferenceLifecycle()`。

验收要求：

- graph 仍只负责拓扑排序、cycle diagnostic、blocked object 和 dependency traversal。
- `missing_external_geometry_snapshot`、`missing_external_document`、`external_document_pending_reload`、`external_document_unloaded` 的 code / stage / object / property / target / subname 不漂移。
- `Detached`、old native `ExternalGeo`、`ReferenceShadow.brep` 证据仍能放行 missing source，不退化成 `missing_link_target`。

### S3：切换 runtime reference validation

把 `runtime/reference_resolution.cpp` 的 `isFrozenExternalGeometryReference()`、`isMissingOldExternalGeometrySnapshotReference()` 替换为 lifecycle decision。

验收要求：

- `validateObjectReferences()` 不再知道 ExternalGeometry old snapshot 的 graph 放行细节。
- `ReferenceShadow` 成功 recovery、split/deleted/ambiguous diagnostic 和 MapperHistory diagnostic 不改变。
- `Frozen/Missing` old snapshot 不被错误验证成当前 source 缺失。

### S4：收拢 metadata-only update

把 `runtime/recompute.cpp` 中 standalone label reference rename、document reference rename、document stamp warning 和 metadata-only `elementReferenceUpdates` 的判断移到 `reference_lifecycle` + `element_reference_update`。

验收要求：

- `recompute.cpp` 只负责在 object recompute 前后 append typed result，不手写 `documentReferenceRenameChanged()`、`hasStandaloneLabelReferenceRename()` 这类 lifecycle 判断。
- `elementReferenceUpdates` JSON 字段名和空值行为不变。
- document stamp mismatch 仍是 runtime warning，不变成 graph block。

### S5：接入 Sketch ExternalGeometry

`sketcher/sketch_object_external.cpp` 继续保留 ExternalGeometry 几何构建和 projection，但 ExternalGeometry 状态判定使用 lifecycle module。

验收要求：

- `Detached` 仍清空 request graph 中的 external reference，并发布 `external_geometry_detach`。
- `Frozen && !Sync` 仍复用旧 geometry，不强行访问 missing source。
- `Missing` source 成功恢复时仍清 `Missing`；source 缺失但有旧证据时仍保留 old geometry。
- native `ExternalGeo` pool 仍是 request-local evidence，不变成长期 cache。

### S6：删除重复 helper 并收口文档

完成切换后删除旧 helper，防止“新 module + 旧规则并存”。

建议收口检查：

```bash
cd ~/Chili3DProject/FreeCAD
rg -n "documentRefPendingReload|documentRefUnloaded|isFrozenExternalGeometryReference|isMissingExternalGeometryReference|isDetachedExternalGeometryReference|hasReferenceShadowBrepSnapshot" cad-core/src/graph/recompute_plan.cpp cad-core/src/runtime/reference_resolution.cpp cad-core/src/runtime/recompute.cpp
```

预期：上述生命周期 helper 只保留在 `runtime/reference_lifecycle.cpp`，caller 文件里不再有本地副本。

实现完成并通过验收后，把本文件改名为 `6-23-16-39-【已实现】ReferenceLifecycle深模块重构方案.md`。

## 非目标

- 不改变 `ReferenceResolution` 已实现的 recovery 语义。
- 不改变 `Element Reference Update` 已实现的 JSON contract。
- 不新增 FreeCAD oracle collector。
- 不实现跨请求 document session、ElementCache 或 backend BREP cache。
- 不改 `part/topo_shape_reference` 的 fingerprint / BREP matcher。
- 不把 Sketch ExternalGeometry 的几何 projection 挪进 runtime policy module。
- 不修复新的 per-kind ExternalGeometry 几何支持缺口；发现缺口时另开语义方案。

## 风险与控制

- 风险：把 deep module 做成一堆 pass-through helper。控制：caller 只能拿 `ReferenceLifecycleDecision`，不能继续直接调用 flag/status 小函数。
- 风险：graph 和 runtime 行为切换时 diagnostic 漂移。控制：先跑 S0 matrix，再按 S2/S3 分批切换。
- 风险：module 接收整个 `ComputeContext` 后形成新隐式依赖。控制：只传 `ReferenceLifecycleView`，不要让 lifecycle module mutate runtime state。
- 风险：metadata update projection 被误并入 graph。控制：graph 只消费 dependency/block decision；JSON projection 只在 runtime update publication 层发生。
- 风险：ExternalGeometry 几何构建和 lifecycle policy 混在一起。控制：lifecycle module 只输出状态和证据可用性；OCCT geometry 构建仍留在 sketcher module。

## 验收命令

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_frozen_brep_snapshot_reuses_old_subshape \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_frozen_missing_snapshot_reports_diagnostic \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_missing_brep_snapshot_reuses_old_subshape \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_missing_without_snapshot_reports_diagnostic \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_frozen_native_external_geo_reuses_old_geometry \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_c3m2_external_geometry_missing_native_external_geo_keeps_missing_flag \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m2_xlink_missing_external_document_reports_graph_diagnostic \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m2_xlink_pending_external_document_reports_reload_diagnostic \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m2_xlink_unloaded_external_document_reports_detached_diagnostic \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m2_xlink_pending_external_document_restored_by_request_graph \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_recompute_returns_reference_shadow_update_for_link_sub_list \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_recompute_link_sub_list_update_preserves_unupdated_items
git diff --check
```

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_p5_sketch.py tests/test_p6_topology.py tests/test_p8_features.py tests/test_adapters.py
```

### 重型收口

仅在 S5 完成并触碰 Sketch ExternalGeometry 主路径、capability contract 或 adapter projection 公共路径后执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐执行顺序

先做 S0-S3。理由是 graph/runtime 的重复生命周期判定是当前最直接的浅 Interface；它们都是 in-process dependency，不需要新 adapter。S4/S5 再把 metadata update 和 Sketch ExternalGeometry 接入同一 decision，避免一次性移动几何 projection 主路径。每完成一个切换点就删除旧 helper，不保留双轨规则。
