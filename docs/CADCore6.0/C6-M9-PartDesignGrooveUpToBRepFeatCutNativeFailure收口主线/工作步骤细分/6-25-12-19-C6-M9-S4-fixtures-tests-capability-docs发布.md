# C6-M9 S4 fixtures tests capability docs 发布

## 目标

核对 S3 的 code/product/native-failure 结果、capability、adapter assertion、C6-M9 docs 和 root README。S4 负责让用户和前端看到一致的 `part_design.revolution_groove` 合同。

## 动作

1. 复核 `cad-core/src/runtime/capability_contract.cpp` 的 `part_design.revolution_groove` 状态已经采用 S3 发布口径。
2. 复核 `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已锁定 Groove UpTo route。
3. 更新 C6-M9 矩阵，把 S2/S3 route 写成 published / closed / retained / historical。
4. 更新 `docs/CADCore6.0/README.md` 的 C6-M9 当前状态。
5. 复核 fixtures、object_fields、diagnostics 和 capability evidence 没有被错误扩大。

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

## 通过条件

- capability、adapter assertion、README 和矩阵口径一致。
- Groove UpTo blocker 不再处于模糊状态：要么已实现并发布 product/non-parity，要么保留为明确 historical/native/exact blocker。
- S4 文件名和标题标记为 `【已实现】` 后，队列推进到 S5。
