# C8-M3-S3 PartConicCurveDTO 生产消费 oracle 批量复核

## 目标

复核或扩展 `PartConicCurveDTO` producer / consumer 的 expected-backed 批量证据。S3 可以新增 oracle plan、fixtures、expected 或 probe；不得注册 fake Part conic DocumentObject。

## 批量范围

| 批次 | 对应 oracle | 目标 |
| --- | --- | --- |
| producer | `C8M3-ORACLE-101` | Hyperbola / Parabola edge、metadata、invalid diagnostics |
| consumer | `C8M3-ORACLE-102` | Extrusion / RuledSurface current coverage；裁决是否补同类代表 consumer |
| publication | `C8M3-ORACLE-103` | existing fixtures 与 capability covered / remaining_gaps 是否一致 |

## 预期产物

- 可选：新增 `cad-core/fixtures/c8m3/*.json` 或复用 `p8` fixtures。
- 可选：新增 expected，必须来自 FreeCAD native / source-backed oracle。
- 可选：更新 `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`。
- 若不新增 fixture：必须记录 existing batch 足以覆盖该 producer / consumer API 的理由。

## 必须回写的矩阵行

- `c8m3_conic_requestlocal_oracle_plan.tsv`
- `c8m3_conic_requestlocal_scope_review_matrix.tsv`
- `c8m3_conic_requestlocal_blocker_queue.tsv`
- `c8m3_conic_requestlocal_backend_gap_classification.tsv`

## 验收标准

- `C8M3-ORACLE-101` 和 `102` 明确是 expected-backed、already-supported、backend-gap 还是 split-required。
- 若新增 fixtures，同一 API 边界至少覆盖 Hyperbola / Parabola 或 producer / consumer 双代表，不接受单 fixture 特判。
- 若不新增 fixtures，S3 必须证明现有 p8 fixtures 已覆盖该批次。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PartConicCurveDTO|part-hyperbola-edge|part-parabola-edge|part-conic-edge-extrusion|part-ruled-surface-conic-line|C8M3-ORACLE|expected-backed|split_required' cad-core/fixtures cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-04-【已实现】C8-M3-S3-PartConicCurveDTO生产消费oracle批量复核.md`。

## 非目标

- 不注册 `Part::Hyperbola` / `Part::Parabola` DocumentObject。
- 不把 fixture output 倒推业务逻辑。
- 不扩大到 full Part surface family。
