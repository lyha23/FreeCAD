# C6-M2 Expected Fixture Regression Recovery 方案

## 核心判断

C6-M1 的产品能力 focused 已通过，但阶段回归仍有 expected fixture mismatch。下一步不应该直接做 Pipe Interpolation 或新几何能力，而应先把 expected fixture 闸门恢复到可依赖状态。否则后续任何新能力都会混进旧 expected 漂移、schema 漂移和环境 / OCCT 差异，无法判断真实回归。

## 实施框架

1. S0 复跑阶段回归，只冻结当前失败清单和命令环境。
2. S1 把每个 failure 拆成 fixture 行，并按 owner 分类：`bbox_geometry_or_occt_drift`、`diagnostic_codes_contract_drift`、`external_geometry_count_contract_drift`、`link_schema_or_type_drift`、`assembly_solver_adapter_schema_drift`、`expected_stale_after_intended_behavior`、`implementation_regression`。
3. S2 复核 authority：checked-in expected、当前 recompute output、collector / expected 生成历史、FreeCAD / LibPack / OCCT 基线和当前代码语义。
4. S3 先收 schema 类差异，避免用 expected refresh 掩盖协议字段漂移。
5. S4 再收 bbox / geometry / OCCT 类差异，区分 tolerance、环境兼容和真正实现回归。
6. S5 执行最小批次的 approved expected refresh 或 C++ 修复。
7. S6 运行阶段回归并发布结果；若仍有失败，必须写成 owner 明确的 known gap，而不是散落在测试输出里。

## 决策规则

| mismatch owner | 默认处理 | 不允许的处理 |
| --- | --- | --- |
| `diagnostic_codes_contract_drift` | 先确认新 diagnostic 是否是已实现语义的公开合同；是则更新 expected 和 docs，否则修代码。 | 删除 diagnostic 断言。 |
| `external_geometry_count_contract_drift` | 复核 collector / response schema 与 feature executor 输出，再决定 expected 或 code。 | 只改 expected count 不解释 source。 |
| `link_schema_or_type_drift` | 优先查 document parser / PropertyLinkSub contract。 | 把 link type 宽松匹配成任意字符串。 |
| `assembly_solver_adapter_schema_drift` | 优先查 adapter schema 和 assembly fixture owner。 | 用 fixture 名称特判 adapter 字段。 |
| `bbox_geometry_or_occt_drift` | 区分环境 / OCCT 差异、tolerance 和实现回归；必要时登记 known gap。 | 放宽全局 bbox 断言。 |
| `expected_stale_after_intended_behavior` | 最小批次刷新 expected，并保留命令证据。 | blanket refresh。 |
| `implementation_regression` | 修 C++ / parser / adapter / tests 后再跑 focused。 | 用 expected refresh 接受错误行为。 |

## 文档和矩阵要求

- S0 必须保留原始失败列表摘要、命令、HEAD 和工作区状态。
- S1 必须在 `fixture_oracle_matrix` 里给每个 failure 一个 row，不允许只写类别总数。
- S2 必须记录 authority 结论：`refresh_expected`、`fix_implementation`、`known_environment_gap` 或 `leave_blocked`。
- S3/S4/S5 必须只消费 S1/S2 已批准的行，不新增无证据范围。
- S6 只有在阶段回归通过，或剩余 failure 都有 owner / 删除条件 / 验证命令时，才能把步骤改名为 `【已实现】`。

## 成功标准

- `tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 通过，或剩余失败被归入明确 known gap。
- `tests.test_p7_features tests.test_expected_fixtures tests.test_adapters` 通过，或剩余失败有明确 owner、删除条件和非阻塞理由。
- `docs/CADCore6.0/README.md` 反映 C6-M1 已实现、C6-M2 当前状态和后续 Pipe Interpolation 非目标。
- C6-M2 队列只剩真实未执行步骤。
