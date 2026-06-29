# 【已实现】C11-M2 S4 ProductContract 到 Parity 升级审计

## 目标

消费 S3 native helper 复采集结果，判断 C6-M5 current product contract 是否可以升级为 FreeCAD native parity。S4 只在 stable native oracle 存在时比较；没有 stable oracle 时关闭为 diagnostic retained。

## live baseline

本轮 S4 执行基线：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=e702be0840
git log -1 --oneline=e702be0840 docs: 完成 C11-M2 S3 Filling helper复采集
git -c core.quotepath=false status --short -uall=<clean>
```

S4 起点工作区干净；本步只更新 C11-M2 S4 文档、总入口 / 索引必要状态和 S4 指定矩阵行。

## 比较对象

| scope | native oracle | current cad-core target |
| --- | --- | --- |
| `C11M2-SCOPE-101` | Surface initial face stable expected | `c6m5/part-filling-surface-initial-face-product`、`surface_source_status`、`initial_surface_source_evidence`。 |
| `C11M2-SCOPE-102` | Supports/Orders G1/G2 stable expected | `c6m5/part-filling-support-order-c0-g1-g2-product`、`support_order_source_evidence`、invalid diagnostics。 |
| `C11M2-SCOPE-201` | Explicit params stable expected | `c6m5/part-filling-explicit-params-product`、`params_source_status`、`explicit_param_fields`。 |
| `C11M2-SCOPE-202` | Non-boundary support/order stable expected | `c6m5/part-filling-non-boundary-support-order-product`、`non_boundary_support_order_status`、`Add(edge support order IsBound=false)` / `Add(face order)` evidence。 |

## S4 审计结论

S3 没有任何可进入 S4 的 stable native helper expected：

| scope | S3 结果 | S4 结论 |
| --- | --- | --- |
| `C11M2-SCOPE-101` Surface initial face | `notCollected_s3_reconfirmed`：`helper_surface_initial_face` 返回 `TypeError: argument 2 must be , not Part.Face`，无 stable `shape_summary`。 | 不比较 C6-M5 surface product fixture；保留为 `diagnostic_retained`，不打开 backend gap。 |
| `C11M2-SCOPE-102` Supports/Orders G1/G2 | `notCollected_s3_reconfirmed`：G1 / G2 helper support/order 均返回同类 `TypeError`，direct wrapper `Add(...)` 只是 diagnostic control。 | 不比较 C6-M5 support/order product fixture；保留为 `diagnostic_retained`，不打开 backend gap。 |
| `C11M2-SCOPE-201` Explicit params blocked subset | `notCollected_s3_reconfirmed`：PtsOnCurve、TolG1/TolG2、MaxSegments、all params 为 SIGSEGV/no payload，Anisotropy timeout。 | 不比较 C6-M5 explicit params product fixture；C5-M13 subset 继续只是 expected-backed subset，不证明 blocked params native parity。 |
| `C11M2-SCOPE-202` Non-boundary support/order | `notCollected_s3_reconfirmed`：G1 / G2 non-boundary support/order 为 SIGSEGV/no stable payload。 | 不比较 C6-M5 non-boundary support/order product fixture；C5-M12 no-support subset 继续只是 expected-backed subset。 |
| `C11M2-SCOPE-203` Direct wrapper controls | `dependency_retained_s3_diagnostic_control_only`。 | wrapper controls 不进入 request-local `Part.makeFilledFace(...)` expected；交给 S5 关闭 protocol / non-goal 边界。 |

C6-M5 四个 product fixtures 仍是当前 CAD Core product-contract non-parity evidence：

- `part-filling-surface-initial-face-product`
- `part-filling-support-order-c0-g1-g2-product`
- `part-filling-explicit-params-product`
- `part-filling-non-boundary-support-order-product`

本轮没有 native expected 与 current output 的可比较对，因此 S4 关闭为 `diagnostic_retained` / no-code gate：不新增 `backend_gap_candidate`，不创建 C11-M2 expected / fixtures，不修改 cad-core C++ / tests / collectors / capability，也不把 C6-M5 product contract 声明为 FreeCAD parity。

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

## S4 矩阵回写

- `c11m2_part_workbench_filling_native_helper_scope_review_matrix.tsv`：`C11M2-SCOPE-301` 关闭为 `diagnostic_retained_s4_no_stable_native_expected`。
- `c11m2_part_workbench_filling_native_helper_blocker_queue.tsv`：`C11M2-BLOCKER-401` 关闭为 `closed_s4_diagnostic_retained`。
- `c11m2_part_workbench_filling_native_helper_backend_gap_classification.tsv`：`C11M2-CAT-301` 标为 `no_s4_diagnostic_retained`；S4 不产生 `backend_gap_candidate`。
- `c11m2_part_workbench_filling_native_helper_oracle_fixture_matrix.tsv`：`C11M2-ORC-301` 记录 comparison gated / diagnostic retained；`C11M2-ORC-401` 保持 S6 发布闸门，但消费 S4 no-gap-candidate 结论。
- `c11m2_part_workbench_filling_native_helper_validation_matrix.tsv`：`C11M2-VAL-401..403` 记录 product-contract evidence grep、classification grep 和 S4 后队列检查。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part-filling-surface-initial-face-product|part-filling-support-order-c0-g1-g2-product|part-filling-explicit-params-product|part-filling-non-boundary-support-order-product' cad-core/fixtures/c6m5 cad-core/tests/test_p8_features.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
rg -n 'C11M2-CAT-301|backend_gap_candidate|diagnostic_retained|no_gap|stable_native_expected|notCollected' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/工作步骤细分 --format markdown
rg -n '[ \t]$' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次 docs/CADCore11.0/README.md
git diff --check
```

通过条件：

- S3 的 stable native oracle 集合为空，因此没有 row 进入 current-output comparison。
- C6-M5 product fixtures 只作为 current product-contract non-parity evidence 保留。
- 没有 stable native expected 的 row 只能是 `diagnostic_retained` / no-code gate。
- S4 没有 `backend_gap_candidate`，S6 不得基于 S4 打开 C++ implementation。

## 非目标

- S4 不修代码。
- S4 不创建 adapter patch。
- S4 不把 wrapper lifecycle 或 GUI 行当 backend gap。

## 完成状态

S4 文件已在验证后重命名为 `6-29-12-28-【已实现】C11-M2-S4-ProductContract到Parity升级审计.md`，并同步更新 C11-M2 总入口、工作步骤索引和 S4 矩阵状态。S5/S6 仍待执行。
