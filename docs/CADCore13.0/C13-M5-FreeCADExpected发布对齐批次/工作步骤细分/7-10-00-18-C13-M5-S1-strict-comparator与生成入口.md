# C13-M5 S1 strict comparator 与生成入口

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
