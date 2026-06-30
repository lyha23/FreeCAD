# C12-M5 S2 native 证据刷新与 probe 准入【已实现】

## 目标

判断是否需要刷新或新增 FreeCAD native probe，并明确 copied-object evidence 是否足够进入 request-local DTO 决策。

## 必读文件

- `../矩阵/c12m5_copy_on_change_scope_review_matrix.tsv`
- `../矩阵/c12m5_copy_on_change_backend_gap_classification.tsv`
- `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`
- `cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`
- `cad-core/tools/collect_c8m1_shapebinder_expected.py`
- `cad-core/fixtures/c9m5/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`
- C9-M5 / C10-M4 README 中 S3 native evidence 结论。

## 操作

1. 复核旧 probe 是否仍覆盖 Disabled / Enabled / Mutated / PartialLoad、`_tmp_binder`、`_CopiedLink` 和 property state。
2. 判断 `_CopiedObjs`、dependency order、support rewrite、ElementMap lifecycle 是否仍不可观测。
3. 如果旧 evidence 足够证明 retained diagnostic，关闭为 `native_evidence_retained_blocker`。
4. 如果发现可采集新 evidence，先设计 probe schema 和 artifact path，不直接改 expected 或 C++。

## S2 live 基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=b38b0647d6`。
- `git log -1 --oneline=b38b0647d6 docs: 完成 C12-M5 S1 source 覆盖复核`。
- `git -c core.quotepath=false status --short -uall` 无输出，dirty boundary 为 `<clean>`；未发现非本任务 dirty work。
- S2 开始前队列首项为本文件，S3-S5 仍 pending。

## S2 证据复核结果

- 已复核 `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`、`cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`、`cad-core/tools/collect_c8m1_shapebinder_expected.py`、C9-M5 checked-in expected 和 C9/C10 README；未修改 probe、expected、fixture、C++、tests 或 adapter。
- C9-M5 / C8-M2 probe schema 仍覆盖 Disabled / Enabled / Mutated / PartialLoad、dynamic CopyOnChange property、mutation-triggered Mutated、`_tmp_binder` document visibility、`_CopiedLink` hidden property value 和 Python-visible property state。
- C9-M5 expected 仍是 FreeCAD `1.2.0 revision 20260519`，route 为 `native_evidence_collected_with_known_gap_blocker`；初次 recompute 与 mutation recompute 后均可见 `_tmp_binder` document。
- `_CopiedObjs` 在 checked-in expected 中仍表现为 Python API 不可见：`hasattr=false`，property API 返回 no property / no attribute；它不能作为 request-local copied-object graph DTO。
- `_CopiedLink` 可见值只能证明 native session 内 copied link 被写入；它是单值 link evidence，不包含完整 copied-object graph、dependency order 或 frontend graph writeback target。
- `PartialLoad=True` 与 `Support` 的 Python-visible value / property status 可见，但 `getAllowPartial` / `allowPartial` / `isAllowedPartial` 均不可用；这仍不足以证明 internal allow-partial 或 copied-object lifecycle 可稳定序列化。
- `copyObject()` dependency order、copied support rewrite 的完整 graph 和 `recomputeFeature(true)` internal ElementMap lifecycle 仍列在 unobservable fields 中；没有新的 stable copied-object graph evidence。
- 因此 S2 不设计新 probe schema，不采新 oracle，不改 expected，不打开 C++ implementation candidate。

## S2 关闭结论

- `C12M5-SCOPE-003` 关闭为 `native_evidence_retained_blocker`。
- `C12M5-CAT-001` 继续保留 `copy_on_change_full_temporary_document_cache` diagnostic。
- `C12M5-CAT-002` 只保留未来 probe / DTO 设计候选，当前不进入 implementation。
- `C12M5-BLOCKER-201` 关闭为 `native_evidence_retained_blocker`。
- S3 只能做 request-local DTO 产品边界裁决：允许 / 拒绝 DTO 字段和前端 graph 写回职责，不能基于 S2 打开 C++ implementation candidate。

## 非目标

- 不在没有 schema 的情况下运行散乱 probe。
- 不把 property-visible evidence 当作 copied-object graph evidence。
- 不用 FreeCAD native session object pointer 当 request-local DTO 字段。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'CopiedObjs|tmp_binder|CopiedLink|PartialLoad|BindCopyOnChange|native_oracle|notCollected|diagnostic_retained|retained|oracle_blocked' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 cad-core/tools cad-core/fixtures/c9m5
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```
