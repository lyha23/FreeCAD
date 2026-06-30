# C12-M8 SubShapeBinder CopyOnChange Native Copied Graph DTO 准入解锁批次方案

## 结论

C12-M8 是 CopyOnChange 的“解锁包”，不是代码实现包。它只回答一个问题：是否已经具备把 FreeCAD `SubShapeBinder` CopyOnChange full path 转成 CAD Core request-local 产品契约的三段证据链。

如果三段证据链成立，S5/S6 产出后续 implementation package；如果不成立，保留 `copy_on_change_full_temporary_document_cache_not_supported`，并把缺口继续标为 `known_gap_diagnostic` / `oracle_blocked`。

## 为什么不能直接实现

`SubShapeBinder` CopyOnChange 的 full path 不是普通 shape copy。FreeCAD 里它会进入 `_tmp_binder`、`_CopiedObjs`、`copyObject()`、`recomputeFeature(true)`、`_CopiedLink` 和 copied support rewrite 的组合流程。CAD Core 的边界是无状态 request graph：后端不能保存 temporary document、TopoDS、BREP、NamedShape、ElementMap 或 copied-object cache。

因此实现前必须先证明：这些 native 行为能被表达为前端持久化的 DocumentObject graph / `documentObjectUpdates`，而不是把 FreeCAD 的临时文档生命周期搬进后端。

## 最小完整语义批次

本批次不拆成单个 fixture。`BindCopyOnChange=Enabled`、`BindCopyOnChange=Mutated`、`PartialLoad=True`、`Cache_*`、copied dependency order、support rewrite 和 ElementMap lifecycle 是同一条 FreeCAD CopyOnChange 调用链。只挑一个 property case 会重复 C12-M5 的问题：看到 property 状态，却没有 copied graph 语义。

本批次一次性覆盖：

- FreeCAD `SubShapeBinder` lifecycle source authority。
- current `cad-core` retained diagnostic 与 App::Link CopyOnChange transport。
- native copied graph probe schema。
- DTO 允许字段 / 禁止字段。
- current mismatch gate。
- implementation package authorization / no-code retained 出口。

## 交付物

- `README.md`
- `6-30-21-24-C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次总入口.md`
- `工作步骤细分/`
- `矩阵/c12m8_copy_on_change_source_candidates.tsv`
- `矩阵/c12m8_copy_on_change_scope_review_matrix.tsv`
- `矩阵/c12m8_copy_on_change_native_graph_probe_matrix.tsv`
- `矩阵/c12m8_copy_on_change_dto_contract_fields.tsv`
- `矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `矩阵/c12m8_copy_on_change_blocker_queue.tsv`
- `矩阵/c12m8_copy_on_change_non_goal_registry.tsv`
- `矩阵/c12m8_copy_on_change_validation_matrix.tsv`

## 裁决出口

| 出口 | 含义 |
| --- | --- |
| `implementation_package_authorized` | S2 native copied graph evidence、S3 DTO approval、S4 current mismatch 全成立；另开 C++ implementation 包。 |
| `no_code_retained_diagnostic` | 任一解锁条件不成立；保留 current diagnostic，不改代码。 |
| `probe_blocked_retained` | FreeCADCmd / native probe 不能稳定导出 copied graph；保留 oracle_blocked。 |
| `dto_rejected_retained` | native evidence 仍要求 temporary document / cache / persistent geometry state；拒绝进入 request-local DTO。 |
| `publication_repair_only` | current capability / docs wording 需要修正，但没有 C++ mismatch。 |

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次 docs/CADCore12.0/README.md
git diff --check
```

后续若 S5 授权 implementation package，再由新包决定 focused tests、fixtures、expected 和 C++ 修改范围。C12-M8 本身不要求 full build 或 full FreeCAD CI。
