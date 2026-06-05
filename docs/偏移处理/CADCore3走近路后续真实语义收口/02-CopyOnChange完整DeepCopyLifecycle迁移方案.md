# CopyOnChange 完整 Deep Copy Lifecycle 迁移方案

## 1. 目标

把 CADCore3 Link CopyOnChange 从当前 writeback contract 推进到完整 deep copy 生命周期。

完成后，能力口径应从：

- `copy_on_change_writeback_contract.status=covered`
- `copy_on_change_deep_copy_lifecycle.status=partial`

推进到：

- `copy_on_change_deep_copy_lifecycle.status=covered_full`
- writeback contract 只表示前后端传输和持久化建议，不再替代真实 deep copy
- CopyOnChange 生成的新对象能作为下一次请求 graph 中的普通对象继续参与 recompute

## 2. 非目标

- 不引入 backend 跨请求 copy cache。copy 后的对象必须由前端保存进下一次请求 graph。
- 不把 CopyOnChange 写成 adapter 层输出修正。
- 不用固定名称 `_CopyOnChangeObject` 作为真实生命周期替代。
- 不迁移 GUI / ViewProvider / TreeView 展示语义。

## 3. FreeCAD 依据入口

开工前必须先复核这些本地源码入口：

| FreeCAD 源码 | 需要提取的语义 |
| --- | --- |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::LinkBaseExtension::makeCopyOnChange()` | CopyOnChange 创建、复用、mode 判断、copy group 关系 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::LinkBaseExtension::syncCopyOnChange()` | 源对象变更后 copy 对象同步规则 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.h::LinkCopyOnChangeTouched` | touched state 和同步触发语义 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp` | 链接属性复制和 relink 规则 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObject.cpp` | 对象复制、属性复制和 document ownership 基础语义 |
| `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObjectGroup.cpp` | group / child object ownership 语义 |

## 4. 当前缺口

当前 CADCore3 已经能表达 CopyOnChange writeback contract，但还不是完整生命周期：

- 缺少 source object 的 property tree deep copy。
- 缺少 child object / group ownership copy。
- 缺少 copied object 内部 links 的 relink。
- 缺少 dependency graph rewrite。
- 缺少 NamedShape / ElementMap / history preserve。
- 缺少 touched 后 `syncCopyOnChange()` 的等价同步规则。

## 5. 目标生命周期

目标 CAD Core 链路：

1. Link executor 读取 Link 对象、source object、CopyOnChange mode 和 touched state。
2. `CopyOnChangePlanner` 判断是否需要创建、复用或同步 copy object。
3. `CopyOnChangeBuilder` 复制 source object 的属性树、placement、label、children 和 link properties。
4. relink 规则把 copy 内部指向 source subtree 的 links 改到 copy subtree。
5. topo / history preserve 把 NamedShape、ElementMap、subshape reference 语义迁到 copy object。
6. CAD Core 返回 `documentObjectUpdates`，提示前端保存新对象、新 links 和 Link 指向关系。
7. 下一次请求中，前端传入包含 copy object 的完整 graph，CAD Core 按普通 graph recompute。

## 6. 候选落点

| CAD Core 落点 | 职责 |
| --- | --- |
| `cad-core/include/cad_core/app/copy_on_change.h` | CopyOnChange plan、copy result、relink result、diagnostics |
| `cad-core/src/app/copy_on_change.cpp` | FreeCAD CopyOnChange 生命周期、属性复制、child copy、relink |
| `cad-core/src/app/link.cpp` | 只负责识别 Link mode 并调用 CopyOnChange API |
| `cad-core/include/cad_core/document` | object clone、property clone、document update DTO |
| `cad-core/include/cad_core/topo` 与 `cad-core/src/topo` | NamedShape / ElementMap / subshape history preserve |
| `cad-core/src/adapters` | 只序列化 `documentObjectUpdates`，不承载复制语义 |

## 7. 实施切片

### A. CopyOnChange plan 和当前 writeback 收敛

- 把当前 writeback contract 归一到 `CopyOnChangePlan`。
- 明确 mode 1 / mode 3 的差异。
- 固定名称 `_CopyOnChangeObject` 只能作为临时诊断，不作为真实 object identity 策略。

验收：现有 writeback fixtures 不退化，capability 仍标记为 partial。

### B. Property tree deep copy

- 迁移 FreeCAD property copy 规则。
- 处理基础属性、placement、label、visibility、link property、unsupported property diagnostics。
- 保持属性失败显式诊断，不静默丢字段。

验收：source object property 修改后，copy object 在 update 中包含完整属性树。

### C. Child object 和 group ownership copy

- 迁移 source subtree 的 child object copy。
- 维护 copy group / parent group 关系。
- 生成 stable new object names，避免名称碰撞。

验收：带 child/group 的 source 被 CopyOnChange 后，copy subtree 完整出现在 update 中。

### D. Link relink 和 dependency graph rewrite

- copy subtree 内部 links 从 source subtree 重定向到 copy subtree。
- 外部 links 保持原语义，不误改到 copy。
- dependency graph 使用 copy 后对象参与 recompute。

验收：copy object 内部引用不再指回原 source 的 owned child。

### E. History preserve

- 复制或映射 NamedShape、ElementMap、stable subname、ReferenceShadow 相关证据。
- 保证 subshape reference 在 copy 后仍可恢复。
- 不把完整 BREP 作为 copy 输入或长期状态，只允许既有 `ReferenceShadow.brep` 例外继续作为旧 subshape snapshot。

验收：CopyOnChange 后引用到 copied subshape 的 link / external reference 不丢失。

### F. syncCopyOnChange

- 迁移 touched state 触发同步的规则。
- source 变化后，按 FreeCAD 语义更新已有 copy，而不是每次生成新 copy。
- 对冲突、用户修改 copy、unsupported property 给出 diagnostics。

验收：同一 Link 多次请求不会无限生成 copy object；已有 copy 能稳定同步。

### G. capability flip

- 更新 CADCore3 gap 表和语义矩阵。
- `copy_on_change_writeback_contract.status=covered` 保留为 transport 能力。
- `copy_on_change_deep_copy_lifecycle.status=covered_full` 只能在 deep copy、relink、history preserve 和 sync case 全部通过后设置。

## 8. 验收矩阵

必须新增或补齐的 case：

| Case | 期望 |
| --- | --- |
| mode 1 创建 copy | 返回完整 copy object update，Link 指向 copy |
| mode 3 创建或同步 copy | 按 FreeCAD mode 语义执行，不退化成简单写回 |
| source 有 child object | copy subtree 完整 |
| source 有内部 links | copy 内部 links relink 到 copy subtree |
| source 有外部 links | 外部 links 不被误改 |
| copy 后再次 recompute | copy object 作为普通 graph object 参与 recompute |
| touched 后 sync | 复用已有 copy 并同步，不无限新建 |
| unsupported property | 结构化 diagnostic，不静默丢失 |
| subshape reference | stable subname / ReferenceShadow 证据保留 |

阶段回归命令：

```bash
cd /Users/admin/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_p8_features
```

代码改动后的轻量检查：

```bash
git diff --check -- cad-core docs/CADCore3.0 docs/偏移处理
```

## 9. 完成条件

- Link executor 不再直接拼凑 CopyOnChange 输出，而是调用完整生命周期 API。
- copy object、child objects、properties、links、history 均能进入 `documentObjectUpdates`。
- 前端保存 update 后，下一次请求 graph 能正常 recompute copied object。
- capability gap 从 partial 更新为 covered_full，并列出仍 unsupported 的 property 类型。

## 10. 已实现状态

当前 CopyOnChange deep copy lifecycle 已收口到 `covered_full`：

- FreeCAD 依据链路：
  - `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::LinkBaseExtension::makeCopyOnChange()`：`getOnChangeCopyObjects()` 后调用 `copyObject(srcobjs)`，copy 后 `LinkedObject` 指向新 copy，`CopyOnChangeEnabled` 进入 `CopyOnChangeOwned`。
  - `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::LinkBaseExtension::syncCopyOnChange()`：使用 `CopyOnChangeGroup` 取旧 copy，重新 copy source dependency order，复制 CopyOnChange properties，并通过 `CopyOnLinkReplace()` 对旧 copy 到新 copy 做 link 替换。
  - `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Document.cpp::Document::copyObject()`：通过 `exportObjects()` / `importObjects()` 复制对象，并用 `_ObjectUUID` / `_SourceUUID` 建立 source-copy 对应。
  - `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::CopyOnLinkReplace()` 各实现：`PropertyLink`、`PropertyLinkList`、`PropertyLinkSub`、`PropertyLinkSubList`、`PropertyXLink`、`PropertyXLinkSubList` 对 copied subtree 做 link replacement。
- CAD Core 落点：
  - `cad-core/include/cad_core/app/copy_on_change.h` 和 `cad-core/src/app/copy_on_change.cpp` 承接 App 层 CopyOnChange lifecycle、source dependency copy order、property tree clone、copied-subtree relink、ReferenceShadow / StableSubList / FullSubList evidence preserve、group sync 和 touched sync。
  - `cad-core/src/app/link.cpp` 只识别 Link 并调用 `buildCopyOnChangeLifecycleUpdates()`，不再直接拼 CopyOnChange 业务输出。
  - `cad-core/src/runtime/feature_executor.cpp` 把 `_CopyOnChangeControl` 与 cad-core 的 `_CopyOnChange*` hidden provenance 视为 App 层通用隐藏属性，避免 copy object 在下一次请求作为普通 feature recompute 时被几何 executor 误判为 unsupported。
- 验收 fixture：
  - `cad-core/fixtures/c3m6/app-link-copy-on-change-deep-copy.json`：基础 property tree deep copy 和前端保存后二次 recompute。
  - `cad-core/fixtures/c3m6/app-link-copy-on-change-subtree-relink-history.json`：child/group copy、`_CopyOnChangeControl` 外部对象排除、内部 link relink、ReferenceShadow / StableSubList / FullSubList retarget。
  - `cad-core/fixtures/c3m6/app-link-copy-on-change-touched-tracking.json`：touched 后复用已有 copy 同步，不无限生成新 copy。
