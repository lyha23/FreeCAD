# C6-M2 S2 ExpectedAuthority 复核

## 目标

对 S1 分类后的每个 row 做 authority 复核，明确最终动作：`refresh_expected`、`fix_implementation`、`known_environment_gap` 或 `leave_blocked`。S2 可以读取代码和 fixture，但不执行批量刷新或大改实现。

## 必读输入

- S1 已实现文档
- `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`
- `矩阵/c6m2_expected_fixture_regression_blocker_queue.tsv`
- `cad-core/tests/test_expected_fixtures.py`
- 相关 fixture 的 checked-in expected JSON
- 相关 feature / adapter / document parser 源码

## 复核规则

- 如果 current output 来自 C6-M1 已批准行为，且 expected 没同步，标 `refresh_expected`。
- 如果 current output 缺字段、字段类型错误或 diagnostic 错误，标 `fix_implementation`。
- 如果差异只在非 expected 采集基线 OCCT 上出现，标 `known_environment_gap`，并记录环境。
- 如果缺少 oracle 或产品合同，标 `leave_blocked`，不要刷新 expected。

## 实施内容

1. 对每个 `C6M2-ORC-*` 行补 `authority_decision`、`evidence_command`、`code_or_expected_landing`。
2. 对需要 code fix 的行写出具体落点：`document`、`graph`、`runtime`、`features`、`geometry`、`topo` 或 `adapters`。
3. 对允许 expected refresh 的行写出最小批次和禁止刷新范围。
4. 更新 blocker queue 的 next_step 到 S3、S4、S5 或 known gap。
5. 完成后将本文件改名为 `6-23-22-38-【已实现】C6-M2-S2-ExpectedAuthority复核.md`。

## 非目标

- 不用当前失败输出直接覆盖 expected。
- 不把 implementation regression 标成 expected stale。
- 不采集 native FreeCAD expected，除非当前 checked-in expected 无法裁决且记录环境。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'refresh_expected|fix_implementation|known_environment_gap|leave_blocked' docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵
for f in docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
