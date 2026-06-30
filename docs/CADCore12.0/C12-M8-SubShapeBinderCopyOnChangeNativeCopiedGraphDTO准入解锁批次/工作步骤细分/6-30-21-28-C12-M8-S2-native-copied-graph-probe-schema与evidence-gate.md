# C12-M8 S2 native copied graph probe schema 与 evidence gate

## 目标

定义并执行或阻断 FreeCAD native probe：证明 `SubShapeBinder` CopyOnChange 是否能输出 stable copied-object graph evidence，而不只是 property/session 状态。

## 必读文件

- `../矩阵/c12m8_copy_on_change_native_graph_probe_matrix.tsv`
- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m8_copy_on_change_blocker_queue.tsv`
- `docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/c12m5_copy_on_change_source_candidates.tsv`
- `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`
- `cad-core/fixtures/c9m5/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`

## 操作

1. 固定 artifact schema：`c12m8.subshapebinder-copy-on-change-native-copied-graph.v1`。
2. schema 必须包含：FreeCAD / OCCT baseline、mode、PartialLoad、support before/after、`_tmp_binder`、`_CopiedObjs` identity、copied dependency order、copied support rewrite、`_CopiedLink` subvalues、recompute status、ElementMap / NamedShape lifecycle、request-local serializability judgement。
3. 如果 FreeCADCmd probe 可以安全运行，采集 artifact 到 `docs/temp/`，并把路径写入矩阵。
4. 如果 probe 不能稳定导出 copied graph evidence，关闭为 `native_evidence_retained_blocker`，不要进入 S3 approval。
5. 不允许把 Python-visible property 状态、object label、bbox 或 shape count 当作 copied graph evidence。

## 关闭条件

- `C12M8-PROBE-001..009` 均有 `required_evidence`、`observed_status` 和 `decision`。
- 若所有必填 evidence 成立，S2 输出 `native_copied_graph_evidence_ready`。
- 若任一核心 evidence 缺失，S2 输出 `native_evidence_retained_blocker` 并更新 `C12M8-BLOCKER-201`。

## 非目标

- 不改 collector checked-in expected。
- 不改 `cad-core/src`。
- 不把 backend session 或 temporary document 保存作为解决方案。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
