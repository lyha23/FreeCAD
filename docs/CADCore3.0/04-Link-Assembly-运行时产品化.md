# Link / Assembly / 运行时产品化

## 目标

C3-M6 / C3-M7 的目标是把 2.0 的 Link display、Link transaction 子集、Assembly solver adapter 和 CLI / C ABI contract 推进到可支撑前端长期编辑和 Web 部署的产品化边界。

该主线必须保持无状态 CAD Core 边界：前端持久化 `DocumentObject graph`，后端只根据单次请求计算 shape、mesh、NamedShape、引用更新建议和 diagnostics。

## C3-M6：Link 完整生命周期

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.h`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedName.cpp`

交付内容：

- 完整 `_ChildCache` 生命周期：create、claim、sync、delete、nested plain group child cache、orphan reclaim。
- copy-on-change complete deep copy lifecycle：source group、owned child、touched / mutated 传播写回建议，属性树 deep copy、child/group copy、copied subtree relink、依赖 graph rewrite 与 ReferenceShadow / stable subname evidence preserve。
- document hash lifecycle：跨文档引用、文档缺失、hash 变化、source object rename、label rename。
- Link retag 与 MapperHistory 统一：FullSubList、mapped postfix、source-prefixed stable key、多层 LinkSub 不建立专用旁路。
- `documentObjectUpdates` schema 冻结到前端可直接应用。

完成判定：

- 前端应用 `documentObjectUpdates` 后，下次 recompute 不丢 Link display、picking、stable subname。
- source rename / label rename / document hash mismatch / missing external document 有明确恢复建议或 diagnostics。
- toggle ShowElement、ElementCount、ElementList、PlacementList、ScaleList、VisibilityList 的写回优先级稳定。
- C3-M6 当前已覆盖 ShowElement child cache、plain group nested traversal、CopyOnChange writeback transport、complete deep copy lifecycle、owned child sync 和 touched sync；`copy_on_change_deep_copy_lifecycle.status=covered_full`，`link_transaction.remaining_gaps=[]`。

## C3-M6b：Assembly solver 与 placement write-back

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/JointGroup.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py`

交付内容：

- 提供 stateless representative solver DTO，不保留跨请求 Ondsel solver session；该路径不能命名为 full Ondsel solver。
- Joint / GroundedJoint 解析 Reference1 / Reference2 / ObjectToGround、JointType、placement chain。
- Fixed、Revolute、Slider、Ball、Distance、Angle 等常用 JointType 至少有代表 oracle。
- solver 成功后通过 `documentObjectUpdates` 写回 component placement。
- solver failure、unsupported joint、missing reference、ambiguous subshape 均有 diagnostics。

完成判定：

- GroundedJoint-only 不再是唯一成功路径。
- 普通 JointType 有 representative placement update 或明确 unsupported matrix。
- Assembly solver 不依赖 GUI 或跨请求 CAD Core session。
- C3-M6 当前已覆盖 Fixed / Revolute / Slider / Ball / Distance / Angle grounded real Ondsel `runPreDrag()` fixture、双 grounded + Distance 矛盾 validation fixture、representative fallback、six JointType fallback capability keys 和 RackPinion / Screw / Gears / Belt / Cylindrical unsupported matrix；`representative_solver_adapter.status=covered_representative` 仅表示 fallback，`placement_writeback.status=covered_contract`，`ondsel_solver_adapter.status=covered_full`，`assembly.remaining_gaps=[]`。

## C3-M7：Worker / WASM / Web adapter 产品化

FreeCAD / cad-core 依据：

- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/src/adapters/cli/*`
- `cad-core/include/cad_core/adapters/*`
- 前端 / 后端接口文档以 `docs/接口规定` 和 Rust Web adapter contract 为准。

交付内容：

- Worker adapter 与 WASM adapter 共用 `cad-core-lib` recompute 入口。
- streaming mesh limits：大模型 mesh 分块、顶点 / 面数限制、超限 diagnostics。
- binary mesh protocol：保留 JSON metadata，同时允许二进制 mesh / export buffer。
- import/export 资源限制：文件大小、格式、超时、内存、deflection。
- Web 错误码映射：diagnostic code、severity、object、property、subname、recovery suggestion。

完成判定：

- CLI / C ABI / Worker / WASM / Web 对同一 fixture 的核心结果一致。
- adapter 不修改 `DocumentObject graph`，不新增建模语义。
- 前端能消费 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、capabilities 和 binary payload metadata。
- C3-M7 当前已覆盖 `cad_core_worker_recompute_json`、`cad_core_wasm_recompute_json`、`mesh_limit_exceeded` streaming metadata 和 `cad_core_mesh_binary_json`；`adapters.remaining_gaps=[]`。

## 不允许的实现路径

- 不在 Web adapter 中补拓扑命名或 Link 语义。
- 不把 Worker / WASM 内部缓存当作几何状态来源。
- 不把 Assembly solver 的跨请求状态藏在后端 session 中。
- 不让 frontend 根据 bbox / area / output order 修正 subname。

## 验收命令

本主线代码修改后优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters tests.test_p8_features tests.test_feature_flows
```

阶段收口时执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures
```
