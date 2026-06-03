# P8：Link、Assembly 与 Adapter 产品化

本主线在 P5/P6/P7 的 topology 和 history 主路径稳定后推进。P8 2.0 不再只是请求内 display，而要补齐可让前端长期编辑的 Link / Assembly / adapter 边界。

## 目标边界

- Link / LinkElement / LinkGroup / DocumentObjectGroup 从请求内 display 扩展到完整 FreeCAD Link 账本语义。
- ShowElement 的 create / claim / sync / delete 建议形成前端可应用的持久写回事务协议。
- cross-document 文档哈希、mapped postfix、FullSubList、多层 LinkSub 链具备稳定生命周期。
- Assembly display 升级为可执行 Joint placement / constraint 求解，仍不引入 GUI 或会话状态。
- Worker / WASM / Web adapter 产品化，但不改变 CAD Core 无状态核心边界。

当前前置基线：C2-M4 已让 ExternalGeometry / ReferenceShadow 通过 `elementReferenceUpdates` / `documentObjectUpdates` 返回请求内写回建议，并让 Sketch InternalShape 的 split / deleted / open-export 引用恢复只消费 producer evidence 或 diagnostics；C2-M5 已把 taper ThruSections、RefineModel 当前 P7 覆盖子路径与 DressUp SupportTransform AddSubShape slot 接入统一 MapperHistory；C2-M6 已把 transformed / pattern Add/Sub ownership 子路径接入统一 MapperHistory；C2-M7a/b/c/d 已把 ShowElement / ElementList transaction、PropertyXLink / FullSubList representative lifecycle、Assembly solver adapter 和 adapter contract 子路径接到同一无状态协议。P8 剩余深层缺口集中在完整 `_ChildCache` / copy-on-change deep copy、document hash / rename recovery、Ondsel solver 真求解和 Worker / WASM 产品化。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.h`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedName.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/JointGroup.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/JointObject.cpp`

## C2-M7a：完整 Link 账本与写回事务

交付内容：

- 把 ShowElement create / claim / sync / delete 建议整理成稳定 transaction schema。
- 明确 `ElementList`、`ElementCount`、`PlacementList`、`ScaleList`、`VisibilityList` 的写回优先级。
- 补完整 `_ChildCache` 生命周期、copy-on-change / linked-owner 矩阵和 plain group nested child 生命周期。
- Link retag 消费完整 MapperHistory，不仅保留当前 terminal / merge 子集。

完成判定：

- 前端可以按 `documentObjectUpdates` 更新 `DocumentObject graph`，下次 recompute 不丢 display / picking / stable subname。
- toggle ShowElement true / false、owner mismatch、child mismatch、CopyOnChangeOwned 都有确定更新建议。
- 多层 LinkSub 和 `$Label` 路由不靠前端猜测。

当前状态：

- ShowElement transaction 当前子集已稳定到 `documentObjectUpdates`：缺失 child 输出 `create`，可复用 child 输出 `claim`，owner / child mismatch 输出 `update`，多余 owned child 输出 `delete`，ShowElement false 输出 owner `ElementList=[]` + `PlacementList` / `ScaleList` 保留和 child delete。
- `ElementCount` owner list sync、显式 `ElementList` owner sync、显式 `ElementList` child sync 与 CopyOnChangeOwned child sync 已有 P8 fixture 和 C ABI 回归；写回字段覆盖 `ElementList`、`ElementCount`、`PlacementList`、`ScaleList`、`VisibilityList`、`LinkedObject`、`_LinkOwner`、`LinkTransform`。
- capabilities 暴露 `link_transaction.document_object_updates`、`writeback_properties`、`request_local_boundaries` 和 `remaining_gaps`；plain group child 展开仍明确为 request-local，不伪装成持久 `_ChildCache`。

剩余缺口：

- FreeCAD `_ChildCache` 的完整持久生命周期、nested plain group child cache 写回、undo/redo 级 orphan child reclaim 仍未完整抽入 `cad-core`。
- Copy-on-change 目前只锁定 owned child sync，不包含 deep copy source group、copy-on-change touched/mutated 传播和属性监听生命周期。

## C2-M7b：cross-document 与 mapped postfix 生命周期

交付内容：

- 完整 `PropertyXLink*`、`FullSubList`、external-tag mapped postfix、文档哈希、source-prefixed stable key 生命周期。
- 多文档引用缺失、文档哈希变化、source object rename、label rename 的 diagnostics 和恢复建议。
- 与 ReferenceShadow / MapperHistory resolver 统一，不建立 Link 专用旁路。

完成判定：

- external full subname、mapped postfix alias、source-prefixed key 能跨 Link retag / Body boolean / transformed copy 保持可追溯。
- 文档或对象缺失时进入 stable diagnostics，不输出错误 shape。

当前状态：

- `PropertyXLink*` 与 `FullSubList` 解析已接入 document link shape schema；`elementReferenceUpdates` 成功路径会保留 `FullSubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow` 和 `ExternalFlags`。
- Link retag 当前子集保留 external tag mapped postfix alias：`ExternalDoc#Box.Face1` 和 `Face1;:X;ExternalDoc#Box.Face1` 均能回到目标 `Face1`，并进入 `NamedShape.element_map` / `mapper_history`。
- 多层 object / label LinkSub、source-prefixed stable key、`PropertyXLinkList` subset compound 已有 P8 fixture 回归；capabilities 暴露 `link_reference_lifecycle.retag_aliases` 与 `reference_update_fields`。

剩余缺口：

- cross-document 文档哈希、源对象 rename 和 label rename 的恢复建议仍未完整实现；当前只锁定请求里已给出 full subname / postfix / label-qualified alias 的可追溯路径。
- 多文档缺失和哈希变化尚未形成专用 diagnostics 矩阵，仍依赖现有 missing target / invalid subshape 诊断。

## C2-M7c：Assembly solver 与 Joint placement

交付内容：

- 从旧 `solve=not_migrated` 元数据升级到 Assembly Joint 求解入口。
- 解析 Joint / GroundedJoint 的 Reference1 / Reference2 / ObjectToGround、JointType、placement chain。
- 明确 solver 输入、输出 placement update、失败 diagnostics 和前端写回建议。
- 若 OndselSolver 不能直接抽入 `cad-core`，先建立 solver adapter interface 和 no-op / diagnostic fallback。

完成判定：

- component placement 可由 Joint 求解结果更新为 `documentObjectUpdates`。
- solver failure、unsupported joint、missing reference、ambiguous subshape 都有 diagnostics。
- Assembly solver 不依赖 GUI 或跨请求 CAD Core 会话。

当前状态：

- Assembly solver adapter interface 已落地到 `features/link.cpp`：AssemblyObject 根据 request-local JointGroup / Joint / GroundedJoint 输入输出 `skipped_no_joints`、`solved_noop` 或 `unsupported`；GroundedJoint-only fixture 作为无状态 no-op success 路径，普通 `JointType` 通过 `unsupported_assembly_solver` warning 暴露 full solver gap。
- `App::FeaturePython` Joint / GroundedJoint 不再标成 `solve=not_migrated`，而是输出 `joint_input` / `grounded_input`；JointGroup 输出 `solver_inputs`。
- capabilities 暴露 `assembly.solver_adapter = [skipped_no_joints, grounded_only_noop, unsupported_joint_diagnostics]`，同时保留 `full_ondsel_solver` 与 `solver_placement_updates`。

剩余缺口：

- OndselSolver 的真实 `runPreDrag()` / placement update 仍未抽入 `cad-core`；普通 Fixed / Revolute / 其他 JointType 仍只走 unsupported diagnostics。
- solver 成功后写回 component placement 的 `documentObjectUpdates` 仍未实现。

## C2-M7d：Worker / WASM / Web adapter

交付内容：

- CLI、C ABI、Worker、WASM、Web adapter 共用同一 `cad-core-lib` recompute 入口。
- adapter 只做 JSON / binary / file protocol 转换。
- capabilities 输出包含 feature coverage、topo history coverage、known gaps、adapter limits。
- 大模型 mesh / export / import 路径有资源限制和 diagnostics。

完成判定：

- 同一 fixture 在 CLI / C ABI / Web adapter 下核心结果一致。
- adapter 不新增建模语义，不修改 `DocumentObject graph`。
- Web 返回字段足以被前端应用 Link transaction、elementReferenceUpdates 和 diagnostics。

当前状态：

- CLI 和 C ABI 当前共用 `cad-core-lib` recompute 主入口；C ABI 暴露 `cad_core_recompute_json`、`cad_core_capabilities_json` 和 `cad_core_export_json`，CLI 暴露 `recompute` 与 file export。
- C API export 是 buffer-only 协议：请求不得包含 `export_file` / `path` / `file` server path，只返回 data buffer、filename/content-type/bytes 和 diagnostics metadata；CLI file export 仍是本地工具协议，需要 object / format / file 三项同时给出。
- capabilities 暴露 `adapters.core_entrypoints`、`stateless_result_channels`、`c_api_export`、`cli_export` 和 `remaining_gaps`；当前 Web 可消费 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`link_transaction` 和 `link_reference_lifecycle`。

剩余缺口：

- Worker adapter、WASM adapter、streaming mesh limits 和 binary mesh protocol 仍未产品化。
- Web adapter 级资源限制、分块 mesh/export、前端错误码映射还未冻结。
