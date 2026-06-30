# C12-M7 S2 product diagnostic contract 准入裁决

## 目标

决定 Groove UpTo current exact diagnostic 是否可以作为 CAD Core product diagnostic contract 发布。

## 必读来源

- S0 / S1 已实现文档
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers`
- `cad-core/tests/test_adapters.py` 的 `part_design.revolution_groove` capability assertion
- 本包 `c12m7_groove_upto_contract_matrix.tsv`

## 操作

1. 区分三种状态：FreeCAD parity success、historical native failure、CAD Core product diagnostic contract。
2. 若 native 仍失败，检查 current diagnostic 是否足够稳定、locatable、request-local，并能作为产品可见行为。
3. 若批准 product contract，写清 S3 需要改的 expected/test/capability/docs 文件面。
4. 若不批准，保留 `retained_historical_native_failure` 并写清重开条件。

## 裁决规则

- Native 仍失败时，不允许几何 C++ parity 实现。
- Product diagnostic contract 必须同时覆盖 UpToFirst 与 UpToFace。
- Capability wording 必须保留 native failure note，不能写成 FreeCAD parity。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
git diff --check
```
