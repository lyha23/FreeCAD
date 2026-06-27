# C8-M7 S3 import 文件生命周期 oracle 复审

## 目标

确认 import 文件生命周期中哪些语义需要 FreeCAD oracle，哪些已由 source 和 current tests 覆盖，哪些只能保持 diagnostic / non-goal。

## 必须复核

- changed-file：如果同一路径或新路径在当前请求中可读，cad-core 是否已经每次从 `FileName` 重新导入并重建 shape / NamedShape。
- deleted-file：如果 `FileName` 不存在或不可读，FreeCAD 与 cad-core 是否都应报错，而不是从后端旧缓存恢复。
- stale reference：如果引用的是旧 subshape，能否只靠当前请求中的 stable subname、`ReferenceShadow` 和 current imported shape 做恢复。
- C7-M7 oracle-blocked rows 是否仍然阻止完整 persistent imported ElementMap / Link writeback 实现。

## 可选 oracle

只有 S2 把某行标为 `oracle_candidate` 时才采集 native evidence。采集前必须写明 FreeCADCmd / LibPack / OCCT 基线，并避免用当前 cad-core 输出刷新 expected。

## 必须回写

- `c8m7_import_shape_recovery_scope_review_matrix.tsv`
- `c8m7_import_shape_recovery_blocker_queue.tsv`
- `c8m7_import_shape_recovery_validation_matrix.tsv`
- README 的 S3 结论。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Cannot open file|isReadable|ReadFile|TransferRoots|OneShape|import_shape_element_map|FileName' src/Mod/Part/App cad-core/src/part cad-core/tests
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不采 GUI / ViewProvider 行为。
- 不引入跨请求导入缓存。
- 不把文件删除后的旧几何恢复写成 supported。
