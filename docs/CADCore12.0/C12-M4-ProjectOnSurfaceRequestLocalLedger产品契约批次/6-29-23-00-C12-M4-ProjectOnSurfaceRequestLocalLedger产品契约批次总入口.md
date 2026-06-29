# C12-M4 ProjectOnSurface Request-Local Ledger 产品契约批次总入口

## 包目标

C12-M4 的目标是把 `cad-core` 当前 ProjectOnSurface request-local projection ledger 升级为明确产品契约。它承认 C12-M3 的负结论：FreeCAD 原生 ProjectOnSurface history API 没有暴露可用于替换 C5-M9 的 `native_provenance_expected_ready`。因此后续工作不再以 native expected 作为唯一 gate，而是把 `cad-core` 为前端引用恢复提供的 request-local ledger 定义为 CAD Core 自有 contract。

S0 live 起点为 `HEAD=fe563906c8`（`fe563906c8 docs: 完成 C12-M3 S6 no-code 发布闸门`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点工作区为 clean。

## 决策

1. `ProjectOnSurface` 的几何构造仍必须沿 FreeCAD 调用链对齐；source authority 继续写明 FreeCAD 源文件、类/函数和关键短句。
2. Provenance contract 不再等待 FreeCAD native `getElementHistory` / `ElementMap` artifact；C12-M3 artifact 已证明公开路径仍是 `native_hidden_retained`。
3. `projection_item_ledger`、`mapper_history`、`element_history_status`、`reference_recovery_hook`、`face_wire_sources`、`height_solid_ownership`、`compound_child_ownership` 等字段是 CAD Core request-local contract。
4. C5-M9 expected 的 `known_gap` wording 需要后续迁移为 product-contract wording；迁移只改变契约状态，不降低测试覆盖或 source-backed 字段要求。

## 最小完整语义批次

C12-M4 不拆成单个 fixture，因为 edge/wire split、face rebuild、height solid、compound child、offset 和 invalid diagnostic 都依赖同一 request-local ledger 语义。批次闭环如下：

- S0 冻结 C12-M3 继承口径、live baseline 和禁止项。
- S1 盘点当前 `cad-core` ledger 字段、focused tests、C5-M9 expected wording 和前端引用恢复需求。
- S2 发布产品契约边界：哪些字段是 CAD Core contract，哪些仍是 FreeCAD geometry parity 或 non-goal。
- S3 设计 expected/test/capability wording 迁移包，但不在决策包里直接改代码。
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
