# 【已实现】C13-M5 S1 strict comparator 与生成入口

## 目标

新增一个统一工具链，让 `cad-core` 可以按 expected discovery 重生成 `cad-core-res`，并输出稳定的 strict diff report。

## 建议落点

- `cad-core/tools/compare_freecad_expected.py`
- `cad-core/tools/regenerate_cad_core_res.py`
- `cad-core/tests/test_freecad_expected_public_parity.py`
- `cad-core/tests/topo_naming_state_test_helpers.py` 中可复用 raw hash canonicalization。

## 必做

1. discovery 只收集 `fixtures/<phase>/expected/*.freecad.json`。
2. 生成时只对同名 input 执行 `build/cad-core recompute`，输出到 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`。
3. comparator 支持 `--phase`、`--case`、`--strict`、`--write-current`。
4. strict diff report 至少包含：
   - missing/extra diagnostics。
   - missing/extra results。
   - missing/extra topoNamingState objects。
   - subshape / elementMap / childElementMaps / mapperHistory 差异。
   - geometry numeric diff 分类。
5. normalization 只处理明确非语义漂移，例如 raw mapped-name hash。

## 实现结果

- 已新增 `cad-core/tools/compare_freecad_expected.py`：按 `fixtures/<phase>/expected/*.freecad.json` discovery，支持 `--phase`、`--case`、`--strict`、`--write-current`，strict report 写入 `cad-core/out/freecad-expected-parity/<phase>.json`。
- 已新增 `cad-core/tools/regenerate_cad_core_res.py`：只按 expected discovery 查找同名 input，调用 `build/cad-core recompute`，写入 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`，不写 `expected/`。
- strict report schema 固定为 `cad-core.freecad-expected-parity.v1`，分类包含 diagnostics、results、results.subshapes、topoNamingState.objects、topoNamingState.subshapes、topoNamingState.elementMap、topoNamingState.childElementMaps、topoNamingState.mapperHistory、geometry.numeric。
- raw mapped-name hash 只在 `rawFreecadMappedName`、`mappedName.raw` 等 raw 字段 canonicalize；stableSubname、canonical key、object set、diagnostic code 和 topology publication 仍严格比较。
- 已新增 `cad-core/tests/test_freecad_expected_public_parity.py`，覆盖 discovery 边界、raw hash canonicalization、生成入口不写 expected、c4m6 strict report 生成。
- 当前 `c4m6` strict report 可稳定生成且 status 为 `red`：9 个 case 中 2 个 green、7 个 red；红灯留给 S2/S3 做分类和实现。

## 非目标

- 不在 comparator 中实现业务补偿。
- 不按 fixture 名称忽略字段。
- 不把 known gap 直接当 passed。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core
python3 tools/compare_freecad_expected.py --phase c4m6 --write-current
python3 tools/compare_freecad_expected.py --phase c4m6 --strict
python3 -m unittest tests.test_freecad_expected_public_parity
```
