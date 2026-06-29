# C11-M2 S4 ProductContract 到 Parity 升级审计

## 目标

消费 S3 native helper 复采集结果，判断 C6-M5 current product contract 是否可以升级为 FreeCAD native parity。S4 只在 stable native oracle 存在时比较；没有 stable oracle 时关闭为 diagnostic retained。

## 比较对象

| scope | native oracle | current cad-core target |
| --- | --- | --- |
| `C11M2-SCOPE-101` | Surface initial face stable expected | `c6m5/part-filling-surface-initial-face-product`、`surface_source_status`、`initial_surface_source_evidence`。 |
| `C11M2-SCOPE-102` | Supports/Orders G1/G2 stable expected | `c6m5/part-filling-support-order-c0-g1-g2-product`、`support_order_source_evidence`、invalid diagnostics。 |
| `C11M2-SCOPE-201` | Explicit params stable expected | `c6m5/part-filling-explicit-params-product`、`params_source_status`、`explicit_param_fields`。 |
| `C11M2-SCOPE-202` | Non-boundary support/order stable expected | `c6m5/part-filling-non-boundary-support-order-product`、`non_boundary_support_order_status`、`Add(edge support order IsBound=false)` / `Add(face order)` evidence。 |

## 结果分类

- `no_gap`: native expected 与 current cad-core request-local output 一致；只需 docs / capability 口径保留。
- `diagnostic_retained`: S3 没有 stable expected，或 native helper 只产出 crash / timeout / notCollected；不得创建 backend gap。
- `backend_gap_candidate`: stable native expected 存在，且 current cad-core request-local output 有可复现 mismatch；进入 S6 implementation。
- `non_goal`: mismatch 来自 native DocumentObject、GUI、wrapper lifecycle、cross-request state 或 direct wrapper branch；交给 S5/S6 关闭。

## 必须回写的矩阵行

- `C11M2-SCOPE-301`
- `C11M2-BLOCKER-401`
- `C11M2-CAT-301`
- `C11M2-ORC-301..401`
- `C11M2-VAL-401..403`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part-filling-surface-initial-face-product|part-filling-support-order-c0-g1-g2-product|part-filling-explicit-params-product|part-filling-non-boundary-support-order-product' cad-core/fixtures/c6m5 cad-core/tests/test_p8_features.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
rg -n 'C11M2-CAT-301|backend_gap_candidate|diagnostic_retained|no_gap|stable_native_expected|notCollected' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
git diff --check
```

通过条件：

- 每个 stable native oracle 都有 explicit comparison conclusion。
- 每个 mismatch 都能追溯到 native expected、current cad-core output、FreeCAD source authority 和 focused test route。
- 没有 stable native expected 的 row 只能是 `diagnostic_retained` / no-code gate。
- 未通过 S4 的 row 不得进入 S6 C++ implementation。

## 非目标

- S4 不修代码。
- S4 不创建 adapter patch。
- S4 不把 wrapper lifecycle 或 GUI 行当 backend gap。
