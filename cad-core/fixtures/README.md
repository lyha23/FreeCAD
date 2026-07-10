# Fixture expected 文件约定

`fixtures/<phase>/*.json` 是 CAD Core 输入 fixture。是否进入 native expected discovery，不能再从后缀、case 名或目录猜测；唯一机器来源是：

```text
cad-core/tools/freecad_expected_parity/fixture_roles.v1.json
```

manifest 对每个 input 恰好声明一个 role，并 fail closed：缺失、重复、orphan、stale entry 或 role/artifact 不一致都会使 parity evaluation 无效。

## role 与 artifact

| role | 必需 artifact | 是否进入 native parity / release gate |
| --- | --- | --- |
| `native` | 同名 `.freecad.json` 与 `.freecad.ledger.json` | 是 |
| `protocol_only` | 同名 `.expeted.json` | 否；由 focused protocol test 保护 |
| `unsupported` | manifest 的 reason、authority、nextAction、closeCondition | 否；不得因少发现 case 假绿 |

`legacyNativeExpectedDiscovery=false`：collector、catalog、generator、tests 和 release gate 都消费该 manifest，而不是各自 glob `*.freecad.json`。

## `.freecad.json` 与 ledger

`.freecad.json` 是 collector-owned public expected，`.freecad.ledger.json` 是同次生成的 provenance sidecar；二者必须成对出现，不能手改。accepted ledger 证明 public state 的 projection / coverage / round-trip；rejected ledger 证明 diagnostics-only 请求拒绝，不要求 `topoNamingState` 或 round-trip。

`.freecad.json` 可以表达 FreeCADCmd native semantic result，也可以表达由 collector 同次生成的请求完整性 hard fail。两者都必须由同一 pair 的 ledger 解释；不能用 CAD Core 当前输出补写 expected。

## `.expeted.json`

`.expeted.json` 是人工维护的 CAD Core 产品或诊断合同，拼写沿用现有 `expeted`。它不是 FreeCADCmd native oracle，不进入 native discovery、ledger validator 或 release verdict。若日后能稳定采集原生证据，先生成 `.freecad.json` + ledger，再删除或替换该人工合同。

## `c4m6` topoNamingState 基线

`c4m6` 是 request validation、native result、ReferenceShadow 与 history provenance 的最小完整批次，共 10 个 inputs：9 个 `native`、1 个 `protocol_only`。

- native accepted：`topo-state-first-recompute-empty`、`topo-state-body-tip-stable-recovery`、`topo-state-link-compound-child-maps`、`topo-state-reference-shadow-brep`。其中 CompoundLink expected 有 bbox、volume、topology 与 subshape identity 等 native semantic result。
- native rejected：schema、producer、document hash、object hash、foreign top-level owner。它们必须只返回 `diagnostics`、空 `results`、空 `elementReferenceUpdates`，且不返回新 `topoNamingState`。
- protocol-only：`topo-state-mapper-history-events.expeted.json`。`CadCore::TopoNamingStateProbe` 构造 synthetic helper shape，而 FreeCAD Python 不能导出同一 producer history；因此它由 focused protocol test 保护，明确排除 native release verdict。
- `ReferenceShadow.brep` 只可作为 item-local 单 subshape recovery evidence；不得进入 `topoNamingState`、results 或完整对象状态，也不得被直接复用为建模几何。

display mesh、前端 transport 字段和 producer-local raw/stable token 属于 artifact representation。它们会保留在 artifact diff 观察中，但不进入公共 semantic parity；对应产品字段仍必须由 adapter/consumer contract 测试保护。diagnostics、elementReferenceUpdates、canonical mapped-name、几何公共字段和必需 topoNamingState 仍按 semantic diff 严格比较。`protocol_divergences.v1.json` 继续只处理公共语义层明确批准的例外。

## 常用验证

```bash
cd /Users/li/Chili3DProject/FreeCAD

python3 cad-core/tools/validate_freecad_expected_ledger.py --phase c4m6 --strict

cd cad-core
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate
python3 tools/compare_freecad_expected.py --phase c4m6 --run-contract-tests
```

`--release-gate` 运行 current binary、strict ledger preflight 与 current freshness 校验。snapshot report 只能诊断 exact / semantic diff，不能充当 release 通过证明。
