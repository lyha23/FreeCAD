# C13-M5 S5 release gate 收口

## 实现结果

- `cad-core/tools/freecad_expected_parity/` 成为唯一 parity interface：它统一 fixture role、exact diff、registry、live source、ledger preflight 与 current materialization；CLI 不再自带宽泛 classifier。
- `fixture_roles.v1.json` 为每个 input 固定 native / protocol-only / unsupported。`c4m6` 当前为 9 个 native pair 加 1 个 protocol-only HistoryProbe；后者只由 `.expeted.json` focused contract 保护。
- request integrity 固定为 schema、producer、document/object hash、object/child encoding、foreign top-level owner 均在 recompute 前 diagnostics-only hard fail。hash mismatch 不再作为 stale state 重算。
- CompoundLink 有 native semantic result；HistoryProbe 不再冒充 native geometry；ReferenceShadow 只作为 item-local evidence。
- `protocol_divergences.v1.json` 只登记五个精确 c4m6 transport path。selector、actual contract、consumer test、authority 与 remove condition 缺一不可；没有 whole-result 或 suffix-based acceptance。

## verdict 与范围

report v2 分开 `exactStatus`、`semanticStatus`、`releaseStatus`：exact red 的 c4m6 只要五个精确 registry contract 都成立，可以是 semantic green / live `protocol_divergence`；这不是 exact green。0 case、role/ledger/registry audit 失败、runner/JSON 错误或 live/current 不新鲜一律 `invalid`。

S5 关闭的是 release-gate 基础设施和 c4m6 模板，不关闭 S4 的五个 phase-family known gaps。它们继续在矩阵中保持 red / known gap，必须由后续实现批次关闭。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 --check --skip-unsupported
python3 cad-core/tools/validate_freecad_expected_ledger.py --phase c4m6 --strict

cd cad-core
cmake --build build --target cad-core cad_core_ffi
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate --run-contract-tests
python3 -m unittest tests.test_freecad_expected_public_parity tests.test_topo_naming_state_response
```

此外，工作步骤队列必须为空，矩阵 TSV 字段数与本包 `git diff --check` 必须通过。
