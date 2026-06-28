# 【已实现】C9-M5-S5 cad-core 实现闸门与 diagnostic 发布复审

## 目标

把 S3 native evidence 和 S4 产品裁决映射到 cad-core implementation gate。S5 可以更新文档、矩阵和测试计划；只有在必要时才修改 capability assertions。S5 本身不做主要 C++ 实现。

## 执行基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`b9f370ab76`
- `git log -1 --oneline`：`b9f370ab76 docs: 关闭 C9-M5 S4 DTO 产品边界复审`
- `git -c core.quotepath=false status --short -uall`：无输出，工作区干净。
- 队列首项是本 S5 文件；S6 仍 pending。

## S3 / S4 输入

- S3 native evidence 已观察 Disabled / Enabled / Mutated / PartialLoad、`_tmp_binder` 和 `_CopiedLink`，但 `_CopiedObjs`、`copyObject()` dependency order 与 `recomputeFeature(true)` internal `ElementMap` lifecycle 仍不可导出为稳定 request-local DTO。
- S4 产品裁决为 `dto_rejected_known_gap_retained`；允许字段只限 request graph、request-local `documentObjectUpdates` / diagnostics、source / support / mutation intent 和 deletion / update / reselect 建议。
- S4 forbidden fields 仍禁止 hidden temporary document、BREP / `TopoDS_Shape` / full shape cache、request 后继续有效的 `NamedShape` / `ElementMap` / copied-object cache、`_CopiedObjs` 和 native pointer identity。
- 因 S4 未批准 DTO，S5 不生成 `backend_gap_requires_implementation`，也不打开 S6 C++ support path。

## current cad-core 检查

- `cad-core/src/part_design/feature_shape_binder.cpp`：当前 CopyOnChange Enabled / Mutated / PartialLoad diagnostic boundary。
- `cad-core/src/app/copy_on_change.cpp`：App Link CopyOnChange request-local updates，可作为 DTO 对照。
- `cad-core/src/runtime/capability_contract.cpp`：`known_gaps`、`remaining_gaps`、diagnostic、delete / reopen condition。
- `cad-core/tests/test_c8_shapebinder.py`：SubShapeBinder capability 和 known gap focused assertion。
- `cad-core/tests/test_diagnostics.py`、`cad-core/tests/test_adapters.py`：diagnostic vocabulary 和 capability smoke。

## 闸门规则

| S4 裁决 | S5 动作 |
| --- | --- |
| `dto_approved_candidate` | 将对应 backend category 升级为 `backend_gap_requires_implementation`，为 S6 列出 C++ / fixture / test / capability 变更。 |
| `dto_rejected_known_gap_retained` | 保持 `known_gap_diagnostic`，S6 no-code release gate。 |
| `needs_more_native_evidence` | 保持 `oracle_blocked`，补充 reopen condition 和下一轮 probe 条件。 |

## current cad-core 发布复审结论

S5 复核后，当前 cad-core 发布口径与 S4 拒绝裁决一致，无需修改业务代码或测试：

- `cad-core/src/part_design/feature_shape_binder.cpp::addLifecycleMetadata()` 仍在 `BindCopyOnChange=Enabled`、`BindCopyOnChange=Mutated` 或 `PartialLoad=true` 时发布 `copy_on_change_full_temporary_document_cache_not_supported`，并把 metadata 写为 `copy_on_change_boundary=known_gap_full_temporary_document_cache`。
- `cad-core/src/app/copy_on_change.cpp::buildCopyOnChangeLifecycleUpdates()` 仍只作为 App Link request-local `documentObjectUpdates` 词汇对照，不隐式替代 SubShapeBinder full temporary-document copied-object cache。
- `cad-core/src/runtime/capability_contract.cpp` 仍发布 `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache.status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`，且 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- `cad-core/tests/test_c8_shapebinder.py` focused tests 仍证明 CopyOnChange fixture 产生三条 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic，并断言 capability remaining gap 保留。
- `cad-core/tests/test_adapters.py` 继续把 App Link CopyOnChange `documentObjectUpdates` 作为 `link_transaction` 能力发布；这不是 SubShapeBinder full cache support。

## S5 闸门裁决

本轮裁决：`no_code_retained_known_gap_release_gate`。

理由：

- S4 裁决不是 `dto_approved_candidate`，因此 S5 不得把 `C9M5-SCOPE-201` 或 `C9M5-CAT-102` 升级为 `backend_gap_requires_implementation`。
- 当前 diagnostic、capability 和 focused tests 已经覆盖 retained known gap 的发布口径，不需要删除 diagnostic 或 capability `remaining_gaps` 来伪装支持。
- S6 路由必须是 no-code retained known gap：只发布并复核 `known_gap_diagnostic` / `oracle_blocked`，不实现 CopyOnChange support。

## 矩阵回写

- `C9M5-SCOPE-201`：保持 `dto_rejected_known_gap_retained`，S6 路由为 no-code retained known gap。
- `C9M5-SCOPE-202`：保持 `release_gate`，S6 只复核 `known_gap_diagnostic`、diagnostic code、delete / reopen condition 和 `remaining_gaps`。
- `C9M5-BLOCKER-501`：关闭为 `closed_S5`，close condition 是 current cad-core diagnostic/capability/focused tests 与 S4 拒绝裁决一致。
- `C9M5-CAT-102`：保持 `dto_rejected_known_gap_retained`，不生成 implementation backend gap。
- `C9M5-CAT-104`：保持 release gate，S6 发布 no-code retained known gap。

## S6 路由

S6 是 no-code release gate。它应消费 S3 native evidence、S4 `dto_rejected_known_gap_retained` 和本 S5 diagnostic/capability 复审结论，证明：

- `copy_on_change_full_temporary_document_cache` 仍是 retained known gap。
- `copy_on_change_full_temporary_document_cache_not_supported` 仍是公开 diagnostic。
- `remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 仍由 capability 和 focused tests 约束。
- 只有未来 native oracle 暴露稳定 request-local copied-object evidence 且产品批准 DTO 后，才允许重开 implementation gate。

## 必须回写的矩阵行

- `C9M5-SCOPE-201`
- `C9M5-SCOPE-202`
- `C9M5-BLOCKER-501`
- `C9M5-CAT-102`
- `C9M5-CAT-104`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'copy_on_change_full_temporary|copy_on_change_boundary|known_gaps|remaining_gaps|BindCopyOnChange' cad-core/src/part_design/feature_shape_binder.cpp cad-core/src/app/copy_on_change.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_c8_shapebinder.py cad-core/tests/test_adapters.py
(cd cad-core && ./cad-core capabilities > /tmp/c9m5-capabilities.json)
(cd cad-core && python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics)
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore9.0/README.md
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分 --format markdown
```

验收标准：

- 若 S4 未批准 DTO，focused tests 必须仍证明 `copy_on_change_full_temporary_document_cache_not_supported` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- 若 S4 批准 DTO，S5 必须列出 S6 的 exact code landing、fixture、expected、focused tests 和 capability update。
- S5 不允许靠删除 diagnostic 或 capabilities 字段伪装支持。

## 非目标

- 不运行全量 FreeCAD build。
- 不执行下游 Rust 同步。
