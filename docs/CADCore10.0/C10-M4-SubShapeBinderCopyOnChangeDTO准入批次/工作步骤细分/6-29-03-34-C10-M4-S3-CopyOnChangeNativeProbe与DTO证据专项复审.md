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

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "CopyOnChange|BindCopyOnChange|PartialLoad|temporary document|copied-object|request-local|not_collected|blocker" cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py cad-core/tools/collect_c8m1_shapebinder_expected.py cad-core/fixtures/c8m1 cad-core/fixtures/c8m2
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```

如 S3 修改 collector、probe 或 expected，应额外运行对应 probe / fixture validation，并在本文件写明最终命令与结论。
