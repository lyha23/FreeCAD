# C12-M8 S2 native copied graph probe schema 与 evidence gate【已实现】

## 目标

定义并执行或阻断 FreeCAD native probe：证明 `SubShapeBinder` CopyOnChange 是否能输出 stable copied-object graph evidence，而不只是 property/session 状态。

## 必读文件

- `../矩阵/c12m8_copy_on_change_native_graph_probe_matrix.tsv`
- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m8_copy_on_change_blocker_queue.tsv`
- `docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/c12m5_copy_on_change_source_candidates.tsv`
- `C9-M5 CopyOnChange native probe（脚本已移除）`
- `cad-core/fixtures/c9m5/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`

## 操作

1. 固定 artifact schema：`c12m8.subshapebinder-copy-on-change-native-copied-graph.v1`。
2. schema 必须包含：FreeCAD / OCCT baseline、mode、PartialLoad、support before/after、`_tmp_binder`、`_CopiedObjs` identity、copied dependency order、copied support rewrite、`_CopiedLink` subvalues、recompute status、ElementMap / NamedShape lifecycle、request-local serializability judgement。
3. 如果 FreeCADCmd probe 可以安全运行，采集 artifact 到 `docs/temp/`，并把路径写入矩阵。
4. 如果 probe 不能稳定导出 copied graph evidence，关闭为 `native_evidence_retained_blocker`，不要进入 S3 approval。
5. 不允许把 Python-visible property 状态、object label、bbox 或 shape count 当作 copied graph evidence。

## 关闭条件

- `C12M8-PROBE-001..010` 均有 `required_evidence`、`observed_status` 和 `decision`。
- 若所有必填 evidence 成立，S2 输出 `native_copied_graph_evidence_ready`。
- 若任一核心 evidence 缺失，S2 输出 `native_evidence_retained_blocker` 并更新 `C12M8-BLOCKER-201`。

## S2 结论

- 本轮 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=f5ed6d2397`，`git log -1 --oneline=f5ed6d2397 文档：关闭 C12-M8 S1 源码复核`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S2 执行前队列确认：`6-30-21-28-C12-M8-S2-native-copied-graph-probe-schema与evidence-gate.md` 是第一条 pending，后续为 S3-S6。
- 本机 FreeCADCmd 可运行；本轮只写入 `docs/temp/`，未刷新 checked-in expected。
- FreeCAD / OCCT baseline：`freecadcmd=/Users/li/.cargo/bin/freecadcmd`，FreeCAD `1.2.0 revision 20260519`，OCCT `7.8.1`。
- raw native probe artifact：`docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-probe.raw.c9m5.freecad.json`。该 artifact 仍是旧 `cad-core.c9m5-subshapebinder-copyonchange-native-probe.v1` schema，只能证明 Python-visible `BindCopyOnChange` / `PartialLoad` / `Support` 状态、`_tmp_binder` document name、部分 `_CopiedLink` hidden property value 和 shape summary。
- C12-M8 gate artifact：`docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json`，schema 固定为 `c12m8.subshapebinder-copy-on-change-native-copied-graph.v1`。
- `C12M8-PROBE-001..010` 均已写入 observed_status / decision / artifact_or_note。
- `C12M8-PROBE-005`、`C12M8-PROBE-006`、`C12M8-PROBE-009` 继续缺核心 copied graph evidence：`_CopiedObjs` private vector contents / payload、`_tmp_binder copyObject object graph and dependency order`、`copyObject dependency mapping behind temporary document cache`、`recomputeFeature(true) internal copied-object ElementMap lifecycle` 仍为 unobservable。
- `C12M8-PROBE-007` 只能看到 `_CopiedLink` after recompute 指向 `SupportBox001` 且 subvalues 为空，不能证明 source-to-copy rewrite map、dependency edges 或 deterministic copied support rewrite。
- `C12M8-PROBE-008` 只能看到 `doc.recompute` 成功，不能导出 `SubShapeBinder::update()` 内两次 `copied->recomputeFeature(true)` 的状态、validity transition、logs 或 failure modes。
- `Cache_*` 已按 source classified 为 transform matrix cache hit/update/cleanup optimization；没有 native evidence 证明它是 copied graph semantic state，backend persistent `Cache_*` 继续禁止。
- S2 输出：`native_evidence_retained_blocker`。
- `C12M8-BLOCKER-201` 已关闭为 `closed_s2_retained_blocker`；S3/S4 只能继承该 blocker 做 DTO rejection/defer 和 retained diagnostic path，不能跳到 implementation approval。

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
