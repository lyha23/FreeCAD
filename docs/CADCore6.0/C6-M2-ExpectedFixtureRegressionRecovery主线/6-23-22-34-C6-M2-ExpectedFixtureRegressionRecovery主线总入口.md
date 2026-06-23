# C6-M2 Expected Fixture Regression Recovery 主线总入口

## 主线目标

C6-M2 的目标是恢复 CADCore6.0 阶段回归闸门可信度：从 C6-M1 S6 记录的 `tests.test_expected_fixtures` 失败出发，逐项确认 mismatch owner，按证据决定刷新 expected、修复实现、登记环境 / OCCT known gap 或保留 blocker。C6-M2 不是新产品能力包，也不是 Pipe Interpolation 的几何合同实现。

## 当前基线

- live repo：`/Users/li/Chili3DProject/FreeCAD`
- S6 开始 HEAD：`76acc921aa`
- S6 开始 last commit：`76acc921aa 完成 C6-M2 S5 Pocket 无 base 修复`
- C6-M1 队列：空队列，S0-S6 均已 `【已实现】`。
- C6-M1 focused：`python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`，144 tests OK。
- C6-M1 阶段回归：`python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`，178 tests，15 failures，28 skipped。
- C6-M2 关闭状态：S0 冻结的 15 条 mismatch 已全部收口，expected fixture gate 和阶段回归恢复通过。
- C6-M2 focused：`python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results`，`Ran 1 test in 49.586s`，`OK (skipped=29)`。
- C6-M2 阶段回归：`python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`，`Ran 180 tests in 80.086s`，`OK (skipped=29)`。
- C6-M2 重型收口：`python3 -m unittest tests.test_p6_topology tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`，`Ran 216 tests in 88.451s`，`OK (skipped=29)`。
- 剩余 known gap：`ORC-013` 是本地 OCCT 7.9.3 imported LinkGroup bbox 环境差异，通过 fixture-local `bbox_delta=0.028` 容纳；不声明 full FreeCAD parity。

## 证明链条

```text
live failure freeze
  -> fixture / owner 分类矩阵
  -> expected authority 复核
  -> schema drift 收口
  -> geometry / bbox / OCCT 收口
  -> approved expected refresh or C++ fix
  -> stage regression gate
```

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-23-22-35-【已实现】C6-M2工作步骤总入口.md` | S0-S6 执行索引。 |
| S0 | `工作步骤细分/6-23-22-36-【已实现】C6-M2-S0-live回归基线冻结.md` | 已在 `37fc024364` 复跑并冻结当前 15 条 expected fixture failure，不改代码。 |
| S1 | `工作步骤细分/6-23-22-37-【已实现】C6-M2-S1-失败清单与owner分类矩阵.md` | 已为 15 条 fixture mismatch 绑定 owner、落点和处理动作。 |
| S2 | `工作步骤细分/6-23-22-38-【已实现】C6-M2-S2-ExpectedAuthority复核.md` | 已完成 ExpectedAuthority 复核：8 条 expected refresh、3 条 implementation fix、4 条 bbox leave_blocked 进入 S4。 |
| S3 | `工作步骤细分/6-23-22-39-【已实现】C6-M2-S3-SchemaDrift收口.md` | 已收口 schema 类差异：8 条 expected refresh，2 条 implementation fix，ORC-007 转 S5。 |
| S4 | `工作步骤细分/6-23-22-40-【已实现】C6-M2-S4-GeometryTolerance与OCCT差异收口.md` | 已收口 bbox rows：ORC-001/003/006 为 object bbox implementation fix，ORC-013 为局部 OCCT known environment gap。 |
| S5 | `工作步骤细分/6-23-22-41-【已实现】C6-M2-S5-ApprovedExpectedOrCodeFix实施.md` | 已收口 ORC-007 Body/Pocket first subtractive without base implementation fix；expected fixture gate 通过。 |
| S6 | `工作步骤细分/6-23-22-42-【已实现】C6-M2-S6-阶段回归发布闸门.md` | 阶段回归、heavy 收口、docs / matrix 状态发布；C6-M2 关闭。 |
| source candidates | `矩阵/c6m2_expected_fixture_regression_source_candidates.tsv` | 失败来源和代码 / fixture authority 候选。 |
| scope review | `矩阵/c6m2_expected_fixture_regression_scope_review_matrix.tsv` | mismatch 范围、owner、状态词典。 |
| blocker queue | `矩阵/c6m2_expected_fixture_regression_blocker_queue.tsv` | C6-M2 待关闭 blocker。 |
| fixture oracle | `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv` | 每类 fixture 的 authority、命令和处理策略。 |
| non-goal registry | `矩阵/c6m2_expected_fixture_regression_non_goal_registry.tsv` | 禁止路径与 reopen condition。 |
| backend gap classification | `矩阵/c6m2_expected_fixture_regression_backend_gap_classification.tsv` | schema / geometry / expected / environment 分类。 |
| validation matrix | `矩阵/c6m2_expected_fixture_regression_validation_matrix.tsv` | 短跑、focused、阶段和重型验收。 |

## 状态词典

| 状态 | 含义 |
| --- | --- |
| `baselineFrozen` | S0 已记录 live failure list，后续只能消费该清单或记录新增差异来源。 |
| `ownerClassified` | S1 已把每个 mismatch 分到 expected、schema、geometry、environment 或 implementation owner。 |
| `authorityRequired` | S2 必须复核 checked-in expected、collector、代码行为和环境基线后才能修改。 |
| `schemaDrift` | 输出字段或 DTO contract 漂移，优先修 schema / tests，不直接刷新 geometry expected。 |
| `geometryOrOcctDrift` | bbox / topology / OCCT 兼容性差异，必须区分环境差异和实现回归。 |
| `approvedRefresh` | expected 更新已通过 authority 复核，允许最小批次刷新。 |
| `implementationFix` | 行为错误，应修 C++ / parser / adapter / test，而不是刷新 expected。 |
| `knownGap` | 本包无法关闭但 owner、删除条件和验证命令已明确。 |
| `released` | S6 阶段回归发布闸门通过，包线可关闭。 |

## 非目标

- 不采集 native FreeCAD expected，除非 S0/S2 证明 checked-in expected 无法裁决，并记录 FreeCAD / LibPack / OCCT 环境。
- 不实现 Pipe `Transformation=Interpolation` / `LawSamples`。
- 不重开 C5 / C51 broad deferred 或 C6-M1 已关闭 blocker。
- 不通过放宽断言、删除字段或 blanket refresh 隐藏 mismatch。
- 不修改上游 FreeCAD `src/`。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```

Focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

重型收口条件：若修改 topo/history、ShapeFix、assembly solver adapter 或 shared recompute schema，补跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p6_topology tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
