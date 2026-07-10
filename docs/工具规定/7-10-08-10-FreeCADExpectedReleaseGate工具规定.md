# FreeCAD Expected Release Gate 工具规定

## 目标

`cad-core/tools/freecad_expected_parity/` 是 FreeCAD public expected 与 CAD Core live output 的唯一 release-gate 深模块。外部只使用：

```python
evaluate(EvaluationRequest) -> ParityReport
materialize_current(MaterializeRequest) -> GenerationReport
```

`compare_freecad_expected.py` 与 `regenerate_cad_core_res.py` 只是 CLI adapter；不得在 CLI、unittest 或新脚本各自复制 discovery、diff、豁免或退出码规则。

## 输入边界

- `fixture_roles.v1.json` 是 native / protocol-only / unsupported 的唯一机器来源。role audit 缺项、重复、orphan、stale 或 artifact 不一致，结果为 `invalid`。
- native case 必须有 `.freecad.json` + `.freecad.ledger.json`；每个 live evaluation 复用 strict ledger validator 的真实规则。
- protocol-only case 只由其 `.expeted.json` focused contract 保护，不进入 native release verdict。
- live source 必须运行当前 binary；同一 actual payload 同时用于 diff、registry audit 和 freshness。checked-in current 与该 payload 的 comparison-profile normalized digest 不一致，结果为 `invalid`。

## 三层 verdict

report schema 为 `cad-core.freecad-expected-parity.v2`，分别输出：

| 字段 | 含义 |
| --- | --- |
| `exactStatus` | comparison profile 规范化后的精确比较：`green` / `red` / `not_evaluated`。 |
| `semanticStatus` | 所有 diff 都被精确 registry 消费且 actual contract 成立时才是 `green`。 |
| `releaseStatus` | live-only 的 `green` / `protocol_divergence` / `red` / `invalid` / `not_evaluated`。 |

`protocol_divergence` 不等于 exact green：它只表示 live result 的剩余 diff 全部是已批准且仍满足 consumer contract 的 protocol divergence。0 case、runner 失败、非法 JSON、ledger/role/registry 失败或 stale current 一律是 `invalid`；snapshot 不能给 release 通过结论。

## divergence registry

`protocol_divergences.v1.json` 的 selector 必须精确到 `phase + case + category + kind + path`。一个 diff 恰好匹配 0 或 1 条；duplicate、ambiguous、stale 或未消费 entry 都是 `invalid`。禁止 glob、regex、`endswith()`、whole-case、whole-category 或“所有 mesh/subshapes”放行。

每项还必须有 nested `actualContract`、native expected / CAD Core protocol / frontend impact、authority、可加载的 dotted `contractTests` 与 `removeWhen`。contract 不匹配是 runtime `red`，不是 registry 配置豁免。

## CLI 与退出码

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core

# report-only；valid red 仍可用于诊断
python3 tools/compare_freecad_expected.py --phase c4m6 --strict

# 先以 live binary 原子写同名 current
python3 tools/compare_freecad_expected.py --phase c4m6 --write-current

# release gate：自动 live、ledger preflight 与 current freshness
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate

# 执行本 scope registry 精确引用的 consumer contracts
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate --run-contract-tests
```

`--strict` 可返回 valid red report；但 `invalid` 仍非零。`--release-gate` 对 `red`、`invalid`、`not_evaluated` 或不通过的 gate 返回非零。`--run-contract-tests` 先验证 registry test id 可加载，再以一次 `python3 -m unittest` 执行；缺失、无法加载或失败均非零。

`materialize_current()` 必须先取得 live JSON、执行预检，再原子替换 selected `cad-core-res`。任何 case 失败、0 selection 或 JSON 非法都不得留下半写 current。

## c4m6 当前界限

当前 c4m6 registry 只接受五个精确 frontend transport path：四个 display mesh 和 `results.ProbeSketch.subshapes`。CompoundLink 的 bbox、volume、topology 与 subshape identity 已是 native semantic parity，不能再将整个 result object 作为 divergence 接受。

Hash / foreign-owner 是 request-integrity hard fail，不能以 registry 把它们改回 stale-state recompute。ReferenceShadow 保持 evidence-only；registry 不能把 snapshot BREP 或完整 object state 变成几何输入。

## 非目标

- 不用 parity module 制造 FreeCAD 业务语义、修剪 runtime 输出或手改 collector artifact。
- 不把 ledger、`topoNamingState` 或 `ReferenceShadow.brep` 当服务端 session / 建模输入。
- 不把 S4 的五个 phase family red known gaps 伪装成 release green。
