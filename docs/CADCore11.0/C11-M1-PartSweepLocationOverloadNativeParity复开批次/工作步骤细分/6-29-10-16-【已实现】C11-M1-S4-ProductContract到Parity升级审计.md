# 【已实现】C11-M1 S4 ProductContract 到 Parity 升级审计

## 目标

消费 S3 native oracle 结果，把 current C6-M4 product contract 与 native FreeCAD expected 做 comparison。S4 只能在 S3 stable 时打开；如果 S3 是 `notCollected`，S4 必须关闭为 retained non-parity / no-code gate。

## live baseline

本轮 S4 执行基线：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=c42c3c2d39
git log -1 --oneline=c42c3c2d39 docs: 完成 C11-M1 S3 原生 LocationOverload 复采集
git -c core.quotepath=false status --short -uall=<clean>
```

S4 起点工作区干净；本步只允许更新
`docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次`
内 S4 文档、总入口 / 索引必要状态和本步骤要求的矩阵行。S4 不运行
FreeCADCmd，不新增 fixture，不改 `cad-core/src`、tests、fixtures、expected、
capability 或历史 guard。

## 输入

- S3 native located profile / advanced combined oracle 结果。
- `docs/temp/6-29-10-15-c11m1-s3-sweep-location-combined-probe-output.json`
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

## S4 审计结论

| 项 | S4 结论 | 依据 | 后续路由 |
| --- | --- | --- | --- |
| located profile native oracle | `notCollected` retained | S3 probe 中 `located_free_vertex`、`located_profile_owned_vertex`、`located_profile_coordinate_free_vertex`、`located_spine_owned_vertex`、`located_open_wire_profile`、call-order/tolerance 变体均在 `builder.build()` 返回 `OCCError: NCollection_Array1::Value`；没有 stable native `shape_summary`，也没有 `cad-core/fixtures/c11m1` native expected。 | 不能做 parity comparison；不能生成 `backend_gap_candidate`。 |
| no-location control | control only | `plain_control` 和 `combined_no_location_control` 能返回 Shell shape summary，但不经过 `add(Profile, Location, WithContact, WithCorrection)` located overload。 | 不能作为 located parity oracle。 |
| current c6m4 located product contract | comparison target retained | `part-sweep-located-profile-product` fixture、expected、`test_p8_features.py`、`part_sweep.cpp` 和 `topo_shape_expansion.cpp` 均保留 `cad_core_product_contract_non_parity`、`freecadcmd_location_overload_status=notCollected`、`part_sweep:location_product_contract_profile_placement` / `pipeshell_history` 证据。 | 保留为 CAD Core product-contract non-parity evidence，不改写成 native expected。 |
| advanced combined product contract | location dependency-retained | advanced combined 的 auxiliary/tolerance/location 变体在 S3 仍依赖 located overload，`combined_no_location_control` 不是 located parity。 | 保留 C6-M4 advanced product non-parity 与 c5m10 advanced guard。 |

因此 S4 关闭为 `diagnostic_retained` / no-code gate：current C6-M4 product
contract 是 future comparison target，但当前缺少 stable native oracle，不能比较
FreeCAD parity，不能判断 `no_gap`、`naming_order_difference` 或
`backend_gap_candidate`。S6 后续只能做 no-code retained non-parity release
gate，除非另有 stable native oracle。

## 必须回写的矩阵行

- `C11M1-SCOPE-103`
- `C11M1-SCOPE-104`
- `C11M1-BLOCKER-401`
- `C11M1-CAT-102`
- `C11M1-CAT-103`
- `C11M1-VAL-401..403`

## S4 矩阵回写

- `c11m1_part_sweep_location_overload_scope_review_matrix.tsv`：
  `C11M1-SCOPE-103` 已标为 `diagnostic_retained_s4`，
  `C11M1-SCOPE-104` 已标为 `dependency_retained_s4`。
- `c11m1_part_sweep_location_overload_blocker_queue.tsv`：
  `C11M1-BLOCKER-401` 已关闭为
  `closed_s4_diagnostic_retained_no_native_oracle`。
- `c11m1_part_sweep_location_overload_backend_gap_classification.tsv`：
  `C11M1-CAT-102` / `C11M1-CAT-103` 保持 no-C++ gate，不产生
  `backend_gap_candidate`。
- `c11m1_part_sweep_location_overload_validation_matrix.tsv`：
  补齐 `C11M1-VAL-401..403`，覆盖 product target rg、矩阵 rg 和 TSV
  field-count 检查。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part-sweep-located-profile-product|part-sweep-advanced-combined-product|contract_provenance|location_product_contract|pipeshell_history' cad-core/fixtures/c6m4 cad-core/tests/test_p8_features.py cad-core/src/part/part_sweep.cpp cad-core/src/part/topo_shape_expansion.cpp
rg -n 'C11M1-SCOPE-103|C11M1-BLOCKER-401|backend_gap_candidate|no_gap|diagnostic_retained|naming_order_difference' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
git diff --check
```

S4 已按本节命令验收通过：product contract target rg 命中 c6m4 fixtures /
expected、focused tests 和 C++ metadata / history；矩阵 rg 命中
`diagnostic_retained`、`C11M1-SCOPE-103` 和 `C11M1-BLOCKER-401` 等 gate
状态；TSV field-count 检查通过；`git diff --check` 通过。本步骤未运行
FreeCADCmd、cad-core build 或 cad-core unittest。

若 S4 产生 `backend_gap_candidate`，还必须补充：

- mismatch 的 native expected 路径。
- current cad-core result 路径或 test output。
- exact C++ landing。
- focused test 名称。
- 禁止 shortcut path。

本轮 S4 没有产生 `backend_gap_candidate`，所以上述 implementation 补充项不适用。

## 非目标

- 不用 fixture 名称、bbox、面积或输出顺序修复 parity。
- 不把 native DocumentObject direct property 缺失误判为 wrapper implementation gap。
- 不删除 current product contract guard，除非 S6 正式升级并保留历史证据。
- 不把 `notCollected` 当 backend gap。
- 不把 c6m4 product fixtures 改写成 native expected。
- 不把 no-location control 当 located parity。
