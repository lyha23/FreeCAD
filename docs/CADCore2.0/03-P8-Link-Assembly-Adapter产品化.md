# P8：Link、Assembly 与 Adapter 产品化

本主线在 P5/P6/P7 的 topology 和 history 主路径稳定后推进。P8 2.0 不再只是请求内 display，而要补齐可让前端长期编辑的 Link / Assembly / adapter 边界。

## 目标边界

- Link / LinkElement / LinkGroup / DocumentObjectGroup 从请求内 display 扩展到完整 FreeCAD Link 账本语义。
- ShowElement 的 create / claim / sync / delete 建议形成前端可应用的持久写回事务协议。
- cross-document 文档哈希、mapped postfix、FullSubList、多层 LinkSub 链具备稳定生命周期。
- Assembly display 升级为可执行 Joint placement / constraint 求解，仍不引入 GUI 或会话状态。
- Worker / WASM / Web adapter 产品化，但不改变 CAD Core 无状态核心边界。

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

## C2-M7b：cross-document 与 mapped postfix 生命周期

交付内容：

- 完整 `PropertyXLink*`、`FullSubList`、external-tag mapped postfix、文档哈希、source-prefixed stable key 生命周期。
- 多文档引用缺失、文档哈希变化、source object rename、label rename 的 diagnostics 和恢复建议。
- 与 ReferenceShadow / MapperHistory resolver 统一，不建立 Link 专用旁路。

完成判定：

- external full subname、mapped postfix alias、source-prefixed key 能跨 Link retag / Body boolean / transformed copy 保持可追溯。
- 文档或对象缺失时进入 stable diagnostics，不输出错误 shape。

## C2-M7c：Assembly solver 与 Joint placement

交付内容：

- 从当前 `solve=not_migrated` 元数据升级到 Assembly Joint 求解入口。
- 解析 Joint / GroundedJoint 的 Reference1 / Reference2 / ObjectToGround、JointType、placement chain。
- 明确 solver 输入、输出 placement update、失败 diagnostics 和前端写回建议。
- 若 OndselSolver 不能直接抽入 `cad-core`，先建立 solver adapter interface 和 no-op / diagnostic fallback。

完成判定：

- component placement 可由 Joint 求解结果更新为 `documentObjectUpdates`。
- solver failure、unsupported joint、missing reference、ambiguous subshape 都有 diagnostics。
- Assembly solver 不依赖 GUI 或跨请求 CAD Core 会话。

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
