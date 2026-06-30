# C12-M8 SubShapeBinder CopyOnChange Native Copied Graph DTO 准入解锁批次总入口

## 目标

按顺序执行 C12-M8 S0-S6，裁决 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 是否具备从 retained diagnostic 进入后续 implementation package 的条件。

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 每步执行前刷新 C12-M8 队列；只处理当前第一条未完成 step。
3. S0-S4 默认只改本包 docs / matrices，不改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。
4. 只有 S5 同时确认 native copied graph evidence、request-local DTO approval 和 current mismatch，才允许输出后续 implementation package。
5. 不把 App::Link `documentObjectUpdates` 直接等同于 SubShapeBinder CopyOnChange full support。
6. 不从 fixture 输出倒推业务逻辑；必须记录 FreeCAD source、native artifact、DTO 边界和 current comparison。
7. 每步完成后重命名为 `【已实现】` 并更新 README / 总入口 / 矩阵中对应状态。

## 顺序

- S0：live 基线与 C12-M5/C12-M7 继承口径冻结（已完成）。
- S1：FreeCAD source、current coverage 和 App::Link transport 证据复核。
- S2：native copied graph probe schema 与 evidence gate。
- S3：request-local DTO 产品边界裁决。
- S4：current mismatch 与 implementation candidate gate。
- S5：implementation package authorization / no-code retained decision。
- S6：发布闸门、README 更新和后续分流。

## 当前执行状态

- 工作步骤总入口已随 S0 标记为 `【已实现】`，仅承担 C12-M8 S0-S6 队列索引职责。
- S0 live 基线与继承口径冻结已完成：`HEAD=fd9810dc23`，起点 worktree clean，C12-M1..M7 队列均只输出表头。
- live `part_design.sub_shape_binder` capability 仍为 `supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`；known gap 仍是 `known_gap_diagnostic` / `oracle_blocked` / `copy_on_change_full_temporary_document_cache_not_supported`。
- C12-M5 `no_code_retained_diagnostic` 与 C12-M7 `product_diagnostic_contract_published` 后续分流口径均继续有效；S1 仍只进入 source/current 覆盖复核，不推进实现或 oracle。

## 必要裁决

- 若 S2 不是 `native_copied_graph_evidence_ready`，S3/S4 不能跳到 implementation。
- 若 S3 不是 `dto_approved_for_request_local_graph`，S4 只能保留 diagnostic。
- 若 S4 无 current mismatch，S5 不能创建 implementation package。
- 若任一条件失败，最终出口必须写明 retained blocker、删除条件和下一次重开条件。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
