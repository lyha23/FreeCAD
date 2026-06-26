# 【已实现】C8-M3-S3 PartConicCurveDTO 生产消费 oracle 批量复核

## 目标

复核或扩展 `PartConicCurveDTO` producer / consumer 的 expected-backed 批量证据。S3 可以新增 oracle plan、fixtures、expected 或 probe；不得注册 fake Part conic DocumentObject。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=c144cf43dd`
- `git log -1 --oneline`：`c144cf43dd docs: 完成 C8-M3 S2 scope 准入与矩阵路由`
- S3 开始时 `git -c core.quotepath=false status --short -uall` 无输出，工作区干净。
- 队列首项是本 S3 文件；S3 只关闭 Part batch，不消费 S4/S5/S6。

## 批量范围

| 批次 | 对应 oracle | 目标 |
| --- | --- | --- |
| producer | `C8M3-ORACLE-101` | Hyperbola / Parabola edge、metadata、invalid diagnostics |
| consumer | `C8M3-ORACLE-102` | Extrusion / RuledSurface current coverage；裁决是否补同类代表 consumer |
| publication | S3 不关闭 | capability covered / remaining_gaps 仍由 S4-S6 裁决；`C8M3-ORACLE-103` 属于 S4 Sketcher |

## S3 裁决

S3 不新增 fixture、expected、probe、C++ 或 Python test。现有 p8 证据已经覆盖同一 Part API 边界，不需要 `split_required`。

| 行 | S3 结论 | 证据 |
| --- | --- | --- |
| `C8M3-ORACLE-101` | `already_supported` / expected-backed | `cad-core/fixtures/p8/part-hyperbola-edge.json`、`part-parabola-edge.json` 各自有 `cad-core/fixtures/p8/expected/*.freecad.json`；expected 记录 `FreeCADCmd PartConicCurveDTO oracle`、`PartConicCurveDTO`、`Part.Hyperbola` / `Part.Parabola`、`GeomAbs_Hyperbola` / `GeomAbs_Parabola`、bbox、length 和 topology counts。 |
| `C8M3-ORACLE-101` diagnostics | `already_supported` / focused-test-backed | `cad-core/fixtures/p8/part-conic-edge-invalid-params.json` 覆盖 invalid kind、radius、focal、trim、number；`cad-core/tests/test_diagnostics.py` 固定对应 diagnostic codes。 |
| `C8M3-ORACLE-102` Extrusion consumer | `already_supported` / expected-backed | `cad-core/fixtures/p8/part-conic-edge-extrusion.json` 同时把 Hyperbola 和 Parabola 作为 `Part::Extrusion` Base；expected 记录两个 producer 和两个 extrusion object 的 source metadata、topology counts、bbox 和 volume。 |
| `C8M3-ORACLE-102` RuledSurface consumer | `already_supported` / expected-backed | `cad-core/fixtures/p8/part-ruled-surface-conic-line.json` 先生成 Hyperbola edge，再用 `Part::RuledSurface` 消费 conic edge + line；expected 记录 `source_curve1_dto=PartConicCurveDTO`、`source_curve1_curve_kind=hyperbola`、`source_curve1_curve_type=GeomAbs_Hyperbola` 和 source edge provenance。 |
| `C8M3-BG-103` | no Part backend gap | `cad-core/src/part/part_geometry_curve.cpp` 先执行 `PartConicCurveDTO`，再通过 request-local bridge 调用正常 `Part::Extrusion` / `Part::RuledSurface` executor；代码注释明确不注册 fake `Part::Hyperbola` / `Part::Parabola` DocumentObject。 |

## 批量充分性

- producer 不是单 fixture：Hyperbola 与 Parabola 两个不同 conic DTO 都有 native expected，且 metadata 字段包含 DTO、Part geometry type、OCCT curve type、shape、length 与 topology。
- diagnostics 不是 output 倒推：invalid fixture 只约束 parse / validation diagnostics，不用 bbox、面积或 fixture 名猜业务逻辑。
- consumer 不是单 fixture：`Part::Extrusion` fixture 同时覆盖 Hyperbola / Parabola，`Part::RuledSurface` fixture覆盖 conic edge 到 surface executor 的另一条 Part consumer 入口。
- capability publication 仍等 S6：S3 只证明 Part producer / consumer 无实现缺口，不删除 `gui_conic_edit`、`full_sketcher_solver_conic_constraints` 或 `distance_type_default_todo`。
- `C8M3-ORACLE-103` 仍属 S4 Sketcher conic input / external-reference 边界，S3 不关闭 `C8M3-BLOCKER-401`。

## 预期产物

- 可选：新增 `cad-core/fixtures/c8m3/*.json` 或复用 `p8` fixtures。
- 可选：新增 expected，必须来自 FreeCAD native / source-backed oracle。
- 可选：更新 `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`。
- 若不新增 fixture：必须记录 existing batch 足以覆盖该 producer / consumer API 的理由。

本轮实际产物为 docs / TSV 回写；未修改 `cad-core/fixtures`、`cad-core/tests` 或 C++。

## 必须回写的矩阵行

- `c8m3_conic_requestlocal_oracle_plan.tsv`
- `c8m3_conic_requestlocal_scope_review_matrix.tsv`
- `c8m3_conic_requestlocal_blocker_queue.tsv`
- `c8m3_conic_requestlocal_backend_gap_classification.tsv`

## 矩阵回写

- `C8M3-ORACLE-101` 关闭为 existing p8 expected-backed producer / diagnostics batch。
- `C8M3-ORACLE-102` 关闭为 existing p8 expected-backed Extrusion + RuledSurface consumer batch。
- `C8M3-SCOPE-101` 保持 `already_supported`，证据更新为 S3 confirmed expected-backed producer。
- `C8M3-SCOPE-102` 从 `oracle_candidate` 更新为 `already_supported`，不需要同边界扩展或拆分。
- `C8M3-SCOPE-103` 保持 `already_supported`，metadata / diagnostics 已有证据；capability publication 仍归 S6。
- `C8M3-BG-103` 更新为 `already_supported`，无 S3 backend gap。
- `C8M3-BLOCKER-301` 关闭为 `closed_S3_part_conic_producer_consumer_expected_backed_batch_reviewed`。
- `C8M3-BLOCKER-401/501/601` 未关闭。

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

## 验收结果

已按本步骤运行以下验证；`rg -n '[ \t]$' ...` 无输出，表示无 trailing whitespace 命中。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PartConicCurveDTO|part-hyperbola-edge|part-parabola-edge|part-conic-edge-extrusion|part-ruled-surface-conic-line|C8M3-ORACLE|expected-backed|split_required' cad-core/fixtures cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

本轮未修改 `cad-core/fixtures`、`cad-core/tests` 或 C++，因此未运行 `python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_diagnostics`。

本文件已按原时间前缀重命名为 `6-27-01-04-【已实现】C8-M3-S3-PartConicCurveDTO生产消费oracle批量复核.md`。

## 非目标

- 不注册 `Part::Hyperbola` / `Part::Parabola` DocumentObject。
- 不把 fixture output 倒推业务逻辑。
- 不扩大到 full Part surface family。
