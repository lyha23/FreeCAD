# Deepen Reference Lifecycle 重构方案

## 迁移说明

本文件从 `/home/user/Chili3DProject/my-chili3d/docs/重构/6-23-18-52-Deepen Reference Lifecycle重构方案.md` 迁回 FreeCAD 仓库。原文把 `/tmp/architecture-review-20260623-162528.html` 中的 `Deepen the reference lifecycle` 候选项映射到了 `my-chili3d` 前端 adapter；迁移后以 FreeCAD / cad-core 为准，不再把前端 `selection intent`、`feature params`、Svelte runtime 或 Vitest 用例当作本方案的实现落点。

目标目录已有主方案 `docs/重构/6-23-16-39-【已实现】ReferenceLifecycle深模块重构方案.md`。本文保留 18:52 这份误落点文档的校正记录，内容收敛为对当前 cad-core 已落地实现的补充说明：reference lifecycle 的权威实现位于 `cad-core/runtime`，前端只消费 `/cad/recompute` 返回的 `elementReferenceUpdates`、diagnostics 和 document object updates。

## 当前结论

`Deepen the reference lifecycle` 在 FreeCAD 仓库中已经落地为 `runtime/reference_lifecycle.*` deep module：

- `cad-core/include/cad_core/runtime/reference_lifecycle.h`
- `cad-core/src/runtime/reference_lifecycle.cpp`

该 module 只做 request-local reference lifecycle 分类，不做 OCCT 几何匹配，不生成 adapter JSON，也不保存跨请求状态。它把某条 `app::Link` 在当前 `DocumentObject graph` 中的生命周期状态归一成 `ReferenceLifecycleDecision`，再供 graph planning、runtime reference validation、metadata update publication 和 Sketch ExternalGeometry 构建入口共用。

## 与原 my-chili3d 方案的差异

原文中的这些前端落点不再属于本文范围：

- `src/lib/services/cad-recompute/reference-lifecycle.ts`
- `src/lib/services/cad-recompute/references.ts`
- `src/lib/services/cad-recompute/diagnostics.ts`
- `src/lib/services/cad-recompute/consumer.ts`
- `src/lib/feature/runtime/cad-build-application.ts`
- `src/lib/services/cad-recompute/subshape-intent/`

这些文件可以在前端侧继续作为 `/cad/recompute` response 消费层存在，但不能替代 cad-core 对 FreeCAD link lifecycle 的源头判定。前端如果需要收敛 `referenceStatus`、UI diagnostic 或 params 写回策略，应另写 `my-chili3d` 前端方案，并明确它只消费 cad-core 已发布的 update，不重新发明 FreeCAD link / ExternalGeometry 生命周期规则。

## FreeCAD / cad-core 实现边界

当前实现边界如下：

- `runtime/reference_lifecycle.*`：集中分类 `CurrentTarget`、`MissingTarget`、`FrozenOldExternalGeometry`、`MissingOldExternalGeometry`、`DetachedExternalGeometry`、`ExternalDocumentMissing`、`ExternalDocumentPendingReload`、`ExternalDocumentUnloaded`、`MetadataOnlyUpdate`。
- `graph/recompute_plan.cpp`：只根据 `ReferenceLifecycleDecision` 决定 dependency traversal、blocked object 和 graph diagnostic，不再维护本地生命周期 helper。
- `runtime/reference_resolution.cpp`：用 lifecycle decision 判断是否验证 `ReferenceShadow`，不再自己识别 Frozen / Missing old external geometry 的放行细节。
- `runtime/element_reference_update.cpp`：消费 lifecycle decision 发布 metadata-only update，包括 label rename、document reference rename、`StableSubList`、`FullSubList`、`ShadowSub` 和 `ExternalFlags`。
- `runtime/recompute.cpp`：组装 `ReferenceLifecycleView`，调用 reference validation、metadata update 和 document reference diagnostic，不直接展开生命周期状态机。
- `sketcher/sketch_object_external.cpp`：保留 ExternalGeometry 几何构建和 projection，但状态判定复用 lifecycle module。

## FreeCAD 依据

本方案对应的 FreeCAD 语义仍以这些源码为依据：

- `src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`：先解析当前元素，再维护 `ShadowSub` 和 element reference。
- `src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub`：保存 `newName` / `oldName` 的 element name pair。
- `src/App/PropertyLinks.cpp::PropertyLinkBase::updateLabelReference()`：label 改名需要确认 found subobject 仍指向目标 object。
- `src/App/PropertyLinks.cpp::DocInfo::init()/attach()/slotDeleteDocument()`：外部文档经历 pending、restored、unloaded / deleted 状态。
- `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::onExternalGeoChanged()`：Detached external geometry 清空引用并清掉相关 flag。
- `src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()`：`Frozen` / `Missing` external geometry 在有旧证据时继续使用旧几何；恢复后按 source 状态调整 `Missing` / `Sync`。

cad-core 的无状态边界不变：`DocumentObject graph` 是唯一真实数据；`ReferenceShadow.brep` 只能作为 request-local 的单 subshape 恢复证据，不能成为建模输入、session cache 或长期 backend 状态。

## 已落地接口

核心 public interface 为：

```cpp
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
    bool targetExists;
    bool requiresGraphDependency;
    bool shouldValidateReferenceShadow;
    bool canUseNativeExternalGeoEvidence;
    bool canUseReferenceShadowBrepEvidence;
    bool shouldPublishElementReferenceUpdate;
    bool hasLabelReferenceRename;
    bool hasDocumentReferenceRename;
    bool hasDocumentReferenceStampMismatch;
    std::optional<Diagnostic> diagnostic;
    std::optional<Diagnostic> runtimeWarning;
};

ReferenceLifecycleDecision classifyReferenceLifecycle(
    const app::DocumentObject& owner,
    const app::PropertyValue& propertyValue,
    const app::Link& link,
    const ReferenceLifecycleView& view);
```

实际实现还保留了 `ExternalGeometryLifecycleFlags`、`externalGeometryLifecycleFlags()`、`normalizedExternalGeometryFlagSet()` 和 `externalGeometryReferenceKey()`，用于 Sketch ExternalGeometry flag 规范化与 request graph update。

## 当前状态矩阵

| 场景 | Graph / runtime 决策 | 输出 |
| --- | --- | --- |
| target 存在 | follow dependency，正常 validate reference shadow | 保持当前 recompute 行为 |
| 普通 target 缺失 | `BlockRecompute` | `missing_link_target` |
| `Frozen && !Sync` 且有 native `ExternalGeo` | ignore missing dependency，跳过当前 source validation | 复用旧几何 |
| `Frozen && !Sync` 且有 `ReferenceShadow.brep` | ignore missing dependency，跳过当前 source validation | 复用旧几何 |
| `Frozen/Missing && !Sync` 但无旧证据 | `BlockRecompute` | `missing_external_geometry_snapshot` |
| `Detached` ExternalGeometry | detach reference，不验证旧 source | `external_geometry_detach` document update |
| `Missing` source 恢复 | follow dependency，重建后同步 flags | `external_geometry_flags_sync` document update |
| XLink missing document | `BlockRecompute` | `missing_external_document` |
| XLink pending reload | `BlockRecompute` | `external_document_pending_reload` |
| XLink unloaded / deleted | `BlockRecompute` | `external_document_unloaded` |
| label / document rename metadata-only | publish metadata update | `elementReferenceUpdates` 中带 `labelReferenceRename` 或 `documentReference` |
| document stamp mismatch | runtime warning，不阻断 graph | `document_hash_mismatch` diagnostic |

## 与既有方案的关系

- `6-23-11-37-【已实现】ReferenceResolution深模块重构方案.md` 解决的是 subshape 恢复、`ReferenceShadow` 验证、`StableSubList` / `ShadowSub` 写回和 `elementReferenceUpdates` projection。
- `6-23-16-39-【已实现】ReferenceLifecycle深模块重构方案.md` 是本主题的主实现方案，定义并落地了 `runtime/reference_lifecycle.*`。
- 本文件是从前端仓库迁回后的校正记录，用来消除“reference lifecycle 应落在 my-chili3d 前端”的误导；后续引用本主题时优先看 16:39 主方案和当前 cad-core 代码。

## 非目标

- 不新增 `my-chili3d` 前端 `reference-lifecycle.ts`。
- 不改变 `/cad/recompute` JSON contract。
- 不扩大 `ReferenceShadow.brep` 边界。
- 不把 Sketch ExternalGeometry 的 OCCT 几何构建挪入 runtime policy module。
- 不引入跨请求 document session、ElementCache 或 backend BREP cache。
- 不把前端 feature params 作为 FreeCAD link lifecycle 的状态源。

## 复核命令

文档迁移本身不要求执行构建。需要复核当前实现时，使用 cad-core 侧命令：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_diagnostics.CadCoreDiagnosticsTest.test_reference_lifecycle_matrix
```

需要检查 lifecycle helper 是否仍只集中在 module 内：

```bash
cd ~/Chili3DProject/FreeCAD
rg -n "documentRefPendingReload|documentRefUnloaded|isFrozenExternalGeometryReference|isMissingExternalGeometryReference|isDetachedExternalGeometryReference|hasReferenceShadowBrepSnapshot" \
  cad-core/src/graph/recompute_plan.cpp \
  cad-core/src/runtime/reference_resolution.cpp \
  cad-core/src/runtime/recompute.cpp \
  cad-core/src/runtime/reference_lifecycle.cpp
```

预期结果：这些底层 helper 只出现在 `cad-core/src/runtime/reference_lifecycle.cpp`，graph / runtime caller 只消费 `classifyReferenceLifecycle()` 或 `ReferenceLifecycleDecision`。
