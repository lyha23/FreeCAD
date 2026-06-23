# 【已实现】C6-M2 S2 ExpectedAuthority 复核

## 目标

对 S1 分类后的 15 条 `C6M2-ORC-*` row 做 authority 复核，明确最终动作：`refresh_expected`、`fix_implementation`、`known_environment_gap` 或 `leave_blocked`。S2 只做决策和证据记录，不刷新 expected、不修改 C++/adapter/tests/fixtures。

## 本轮基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`6d35699475`
- `git log -1 --oneline`：`6d35699475 docs: 完成 C6-M2 S1 owner 分类矩阵`
- `git -c core.quotepath=false status --short -uall`：无输出，S2 开始时工作区干净。

## 证据命令

本轮用 focused recompute 摘要读取 15 个 fixture 的 checked-in expected 与 current output：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 - <<'PY'
# 读取 fixture_oracle_matrix.tsv 中的 C6M2-ORC-001..015，
# 对每个 fixture 运行 CAD_CORE_TEST_LEGACY_OUTPUT=1 cad-core/build/cad-core recompute，
# 输出 failing field、diagnostic_codes、bbox/link/external_geometry_count/solver_adapter 摘要。
PY
```

另用 targeted source/test reads 复核：

- `cad-core/tests/test_expected_fixtures.py` 与 `cad-core/tests/fixture_expected.py`：expected fixture assertion 入口。
- `cad-core/tests/test_p5_sketch.py`、`cad-core/tests/test_p6_topology.py`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`：已批准行为证据。
- `cad-core/src/part/shape_exporter.cpp`、`cad-core/src/sketcher/sketch_object.cpp`、`cad-core/src/part_design/body.cpp`、`cad-core/src/assembly/assembly_utils.cpp`：bbox、external geometry count、Body subtractive、solver adapter 字段落点。

## Decision 分布

| decision | 数量 | ORC |
| --- | ---: | --- |
| `refresh_expected` | 8 | `002`, `004`, `008`, `009`, `010`, `011`, `012`, `014` |
| `fix_implementation` | 3 | `005`, `007`, `015` |
| `known_environment_gap` | 0 | 无 |
| `leave_blocked` | 4 | `001`, `003`, `006`, `013` |

## 逐项结论

| ORC | decision | 证据摘要 | 落点 | 禁止范围 / 下一步 |
| --- | --- | --- | --- | --- |
| `C6M2-ORC-001` | `leave_blocked` | `c3m1/element-map-child-map-recursive-compound` current 在 OCCT 7.9.3 下 bbox 为 `[-0.1,-0.1,-0.1]` 到 `[45.1,5.1,0.1]`，checked-in FreeCAD 1.2.0 expected 为 `[0,0,0]` 到 `[45,5,0]`。 | `geometry` | 不刷新 expected；S4 需用同基线 OCCT 或 precise bbox 证据判定实现修复 / environment gap。 |
| `C6M2-ORC-002` | `refresh_expected` | current `document_hash_mismatch` 被 `test_c3m2_xlink_document_hash_mismatch_reports_doc_reference_update` 明确断言为 warning diagnostic。 | expected diagnostic metadata | S3 做最小 expected refresh；不改 XLink parser。 |
| `C6M2-ORC-003` | `leave_blocked` | imported STEP bbox current 与 FreeCAD 1.2.0 expected 不一致；P6 专项测试只证明 update channel quiet，不证明 bbox authority。 | `geometry` / data-exchange | 不刷新 expected；S4 区分 OCCT 环境漂移与 bbox export 实现问题。 |
| `C6M2-ORC-004` | `refresh_expected` | current `deleted_stable_subname` / `ProbePad` error 被 P6 c4m4 diagnostic test 明确断言。 | expected diagnostic + error object metadata | S3 最小 refresh；不改 shapefix/refine 实现。 |
| `C6M2-ORC-005` | `fix_implementation` | current `ProbeSketch.external_geometry_count=4`，checked-in FreeCAD expected 为 `1`；S2 未发现 focused test 批准 count=4。 | `features`：sketch external geometry recovery/count | S3 修合同；不得把 expected 直接刷新成 4。 |
| `C6M2-ORC-006` | `leave_blocked` | Revolution linked-face current bbox 比 FreeCAD expected 宽；P7 test 只断言 status/topology/volume positive。 | `geometry` / `features` | S4 判定 bbox policy、revolution geometry 或 environment gap；不做 fixture 特判。 |
| `C6M2-ORC-007` | `fix_implementation` | `Pocket` 自身生成 solid，但 `Body` 报 `Body cannot apply subtractive feature Pocket without a base solid`；checked-in FreeCAD expected 是 `Body` solid bbox/volume。 | `features`：PartDesign Body/Pocket subtractive-without-base | S3 记录 schema impact，S5 修 Body/Pocket；不得把成功 oracle 刷成 error。 |
| `C6M2-ORC-008` | `refresh_expected` | current `subname_semantic_drift` / `Pad` error 被 `test_p5_pad_rejects_reference_shadow_semantic_drift` 批准。 | expected diagnostic + error object metadata | S3 最小 refresh；不改 ReferenceShadow 语义。 |
| `C6M2-ORC-009` | `refresh_expected` | current `deleted_stable_subname` / `ProbeSketch` error 被 `test_p6_stable_subname_history_diagnostics` 批准。 | expected diagnostic + error object metadata | S3 最小 refresh；只限该 fixture。 |
| `C6M2-ORC-010` | `refresh_expected` | current `deleted_stable_subname` / `ProbeSketch` error 被 `test_p6_stable_subname_history_diagnostics` 批准。 | expected diagnostic + error object metadata | S3 最小 refresh；只限该 fixture。 |
| `C6M2-ORC-011` | `refresh_expected` | current `deleted_stable_subname` / `ProbePad` error 被 `test_p6_stable_subname_history_diagnostics` 批准。 | expected diagnostic + error object metadata | S3 最小 refresh；只限该 fixture。 |
| `C6M2-ORC-012` | `refresh_expected` | current `object_fields.link=app_link_group` 与 P8 App::Link group tests 一致。 | expected App::Link group metadata | S3 最小 refresh；不改 App::Link group runtime。 |
| `C6M2-ORC-013` | `leave_blocked` | imported link group bbox current 与 FreeCAD 1.2.0 expected 不一致；P8 test 只证明 element map chain 和 link group 语义。 | `geometry` / App::Link bbox aggregation | S4 判定 aggregation bbox fix 或 OCCT environment gap。 |
| `C6M2-ORC-014` | `refresh_expected` | current `deleted_stable_subname` / `ProbePad` error 被 `test_p8_app_link_preserves_terminal_stable_history` 批准。 | expected diagnostic + error object metadata | S3 最小 refresh；只限该 fixture。 |
| `C6M2-ORC-015` | `fix_implementation` | expected 要求 `solver_adapter.placement_updates=[]`，current 缺字段；adapter tests/capability 不再登记 `assembly_solver_placement_updates` known gap。 | `adapters`：assembly solver adapter schema writer | S3 修 grounded-only noop 输出 `placement_updates=[]`；不删 expected 字段。 |

## 更新产物

- `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`：新增 `evidence_command` 与 `code_or_expected_landing`，15 条 ORC 全部完成 authority decision。
- `矩阵/c6m2_expected_fixture_regression_blocker_queue.tsv`：S2 authority 完成，schema 行路由 S3，bbox 行路由 S4，S5 只消费已批准 refresh/fix。
- `矩阵/c6m2_expected_fixture_regression_scope_review_matrix.tsv`：`SCOPE-101/201/301/401` 同步为 S2 后状态。
- `矩阵/c6m2_expected_fixture_regression_backend_gap_classification.tsv`：分类写入 `refresh_expected`、`fix_implementation`、`leave_blocked` 分布和 close condition。
- `工作步骤细分/6-23-22-35-【已实现】C6-M2工作步骤总入口.md` 与主线总入口：同步 S2 已实现路径。

## 非目标保持

- 未覆盖 expected JSON。
- 未修改 `cad-core` C++、adapter、tests 或 fixtures。
- 未新增 fixture。
- 未采集 FreeCADCmd/native expected。
- 未把 bbox 差异伪装成 expected stale。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'refresh_expected|fix_implementation|known_environment_gap|leave_blocked' docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵
for f in docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```
