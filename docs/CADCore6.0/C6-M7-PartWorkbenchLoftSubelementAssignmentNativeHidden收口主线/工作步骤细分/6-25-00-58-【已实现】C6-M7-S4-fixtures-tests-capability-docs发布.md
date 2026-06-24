# 【已实现】C6-M7-S4 fixtures tests capability docs 发布

## 目标

把 S3 的实现或收窄结果发布到 fixtures、focused tests、capability contract 和 C6-M7 文档矩阵。S4 只做发布同步，不引入新语义。

## 执行基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=b831431c8f`。
- `git log -1 --oneline=b831431c8f feat: 实现 C6-M7 Loft 子元素合同`。
- S4 开始时工作区干净。
- S4 开始时 C6-M7 队列从 `6-25-00-58-C6-M7-S4-fixtures-tests-capability-docs发布.md` 继续。

## 产物

- `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.loft` 已发布 C6-M7 product fixtures、covered/request-local product-contract evidence、`cad_core_product_contract_non_parity` provenance 和 `narrowed_gaps` historical evidence。
- `part_workbench.loft.remaining_gaps=[]`；原 `part_loft_subelement_assignment_native_hidden` 已从 active implementation gap 转入 `narrowed_gaps` / field boundary。
- `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已同步 status、fixtures、diagnostics、request_local_boundaries、field_boundaries、remaining_gaps、narrowed_gaps 和 non_goals 断言。
- `cad-core/tests/test_p8_features.py` / `tests/test_expected_fixtures.py` 保留 S3 valid/invalid product fixtures 和 C5-M12 native-hidden diagnostic expected 覆盖。
- C6-M7 README、主线总入口、方案、步骤总入口、矩阵和 `docs/CADCore6.0/README.md` 已同步。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features -k loft
python3 -m unittest tests.test_expected_fixtures -k loft
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```

S4 实测：

- `cmake --build build` 通过（用于刷新 capability FFI/CLI 产物）。
- `python3 -m unittest tests.test_p8_features -k loft`：12 tests OK。
- `python3 -m unittest tests.test_expected_fixtures -k loft`：2 tests OK。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`：1 test OK。
- S4 rename 后 queue 只剩 S5 pending。

## 非目标

- 不跑 heavy gate。
- 不做 Surface Family freeze。
- 不做 FreeCAD native selected subelement expected。
- 不做 PartDesign Loft。
