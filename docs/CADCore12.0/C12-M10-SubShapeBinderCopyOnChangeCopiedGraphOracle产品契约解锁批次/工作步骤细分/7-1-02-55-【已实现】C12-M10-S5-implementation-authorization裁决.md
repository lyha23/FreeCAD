# C12-M10 S5 implementation authorization 裁决

## 目标

汇总 S1-S4 证据，决定是否授权后续 implementation package，或关闭为 product-contract follow-up / oracle retained / no-code retained diagnostic。

## 必读文件

- `../README.md`
- `../7-1-02-48-C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次总入口.md`
- `../矩阵/c12m10_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m10_copy_on_change_blocker_queue.tsv`
- `../矩阵/c12m10_copy_on_change_validation_matrix.tsv`

## 操作

1. 核对 S2 native copied graph evidence 是否 ready。
2. 核对 S3 DTO / product contract 是否 approved。
3. 核对 S4 current mismatch 是否 confirmed。
4. 三项均成立时输出 `implementation_package_authorized`，并写后续 implementation package 最小完整语义批次。
5. 若缺 product decision，输出 `product_contract_package_required`。
6. 若缺 native evidence，输出 `oracle_blocked_retained`。
7. 若无 mismatch，输出 `no_code_retained_diagnostic`。
8. 将本 S5 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M10-BLOCKER-501` 关闭。
- `C12M10-CAT-004..005` 写最终分类。
- README / 总入口记录 S5 出口。

## 非目标

- 不直接修改 C++。
- 不刷新 expected。
- 不删除已发布 known gap，除非 S5 明确授权后续 implementation package。

## S5 结论

- S5 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=160c4104de`（`160c4104de 文档：关闭 C12-M10 S4 current mismatch 准入`），起点 worktree clean。
- S2 继承为 `native_oracle_blocked_retained`：S2 gate artifact 仍缺 `_CopiedObjs` stored identity/order、`Document::copyObject()` dependency order/mapping、内部 `recomputeFeature(true)` lifecycle 与 ElementMap / NamedShape 分阶段 lifecycle。
- S3 继承为 `dto_not_reviewed_due_to_native_blocker`：没有 approved copied graph execution DTO 或产品契约；App::Link `documentObjectUpdates` 仍仅作 reference vocabulary。
- S4 继承为 `not_comparable` / `no_current_mismatch_retained_diagnostic`：没有 admitted mismatch-confirmed row，current diagnostic `copy_on_change_full_temporary_document_cache_not_supported` 继续作为 known gap / oracle_blocked retained diagnostic。
- S5 final decision：`oracle_blocked_retained` + `no_code_retained_diagnostic`。三闸门未同时成立，因此不授权 implementation package，不创建后续实现包，不删除 known gap。
- `C12M10-CAT-004` 已关闭为 `not_authorized_oracle_blocked_retained_no_code_retained_diagnostic`；`C12M10-CAT-005` 已关闭为 `no_code_retained_diagnostic`。
- `C12M10-BLOCKER-501=closed_s5_oracle_blocked_retained_no_code_retained_diagnostic`；`C12M10-VAL-501=passed_s5_oracle_blocked_retained_no_code_retained_diagnostic`。
- Delete / reopen condition：只有新的 FreeCAD native artifact 稳定暴露 copied graph 核心证据，产品批准 request-local copied graph DTO / contract，且 current comparison 形成 mismatch-confirmed row 后，才允许重开 implementation authorization 并替换 retained diagnostic。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
