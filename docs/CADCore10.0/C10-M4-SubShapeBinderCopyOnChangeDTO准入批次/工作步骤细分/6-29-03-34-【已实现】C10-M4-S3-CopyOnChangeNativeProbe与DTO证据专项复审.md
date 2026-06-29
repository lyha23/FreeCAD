# C10-M4 S3 CopyOnChange Native Probe 与 DTO 证据专项复审

## 目标

复审 FreeCAD native SubShapeBinder CopyOnChange lifecycle 是否能稳定暴露 request-local copied-object evidence。S3 可以更新 probe / expected / docs evidence；只有证据证明可观测时，才把后续 S4/S6 继续推进到 DTO comparison。

## 必做动作

1. 复核既有 C8/C9 probe 和 expected：`probe_c9m5_subshapebinder_copyonchange.py`、`probe_c8m2_subshapebinder_copyonchange.py`、`collect_c8m1_shapebinder_expected.py`、`cad-core/fixtures/c8m1`、`cad-core/fixtures/c8m2`。
2. 如需重跑 FreeCADCmd，记录 FreeCAD / LibPack / OCCT 版本和执行命令；sandbox Qt/FreeCADCmd 错误不能当作语义失败。
3. 判断 evidence 是否满足 request-local DTO：copied object identity、source mutation trigger、front-end writeback target、no backend session dependency。
4. 更新 scope / blocker / backend-gap matrix：可观测则进入 S4 DTO comparison，不可观测则关闭为 `notCollected` 或 `diagnostic_retained`。
5. 通过验收后重命名本文件为 `【已实现】`。

## 证据通过条件

- FreeCAD native evidence 能区分 `BindCopyOnChange=Disabled/Enabled/Mutated` 和 `PartialLoad`。
- copied-object state 可以被序列化为请求内 DTO 或前端 graph writeback，不依赖后端持久 session。
- evidence 能解释 mutation 后的 source/copy 分流，而不是只读 Python-visible enum。

## S3 执行结果

- 起点基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ed483b6c34`（`ed483b6c34 docs: 完成 C10-M4 S2 范围准入矩阵`），`git -c core.quotepath=false status --short -uall` 无输出。
- 已复核既有 `probe_c9m5_subshapebinder_copyonchange.py`、`probe_c8m2_subshapebinder_copyonchange.py`、`collect_c8m1_shapebinder_expected.py`、`cad-core/fixtures/c8m1` 和 `cad-core/fixtures/c8m2`；本步骤未修改 probe / collector / expected，也未改 cad-core C++。
- FreeCADCmd 未重跑：既有 expected 已记录 FreeCAD `1.2.0 revision 20260519`，且直接回答 S3 判定问题；未产生需要重新采集的新 probe/expected。
- copied object identity：未通过。C8-M2 expected 只显示 `_CopiedLink` 可在 recompute 后指向 `SupportBox001`，但 `_CopiedObjs` 不可访问，无法导出完整 copied-object graph 或稳定 identity payload。
- source mutation trigger：未通过 request-local DTO 标准。probe 能通过 Python 可见动态 property 写入把 `BindCopyOnChange` 从 `Enabled` 推到 `Mutated`，但仍未导出 source/copy 分流、`copyObject` dependency mapping 或 `recomputeFeature(true)` ElementMap lifecycle。
- front-end graph writeback target：未通过。现有 native evidence 没有声明 SubShapeBinder copied-object DTO 或前端 graph writeback target；App::Link `documentObjectUpdates` 只是 S4 参考路径，不能替代 SubShapeBinder native evidence。
- no backend session / persistent temp doc dependency：未通过。C8-M2 expected 只暴露 `_tmp_binder` 文档名和 hidden link 局部状态，仍依赖 FreeCAD session-local temporary document/cache，不是 cad-core 无状态请求 payload。
- 结论：`C10M4-SCOPE-101` 关闭为 `diagnostic_retained`，`C10M4-SCOPE-102` 关闭为 `notCollected`，`C10M4-BLOCKER-301` 关闭为 `closed_s3_notCollected`。S4/S6 不能基于当前证据进入实现；`backend_gap_candidate` 仍只能由 future/reopened S3 native expected + S4 DTO approval + S5 stateless clearance 共同产生。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "CopyOnChange|BindCopyOnChange|PartialLoad|temporary document|copied-object|request-local|not_collected|blocker" cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py cad-core/tools/collect_c8m1_shapebinder_expected.py cad-core/fixtures/c8m1 cad-core/fixtures/c8m2
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```

如 S3 修改 collector、probe 或 expected，应额外运行对应 probe / fixture validation，并在本文件写明最终命令与结论。

## 最终验收结果

- S3 `rg`：已运行，退出码 0，命中 C8/C9 probe、collector 和 expected 中的 property-state / known-gap / blocker evidence。
- queue：已运行，S3 被跳过，下一项为 S4。
- TSV 列数：已运行，退出码 0。
- trailing whitespace：已运行，退出码 1，表示无匹配。
- `git diff --check`：已运行，退出码 0。
