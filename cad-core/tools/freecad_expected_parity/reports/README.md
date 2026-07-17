# FreeCAD 原生 fixture authority 报告

本目录保存 `collect_freecad_expected.py`、`validate_freecad_expected_ledger.py`、
`audit_freecad_fixture_authority.py`、`promote_freecad_fixture_authority.py` 和
`collect_non_cad_smoke.py`、`collect_material_resolution_contract.py`、
`collect_help_addon_process_contract.py`、
`generate_assembly_solver_support_matrix.py` 和
`validate_assembly_solver_support_matrix.py` 生成的可复查报告。当前受控 producer 入口固定为：

```text
/Users/li/.cargo/bin/FreeCADCmd
```

当前报告记录 FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`；受控入口解析到
`/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd`，二进制 SHA-256 为
`391b638fa65bd761d55291be0a8e7ac22bd4d5ba40ccbd9b14621209a402181a`。
producer 源工作区的 build directory、CMake 信息和 dirty 状态属于可选
provenance；缺失时只产生 warning，不改变 public expected 或 ledger 门禁结论。

## 当前 inventory

`fixture_authority_inventory.v1.json` 由 fixture、role manifest、authority 产物和
探针/晋升回执重建，不是手工统计。

| 项目 | 数量 |
| --- | ---: |
| input | 858 |
| native | 639 |
| protocol_only | 14 |
| unsupported | 205 |
| public expected | 639 |
| ledger | 639 |
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

- `globalFixtureCount = 846`；
- `nativeBaseline = 480`，`nativeAfter = 639`，净扩容 159；
- `coverageStatus = passed`；
- `producerValidation.status = passed`：639 条 all-native repeat 2 直接闭合当前全部 native authority；
- `retainedModuleCollectorImplementationQueueCount = 0`；
- `notInvestigatedCount = 0`；
- `nativeEligibleWithoutAuthorityCount = 0`。

FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy、Material、Part、Sketcher、
PartDesign、Mesh、Spreadsheet、Assembly 和 OndselSolver 均为 `passed`。Material 有 3 个、
Spreadsheet 有 12 个真实 native fixture。
Sketcher 新增 23 个 operation case，按 trim/extend、fillet、clone/block/conic/B-spline
三个最小完整语义批次覆盖 geometry、constraints、solver-facing state 和 InternalShape。
Assembly/OndselSolver 的源码派生支持矩阵显式闭合 13 个 joint type、10 个 marker
family、37 个 `DistanceType`、placement writeback、RackPinion 缺 sliding-part 边界和
真实 zero-radius solver failure；矩阵只声明源码等价类，不冒充笛卡尔积 runtime parity。
Mesh 除 3 个 import case 外，现有 40 个 primitive/set-operation/defect case；本轮补齐
Cone/Cylinder/Ellipsoid/Torus 的创建与属性重算，以及 FixDefects、FillHoles、
FixDeformations、FixDegenerations、FixDuplicatedPoints、FixIndices、FixNonManifolds、
RemoveComponents 的正常、修改和边界分支。Help 和 AddonManager 没有 CAD public root，
但可确定执行的公开分支已由受控 FreeCADCmd repeat-2 process contract 收口；既有
`non_cad_smoke` 只保留为 import/data 历史证据，不再冒充功能 authority。

这个 `passed` 只表示 role、authority 产物、保留模块归属和 producer manifest 闭包，
不表示模块全部公开 API 已被 fixture 覆盖。

## 公开能力反向覆盖

`retained_public_capabilities.v1.json` 是从保留模块源码维护的公开能力/主要运行分支清单；
`retained_public_capability_coverage.v1.json` 再把这份独立清单反向映射到 fixture。当前
清单含 14 个模块、57 条代表性能力：

| 判定 | 数量 | 含义 |
| --- | ---: | --- |
| `covered` | 48 | 有 fixture 执行证据、支持矩阵 receipt 或通过 fail-closed 校验的 hermetic `native_process_test` |
| `thin` | 0 | 只有入口 smoke，尚未断言公开运行分支 |
| `uncovered` | 0 | 源码能力存在且可扩展 native 测试，但当前 corpus 完全没有表达 |
| `non_native_exception` | 9 | 已按 `protocol_only` 或 `unsupported` 记录依据 |

三个结论必须分开读取：

- `fixtureCorpusClosure.status = passed`：858 条 input 的 role/authority corpus 闭合；
- `moduleApiCoverage.status = covered`：上述 57 条反向能力均有执行证据或 source-backed exception；
- `cadCoreRuntimeParity.status = not_evaluated`：FreeCAD native authority 不证明 CAD Core 已逐条对齐。

已覆盖的关键执行分支包括 Part primitive/boolean/import/extrusion/offset/loft/sweep、
Sketcher profile/solver-facing update/external geometry/diagnostics，以及 trim/extend、fillet、
clone/block/conic/B-spline 编辑操作、PartDesign Body
依赖链中的 Pad/Pocket/Revolution/Groove/Hole/DressUp/Pattern/Pipe/Boolean/Binder，以及
Assembly/OndselSolver 的真实求解成功、placement writeback 和失败诊断。它们不再以
TypeId 或属性点名代替执行证据。

Material card/library/model resolution 已由 8-case hermetic process contract 收口，主机资源路径不进入 CAD request graph。MainCmd/Base/MainPy 由 9-case repeat-2 runtime-entrypoint process contract 收口：未知选项、版本信息、指定 SystemExit、Console stream/status、Python exception translation、重复 import identity 和正常 destruct 有实际执行回执；init return100 与 MainCmd 顶层 Base/unknown return1 保留源码依据和 probe close condition。Spreadsheet style/alignment/行列尺寸/merge/split 已由 9-case `spreadsheet-layout` native phase 收口；FreeCADCmd 直接调用 SheetPy 公开 API，发布 getter/operation receipt 和 invalid range/option 原生诊断，不以 CAD Core 是否消费这些字段缩小 authority。Help/AddonManager 的 7-case repeat-2 process contract 使用本地页面、fake repo、mock transport 和 case-local user/config/temp：6 条可执行分支为 native process authority，WebEngine 可见 GUI 分支由实际 `GuiUp=false` probe 保留 source-backed close condition，真实网络请求为 0。

`retained_public_api_surface.v1.json` 进一步建立 14 个保留模块、74 条源码侧公开 API/
主要运行分支分母，不从 fixture TypeId 反推。对应报告
`retained_public_api_coverage.v1.json` 当前为：API surface 74/74 已分类、0 unclassified，
65 covered、0 uncovered、0 thin、9 source-backed non-native exception。由此
`apiSurfaceClosure = passed`、全局 `moduleApiCoverage = passed`，且
`cadCoreRuntimeParity = not_evaluated`。

非 native 例外中，`Mesh::Transform::execute()` 的 FreeCAD 主体当前整段被注释，只返回
成功，因此明确记为 source-backed `unsupported`，不得因 TypeId 已注册而制造几何
fixture。App/PartDesign/Assembly 的 persisted restore、MainPy host embedding 以及直接
Part/Mesh/Spreadsheet 文件输出均明确归为 `protocol_only`，不再混入普通 uncovered。
Help 的 headless local-page 与 browser dispatch 分支，以及 AddonManager 的 metadata、
安装、更新、删除、ZIP 和 NetworkManager queue/request/reply 分支，均由 item-local
FreeCADCmd processCases 覆盖。真实网络、真实用户 Addon 目录和可见 GUI 效果被明确禁止；
仅 WebEngine `GuiUp=true` 分支因 release FreeCADCmd 实测 `GuiUp=false` 保留可复现边界回执。

## 已签入 Gate 报告

| 报告 | 结论 |
| --- | --- |
| `fixture_authority_inventory.v1.json` | 858 个输入；639/14/205 role 闭合；44 个模块-能力 phase；collector/general 和未调查队列清零 |
| `retained_module_fixture_coverage.v1.json` | 固定保留闭包全部闭合；14 个模块均通过，Help/AddonManager 功能分支使用 native process authority |
| `retained_public_capability_coverage.v1.json` | corpus `passed`；57 条能力 48 covered / 9 exception；模块能力 coverage `covered`；CAD Core parity `not_evaluated` |
| `retained_public_api_coverage.v1.json` | source API surface 74/74 闭合；65 covered / 9 exception / 0 uncovered；模块 API `passed`；CAD Core parity `not_evaluated` |
| `assembly_solver_support_matrix.v1.json` | 50 行/51 evidence；13 joint、10 marker、37 DistanceType；0 missing、0 invalid；CAD Core parity `not_evaluated` |
| `process_contract/material-resolution.v1.json` | Material host resource 8 case、两个独立 run；normal/error/cycle 均有结构化 process receipt |
| `process_contract/runtime-entrypoints.v1.json` | MainCmd/Base/MainPy 9 case、repeat 2；8 条 native process 分支通过，init return100 probe 为 source-backed exception |
| `process_contract/help-addonmanager.v1.json` | Help/AddonManager 7 case、repeat 2；6 条 native process 分支通过，WebEngine GUI probe 为 source-backed exception，真实网络请求为 0 |
| `promotions/spreadsheet-layout-post-repeat2.json` | `spreadsheet-layout` 9/9，repeat 2，0 failure/difference；覆盖 layout mutation、overlap 和 native diagnostics |
| `promotions/mesh-primitives-api-closure-post-repeat2.json` | `mesh-primitives` 15/15，repeat 2，0 failure/difference；四类剩余 primitive 的创建和属性重算闭合 |
| `promotions/mesh-defects-api-closure-post-repeat2.json` | `mesh-defects` 17/17，repeat 2，0 failure/difference；八类剩余 defect API 的正常、修改和边界闭合 |
| `all-native-check.v1.json` | 两个独立 FreeCADCmd run 每轮 639/639，0 failure；跨轮 public/canonical-ledger 差异和 variation 均为 0 |
| `promotions/assembly-solve-a4-post-repeat2.json` | `assembly-solve` 77/77，repeat 2，0 failure/difference；覆盖 3 条 A4 新 authority |
| `ledger-strict-validation.v1.json` | 639/639 strict valid，0 failed/error |
| `probes/*.json` | collector gap、原生不可表达边界和 promotion 候选的实际采集证据 |
| `promotions/*.json` | staging 首采、staging repeat 2、事务化 promotion 和 promotion 后 repeat 2 回执 |
| `revocations/*.json` | repeat 中发现不稳定 authority 后的事务化撤销证据 |
| `non_cad_smoke/*.json` | Help、AddonManager 的真实 import/data smoke 证据 |

base all-native 报告的状态必须分开读取：

- `status = passed`
- `publicExpectedStatus = passed`
- `ledgerValidationStatus = passed`
- `ledgerDriftStatus = drifted`
- `producerTraceStatus = not_evaluated`

这里的 `ledgerDriftStatus = drifted` 只表示候选 ledger 与历史 checked-in raw mapped-name
hash 存在 producer-side 漂移；两个独立候选 run 的 `candidateRunLedgerDrifts = []`，
639 个候选 ledger 均通过 validation，public expected 和 canonical ledger 语义差异为 0。
producer trace 默认不生成、不比较，只在 expected/ledger 分叉调查时使用。

M6 与最终 Mesh 扩容修改了 collector 的通用 Spreadsheet operation/projection 和
`Mesh::PropertyMeshKernel` 语义，因此已用同一 `/Users/li/.cargo/bin/FreeCADCmd` 与当前
collector 身份重跑 639-case all-native repeat 2；最终 audit 直接读取
`all-native-check.v1.json`，不再依赖旧的增量或 producer-reproduction receipt。

## 重建与使用

以下命令均从 `cad-core/` 执行。

### 单 case / phase / all-native 复现

```bash
python3 tools/collect_freecad_expected.py \
  fixtures/part-primitives/part-box.json \
  --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/part-box-repeat2 \
  --report /tmp/part-box-repeat2.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd

python3 tools/collect_freecad_expected.py \
  --phase topology-state --check --validate-ledger --repeat 2 \
  --candidate-root /tmp/topology-state-repeat2 \
  --report /tmp/topology-state-repeat2.json \
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
  --capability-report tools/freecad_expected_parity/reports/retained_public_capability_coverage.v1.json \
  --api-coverage-report tools/freecad_expected_parity/reports/retained_public_api_coverage.v1.json \
  --producer-report tools/freecad_expected_parity/reports/all-native-check.v1.json \
  --non-cad-smoke-root tools/freecad_expected_parity/reports/non_cad_smoke \
  --require-coverage-passed

python3 tools/collect_material_resolution_contract.py \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd --repeat 2

python3 tools/collect_help_addon_process_contract.py \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd --repeat 2 \
  --report tools/freecad_expected_parity/reports/process_contract/help-addonmanager.v1.json
```

新 case 必须走 staging 首采、staging repeat 2、
`promote_freecad_fixture_authority.py` 事务化 promotion、promotion 后 checked-in
repeat 2。禁止手改 `*.freecad.json` 或 `*.freecad.ledger.json`，也禁止按
phase/case 名称向 collector 添加特判。若最终重复采集发现 authority 不稳定，使用同一
工具的 revocation 路径原子撤销 expected、ledger、native role 和 promotion 回执，保留
输入及诊断证据。

## 方案闭合状态

- E0：858 条 inventory、role、authority 产物和异常检查已闭合；fixture 仍保持 `fixtures/<phase>/<case>.json`，44 个 phase 全部使用模块-能力名称，其中 43 个包含 native authority。
- E1：single/phase/all-native check、独立 repeat 2、分栏 verdict 和 fail-closed 已闭合。
- E2：205 条 unsupported 已逐项证据化分类，无未调查项。
- E3：通用 TypeId/property、Sketcher constraints、history-only ledger、Material、Spreadsheet、promotion/revocation 和 non-CAD smoke 能力已闭合，保留模块 collector 队列为 0。
- E4：native authority 从 480 扩容到 639；历史 phase 的 3 个重复 Loft 镜像保留为同一能力 phase 下的独立 case，其中 2 个 native authority 分别保留 public expected/ledger 谱系；其余新增 case 均由 staging/promotion/post-repeat 回执闭合。
- E5：639 个 native 的 all-native repeat 2 直接闭合当前 producer provenance；逐模块 corpus coverage、authority audit 和 639/639 strict ledger 均通过。
- E6：14 模块、57 能力的独立反向清单及 74 条源码侧 API surface 已签入；当前 API coverage 为 65 covered / 9 source-backed exception / 0 uncovered，所有公开 API 都有 item-local FreeCADCmd 证据或实际 blocker receipt，不发布 CAD Core runtime parity。

收尾 review 追加了两个 fail-closed 门禁：promotion 必须把 collect receipt 的
fixture/public/ledger SHA 与当前 staging 文件逐项绑定；coverage 必须验证 all-native
producer report 的身份、manifest、两个独立 run 及差异列表，不能只记录报告路径。
这两条门禁继续由 focused regression tests 覆盖；producer gate 同时支持 fail-closed
base+incremental receipt bundle，并显式发布 base 未覆盖的新 authority。既有 mutation 重算、
Mesh Transform no-op 边界、统一 Mesh 公共投影和 API surface 报告 live 重建一致性测试。
