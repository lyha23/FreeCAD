# FreeCAD 原生 fixture authority 报告

本目录保存 `collect_freecad_expected.py`、`validate_freecad_expected_ledger.py`、
`audit_freecad_fixture_authority.py`、`promote_freecad_fixture_authority.py` 和
`collect_non_cad_smoke.py` 生成的可复查报告。当前受控 producer 入口固定为：

```text
/Users/li/.cargo/bin/FreeCADCmd
```

该入口解析到：

```text
/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
```

当前报告记录 FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`，二进制
SHA-256 为
`391b638fa65bd761d55291be0a8e7ac22bd4d5ba40ccbd9b14621209a402181a`。
producer 源工作区的 build directory、CMake 信息和 dirty 状态属于可选
provenance；缺失时只产生 warning，不改变 public expected 或 ledger 门禁结论。

## 当前 inventory

`fixture_authority_inventory.v1.json` 由 fixture、role manifest、authority 产物和
探针/晋升回执重建，不是手工统计。

| 项目 | 数量 |
| --- | ---: |
| input | 777 |
| native | 558 |
| protocol_only | 14 |
| unsupported | 205 |
| public expected | 558 |
| ledger | 558 |
| 历史 producer trace | 480 |

205 个 unsupported case 均已证据化分类：

| 分类 | 数量 |
| --- | ---: |
| `freecad_native_not_expressible` | 120 |
| `non_native_fixture` | 85 |
| `collector_general_gap` | 0 |
| `not_investigated` | 0 |

历史 producer trace 不是 native authority 的组成部分；新增 authority 不要求发行版
FreeCADCmd 生成 trace。native authority 的硬条件仍是 input、同名 public expected、
同名 ledger、native role 和 producer/promotion 回执闭合。

## 保留模块覆盖

`retained_module_fixture_coverage.v1.json` 从 FreeCAD2 剪枝后的固定保留闭包、
fixture contract 和 authority inventory 重建。当前结果为：

- `globalFixtureCount = 765`；
- `nativeBaseline = 480`，`nativeAfter = 558`，净扩容 78；
- `coverageStatus = passed`；
- `producerValidation.status = passed`，且 manifest 与当前 558 个 native authority 的 input/public/ledger SHA 完全一致；
- `retainedModuleCollectorImplementationQueueCount = 0`；
- `notInvestigatedCount = 0`；
- `nativeEligibleWithoutAuthorityCount = 0`。

FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy、Material、Part、Sketcher、
PartDesign、Mesh、Spreadsheet、Assembly 和 OndselSolver 均为 `passed`。Material 与
Spreadsheet 各有一个真实 native fixture；Help 和 AddonManager 没有 CAD public
root，以受控 FreeCADCmd 执行的 `non_cad_smoke` 回执收口。

## 已签入 Gate 报告

| 报告 | 结论 |
| --- | --- |
| `fixture_authority_inventory.v1.json` | 777 个输入；558/14/205 role 闭合；collector/general 和未调查队列清零 |
| `retained_module_fixture_coverage.v1.json` | 固定保留闭包全部闭合；CAD 模块通过，Help/AddonManager 为 `non_cad_smoke` |
| `all-native-check.v1.json` | 两个独立 FreeCADCmd 进程每轮 558/558，0 failure；跨轮 public/ledger 差异和 variation 均为 0 |
| `ledger-strict-validation.v1.json` | strict 模式 558/558 合法，0 error |
| `probes/*.json` | collector gap、原生不可表达边界和 promotion 候选的实际采集证据 |
| `promotions/*.json` | staging 首采、staging repeat 2、事务化 promotion 和 promotion 后 repeat 2 回执 |
| `revocations/*.json` | repeat 中发现不稳定 authority 后的事务化撤销证据 |
| `non_cad_smoke/*.json` | Help、AddonManager 的真实 import/data smoke 证据 |

最终 all-native 报告的状态必须分开读取：

- `status = passed`
- `publicExpectedStatus = passed`
- `ledgerValidationStatus = passed`
- `ledgerDriftStatus = drifted`
- `producerTraceStatus = not_evaluated`

这里的 `ledgerDriftStatus = drifted` 表示新采候选 ledger 相对部分历史签入 ledger 的
collector/tool hash 或内部证据发生变化，是诊断项；两个最终独立 run 之间 ledger
drift 为 0，558 个候选 ledger 均通过 strict validation，public expected 语义差异也为
0，因此不阻断 authority Gate。producer trace 默认不生成、不比较，只在
expected/ledger 分叉调查时使用。

## 重建与使用

以下命令均从 `cad-core/` 执行。

### 单 case / phase / all-native 复现

```bash
python3 tools/collect_freecad_expected.py \
  fixtures/p8/part-box.json \
  --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/part-box-repeat2 \
  --report /tmp/part-box-repeat2.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd

python3 tools/collect_freecad_expected.py \
  --phase c4m6 --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/c4m6-repeat2 \
  --report /tmp/c4m6-repeat2.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd

python3 tools/collect_freecad_expected.py \
  --all-native --fixtures-root fixtures \
  --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/freecad-expected-all-native \
  --report tools/freecad_expected_parity/reports/all-native-check.v1.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd
```

每次都要读取报告中的 `discovered`/`processed` 或子 run 报告，确认选择非零且全部
实际执行。explicit zero-case、缺 expected、缺 ledger、collection failure，以及
repeat 子报告缺失或陈旧都会 fail closed。

### strict ledger、non-CAD smoke 与最终 audit

```bash
python3 tools/validate_freecad_expected_ledger.py \
  --all --strict \
  --report tools/freecad_expected_parity/reports/ledger-strict-validation.v1.json

python3 tools/collect_non_cad_smoke.py \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd \
  --out-root tools/freecad_expected_parity/reports/non_cad_smoke

python3 tools/audit_freecad_fixture_authority.py \
  --roles tools/freecad_expected_parity/fixture_roles.v1.json \
  --report tools/freecad_expected_parity/reports/fixture_authority_inventory.v1.json \
  --coverage-report tools/freecad_expected_parity/reports/retained_module_fixture_coverage.v1.json \
  --producer-report tools/freecad_expected_parity/reports/all-native-check.v1.json \
  --non-cad-smoke-root tools/freecad_expected_parity/reports/non_cad_smoke \
  --require-coverage-passed
```

新 case 必须走 staging 首采、staging repeat 2、
`promote_freecad_fixture_authority.py` 事务化 promotion、promotion 后 checked-in
repeat 2。禁止手改 `*.freecad.json` 或 `*.freecad.ledger.json`，也禁止按
phase/case 名称向 collector 添加特判。若最终重复采集发现 authority 不稳定，使用同一
工具的 revocation 路径原子撤销 expected、ledger、native role 和 promotion 回执，保留
输入及诊断证据。

## 方案闭合状态

- E0：777 条 inventory、role、authority 产物和异常检查已闭合。
- E1：single/phase/all-native check、独立 repeat 2、分栏 verdict 和 fail-closed 已闭合。
- E2：205 条 unsupported 已逐项证据化分类，无未调查项。
- E3：通用 TypeId/property、Sketcher constraints、history-only ledger、Material、Spreadsheet、promotion/revocation 和 non-CAD smoke 能力已闭合，保留模块 collector 队列为 0。
- E4：native authority 从 480 扩容到 558；新增 case 均由 staging/promotion/post-repeat 回执闭合，发现不稳定的 case 已事务化撤销。
- E5：558 个 native 的全量 repeat 2、strict ledger、producer provenance、逐模块 coverage 和最终 audit 全部通过。

收尾 review 追加了两个 fail-closed 门禁：promotion 必须把 collect receipt 的
fixture/public/ledger SHA 与当前 staging 文件逐项绑定；coverage 必须验证 all-native
producer report 的身份、manifest、两个独立 run 及差异列表，不能只记录报告路径。
包含这两条回归在内，相关测试共 120 项全部通过。
