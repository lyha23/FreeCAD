# C12-M4 ProjectOnSurface Request-Local Ledger 产品契约批次总入口

## 包目标

C12-M4 的目标是把 `cad-core` 当前 ProjectOnSurface request-local projection ledger 升级为明确产品契约。它承认 C12-M3 的负结论：FreeCAD 原生 ProjectOnSurface history API 没有暴露可用于替换 C5-M9 的 `native_provenance_expected_ready`。因此后续工作不再以 native expected 作为唯一 gate，而是把 `cad-core` 为前端引用恢复提供的 request-local ledger 定义为 CAD Core 自有 contract。

## S0 live 记录

- 基线命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=744531f67c`，`git log -1 --oneline=744531f67c docs: 新增 C12-M4 request-local ledger 产品契约包`。
- 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`。边界内没有 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或构建产物。
- 队列检查：C12-M3 `工作步骤细分` 只输出表头；C12-M4 执行前从 S0-S4 pending 开始，S0 完成并重命名后下一步为 S1。
- C12-M3 继承口径：S4 artifact 结论为 `native_hidden_retained`，`s5_input=null`；S5 no-comparison evidence 记录 `native_provenance_expected_ready_count=0`，未运行 current comparison，未创建 `backend_gap_candidate`。C12-M4 只能把 request-local ledger 发布为 CAD Core 产品契约，不能改写成 FreeCAD native expected。
- S0 禁止项：不修改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或 C5-M9 expected wording；不运行 FreeCADCmd、current comparison 或 full build。

## S1 live 记录

- 基线命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=842ce54ad7`，`git log -1 --oneline=842ce54ad7 docs: 冻结 C12-M4 S0 live 基线`。
- 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`；本步只更新 C12-M4 docs/TSV 并重命名 S1 步骤文件，不改 `cad-core/src`、`cad-core/include`、tests、fixtures expected、adapters、capability wording 或 C5-M9 expected JSON。
- 队列检查确认 C12-M4 执行前从 S1 开始，S1 文件未标 `【已实现】`；S1 完成后只应推进到 S2，不关闭 S2 的产品契约边界。
- Current ledger 证据：`ProjectedShapeEvidence` 是字段生产账本；`projectOnSurfaceMapperEvidenceJson()` 输出 `reference_recovery_hook=mapper_history_event_target_subname` 以及 face/height/compound/wire ownership；`namedShapeForProjectOnSurfaceProvenance()` 写入 `mapper_history` 与 `element_history_status`；`MapperHistoryEvent` 公共字段为 source、target、shapeKind、relation、makerStage、evidence、recoverability、diagnosticStatus。
- Consumer/wording 证据：`cad-core/tests/test_p8_features.py` 的 C5-M9 focused methods 覆盖 edge、wire split、invalid diagnostic、face rebuild 和 all-compound 字段；五个 C5-M9 expected JSON 当前仍是 `known_gap` / `native_hidden` / native replacement wording。S1 只把这些迁移点列入 `expected_migration`，继续保持 C12-M3 native oracle unavailable 口径。

## S2 live 记录

- 基线命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=49cdd9d086`，`git log -1 --oneline=49cdd9d086 docs: 完成 C12-M4 S1 ledger 证据矩阵`。
- 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`；本步只更新 C12-M4 docs/TSV、root `docs/CADCore12.0/README.md` 并重命名 S2 步骤文件，不改 `cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability wording 或 C5-M9 expected JSON。
- 队列检查确认 C12-M4 执行前从 S2 开始，S2 文件未标 `【已实现】`；S2 完成后只应推进到 S3，不关闭 S3 expected / capability 迁移或 S4 发布闸门。
- 产品契约冻结：`projection_item_ledger`、`MapperHistoryEvent`、edge/wire、face rebuild、height/compound/offset、`element_history_status` 和 `reference_recovery_hook` 是 CAD Core request-local product contract；invalid projection diagnostic 是 CAD Core product diagnostic contract。
- C12-M3 negative probe 保持原义：S4 artifact 是 `native_hidden_retained`，`s5_input=null`；S5 no-comparison evidence 的 `native_provenance_expected_ready_count=0`，未运行 current comparison，未创建 `backend_gap_candidate`。S2 因此只能发布 `native_oracle_unavailable` 注记，不能发布 FreeCAD native parity success。
- Non-goal 已冻结：FreeCAD native ElementMap parity、output guessing、GUI/Workbench、persistent native Document / TopoDS / NamedShape / ElementMap cache、full BREP transport，以及在 C12-M4 decision package 内直接改 C++、include、fixture expected、tests、adapters 或 capability wording。
- `C12M4-BLOCKER-002` 已关闭；`C12M4-BLOCKER-003`、`C12M4-BLOCKER-004`、`C12M4-BLOCKER-005` 仍分别留给 S3 expected/capability 迁移和 S4 release gate。

## S3 live 记录

- 基线命令已执行：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=5aa11afe11`，`git log -1 --oneline=5aa11afe11 docs: 完成 C12-M4 S2 产品契约冻结`。
- 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 输出为空，即 `<clean>`；本步只更新 C12-M4 包内 README、总入口、矩阵并重命名 S3 步骤文件，不改 root README、`cad-core/src`、`cad-core/include`、fixtures expected、tests、adapters、capability source、接口文档正文或 C5-M9 expected JSON。
- 队列检查确认 C12-M4 执行前从 S3 开始，S3 文件未标 `【已实现】`；S3 完成后只应推进到 S4 产品契约发布闸门。
- Expected migration 设计已列全五个 C5-M9 expected JSON：edge、wire split、face rebuild、all compound 和 invalid diagnostic 均从 `known_gap/native_hidden replacement` wording 迁移为 CAD Core request-local `product_contract` 或 `product_diagnostic_contract` wording，并保留 `native_oracle_unavailable`、source authority、focused assertions 和非目标。
- Capability / adapter / interface 后续迁移目标已定位：`cad-core/src/runtime/capability_contract.cpp:821-952` 的 `part_workbench.project_on_surface`，`cad-core/tests/test_adapters.py:2970-3104` 的 capability assertions，以及 `docs/接口规定/01-cad-recompute全量输入输出接口.md:113-127` 的 ProjectOnSurface DTO 文案。
- `C12M4-BLOCKER-003` 已关闭；`C12M4-BLOCKER-004` 记录 exact sources 后保留为 S4 release decision pending；`C12M4-BLOCKER-005` 仍由 S4 发布闸门关闭。

## 决策

1. `ProjectOnSurface` 的几何构造仍必须沿 FreeCAD 调用链对齐；source authority 继续写明 FreeCAD 源文件、类/函数和关键短句。
2. Provenance contract 不再等待 FreeCAD native `getElementHistory` / `ElementMap` artifact；C12-M3 artifact 已证明公开路径仍是 `native_hidden_retained`，S2 将其保留为 `native_oracle_unavailable`。
3. `projection_item_ledger`、`MapperHistoryEvent`、edge/wire、face rebuild、height/compound/offset、`element_history_status`、`reference_recovery_hook` 等字段是 CAD Core request-local product contract；invalid projection diagnostics 是 product diagnostic contract。
4. C5-M9 expected 的 `known_gap` wording 需要后续迁移为 product-contract wording；S3 已列明五个 expected JSON、capability source、adapter assertion 和接口文档的最小迁移面。迁移只改变契约状态和发布文案，不降低测试覆盖、source-backed 字段要求或 `native_oracle_unavailable` 注记。

## 最小完整语义批次

C12-M4 不拆成单个 fixture，因为 edge/wire split、face rebuild、height solid、compound child、offset 和 invalid diagnostic 都依赖同一 request-local ledger 语义。批次闭环如下：

- S0 冻结 C12-M3 继承口径、live baseline 和禁止项。
- S1 盘点当前 `cad-core` ledger 字段、focused tests、C5-M9 expected wording 和前端引用恢复需求。
- S2 发布产品契约边界：哪些字段是 CAD Core contract，哪些仍是 FreeCAD geometry parity 或 non-goal。
- S3 设计 expected/test/capability/interface wording 迁移包，但不在决策包里直接改代码或 expected JSON。
- S4 发布闸门：决定是否另开 implementation package 迁移 C5-M9 wording 与 capability docs。

## 发布闸门

S4 只有在以下条件都满足时，才建议另开 implementation package：

1. Contract 字段能追溯到 `FeatureProjectOnSurface.cpp` 的 request-local输入/构造顺序和当前 `cad-core` ledger 实现。
2. C12-M3 native-hidden 结论被保留为 native oracle unavailable，而不是被改写成 FreeCAD parity success。
3. Expected / tests / capability wording 的迁移范围清楚，并能用 focused C5-M9 tests 验证。
4. 非目标仍拒绝 output guessing、persistent native cache、full BREP transport 和 GUI/Workbench state。

## 交付物

- `矩阵/c12m4_project_on_surface_request_local_ledger_source_authority.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_contract_fields.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_scope_review_matrix.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_expected_migration_matrix.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_blocker_queue.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_non_goal_registry.tsv`
- `矩阵/c12m4_project_on_surface_request_local_ledger_validation_matrix.tsv`

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次 docs/CADCore12.0/README.md
git diff --check
```
