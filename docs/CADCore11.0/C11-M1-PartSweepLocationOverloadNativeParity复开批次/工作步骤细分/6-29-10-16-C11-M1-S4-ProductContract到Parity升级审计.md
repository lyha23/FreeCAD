# C11-M1 S4 ProductContract 到 Parity 升级审计

## 目标

消费 S3 native oracle 结果，把 current C6-M4 product contract 与 native FreeCAD expected 做 comparison。S4 只能在 S3 stable 时打开；如果 S3 是 `notCollected`，S4 必须关闭为 retained non-parity / no-code gate。

## 输入

- S3 native located profile / advanced combined oracle 结果。
- `cad-core/fixtures/c6m4/part-sweep-located-profile-product.json`
- `cad-core/fixtures/c6m4/part-sweep-advanced-combined-product.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`

## Comparison 规则

| 情况 | S4 结论 |
| --- | --- |
| S3 `notCollected` | `diagnostic_retained`；S6 no-code release gate。 |
| S3 stable，current c6m4 输出几何 / metadata / diagnostics 与 native expected 等价 | `no_gap`；S6 只做 capability wording / docs publication。 |
| S3 stable，current c6m4 输出与 native expected 有 request-local mismatch | `backend_gap_candidate`；S6 指定 C++ / fixtures / focused tests。 |
| S3 stable，但差异是 output order / naming order 且几何等价稳定 | `naming_order_difference`；不得当硬失败。 |

## 必须回写的矩阵行

- `C11M1-SCOPE-103`
- `C11M1-SCOPE-104`
- `C11M1-BLOCKER-401`
- `C11M1-CAT-102`
- `C11M1-CAT-103`
- `C11M1-VAL-401..403`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part-sweep-located-profile-product|part-sweep-advanced-combined-product|contract_provenance|location_product_contract|pipeshell_history' cad-core/fixtures/c6m4 cad-core/tests/test_p8_features.py cad-core/src/part/part_sweep.cpp cad-core/src/part/topo_shape_expansion.cpp
rg -n 'C11M1-SCOPE-103|C11M1-BLOCKER-401|backend_gap_candidate|no_gap|diagnostic_retained|naming_order_difference' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
git diff --check
```

若 S4 产生 `backend_gap_candidate`，还必须补充：

- mismatch 的 native expected 路径。
- current cad-core result 路径或 test output。
- exact C++ landing。
- focused test 名称。
- 禁止 shortcut path。

## 非目标

- 不用 fixture 名称、bbox、面积或输出顺序修复 parity。
- 不把 native DocumentObject direct property 缺失误判为 wrapper implementation gap。
- 不删除 current product contract guard，除非 S6 正式升级并保留历史证据。
