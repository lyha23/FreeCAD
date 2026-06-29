# C12-M4 ProjectOnSurface Request-Local Ledger 产品契约批次

C12-M4 承接 C12-M3 S6 的 `native_hidden_retained` / `no_code_retained_gate`。C12-M3 已证明 FreeCAD 原生 `FeatureProjectOnSurface` 能生成 geometry，但公开的 `TopoShapePy.getElementHistory`、`mapSubElement`、`mapShapes`、`MapperHistory` / `ElementMap` 观察路径没有暴露 source-backed source-to-target provenance；继续等待 native expected 会把 ProjectOnSurface provenance 卡在不可观测状态。

本包把 `cad-core` 现有 request-local projection ledger 升级为 CAD Core 产品契约：它不再伪装成 FreeCAD native parity expected，而是作为前端引用恢复、stable subname、mapper history 和 diagnostic recovery 的 request-local contract。C12-M4 决策包本身不改 C++、fixtures expected、tests、adapters 或 capability wording；发布后续已按本契约把 expected / capability / adapter / 接口文档的公开口径迁移完成。

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

## S2 产品契约边界冻结

- S2 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=49cdd9d086`，`git log -1 --oneline=49cdd9d086 docs: 完成 C12-M4 S1 ledger 证据矩阵`；起点 `git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`。
- 队列检查确认 C12-M4 从 S2 开始，S2 文件执行前未标 `【已实现】`；S2 完成后下一步应为 S3 expected 与 capability 迁移设计。
- 产品契约已冻结：`projection_item_ledger`、`MapperHistoryEvent`、edge/wire provenance、face rebuild provenance、height/compound/offset provenance、`element_history_status` 与 `reference_recovery_hook` 是 CAD Core request-local product contract；invalid projection diagnostic 是 CAD Core product diagnostic contract。
- `native_oracle_unavailable` 注记保留为强制口径：C12-M3 S4/S5 仍是 `native_hidden_retained`、`s5_input=null`、`native_provenance_expected_ready_count=0`，没有 FreeCAD native parity success，也没有 current comparison / backend gap candidate。
- Non-goal 已冻结：FreeCAD native ElementMap parity、output guessing、GUI/Workbench、persistent native Document / TopoDS / NamedShape / ElementMap cache、full BREP transport，以及在本 decision package 内直接改 C++、include、fixture expected、tests、adapters 或 capability wording 都保持 out of scope。
- `C12M4-BLOCKER-002` 已关闭；`C12M4-BLOCKER-003` expected wording 迁移、`C12M4-BLOCKER-004` capability wording audit 和 `C12M4-BLOCKER-005` release gate 仍保持未关闭。

## S3 expected 与 capability 迁移设计

- S3 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5aa11afe11`，`git log -1 --oneline=5aa11afe11 docs: 完成 C12-M4 S2 产品契约冻结`；起点 `git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`。
- 队列检查确认 C12-M4 从 S3 开始，S3 文件执行前未标 `【已实现】`；S3 完成后只应推进到 S4 产品契约发布闸门。
- Implementation package 迁移面已明确：五个 C5-M9 expected JSON 的 `known_gap/native_hidden replacement` wording 应迁移为 `product_contract` 或 `product_diagnostic_contract`，同时保留 `native_oracle_unavailable`、FreeCAD source authority 和现有 focused assertions。
- Capability / adapter / interface wording 的后续目标已定位：`cad-core/src/runtime/capability_contract.cpp:821-952` 的 `part_workbench.project_on_surface`，`cad-core/tests/test_adapters.py:2970-3104` 的 capability assertion，以及 `docs/接口规定/01-cad-recompute全量输入输出接口.md:113-127` 的 “projected edge provenance mapper/history ... 仍是 gap / non-goal” 文案。
- `C12M4-BLOCKER-003` 已关闭为 migration list complete；`C12M4-BLOCKER-004/005` 在 S3 结束时交由 S4 release decision，最终状态见 S4 发布记录。
- S3 不改 C++、include、fixtures expected、tests、adapters、capability source、接口文档正文或 C5-M9 expected JSON；不运行 FreeCADCmd、current comparison 或 full build。

## S4 产品契约发布闸门

- S4 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=af1cc283b1`，`git log -1 --oneline=af1cc283b1 docs: 完成 C12-M4 S3 迁移设计`；起点 `git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`。
- 队列检查确认 C12-M4 只剩 S4，且 S4 文件执行前未标 `【已实现】`。
- S4 逐行复核 `source_authority`、`contract_fields`、`scope_review`、`expected_migration`、`non_goal_registry`、`blocker_queue` 与 `validation_matrix` 后，没有发现新的 retained blocker。`projection_item_ledger`、`MapperHistoryEvent`、edge/wire、face rebuild、height/compound/offset、`element_history_status` / recovery hook 与 invalid projection diagnostics 均保持产品契约边界清楚。
- C12-M3 负结论继续保留为 `native_oracle_unavailable`：S4 artifact 是 `native_hidden_retained`，`s5_input=null`，12 条 observation 只有 `native_hidden_retained` / `product_boundary_rejected`；S5 no-comparison evidence 记录 `native_provenance_expected_ready_count=0`、未运行 current comparison、未创建 backend gap candidate。
- 最终发布 `contract_published`、`contract_migration_ready`、`implementation_deferred` 和 `rejected_native_parity_dependency`。C12-M4 本身不改 C++、include、fixtures expected、tests、adapters、capability source、接口文档正文或 C5-M9 expected JSON。
- `C12M4-BLOCKER-004` 已关闭为 follow-up migration sources located；`C12M4-BLOCKER-005` 已关闭为 release gate published。发布后的公开口径迁移已覆盖 C5-M9 expected wording、capability source、adapter assertion 和接口文档 wording。

## 契约判断

- FreeCAD 语义依据仍来自 `src/Mod/Part/App/FeatureProjectOnSurface.cpp` 的 `Projection` LinkSubList、`projectWire()`、`projectFace()`、`createSolidIfHeight()`、`createCompound()` 和 `getOffsetPlacement()` 调用顺序。
- `cad-core/src/part/part_project_on_surface.cpp` 里的 `projection_item_ledger`、`ProjectedShapeEvidence`、`projectOnSurfaceMapperEvidenceJson()` 和 `namedShapeForProjectOnSurfaceProvenance()` 是产品契约的当前实现语义来源。
- `cad-core/include/cad_core/part/topo_shape_mapper.h` 的 `MapperHistoryEvent` 是 request-local provenance 的公共语义形状；它是 `cad-core` 的产品账本，不要求 FreeCAD native API 输出同构 history。
- C5-M9 expected 已从 `known_gap` / native hidden delete condition 迁移为 `product_contract` 或 `product_diagnostic_contract`；该迁移不改写 geometry parity，也不降低 source-authority 注释、focused assertions 或 native oracle unavailable 注记要求。

## 出口

- `contract_published`：产品契约已在文档/矩阵中冻结，发布后续已迁移 fixtures/capability wording。
- `contract_migration_ready`：expected/test/capability/interface wording 的具体迁移范围和验收命令已经明确，并已按该范围执行。
- `implementation_deferred`：C12-M4 决策包只发布决策和迁移包；后续口径迁移不改变 ProjectOnSurface 几何实现。
- `rejected_native_parity_dependency`：继续要求 FreeCAD native ProjectOnSurface history artifact 作为唯一入口的路线被拒绝。

最终状态：S4 已发布以上四个出口；C12-M4 队列关闭预期为 `step_goal_queue.py` 只输出表头；发布后的 expected / capability / adapter / 接口文档公开口径迁移已完成。

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
