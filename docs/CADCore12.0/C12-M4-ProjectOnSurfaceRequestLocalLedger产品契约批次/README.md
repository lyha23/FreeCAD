# C12-M4 ProjectOnSurface Request-Local Ledger 产品契约批次

C12-M4 承接 C12-M3 S6 的 `native_hidden_retained` / `no_code_retained_gate`。C12-M3 已证明 FreeCAD 原生 `FeatureProjectOnSurface` 能生成 geometry，但公开的 `TopoShapePy.getElementHistory`、`mapSubElement`、`mapShapes`、`MapperHistory` / `ElementMap` 观察路径没有暴露 source-backed source-to-target provenance；继续等待 native expected 会把 ProjectOnSurface provenance 卡在不可观测状态。

本包把 `cad-core` 现有 request-local projection ledger 升级为 CAD Core 产品契约：它不再伪装成 FreeCAD native parity expected，而是作为前端引用恢复、stable subname、mapper history 和 diagnostic recovery 的 request-local contract。C12-M4 仍不在开包阶段改 C++、fixtures expected、tests、adapters 或 capability wording；这些变更必须由后续步骤或独立 implementation 包按本契约执行。

## S0 live 基线冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=744531f67c`，`git log -1 --oneline=744531f67c docs: 新增 C12-M4 request-local ledger 产品契约包`。
- 起点 `git -c core.quotepath=false status --short -uall` 输出为空；dirty boundary 为 `<clean>`，未发现 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或构建产物改动。
- 队列状态：C12-M3 `工作步骤细分` 只输出表头；C12-M4 执行前从 S0-S4 pending 开始，S0 完成并重命名后下一步为 S1。
- C12-M3 继承口径固定为：S4/S5 已证明 ProjectOnSurface native history 仍是 `native_hidden_retained`，`s5_input=null`，`native_provenance_expected_ready_count=0`。C12-M4 因此只发布产品契约决策，不创建 C++ implementation gate。

## S1 current ledger 证据冻结

- S1 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=842ce54ad7`，`git log -1 --oneline=842ce54ad7 docs: 冻结 C12-M4 S0 live 基线`；起点 `git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`。
- 队列检查确认 C12-M4 从 S1 开始，S1 文件执行前未标 `【已实现】`；S1 完成后下一步应为 S2 产品契约边界与 non-goal 冻结。
- Current producer 已冻结到 `source_authority` / `contract_fields`：`ProjectedShapeEvidence` 承载 sourceObject/sourceSubname/stableSubname/projectionItemIndex/sourceShapeKind/makerStage/edgeFragmentIndex/faceWireSources/heightSolidId/compoundChildIndex/preOffsetChildId/offsetApplied；`projectOnSurfaceMapperEvidenceJson()` 输出 `reference_recovery_hook=mapper_history_event_target_subname` 以及 face/height/compound/wire ownership；`namedShapeForProjectOnSurfaceProvenance()` 写入 `mapper_history` 与 `element_history_status`。
- Focused consumer 已冻结为 `cad-core/tests/test_p8_features.py` 的 C5-M9 edge、wire split、invalid diagnostic、face rebuild、all-compound 方法；五个 C5-M9 expected JSON 当前仍保留 `known_gap` / `native_hidden` / native replacement wording，迁移点只记录到 `expected_migration`，不在 S1 修改 expected、tests、capability wording、C++、adapters 或 fixtures。

## 契约判断

- FreeCAD 语义依据仍来自 `src/Mod/Part/App/FeatureProjectOnSurface.cpp` 的 `Projection` LinkSubList、`projectWire()`、`projectFace()`、`createSolidIfHeight()`、`createCompound()` 和 `getOffsetPlacement()` 调用顺序。
- `cad-core/src/part/part_project_on_surface.cpp` 里的 `projection_item_ledger`、`ProjectedShapeEvidence`、`projectOnSurfaceMapperEvidenceJson()` 和 `namedShapeForProjectOnSurfaceProvenance()` 是产品契约的当前实现语义来源。
- `cad-core/include/cad_core/part/topo_shape_mapper.h` 的 `MapperHistoryEvent` 是 request-local provenance 的公共语义形状；它是 `cad-core` 的产品账本，不要求 FreeCAD native API 输出同构 history。
- C5-M9 expected 当前仍写着 `known_gap` / native hidden delete condition；C12-M4 要把这类 wording 迁移为 `product_contract` / `native_oracle_unavailable`，但迁移不得改写 geometry parity，也不得降低 source-authority 注释要求。

## 出口

- `contract_published`：产品契约已在文档/矩阵中冻结，但 fixtures/capability wording 尚未迁移。
- `contract_migration_ready`：expected/test/capability wording 的具体迁移范围和验收命令已经明确，可另开 implementation goal。
- `implementation_deferred`：只发布决策和迁移包，不直接改 C++ 或 fixture expected。
- `rejected_native_parity_dependency`：继续要求 FreeCAD native ProjectOnSurface history artifact 作为唯一入口的路线被拒绝。

## 入口

- 总入口：`6-29-23-00-C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次总入口.md`
- 方案：`6-29-23-00-C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 禁止项

- 不把 C12-M4 产品契约说成 FreeCAD native expected。
- 不用 bbox、输出顺序、EdgeN 数量、fixture 名称或 current-result topology 反推 source ownership。
- 不引入 GUI session、跨请求 native document、持久 TopoDS / NamedShape / ElementMap cache 或完整 BREP transport。
- 不在本包创建时直接修改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters 或 capability wording。
