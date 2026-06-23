# 【已实现】C6-M2 S3 SchemaDrift 收口

## 目标

优先处理 S2 判定为 schema / contract drift 的 mismatch，包括 `diagnostic_codes`、`external_geometry_count`、link 类型和 assembly `solver_adapter` 字段。S3 按合同修 schema 或 expected，不通过放宽测试隐藏字段漂移。

## 本轮基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`ab642dffcd`
- `git log -1 --oneline`：`ab642dffcd docs: 完成 C6-M2 S2 authority 复核`
- `git -c core.quotepath=false status --short -uall`：无输出，S3 开始时工作区干净。

## 当前输出对比

S3 修改前用 `CAD_CORE_TEST_LEGACY_OUTPUT=1 cad-core/build/cad-core recompute` 对 11 条 S3 row 逐项生成当前输出。关键差异：

| ORC | 修改前差异 | S3 决策 |
| --- | --- | --- |
| `ORC-002` | `document_hash_mismatch` warning diagnostic | expected 窄刷新 diagnostic code。 |
| `ORC-004` | `deleted_stable_subname`，`ProbePad.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-005` | `ProbeSketch.external_geometry_count=4`，expected 为 `1` | 实现修复，按 FreeCAD `ExternalGeo.getSize()` 合同报告 ReferenceShadow-backed stable retarget 为 1 个 external entry。 |
| `ORC-007` | `Body` 报 `execution_failed`，expected 为成功 solid | PartDesign Body/Pocket 语义较宽，S3 不 hack，转 S5。 |
| `ORC-008` | `subname_semantic_drift`，`Pad.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-009` | `deleted_stable_subname`，`ProbeSketch.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-010` | `deleted_stable_subname`，`ProbeSketch.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-011` | `deleted_stable_subname`，`ProbePad.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-012` | `ArrayLink.link=app_link_group`，owner-list sync group bbox 为 `[0,0,0]..[5,3,4]` | expected 窄刷新 App::Link group metadata；该 bbox 是 element-count owner-list-sync 合同结果，不归入 S4 OCCT bbox 四行。 |
| `ORC-014` | `deleted_stable_subname`，`ProbePad.status=error` | expected 窄刷新 diagnostic/error metadata。 |
| `ORC-015` | `solver_adapter.placement_updates` 缺字段 | 实现修复，grounded-only noop 输出 `solver_joints=[]` 与 `placement_updates=[]`。 |

## 变更文件

- `cad-core/src/sketcher/sketch_object_external.h`
- `cad-core/src/sketcher/sketch_object_external.cpp`
- `cad-core/src/sketcher/sketch_object.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c3m2/expected/xlink-document-hash-mismatch.freecad.json`
- `cad-core/fixtures/c4m4/expected/topo-reference-pressure-shapefix-refine-deleted.freecad.json`
- `cad-core/fixtures/p5/expected/pad-internal-face-reference-shadow-drift.freecad.json`
- `cad-core/fixtures/p6/expected/sketch-external-edge-stable-body-deleted-after-add.freecad.json`
- `cad-core/fixtures/p6/expected/sketch-external-edge-stable-body-deleted.freecad.json`
- `cad-core/fixtures/p6/expected/up-to-face-stable-body-deleted.freecad.json`
- `cad-core/fixtures/p8/expected/app-link-element-count-owner-list-sync.freecad.json`
- `cad-core/fixtures/p8/expected/app-link-stable-history-deleted.freecad.json`
- `docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`
- `docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/c6m2_expected_fixture_regression_blocker_queue.tsv`
- `docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/c6m2_expected_fixture_regression_backend_gap_classification.tsv`
- `docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/c6m2_expected_fixture_regression_scope_review_matrix.tsv`
- 本步骤文档与工作步骤入口。

## FreeCAD / 本地依据

- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.h::SketchObject::getExternalGeometryCount()` 返回 `ExternalGeo.getSize()`。
- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::execute()` 先 `rebuildExternalGeometry()`，再把 external geometry 交给 solver-facing geometry。
- cad-core：`cad-core/tests/test_p6_topology.py::test_c4m4_topo_reference_pressure_updated_rows_publish_reference_updates` 已批准 `topo-reference-pressure-shapefix-refine-updated` 的 `Sketch.Face1 -> Face5` element reference update。
- cad-core：`cad-core/tests/test_p8_features.py::test_p8_app_link_element_count_reports_owner_list_sync` 已批准 ORC-012 的 App::Link group owner-list sync 语义。
- cad-core：`cad-core/tests/test_p8_features.py::test_p8_assembly_grounded_only_solver_adapter_succeeds_noop` 已批准 grounded-only assembly noop；S3 补齐 schema 字段断言。

## 结果

- expected refresh：8 条，`ORC-002/004/008/009/010/011/012/014`。
- implementation fixes：2 条，`ORC-005` 和 `ORC-015`。
- routed to S5：1 条，`ORC-007`。删除条件：S5 以 PartDesign Body/Pocket subtractive-without-base 通用语义修复，或写明无法在本阶段实现的 owner 与验收条件；不得刷新成功 oracle 为 error。
- S4 保留：4 条，`ORC-001/003/006/013` bbox / OCCT 差异。

## 非目标

- 未处理 `ORC-001/003/006/013` bbox / OCCT 差异。
- 未采集 native FreeCAD expected。
- 未运行全量 FreeCAD build。
- 未删除 schema 字段或断言。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
```

结果：通过。

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

结果：失败数从 15 收敛为 5，剩余为：

- `ORC-001`：`c3m1/element-map-child-map-recursive-compound` bbox，S4。
- `ORC-003`：`c4m4/topo-reference-pressure-import-unchanged` bbox，S4。
- `ORC-006`：`c5m1/partdesign-revolution-profile-linked-face` bbox，S4。
- `ORC-007`：`p2/pocket-without-base` Body/Pocket `execution_failed`，S5。
- `ORC-013`：`p8/app-link-imported-element-map-chain` bbox，S4。

最终收口命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```
