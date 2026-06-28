# 【已实现】C9-M5-S4 request-local DTO 产品边界复审

## 目标

根据 S3 native evidence 判断是否存在可进入 cad-core 的 request-local CopyOnChange DTO。S4 是产品 / 协议边界步骤，不写 C++。

## 执行基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`14c20f0b12`
- `git log -1 --oneline`：`14c20f0b12 docs: 关闭 C9-M5 S3 native CopyOnChange 复审`
- `git -c core.quotepath=false status --short -uall`：无输出，工作区干净。
- 队列首项是本 S4 文件；S5/S6 仍 pending。

## S3 evidence 输入

- FreeCADCmd：`/home/user/.local/bin/freecadcmd`
- Native version：`freecad_version=1.2.0 revision 20260519`，`freecad_revision=20260519`
- 已观察：Disabled / Enabled / Mutated / PartialLoad property-state、`_tmp_binder` temporary document 名、`_CopiedLink` hidden property、mutation-triggered `BindCopyOnChange` 状态。
- 仍不可导出：`_CopiedObjs` private vector payload、`_tmp_binder copyObject` object graph and dependency order、`copyObject()` dependency mapping、`recomputeFeature(true)` internal copied-object `ElementMap` lifecycle。
- 结论输入：`C9M5-ORACLE-103.route=oracle_blocked`，S3 明确说明 native probing 仍没有稳定 stateless request-local payload。

## DTO 准入规则

可接受 DTO 只能包含：

- request DocumentObject graph 中已有或可由前端持久化的字段。
- request-local `documentObjectUpdates` 或 diagnostics。
- source object id/name、support subname、mutated property delta、copy intent 等结构化标量 / JSON。
- 明确的 deletion / update / reselect 建议。

禁止 DTO 包含（forbidden fields）：

- FreeCAD hidden temporary document。
- Raw `TopoDS_Shape`、BREP、full shape cache。
- request 结束后继续有效的 `NamedShape`、`ElementMap` 或 copied-object cache。
- `_CopiedObjs` private vector 或 native object pointer identity。

## 字段边界

S4 不批准一个新的 SubShapeBinder CopyOnChange implementation DTO，但保留未来重开时的产品边界：

| 字段族 | 允许字段 | 字段来源 | 本轮裁定 |
| --- | --- | --- | --- |
| request graph | object id/name、typeId、`Support` object / subname、`BindCopyOnChange`、`PartialLoad`、前端可持久化 mutated property delta | request `DocumentObject graph` 与 S3 property-state evidence | 可作为输入事实，不代表 full cache support |
| request-local updates | `documentObjectUpdates.action=create/update/delete`、`reason`、`object`、`objectId`、`owner`、`properties`、`dependencyRewrite`、`historyPreserve` | `cad-core/src/app/copy_on_change.cpp::buildCopyOnChangeLifecycleUpdates()` 的 App Link 词汇 | 仅作 DTO 词汇对照，不能隐式替代 SubShapeBinder temporary cache |
| diagnostics | `copy_on_change_full_temporary_document_cache_not_supported`、source/support context、reselect/update/delete 建议 | current `feature_shape_binder.cpp` diagnostic boundary 与 capability known gap | S5/S6 应保持 diagnostic publication |
| product intent | copy intent、source id/name、support subname、mutation delta、reselect/update/delete suggestion | request graph + S3 可观察属性态 | 允许记录为产品输入，但本轮缺 stable copied-object mapping |
| forbidden fields | `_tmp_binder` document payload、`_CopiedObjs`、native object pointer identity、TopoDS_Shape/BREP/full shape cache、request 后继续有效的 `NamedShape` / `ElementMap` / copied-object cache | FreeCAD hidden runtime state 或 geometry cache | 禁止进入 request / response contract |

## 产品裁决

本轮裁决：`dto_rejected_known_gap_retained`。

理由：

- S3 已采到更强 native evidence，但只证明 property-state、temporary document 名和 `_CopiedLink` 可观察。
- S3 未证明 copied-object graph、dependency order、`_CopiedObjs` payload 或 `recomputeFeature(true)` internal `ElementMap` lifecycle 能稳定序列化为 request-local DTO。
- `cad-core/src/app/copy_on_change.cpp` 的 `documentObjectUpdates` 是 App Link CopyOnChange 词汇，对 SubShapeBinder full temporary-document copied-object cache 只能作为字段设计参考。
- 因裁决不是 `dto_approved_candidate`，S5/S6 不得落 C++ support，不得把 `copy_on_change_full_temporary_document_cache` 从 retained known gap 中删除。

S5 路由：no-code release gate。S5 只能复审并保持当前 diagnostic / capability / focused tests 发布边界，S6 若继续消费该结论，应发布 retained known gap 而不是 implementation。

## 裁决选项记录

| 裁决 | 条件 | S5 路由 |
| --- | --- | --- |
| `dto_approved_candidate` | S3 证明字段稳定、可序列化、无持久 geometry cache | S5 可升级 implementation gate。 |
| `dto_rejected_known_gap_retained` | S3 仍依赖 hidden cache 或不可序列化 native state | S5 no-code release gate。 |
| `needs_more_native_evidence` | probe 被环境阻断或字段不完整 | S5 保持 known gap，记录重开条件。 |

## 必须回写的矩阵行

- `C9M5-SCOPE-103`
- `C9M5-SCOPE-201`
- `C9M5-BLOCKER-401`
- `C9M5-CAT-102`
- `C9M5-CAT-103`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'dto_approved_candidate|dto_rejected_known_gap_retained|needs_more_native_evidence|documentObjectUpdates|forbidden fields|copy_on_change_full_temporary_document_cache|C9M5-SCOPE-103|C9M5-SCOPE-201' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
git diff --check
```

验收标准：

- S4 明确给出三选一裁决。
- 若裁决为 `dto_approved_candidate`，必须列出 DTO 字段、来源、cad-core landing、focused tests 和禁止字段。
- 若裁决不是批准，S5/S6 不得落 C++ support。

## 非目标

- 不新增前端 / Rust 协议。
- 不修改 capability。
- 不修改 `cad-core` C++、fixtures、expected 或 tests。
