# C8-M7 S5 capability 残留与 non-goal 发布准入

## 目标

根据 S0-S4 裁决 `import_shape.remaining=["changed_file_deleted_reference_recovery"]` 的最终发布方式，并决定 S6 是 no-code release gate 还是受限实现 gate。

## 裁决规则

- 如果 changed-file 当前请求重导入已 covered，删除 `changed_file_deleted_reference_recovery` 或改写为更精确的 non-goal / known-gap 发布。
- 如果 deleted-file 需要跨请求 full shape cache，必须进入 `diagnostic_non_goal` 或 `oracle_blocked`，不得标为 supported。
- 如果 request-local `ReferenceShadow` + current imported shape 存在 source-backed 缺口，标为 `request_local_backend_gap` 并列出 S6 允许落点和 focused tests。
- 如果只是 capability 文案滞后，S6 只改 `capability_contract.cpp` 和 tests，或只做文档 no-code closure，不能扩大 runtime 行为。

## 必须回写

- `c8m7_import_shape_recovery_backend_gap_classification.tsv`
- `c8m7_import_shape_recovery_non_goal_registry.tsv`
- `c8m7_import_shape_recovery_validation_matrix.tsv`
- `C8M7-BLOCKER-501`
- README 的 S5 结论。

## S5 裁决结论

S5 裁决当前 `import_shape.remaining=["changed_file_deleted_reference_recovery"]` 是 stale mixed publication，不是 request-local backend gap。S3 已将 readable changed-file / same-file `FileName` 的 current request reimport 收口为 `already_covered`；S4 已将 current imported shape + request-local `ReferenceShadow` 的恢复边界收口为 `already_covered`；deleted / unreadable `FileName` 没有 current imported shape 时只能走 diagnostic / `diagnostic_non_goal`，不得声明 old geometry recovery supported。

S6 选择受限 capability publication patch：允许修改 `cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py`，移除 stale mixed remaining token，并用 adapter capability test 锁住 `import_shape.remaining` 为空、`changed_file_deleted_reference_recovery` 不再出现、deleted-file old geometry recovery 不进入 supported / covered。S6 不允许改 runtime import、reference resolution、elementReferenceUpdates、fixtures、expected、adapter 字符串改写或下游同步。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m7-s5-capabilities.json
python3 -m unittest tests.test_adapters
```

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不把 C7-M7 oracle-blocked 行改成 supported。
- 不对下游仓库做同步实现。
- 不用 adapter 层隐藏 capability residual。
