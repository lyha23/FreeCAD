# 【已实现】C6-M9 S4 fixtures tests capability docs 发布

## 目标

核对 S3 的 code/product/native-failure 结果、capability、adapter assertion、C6-M9 docs 和 root README。S4 负责让用户和前端看到一致的 `part_design.revolution_groove` 合同。

## 动作

1. 复核 `cad-core/src/runtime/capability_contract.cpp` 的 `part_design.revolution_groove` 状态已经采用 S3 发布口径。
2. 复核 `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已锁定 Groove UpTo route。
3. 更新 C6-M9 矩阵，把 S2/S3 route 写成 published / closed / retained / historical。
4. 更新 `docs/CADCore6.0/README.md` 的 C6-M9 当前状态。
5. 复核 fixtures、object_fields、diagnostics 和 capability evidence 没有被错误扩大。

## S4 发布记录

- live cwd：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`65cb5c2369`（`65cb5c2369 能力：完成 C6-M9 S3 native failure 发布`）。
- S4 执行起点 `git -c core.quotepath=false status --short -uall`：空输出，工作区干净。
- S4 执行起点队列：S4/S5 pending；S4 完成后队列应推进到 S5。
- capability 复核：`cad-core/src/runtime/capability_contract.cpp` 已发布 `part_design.revolution_groove.status=supported_c51s1_advanced_with_historical_groove_upto_native_failure`，`remaining_gaps=[]`，`exact_blockers={}`；`partdesign_groove_upto_brepfeat_cut_native_failure` 保留在 `narrowed_gaps` / `field_boundaries.historical_native_evidence`。
- adapter assertion 复核：`cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已断言同一 id 不在 active `remaining_gaps` / `exact_blockers`，但仍在 narrowed/historical evidence 中，fixtures 仍是两个 `c51m1` Groove UpTo failure guards。
- fixture/test 复核：`cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 仍保持 failure-oriented guard；S4 未改 fixtures、expected、collector、`feature_revolved.cpp`、`topo_shape_expansion.cpp` 或 P7 failure fixture 语义，未新增 product fixtures。
- docs/矩阵发布：C6-M9 README、主线总入口、方案、步骤索引、根 `docs/CADCore6.0/README.md` 与 `矩阵/*.tsv` 已同步为 S4 publication verified；S2/S3 route 保持 `historical_native_failure`，公开状态为 published / closed historical evidence，不声明 product success 或 FreeCAD parity。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'C6-M9|revolution_groove|partdesign_groove_upto_brepfeat_cut_native_failure|remaining_gaps|exact_blockers|narrowed_gaps|product_contract' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```

## 验收记录

- adapter focused test：`Ran 1 test in 0.234s`，`OK`。
- publication grep：通过，最终输出可定位 C6-M9 root README、C6-M9 主线 docs/矩阵、`capability_contract.cpp` 和 `test_adapters.py` 的 `remaining_gaps=[]`、`exact_blockers={}`、`narrowed_gaps` / historical native evidence 口径。
- TSV 字段数检查：通过，全部 C6-M9 `矩阵/*.tsv` 行字段数与表头一致。
- `git diff --check -- cad-core docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md`：通过。
- queue 检查：S4 已跳过，当前队列推进到 `6-25-12-20-C6-M9-S5-阶段回归与release-gate.md`。

## 通过条件

- capability、adapter assertion、README 和矩阵口径一致。
- Groove UpTo blocker 不再处于模糊状态：要么已实现并发布 product/non-parity，要么保留为明确 historical/native/exact blocker。
- S4 文件名和标题标记为 `【已实现】` 后，队列推进到 S5。
