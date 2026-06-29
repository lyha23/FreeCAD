# C12-M4 ProjectOnSurface Request-Local Ledger 产品契约批次方案

## 背景

C12-M3 已经排除了继续用 FreeCAD 原生公开 API 获取 ProjectOnSurface source-backed mapper history 的路径。S4 artifact 显示 object result 与 intermediate projection shape 可以生成，但 `getElementHistory` 返回 `None`，`mapSubElement` / `mapShapes` 只能作为手动 API 操作，`PropertyPartShape` ElementMap save/load 又依赖持久 native document / BREP roundtrip，不属于 CAD Core request-local 产品边界。

这意味着继续等待 native expected 会让 C5-M9 中已经存在的 request-local provenance 能力长期停留在 `known_gap` wording。C12-M4 的方向是把这套 ledger 从“临时 source-backed known gap expected”升级为“CAD Core 产品契约”：它服务前端选择、引用恢复和 stable subname，不再要求 FreeCAD native 输出同形态 oracle。

## 契约对象

| contract area | current source |
| --- | --- |
| Projection input identity | `Projection.getValues()` / `Projection.getSubValues()` 对齐到 `projection_item_index`、`source_object`、`source_subname`。 |
| Edge / wire fragments | `projectWire()` 的 result edge expansion 对齐到 `edge_fragment_index` 和 source endpoint。 |
| Face rebuild ownership | `projectFace()` / `createFaceFromParametricWire()` 对齐到 `face_wire_sources`、outer / inner wire role 和 `face_rebuild_id`。 |
| Height solid ownership | `createSolidIfHeight()` 对齐到 `height_solid_id` 与 `source_face_target`。 |
| Compound child / offset | `filterShapes()` / `createCompound()` / `getOffsetPlacement()` 对齐到 `compound_child_index`、`pre_offset_child_id` 和 offset metadata。 |
| Reference recovery | `MapperHistoryEvent.target.subname` 和 `reference_recovery_hook=mapper_history_event_target_subname`。 |

## 方法

1. 冻结 C12-M3 结论：native provenance unavailable，不再作为 implementation blocker。
2. 盘点当前 `cad-core` ledger 字段与 focused C5-M9 tests，确认它们是否已经表达前端需要的 request-local provenance。
3. 明确 contract 字段的权威层级：FreeCAD 源码提供几何语义和输入顺序，CAD Core ledger 提供 provenance / recovery 产品契约。
4. 规划 expected/test/capability wording 迁移：把 `known_gap` / `delete_condition` wording 改为 product-contract wording，同时保留 `native_oracle_unavailable` 注记。
5. 发布后续 implementation package 边界；本包自身不改 C++、fixture expected、tests 或 capability wording。

## 非目标

- 不声称 FreeCAD native ProjectOnSurface history 已经可观测。
- 不把 current `cad-core` ledger 与 FreeCAD native `ElementMap` 等同。
- 不依赖 GUI session、persistent native document、full BREP transport、bbox/order/EdgeN 推断。
- 不降低 existing C5-M9 focused tests；后续迁移必须保持或加强字段断言。

## 后续分流

- 如果 S4 发布 `contract_migration_ready`：另开 C12-M5 或同级 implementation 包，迁移 C5-M9 expected wording、capability docs 和必要测试名称。
- 如果 S1/S2 发现 ledger 字段不足：另开实现包补 request-local ledger 字段，但仍不要求 FreeCAD native oracle。
- 如果未来 FreeCAD native API 暴露 stable provenance：作为对照 artifact 加入文档，不自动覆盖 CAD Core 产品契约。
