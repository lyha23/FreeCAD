# C8-M2-S4 下游 opencascade-rs 同步契约方案

## 目标

把 C8-M1 / C8-M2 的 FreeCAD 侧能力、fixtures、diagnostics、known_gap 和 protocol vocabulary 转成下游 `opencascade-rs` 可执行的同步合同。S4 在本仓库只写方案和矩阵，不修改 Rust。

## 同步范围

| 同步项 | FreeCAD 源头 | 下游预期 |
| --- | --- | --- |
| TypeIds | `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython` | registry / parser 支持 |
| fixtures | `cad-core/fixtures/c8m1` | blackbox / parity 种子 |
| capability | `part_design.shape_binder`、`part_design.sub_shape_binder` | `/cad/capabilities` 同步 |
| diagnostics | `copy_on_change_full_temporary_document_cache_not_supported` 等 | 错误码 vocabulary |
| known_gap | `copy_on_change_full_temporary_document_cache` | 不发布 supported |
| ElementMap | ShapeBinder / SubShapeBinder output history | front-end picking / reference updates |

## 必须回写的矩阵行

- `C8M2-SYNC-101..103`
- `C8M2-BLOCKER-401`
- `C8M2-NG-005`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-SYNC|opencascade-rs|/cad/capabilities|SubShapeBinderPython|copy_on_change_full_temporary_document_cache|ElementMap' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-26-22-25-【已实现】C8-M2-S4-下游opencascade-rs同步契约方案.md`。

## 非目标

- 不在 FreeCAD repo 修改 `opencascade-rs`。
- 不把下游同步状态写成 FreeCAD `cad-core` supported。
- 不扩大到 GUI、Worker、WASM 或前端持久状态。
