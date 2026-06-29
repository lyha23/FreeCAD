# C12-M5 S2 native 证据刷新与 probe 准入

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

## 非目标

- 不在没有 schema 的情况下运行散乱 probe。
- 不把 property-visible evidence 当作 copied-object graph evidence。
- 不用 FreeCAD native session object pointer 当 request-local DTO 字段。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'CopiedObjs|tmp_binder|CopiedLink|PartialLoad|BindCopyOnChange|native_oracle|notCollected|diagnostic_retained' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 cad-core/tools cad-core/fixtures/c9m5
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```

