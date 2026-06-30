# C12-M5 S4 current mismatch 与实现候选闸门【已实现】

## 目标

只有在 S2 native evidence 和 S3 产品 DTO 均成立时，才比较 current `cad-core` retained diagnostic 与目标 DTO 行为，决定是否发布 implementation candidate。

## 必读文件

- `../矩阵/c12m5_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m5_copy_on_change_blocker_queue.tsv`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/runtime/capability_contract.cpp`

## 操作

1. 检查 S2 是否有 `native_evidence_ready`，S3 是否有 `dto_approved_for_mismatch_gate`。
2. 若任一条件不成立，关闭为 `no_current_mismatch_retained_diagnostic`。
3. 若条件成立，定义最小 implementation candidate：fixtures、expected、core source、adapter/capability/test/docs 修改面。
4. 禁止在 S4 直接落 C++；S4 只发布或拒绝 implementation candidate。

## S4 live 基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=f7c1e83960`。
- `git log -1 --oneline=f7c1e83960 docs: 完成 C12-M5 S3 DTO 边界冻结`。
- `git -c core.quotepath=false status --short -uall` 无输出，dirty boundary 为 `<clean>`；未发现非本任务 dirty work。
- S4 开始前队列首项为本文件，S5 仍 pending。

## S4 闸门结论

- S2 未达到 `native_evidence_ready`，已关闭为 `native_evidence_retained_blocker`：旧 probe 只给出 property / session evidence，仍没有 stable copied-object graph、dependency order、copied support rewrite 或 `recomputeFeature(true)` ElementMap lifecycle DTO 证据。
- S3 未达到 `dto_approved_for_mismatch_gate`，已关闭为 `dto_rejected_known_gap_retained`：当前产品边界拒绝 temporary document、native pointer、full object BREP / TopoDS、post-request NamedShape / ElementMap cache、`_CopiedObjs` 和 `_tmp_binder` session state 作为 request-local DTO。
- 因任一前置条件不成立即不得进入 implementation candidate，本轮两项前置条件均不成立；S4 关闭为 `no_current_mismatch_retained_diagnostic`。
- current `cad-core/src/part_design/feature_shape_binder.cpp` 仍在 `BindCopyOnChange=Enabled`、`BindCopyOnChange=Mutated` 或 `PartialLoad=True` 时发布 `copy_on_change_full_temporary_document_cache_not_supported`。
- current `cad-core/src/runtime/capability_contract.cpp` 仍把 `copy_on_change_full_temporary_document_cache` 发布为 `known_gap_diagnostic` / `oracle_blocked`，并保留 delete / reopen condition。
- focused tests 仍覆盖 retained diagnostic 和 capability wording；因此 current 行为与 S2/S3 的 retained gap 决策一致，不存在需要打开实现包的 current mismatch。

## S4 关闭结论

- `C12M5-BLOCKER-401` 关闭为 `no_current_mismatch_retained_diagnostic`。
- `C12M5-CAT-005` 关闭为 implementation candidate absent。
- S4 不创建 fixtures、expected、adapter、capability 或 C++ implementation package；S5 只能发布 retained diagnostic / no-code release fallback，除非后续另有 approved native evidence 与 DTO。

## 非目标

- 不绕过 S2/S3 直接实现。
- 不删除 diagnostic。
- 不把 unsupported test 改成通过 supported wording。
- 不创建 implementation package。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder.CadCoreC8ShapeBinderTest.test_capability_contract_publishes_c8m1_binder_scope tests.test_c8_shapebinder.CadCoreC8ShapeBinderTest.test_bindmode_and_copy_on_change_lifecycle_boundaries_are_explicit
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```
