# 【已实现】C6-M9 S3 BRepFeat 实现或 native-failure 发布收口

## 目标

消费 S2 route。若存在实现型 gap，S3 必须按 Groove UpToFirst + UpToFace 同一 DTO/API 边界批量补 cad-core C++、fixtures、product metadata、focused tests、capability/docs；若 S2 选择 native failure / retained blocker，S3 必须通过 capability + adapter assertion + docs 硬化收口。

## 实现规则

- 不允许只为一个 fixture 加特判。
- 不允许通过 bbox、输出顺序、fixture 名称或后处理修剪伪造成功。
- 不允许把 FreeCAD native failure 写成 expected-backed success。
- 如果发布 CAD Core product non-parity，fixtures 必须明确 native expected 不成立或为 product contract。
- 如果保留 exact blocker，必须同步 `test_adapters.py` 断言并保留 delete condition。

## 可能落点

| route | 落点 |
| --- | --- |
| `backend_gap_requires_implementation` | `cad-core/src/part_design/feature_revolved.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、fixtures、focused tests |
| `cad_core_product_contract_non_parity` | product fixtures、metadata、capability product_contract evidence、adapter assertion |
| `historical_native_failure` | `narrowed_gaps` 或 exact blocker evidence、delete condition、adapter assertion |
| `retained_exact_blocker` | `exact_blockers`、`remaining_gaps`、focused failure tests、docs |

## S3 发布记录

- live cwd：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`5dd70f0ad5`（`5dd70f0ad5 文档：完成 C6-M9 S2 路由裁决`）。
- S3 执行起点 `git -c core.quotepath=false status --short -uall`：空输出，工作区干净。
- S3 执行起点队列：S3/S4/S5 pending；S3 完成后队列应推进到 S4。
- route 消费：S2 已将 `Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 同批裁决为 `historical_native_failure`；S3 未进入 `backend_gap_requires_implementation` 或 `cad_core_product_contract_non_parity`。
- capability 发布：`part_design.revolution_groove.status=supported_c51s1_advanced_with_historical_groove_upto_native_failure`，`remaining_gaps=[]`，`exact_blockers={}`；`partdesign_groove_upto_brepfeat_cut_native_failure` 保留在 `narrowed_gaps` / `field_boundaries.historical_native_evidence`，包含 FreeCAD source、native message、cad-core diagnostic、两个 c51m1 fixtures、delete condition 和 reopen condition。
- adapter assertion：`cad-core/tests/test_adapters.py` 断言该 id 不再位于 active `remaining_gaps` 或 `exact_blockers`，但仍可在 narrowed/historical evidence 中查到。
- fixture guard：`cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 的失败语义保持不变；两个 c51m1 fixtures 仍作为 historical native failure guard。
- 未改动范围：未修改 `cad-core/src/part_design/feature_revolved.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、fixtures、expected、collector 或 P7 failure fixture 语义；未新增 `c6m9` product fixture；未把 native failure 写成 expected-backed success。
- 文档/矩阵：C6-M9 README、主线总入口、步骤索引、S3 文件和矩阵已同步 S3 publication assertion；root `docs/CADCore6.0/README.md` 记录 S3 已完成、S4/S5 仍 pending。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

若 S3 改了 C++ 或 fixtures，补跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures
```

文档校验：

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```

## 通过条件

- S2 选择的 route 有对应代码/fixture/test/capability/docs 或明确 native failure / exact blocker 证据。
- adapter capability focused test 断言最新发布口径。
- S3 文件名和标题标记为 `【已实现】` 后，队列推进到 S4。
