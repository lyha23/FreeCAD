# FreeCAD 原生 fixture authority 报告

本目录保存 `collect_freecad_expected.py`、`validate_freecad_expected_ledger.py`、
`audit_freecad_fixture_authority.py`、`promote_freecad_fixture_authority.py` 和
`collect_non_cad_smoke.py` 生成的可复查报告。当前受控 producer 入口固定为：

```text
/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
```

当前报告记录 FreeCAD `1.2.0 revision 47063`、OCCT `7.8.1`，二进制
SHA-256 为
`51a6fb775b1f2a4edff5cee1b90a4395674f988748f75d01deb0ce5add0ca674`。
producer 源工作区的 build directory、CMake 信息和 dirty 状态属于可选
provenance；缺失时只产生 warning，不改变 public expected 或 ledger 门禁结论。

## 当前 inventory

`fixture_authority_inventory.v1.json` 由 fixture、role manifest、authority 产物和
探针/晋升回执重建，不是手工统计。

| 项目 | 数量 |
| --- | ---: |
| input | 783 |
| native | 564 |
| protocol_only | 14 |
| unsupported | 205 |
| public expected | 564 |
| ledger | 564 |
| 历史 producer trace | 478 |

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

- `globalFixtureCount = 771`；
- `nativeBaseline = 480`，`nativeAfter = 564`，净扩容 84；
- `coverageStatus = passed`；
- `producerValidation.status = passed`，且 manifest 与当前 564 个 native authority 的 input/public/ledger SHA 完全一致；
- `retainedModuleCollectorImplementationQueueCount = 0`；
- `notInvestigatedCount = 0`；
- `nativeEligibleWithoutAuthorityCount = 0`。

FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy、Material、Part、Sketcher、
PartDesign、Mesh、Spreadsheet、Assembly 和 OndselSolver 均为 `passed`。Material 与
Mesh、Spreadsheet 现各有 3 个真实 native fixture，分别包含基础/正常结果、第二次
recompute 的属性修改，以及最小、闭合或空状态边界。Help 和 AddonManager 没有 CAD
public root，以受控 FreeCADCmd 执行的 `non_cad_smoke` 回执收口。

这个 `passed` 只表示 role、authority 产物、保留模块归属和 producer manifest 闭包，
不表示模块全部公开 API 已被 fixture 覆盖。

## 公开能力反向覆盖

`retained_public_capabilities.v1.json` 是从保留模块源码维护的公开能力/主要运行分支清单；
`retained_public_capability_coverage.v1.json` 再把这份独立清单反向映射到 fixture。当前
清单含 14 个模块、57 条代表性能力：

| 判定 | 数量 | 含义 |
| --- | ---: | --- |
| `covered` | 36 | 有 `target_result`、`dependency_result` 或 `native_diagnostic` 执行证据 |
| `thin` | 2 | 只有入口 smoke，尚未断言公开运行分支 |
| `uncovered` | 6 | 源码能力存在且可扩展 native 测试，但当前 corpus 完全没有表达 |
| `non_native_exception` | 13 | 已按 `protocol_only`、`unsupported` 或 `non_cad_smoke` 记录依据 |

三个结论必须分开读取：

- `fixtureCorpusClosure.status = passed`：783 条 input 的 role/authority corpus 闭合；
- `moduleApiCoverage.status = partial`：上述 57 条反向清单仍有 2 条 thin 和 6 条 uncovered，不能宣称全 API 覆盖；
- `cadCoreRuntimeParity.status = not_evaluated`：FreeCAD native authority 不证明 CAD Core 已逐条对齐。

已覆盖的关键执行分支包括 Part primitive/boolean/import/extrusion/offset/loft/sweep、
Sketcher profile/solver-facing update/external geometry/diagnostics、PartDesign Body
依赖链中的 Pad/Pocket/Revolution/Groove/Hole/DressUp/Pattern/Pipe/Boolean/Binder，以及
Assembly/OndselSolver 的真实求解成功、placement writeback 和失败诊断。它们不再以
TypeId 或属性点名代替执行证据。

当前尚未覆盖且仍可扩展 native 测试的代表性能力包括 MainCmd 错误/退出模式、Material
card/library/model resolution、Sketcher 全编辑操作矩阵、Mesh primitives/set
operations/defect analysis、Spreadsheet style/layout/merge，以及 OndselSolver 全
joint/退化组合穷举。

非 native 例外中，`Mesh::Transform::execute()` 的 FreeCAD 主体当前整段被注释，只返回
成功，因此明确记为 source-backed `unsupported`，不得因 TypeId 已注册而制造几何
fixture。App/PartDesign/Assembly 的 persisted restore、MainPy host embedding 以及直接
Part/Mesh/Spreadsheet 文件输出均明确归为 `protocol_only`，不再混入普通 uncovered。
Help 的 GUI/WebEngine/browser/network 分支，以及 AddonManager 的网络发现、安装、更新、
删除和用户目录写入，也不由 import/data smoke 冒充功能覆盖；对当前无 GUI、确定性
headless 保留范围，现有 smoke 足够，若未来保留范围扩大再单独建立功能测试。

## 已签入 Gate 报告

| 报告 | 结论 |
| --- | --- |
| `fixture_authority_inventory.v1.json` | 783 个输入；564/14/205 role 闭合；39 个模块-能力 phase；collector/general 和未调查队列清零 |
| `retained_module_fixture_coverage.v1.json` | 固定保留闭包全部闭合；CAD 模块通过，Help/AddonManager 为 `non_cad_smoke` |
| `retained_public_capability_coverage.v1.json` | corpus `passed`；模块 API `partial`；CAD Core parity `not_evaluated` |
| `all-native-check.v1.json` | 两个独立 FreeCADCmd 进程每轮 564/564，0 failure；跨轮 public/ledger 差异和 variation 均为 0 |
| `ledger-strict-validation.v1.json` | strict 模式 564/564 合法，0 error |
| `probes/*.json` | collector gap、原生不可表达边界和 promotion 候选的实际采集证据 |
| `promotions/*.json` | staging 首采、staging repeat 2、事务化 promotion 和 promotion 后 repeat 2 回执 |
| `revocations/*.json` | repeat 中发现不稳定 authority 后的事务化撤销证据 |
| `non_cad_smoke/*.json` | Help、AddonManager 的真实 import/data smoke 证据 |

最终 all-native 报告的状态必须分开读取：

- `status = passed`
- `publicExpectedStatus = passed`
- `ledgerValidationStatus = passed`
- `ledgerDriftStatus = unchanged`
- `producerTraceStatus = not_evaluated`

这里的 `ledgerDriftStatus = unchanged` 表示两个最终独立 run 之间 ledger drift 为 0；
564 个候选 ledger 均通过 strict validation，public expected 语义差异也为 0。
producer trace 默认不生成、不比较，只在 expected/ledger 分叉调查时使用。

## 重建与使用

以下命令均从 `cad-core/` 执行。

### 单 case / phase / all-native 复现

```bash
python3 tools/collect_freecad_expected.py \
  fixtures/part-primitives/part-box.json \
  --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/part-box-repeat2 \
  --report /tmp/part-box-repeat2.json \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd

python3 tools/collect_freecad_expected.py \
  --phase topology-state --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/topology-state-repeat2 \
  --report /tmp/topology-state-repeat2.json \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd

python3 tools/collect_freecad_expected.py \
  --all-native --fixtures-root fixtures \
  --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/freecad-expected-all-native \
  --report tools/freecad_expected_parity/reports/all-native-check.v1.json \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
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
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  --out-root tools/freecad_expected_parity/reports/non_cad_smoke

python3 tools/audit_freecad_fixture_authority.py \
  --roles tools/freecad_expected_parity/fixture_roles.v1.json \
  --report tools/freecad_expected_parity/reports/fixture_authority_inventory.v1.json \
  --coverage-report tools/freecad_expected_parity/reports/retained_module_fixture_coverage.v1.json \
  --capability-report tools/freecad_expected_parity/reports/retained_public_capability_coverage.v1.json \
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

- E0：783 条 inventory、role、authority 产物和异常检查已闭合；fixture 仍保持 `fixtures/<phase>/<case>.json`，39 个 phase 全部使用模块-能力名称。
- E1：single/phase/all-native check、独立 repeat 2、分栏 verdict 和 fail-closed 已闭合。
- E2：205 条 unsupported 已逐项证据化分类，无未调查项。
- E3：通用 TypeId/property、Sketcher constraints、history-only ledger、Material、Spreadsheet、promotion/revocation 和 non-CAD smoke 能力已闭合，保留模块 collector 队列为 0。
- E4：native authority 从 480 扩容到 564；历史 phase 的 3 个重复 Loft 镜像保留为同一能力 phase 下的独立 case，其中 2 个 native authority 分别保留 public expected/ledger 谱系；其余新增 case 均由 staging/promotion/post-repeat 回执闭合。
- E5：564 个 native 的全量 repeat 2、strict ledger、producer provenance、逐模块 corpus coverage 和最终 audit 全部通过。
- E6：14 模块、57 能力的独立反向清单已签入；当前 API coverage 明确为 `partial`，不把 corpus closure 升格为全 API 覆盖或 CAD Core runtime parity。

收尾 review 追加了两个 fail-closed 门禁：promotion 必须把 collect receipt 的
fixture/public/ledger SHA 与当前 staging 文件逐项绑定；coverage 必须验证 all-native
producer report 的身份、manifest、两个独立 run 及差异列表，不能只记录报告路径。
这两条门禁继续由 focused regression tests 覆盖；本轮同时增加 mutation 重算、
Mesh Transform no-op 边界和公开能力报告 live 重建一致性测试。
