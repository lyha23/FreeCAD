# C6-M8 S4 fixtures tests capability docs 发布

## 目标

核对 S3 已发布的 capability、adapter assertion、C6-M8 docs 和 root README。S4 负责让用户和前端看到一致的 surface family 合同；若 S3 的 capability/test 已一致，S4 不重新打开 executor、fixtures 或 expected 批次。

## 动作

1. 复核 `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench` surface family 状态已经采用 S3 发布口径。
2. 复核 `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已锁定 ProjectOnSurface `remaining_gaps=[]`、GUI non-goal、native mapper hidden `narrowed_gaps`。
3. 更新 C6-M8 矩阵，把 S2/S3 route 写成 published / closed / non-goal，并删除仍暗示 S3 未发布的措辞。
4. 更新 `docs/CADCore6.0/README.md` 的 C6-M8 当前状态。
5. S3 未新增 fixtures 或 expected/product metadata；S4 只确认 fixture list、object_fields、diagnostics 和 capability evidence 没有被错误扩大。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'C6-M8|project_on_surface|ruled_surface|loft|sweep|filling|geomplate|remaining_gaps|non_goals|narrowed_gaps' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```

## 通过条件

- capability、adapter assertion、README 和矩阵口径一致。
- ProjectOnSurface 不再同时用 active gap 和 non-goal 表达同一个边界，除非 S2 明确保留并写出 delete condition。
- S4 文件名和标题标记为 `【已实现】` 后，队列推进到 S5。
