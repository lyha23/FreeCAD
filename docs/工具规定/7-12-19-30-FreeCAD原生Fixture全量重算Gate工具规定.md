# FreeCAD 原生 Fixture 全量重算 Gate 工具规定

## 1. 目标与唯一入口

FreeCAD 原生 fixture 的全量回归、候选产物保存和双跑确定性检查统一扩展在：

```text
cad-core/tools/collect_freecad_expected.py
```

不得为同一工作流另建平行 runner。fixture 发现、采集、既有 comparator 和 ledger 校验必须继续复用 collector 的同一实现。只有 public/ledger 无法对齐且需要定位原因，或任务明确要求内部过程审计时，才按需启用同一 collector 中的 producer trace 诊断。

## 2. 权威层级

- `expected/<case>.freecad.json` 是 FreeCADCmd 生成的 public expected 权威。
- `expected/<case>.freecad.ledger.json` 是同次运行生成的 authority/provenance sidecar；缺失即 hard fail。
- `expected/<case>.freecad.producer-trace.json` 是按需参考的 producer 诊断证据，不是 public/ledger 验收权威，也不是普通回归的必需 companion。
- `expected/<case>.expeted.json` 属于人工协议或诊断证据，不进入 native manifest。
- `cad-core-res/`、`out/` 和 candidate root 都不能反向成为 checked-in 权威。

普通回归只读 checked-in expected。正式刷新只能由单独批准的 oracle-refresh 流程执行，禁止手改 collector-owned JSON。

## 3. Live manifest

`--all-native` 每次从 `fixtures/*/expected/*.freecad.json` 实时发现 case，不写死 phase 或 case 数。每个 case 必须同时存在：

```text
fixtures/<phase>/<case>.json
fixtures/<phase>/expected/<case>.freecad.json
fixtures/<phase>/expected/<case>.freecad.ledger.json
```

任何输入、public expected 或 ledger 缺失都在启动 FreeCADCmd 前失败，并在指定 `--report` 时生成 `stage=preflight` 的机器可读失败收据。producer trace 只有在 trace diagnostic lane 被显式启用时才要求存在；其缺失不能使 public/ledger Gate 失败。

## 4. 标准命令

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/collect_freecad_expected.py \
  --fixtures-root /Users/li/Chili3DProject/FreeCAD/cad-core/fixtures \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  --all-native \
  --check \
  --repeat 2 \
  --candidate-root /tmp/freecad-fixture-gate \
  --validate-ledger \
  --report /tmp/freecad-fixture-regression.json
```

约束：

- `--all-native` 必须与 `--check` 同用，保证不覆盖 checked-in expected。
- `--repeat N` 的 `N > 1` 支持 single-case、`--phase` 和 `--all-native`，并要求 `--check --candidate-root`。
- candidate root 和 report 必须位于 checked-in fixture 树之外。
- 每个重复运行使用一个全新的 `run-a`、`run-b` 等目录；非空旧目录或遗留的 `run-a.report.json` / `run-b.report.json` 直接失败，避免混入历史产物。
- single-case 和 phase 显式选择必须命中至少一个已登记 native authority；zero-case、缺 public expected、缺 ledger 都在采集前失败。

## 5. 比较合同

每次运行对 checked-in artifacts 执行：

1. public expected 使用 collector 既有 public comparator。
2. ledger 使用 collector 既有 ledger comparator，并在 `--validate-ledger` 下执行 strict closure validation。
3. 仅在 public/ledger 差异需要定位或任务明确要求 trace 审计时，producer trace 才分别绑定并校验各自 request/response，再执行 native trace semantic comparison。

第二次及后续运行必须对 run-a 执行同样的 public、ledger comparator，证明公开候选构建确定性。trace comparator 只在按需诊断 lane 中执行，其结果不参与公开一致性 verdict。

trace comparison 不强制完整输入 document graph 的对象双射。合法 trace 可以有意省略 helper/diagnostic object；每份 trace 已分别对自己的 request/response 做闭包校验，native semantic comparison 继续覆盖实际记录的 object、scope、event、snapshot 和 mapper 语义。

## 6. 报告与失败条件

最终报告 schema 为 `freecad-fixture-regression-report/v1`，至少记录：

- 最低 producer 身份：FreeCADCmd 绝对路径、FreeCAD/OCCT 版本和 collector SHA-256。
- 可选 provenance：FreeCADCmd SHA-256、候选源码根/commit/dirty 状态摘要、build 目录、CMakeCache SHA-256、build type、generator、编译器路径和完整调用参数。缺失只进入 `provenanceWarnings`，不改变 public/ledger/repeat verdict。
- live manifest 的 phase/case 数、逐 artifact SHA-256 和 manifest hash。
- 每次运行的 discovered、processed、skipped、failed 和逐 case artifact 状态。
- `publicExpectedStatus`、`ledgerValidationStatus`、`ledgerDriftStatus` 和 `producerTraceStatus` 四个独立 verdict；若启用 trace，再附独立的 producer trace diagnostic。
- run-a/run-b 差异和第一处失败。

满足以下任一条件时 Gate 失败并返回非零：

- manifest 缺件或为空。
- 任一重复运行返回非零、缺少报告或报告不是 `passed`。
- `processed != discovered`、`skipped != 0` 或 `failed != 0`。
- public 或 ledger 任一比较或 validator 失败。
- run-a 与后续运行不一致。
- candidate/report 位于 checked-in fixture 树内，或重复运行目录已有旧文件。

producer trace 的 `different/missing/invalid` 只使 trace diagnostic lane 失败或不可用，不进入 public/ledger `differences`、`firstFailure` 或 Gate status。public/ledger 已一致时，默认不运行 trace comparator，也不需要 trace equal 作为放行证明。

终端退出码只是一项证据；删减验收必须读取最终 JSON 的 `status`、计数、`cases` 和 `firstFailure`。

## 7. Inventory、分类和 checked-in 收据

fixture 库存和非 native 分类统一由以下只读命令重建：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/audit_freecad_fixture_authority.py \
  --report tools/freecad_expected_parity/reports/fixture_authority_inventory.v1.json
```

报告逐项覆盖所有 input，审计 role、public expected、ledger、protocol expected、orphan/duplicate，并把每个 `unsupported` 归入：`collector_general_gap`、`freecad_native_not_expressible`、`non_native_fixture` 或 `not_investigated`。静态分类和 staging probe 都不能自动提升 role；promotion 仍要求 input、public expected、ledger、role 与 producer receipt 同批闭合。

strict ledger 的独立机器收据使用：

```bash
python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --all \
  --strict \
  --report cad-core/tools/freecad_expected_parity/reports/all-native-ledger-validation.v1.json
```

当前 checked-in 报告、重建命令和 verdict 解释见：

```text
cad-core/tools/freecad_expected_parity/reports/README.md
```
