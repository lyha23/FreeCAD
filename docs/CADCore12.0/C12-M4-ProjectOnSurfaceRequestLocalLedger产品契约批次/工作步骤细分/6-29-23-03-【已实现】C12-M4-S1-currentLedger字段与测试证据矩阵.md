# C12-M4 S1 current ledger 字段与测试证据矩阵【已实现】

## 目标

把当前 `cad-core` ProjectOnSurface request-local ledger 的字段、生产点、消费点、focused tests 和 C5-M9 expected wording 全部落入 source / contract matrix。

## 必读文件

- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-edge-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-wire-split-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-face-rebuild-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-all-compound-provenance.freecad.json`
- `cad-core/fixtures/c5m9/expected/part-project-on-surface-invalid-provenance-diagnostics.freecad.json`
- C12-M4 `source_authority`、`contract_fields`、`expected_migration` matrices。

## 操作

1. 确认 `projection_item_ledger`、`mapper_history`、`reference_recovery_hook`、face/height/compound/diagnostic 字段的 current producer 和 focused tests。
2. 区分 FreeCAD source authority、CAD Core product contract source 和 C5-M9 expected wording。
3. 更新 source / contract / migration matrices，列出仍缺的字段或测试证据。

## 非目标

- 不改 C++。
- 不改 expected JSON。
- 不把 current implementation 反向说成 FreeCAD native oracle。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次 docs/CADCore12.0/README.md
git diff --check
```
