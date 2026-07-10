# C13-M5 S3 topoNamingState 发布策略对齐

> 后续状态：本文件保留 S3 当时的 characterization。C13-M5 S5 已以当前接口合同覆盖其运行时结论：document/object hash 与 foreign top-level owner 现在在 recompute 前 hard fail，且 transport divergence 只剩精确 registry 的五个 path；不得再把下述 hash-recompute 或 `intentional_protocol_divergence=8` 视为当前规则。

## 目标

关闭 `c4m6` strict public expected 中属于 topoNamingState publication 的缺口，为后续 phase 提供可复用的发布策略。

## 实现结果

- public object set 已从 response target 扩展为 `ComputeContext` 中已执行且有 `NamedShape` 的对象，并保留 Link、ReferenceShadow、child owner projection。
- mapperHistory 只发布 expected-facing recovery evidence；内部 indexed history 不再无差别进入 public state。
- schema、producer、element-map encoding 继续 hard fail；document/object hash mismatch 按 c4m6 native expected 重算并发布 topoNamingState。
- Link compound child path projection 可解析时，不再保留 `missing_stable_subname`；剩余 Link result 差异登记为 transport metadata intentional divergence。
- `c4m6` strict report 仍为 red，但只剩 `intentional_protocol_divergence`=8；topoNamingState publication、mapperHistory、hash mismatch、stable diagnostic policy 缺口均已清零。

## 必做

1. 完整 public object set：
   - `topoStateObjectNames()` 不只从 response result targets 取对象。
   - 纳入 `ComputeContext` 中已执行且有 `NamedShape` 的对象。
   - 保留 Link / ReferenceShadow / child owner projection。
2. mapperHistory public policy：
   - 区分内部 indexed history 与 expected-facing recovery evidence。
   - 保留 mapperHistoryIds / ambiguity / split / deleted 这类 public 证据。
   - 不把所有内部 mapper history 直接泄漏为 public state。
3. hash mismatch policy：
   - schema / producer / element-map encoding 继续 hard fail。
   - document/object hash mismatch 是否 hard fail 需要按 expected 裁决；若 cad-core 协议选择 hard fail，必须登记 intentional divergence。
4. Link compound stableSubname diagnostics：
   - child path projection 已发布且可解析时，不应继续报 missing stable subname。
   - 诊断收敛后结果发布策略要与 expected 对齐。

## FreeCAD 依据

- `src/App/ElementMap.cpp`
- `src/App/PropertyLinks.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core
python3 tools/compare_freecad_expected.py --phase c4m6 --write-current
python3 tools/compare_freecad_expected.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response tests.test_freecad_expected_public_parity
```
